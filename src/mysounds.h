// Your own sounds: a sound made on the Sound tab, saved under a name outside any song, so every song can use it.
// One small file per sound (.nzsound, JSON) in the user's data folder, under sounds/<group>/, so sounds can be
// backed up and shared by copying files.
// What's here: Sound, folder(), all(), read(), save(), rename(), remove(), setStarred(), bringIn(), noteUsed(),
// recent(). The window that shows them is the sound library (see roadmap.md, "Saving sounds").
#pragma once
#include "model.h"
#include <QString>
#include <QStringList>
#include <vector>

namespace mysounds {

enum Group { Bass, Sequence, Pad, Lead, GroupCount };
const char* groupName(int group);   // "Bass", … (translated with ui())
const char* groupFolder(int group); // "bass", … (the folder's name, never translated)

struct Sound {
	QString name;
	int group = Bass;
	QStringList words; // words to find it by, optional
	bool starred = false;
	Patch patch{};
	QString file; // where it is saved; empty until saved
};

QString folder();                           // sounds/ in the user's data folder
std::vector<Sound> all();                   // every saved sound, by group, then name
bool read(const QString& path, Sound& out); // a .nzsound file
// writes the sound (a new file for a new one, the same file again for one already saved); false and error on failure
bool save(Sound& s, QString* error = nullptr);
// the sound written to a file of your choosing (to share it); the sound itself is left as it was
bool writeTo(const Sound& s, const QString& path, QString* error = nullptr);
bool rename(Sound& s, const QString& name, QString* error = nullptr);
bool remove(const Sound& s);
bool setStarred(Sound& s, bool on);
// a .nzsound file from elsewhere (shared by someone): copied in as one of your sounds; false when it is not one
bool bringIn(const QString& path, Sound* out = nullptr, QString* error = nullptr);
void noteUsed(const Sound& s); // remembered for "used lately"
QStringList recent();          // files of the sounds used lately, newest first (five at most)

} // namespace mysounds
