// Your own sounds, saved as small files: see mysounds.h.
#include "mysounds.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <algorithm>

namespace mysounds {

const char* groupName(int group) {
	static const char* const names[GroupCount] = {
	    QT_TRANSLATE_NOOP("Model", "Bass"), QT_TRANSLATE_NOOP("Model", "Sequence"), QT_TRANSLATE_NOOP("Model", "Pad"),
	    QT_TRANSLATE_NOOP("Model", "Lead")};
	return names[std::clamp(group, 0, GroupCount - 1)];
}
const char* groupFolder(int group) {
	static const char* const folders[GroupCount] = {"bass", "sequence", "pad", "lead"};
	return folders[std::clamp(group, 0, GroupCount - 1)];
}

QString folder() {
	return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/sounds");
}

static QJsonObject toJson(const Sound& s) {
	QJsonObject o;
	o["format"] = 1;
	o["kind"] = QStringLiteral("nachtzug-sound");
	o["name"] = s.name;
	o["group"] = QString::fromLatin1(groupFolder(s.group));
	o["words"] = QJsonArray::fromStringList(s.words);
	o["starred"] = s.starred;
	o["patch"] = patchToJson(s.patch);
	return o;
}

bool read(const QString& path, Sound& out) {
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly)) return false;
	const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
	if (o["kind"].toString() != QLatin1String("nachtzug-sound") || !o["patch"].isObject()) return false;
	Sound s;
	s.name = o["name"].toString().trimmed();
	if (s.name.isEmpty()) s.name = QFileInfo(path).completeBaseName();
	const QString g = o["group"].toString();
	for (int i = 0; i < GroupCount; i++)
		if (g == QLatin1String(groupFolder(i))) s.group = i;
	for (const QJsonValue& w : o["words"].toArray())
		if (!w.toString().trimmed().isEmpty()) s.words << w.toString().trimmed();
	s.starred = o["starred"].toBool();
	patchFromJson(o["patch"].toObject(), s.patch);
	s.patch.name = s.name;
	s.file = QFileInfo(path).absoluteFilePath();
	out = s;
	return true;
}

std::vector<Sound> all() {
	std::vector<Sound> out;
	for (int g = 0; g < GroupCount; g++) {
		QDir d(folder() + '/' + QString::fromLatin1(groupFolder(g)));
		for (const QFileInfo& fi : d.entryInfoList({QStringLiteral("*.nzsound")}, QDir::Files)) {
			Sound s;
			if (read(fi.absoluteFilePath(), s)) out.push_back(s);
		}
	}
	std::stable_sort(out.begin(), out.end(), [](const Sound& a, const Sound& b) {
		return a.group != b.group ? a.group < b.group : QString::localeAwareCompare(a.name, b.name) < 0;
	});
	return out;
}

// a file name from the sound's name: letters, digits, spaces and dashes; a number added when it is taken
static QString freeFile(const Sound& s) {
	const QString dir = folder() + '/' + QString::fromLatin1(groupFolder(s.group));
	QString base = s.name;
	base.replace(QRegularExpression(QStringLiteral("[^\\w \\-]")), QStringLiteral("_"));
	base = base.trimmed().left(60);
	if (base.isEmpty()) base = QStringLiteral("Sound");
	QString path = dir + '/' + base + QStringLiteral(".nzsound");
	for (int i = 2; QFileInfo::exists(path); i++)
		path = dir + '/' + base + QStringLiteral(" %1.nzsound").arg(i);
	return path;
}

bool save(Sound& s, QString* error) {
	s.name = s.name.trimmed();
	if (s.name.isEmpty()) {
		if (error) *error = QObject::tr("A sound needs a name.");
		return false;
	}
	const QString wantDir = folder() + '/' + QString::fromLatin1(groupFolder(s.group));
	// a sound moved to another group moves to that group's folder
	if (!s.file.isEmpty() && QFileInfo(s.file).absolutePath() != QFileInfo(wantDir).absoluteFilePath()) {
		QFile::remove(s.file);
		s.file.clear();
	}
	if (!QDir().mkpath(wantDir)) {
		if (error) *error = QObject::tr("Could not make the folder %1.").arg(QDir::toNativeSeparators(wantDir));
		return false;
	}
	const QString path = s.file.isEmpty() ? freeFile(s) : s.file;
	s.patch.name = s.name;
	QSaveFile f(path);
	if (!f.open(QIODevice::WriteOnly) || f.write(QJsonDocument(toJson(s)).toJson()) < 0 || !f.commit()) {
		if (error) *error = QObject::tr("Could not write %1.").arg(QDir::toNativeSeparators(path));
		return false;
	}
	s.file = QFileInfo(path).absoluteFilePath();
	return true;
}

bool writeTo(const Sound& s, const QString& path, QString* error) {
	QSaveFile f(path);
	if (!f.open(QIODevice::WriteOnly) || f.write(QJsonDocument(toJson(s)).toJson()) < 0 || !f.commit()) {
		if (error) *error = QObject::tr("Could not write %1.").arg(QDir::toNativeSeparators(path));
		return false;
	}
	return true;
}

bool rename(Sound& s, const QString& name, QString* error) {
	if (name.trimmed().isEmpty()) {
		if (error) *error = QObject::tr("A sound needs a name.");
		return false;
	}
	const QString old = s.file;
	Sound renamed = s;
	renamed.name = name.trimmed();
	renamed.file.clear();
	if (!save(renamed, error)) return false;
	if (!old.isEmpty() && old != renamed.file) QFile::remove(old);
	s = renamed;
	return true;
}

bool remove(const Sound& s) {
	return !s.file.isEmpty() && QFile::remove(s.file);
}

bool setStarred(Sound& s, bool on) {
	s.starred = on;
	return save(s);
}

bool bringIn(const QString& path, Sound* out, QString* error) {
	Sound s;
	if (!read(path, s)) {
		if (error) *error = QObject::tr("%1 is not a Nachtzug sound.").arg(QFileInfo(path).fileName());
		return false;
	}
	s.file.clear();
	if (!save(s, error)) return false;
	if (out) *out = s;
	return true;
}

void noteUsed(const Sound& s) {
	if (s.file.isEmpty()) return;
	QSettings set;
	QStringList r = set.value(QStringLiteral("sounds/recent")).toStringList();
	r.removeAll(s.file);
	r.prepend(s.file);
	set.setValue(QStringLiteral("sounds/recent"), r.mid(0, 5));
}

QStringList recent() {
	QStringList r;
	for (const QString& f : QSettings().value(QStringLiteral("sounds/recent")).toStringList())
		if (QFileInfo::exists(f)) r << f;
	return r;
}

} // namespace mysounds
