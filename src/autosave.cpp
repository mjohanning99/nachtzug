#include "autosave.h"
#include "app.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

QString Autosave::folder(const QString& dir) {
    const QString d = dir.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/autosave") : dir;
    QDir().mkpath(d);
    return d;
}

Autosave::Autosave(App* a, const QString& dir, int intervalMs) : QObject(a), app(a), dir_(folder(dir)) {
    // each running Nachtzug has its own copy, held by a lock file: two windows open at once never mix them up,
    // and a copy whose lock nobody holds any more was left behind by a run that did not end normally
    for (int attempt = 0; attempt < 5; attempt++) {   // without its lock, another Nachtzug would take this copy for a leftover
        const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
        songFile_ = QDir(dir_).filePath(QStringLiteral("session-%1.nachtzug").arg(id));
        metaFile_ = QDir(dir_).filePath(QStringLiteral("session-%1.json").arg(id));
        lock_ = std::make_unique<QLockFile>(QDir(dir_).filePath(QStringLiteral("session-%1.lock").arg(id)));
        lock_->setStaleLockTime(0);
        if (lock_->tryLock(100)) break;
        lock_.reset();
    }
    if (!lock_) songFile_.clear();   // no lock to be had: no spare copy rather than an unsafe one
    connect(app, &App::changed, this, [this] { changed_ = true; });
    connect(&timer_, &QTimer::timeout, this, &Autosave::check);
    timer_.start(intervalMs);
}

Autosave::~Autosave() = default;

void Autosave::check() {
    if (!app->dirty) {   // saved (or a fresh song): nothing to keep
        if (QFile::exists(songFile_)) { QFile::remove(songFile_); QFile::remove(metaFile_); }
        changed_ = false;
        return;
    }
    if (changed_) writeNow();
}

void Autosave::writeNow() {
    if (!app->dirty || songFile_.isEmpty()) return;
    QSaveFile f(songFile_);   // written to the side and swapped in, so a crash while writing never leaves half a song
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write(QJsonDocument(app->song.toJson()).toJson(QJsonDocument::Compact));
    if (!f.commit()) return;
    QJsonObject m;
    m["original"] = app->filePath;
    m["when"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    QSaveFile mf(metaFile_);
    if (mf.open(QIODevice::WriteOnly)) { mf.write(QJsonDocument(m).toJson()); mf.commit(); }
    changed_ = false;
}

void Autosave::finish() {
    timer_.stop();
    QFile::remove(songFile_); QFile::remove(metaFile_);
    if (lock_) lock_->unlock();
}

QList<Autosave::Leftover> Autosave::leftovers(const QString& dir) {
    const QString d = folder(dir);
    QList<Leftover> out;
    for (const QFileInfo& fi : QDir(d).entryInfoList({QStringLiteral("session-*.nachtzug")}, QDir::Files)) {
        const QString base = fi.completeBaseName();
        Leftover l;
        l.song = fi.absoluteFilePath();
        l.meta = QDir(d).filePath(base + QStringLiteral(".json"));
        l.lock = QDir(d).filePath(base + QStringLiteral(".lock"));
        QLockFile probe(l.lock);
        probe.setStaleLockTime(0);
        if (!probe.tryLock(0)) continue;   // that Nachtzug is still running
        probe.unlock();
        QFile mf(l.meta);
        const QJsonObject m = mf.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(mf.readAll()).object() : QJsonObject();
        l.original = m["original"].toString();
        l.when = QDateTime::fromString(m["when"].toString(), Qt::ISODate);
        if (!l.when.isValid()) l.when = fi.lastModified();
        out << l;
    }
    std::sort(out.begin(), out.end(), [](const Leftover& a, const Leftover& b) { return a.when > b.when; });
    return out;
}

void Autosave::discard(const Leftover& l) { QFile::remove(l.song); QFile::remove(l.meta); QFile::remove(l.lock); }

bool Autosave::restore(const Leftover& l) {
    if (!app->load(l.song)) return false;
    app->filePath = l.original;   // Save writes to where the song came from (or asks, for a song never saved)
    app->dirty = true;
    emit app->changed();
    discard(l);
    return true;
}

bool Autosave::offerRecovery(QWidget* parent) {
    const QList<Leftover> list = leftovers(dir_);
    if (list.isEmpty()) return false;
    const Leftover& l = list.first();
    const QString name = l.original.isEmpty() ? tr("an unsaved song") : tr("the song “%1”").arg(QFileInfo(l.original).completeBaseName());
    QMessageBox box(QMessageBox::Question, tr("Bring back your song?"),
                    tr("Last time Nachtzug did not close normally. A copy of %1 from %2 was kept, with the changes that were not saved.")
                        .arg(name, QLocale().toString(l.when, QLocale::ShortFormat)),
                    QMessageBox::NoButton, parent);
    box.setInformativeText(tr("Bring it back to carry on where you were, or throw the copy away."));
    QPushButton* back = box.addButton(tr("Bring it back"), QMessageBox::AcceptRole);
    QPushButton* away = box.addButton(tr("Throw it away"), QMessageBox::DestructiveRole);
    back->setObjectName("primary");
    box.setDefaultButton(back);
    box.exec();
    bool restored = false;
    if (box.clickedButton() == back) restored = restore(l);
    else if (box.clickedButton() == away) discard(l);
    // older copies are rarely wanted; they go once they are a week old
    for (int i = 1; i < list.size(); i++) if (list[i].when.daysTo(QDateTime::currentDateTime()) > 7) discard(list[i]);
    if (restored) emit app->status(tr("Your song is back. Save it to keep it."));
    return restored;
}
