// Autosave: while you work, the song is quietly written to a spare copy every half minute. When Nachtzug ends the
// normal way the copy is deleted; if it crashed (or the computer went off), the next start offers to bring it back.
#pragma once
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QTimer>
#include <memory>

class App;
class QLockFile;
class QWidget;

class Autosave : public QObject {
    Q_OBJECT
public:
    // dir: where the copies go (empty: the program's own data folder)
    explicit Autosave(App* app, const QString& dir = QString(), int intervalMs = 30000);
    ~Autosave() override;
    void writeNow();                 // writes the copy if there are unsaved changes
    void finish();                   // the program ends normally: the copy is not needed
    QString copyPath() const { return songFile_; }

    // copies left behind by a run that did not end normally, newest first
    struct Leftover { QString song, meta, lock, original; QDateTime when; };
    static QList<Leftover> leftovers(const QString& dir = QString());
    static void discard(const Leftover& l);
    // asks whether to bring back the newest leftover; true if a song was restored
    bool offerRecovery(QWidget* parent);
    bool restore(const Leftover& l);    // opens it in the program, as unsaved changes to the song it came from

private:
    static QString folder(const QString& dir);
    void check();
    App* app;
    QString dir_, songFile_, metaFile_;
    std::unique_ptr<QLockFile> lock_;
    QTimer timer_;
    bool changed_ = false;
};
