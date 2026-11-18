// Recent songs: see recent.h.
//
// What's here: recent::list(), add(), remove(), clear() on the settings ("recentSongs"), and the helpers they use:
// added(), isSpareCopy(), exists(), names(), folderOf(). add() is called from App::save() and App::load(); the
// list is shown by MainWindow (File > Open recent) and the start window.
// Changes in the song file: none.

#include "recent.h"
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSettings>
#include <QStandardPaths>

namespace recent {

static const QString KEY = QStringLiteral("recentSongs");

// one way of writing each path, so the same song opened as ./song.nachtzug and /home/…/song.nachtzug is one entry
static QString tidy(const QString& path) {
	if (path.isEmpty()) return {};
	const QFileInfo fi(path);
	const QString canon = fi.canonicalFilePath(); // symbolic links followed, when the file is there
	if (!canon.isEmpty()) return QDir::cleanPath(canon);
	// a song that is gone: its folder's links followed all the same (on a Mac /var is /private/var), so it is still
	// written as it was when it was put on the list
	const QString dir = QFileInfo(fi.absolutePath()).canonicalFilePath();
	return QDir::cleanPath(dir.isEmpty() ? fi.absoluteFilePath() : dir + QLatin1Char('/') + fi.fileName());
}

static bool same(const QString& a, const QString& b) {
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
	return a.compare(b, Qt::CaseInsensitive) == 0; // the usual file systems there ignore case
#else
	return a == b;
#endif
}

QStringList added(const QStringList& list, const QString& path, int max) {
	QStringList out;
	const QString p = tidy(path);
	if (!p.isEmpty() && !isSpareCopy(p)) out << p;
	for (const QString& o : list) {
		if (o.isEmpty() || isSpareCopy(o)) continue;
		bool dup = false;
		for (const QString& k : out)
			dup |= same(k, o);
		if (!dup) out << o;
	}
	while (out.size() > std::max(0, max))
		out.removeLast();
	return out;
}

bool isSpareCopy(const QString& path) {
	const QFileInfo fi(path);
	if (fi.fileName().startsWith(QLatin1String("session-")) && fi.dir().dirName().endsWith(QLatin1String("autosave")))
		return true;
	const QString spare = QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
	                                      QStringLiteral("/autosave"));
	return same(QDir::cleanPath(fi.absolutePath()), spare);
}

bool exists(const QString& path) {
	return QFileInfo(path).isFile();
}

QStringList names(const QStringList& paths) {
	QHash<QString, int> count;
	for (const QString& p : paths)
		count[QFileInfo(p).completeBaseName().toLower()]++;
	QStringList out;
	for (const QString& p : paths) {
		const QFileInfo fi(p);
		const QString name = fi.completeBaseName();
		if (count.value(name.toLower()) > 1)
			out << QStringLiteral("%1 (%2)").arg(name, fi.dir().dirName());
		else
			out << name;
	}
	return out;
}

QString folderOf(const QString& path) {
	return QDir::toNativeSeparators(QFileInfo(path).absolutePath());
}

QStringList list() {
	return QSettings().value(KEY).toStringList();
}

void add(const QString& path) {
	const QStringList now = added(list(), path);
	if (now != list()) QSettings().setValue(KEY, now);
}

void remove(const QString& path) {
	QStringList l = list();
	const QString p = tidy(path);
	l.removeIf([&](const QString& o) { return same(o, p) || same(o, path); });
	QSettings().setValue(KEY, l);
}

void clear() {
	QSettings().remove(KEY);
}

} // namespace recent
