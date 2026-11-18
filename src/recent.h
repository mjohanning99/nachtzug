// Recent songs: the last songs opened or saved, newest first, for File > Open recent and the start window. Kept in
// the settings as a list of paths; the spare copies of autosave never appear in it. No window, so the self-test can
// run it.
#pragma once
#include <QString>
#include <QStringList>

namespace recent {

constexpr int MAX = 8; // songs kept in the list

// the list from the settings, newest first (songs no longer found are still in it: see exists())
QStringList list();
// a song was opened or saved: it moves to the top (a spare copy or a path with no file name is left out)
void add(const QString& path);
void remove(const QString& path);
void clear();

// the parts the above are made of, without the settings
QStringList added(const QStringList& list, const QString& path, int max = MAX); // path on top, once, at most max
bool isSpareCopy(const QString& path); // one of autosave's copies (its folder, or its session-… name)
bool exists(const QString& path);      // the song is still where it was
// what the menu shows for each path: the song's name, with its folder's name beside it where two songs share a name
QStringList names(const QStringList& paths);
QString folderOf(const QString& path); // the folder, as the system writes it, for the tooltip

} // namespace recent
