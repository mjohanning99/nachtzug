// Your own sounds: a library folder of WAV and AIFF files that the drum machine can play instead of its own drums.
// Imported files are copied into the library, so songs keep finding them even if the original is moved.
#pragma once
#include "dsp.h"
#include <QString>
#include <QStringList>
#include <memory>

namespace samples {

// The Nachtzug library: sounds made for the program (see tools/samplegen), built in. A song names one as
// "nz:Kicks/Deep boom"; your own samples are plain file names in the folder below.
struct LibrarySound {
	QString id;
	QString category;
	QString name;
	QString about;
}; // category, name and about are translated
const QList<LibrarySound>& library();
QStringList libraryCategories(); // translated, in the order of the library
bool isLibrary(const QString& name);
QString displayName(const QString& name); // what to show: "Deep boom", or a file's name without .wav
QString about(const QString& name);       // one line about a library sound, empty for your own

QString folder();   // created if it does not exist yet
QStringList list(); // file names in the library, sorted
std::shared_ptr<const dsp::SampleData> get(
    const QString& name); // loaded once and kept; nullptr if missing or unreadable
bool exists(const QString& name);
double seconds(const QString& name); // length, 0 if unreadable

// Reads a WAV (8/16/24/32-bit, float) or AIFF/AIFC file. Long files are cut at maxSeconds.
bool readFile(const QString& path, dsp::SampleData& out, QString& error, double maxSeconds = 60.0);
// Copies a sound file into the library and returns its name there (made unique if needed), or an empty string.
QString import(const QString& path, QString& error);
// Writes mono float samples as a 24-bit WAV file into the library; returns the name or an empty string.
QString saveRecording(const QString& wantedName, const std::vector<float>& mono, int rate, QString& error);
// The same for a sound in stereo (right may be empty for mono). The name is made unique if needed.
QString saveSound(const QString& wantedName, const std::vector<float>& left, const std::vector<float>& right, int rate,
                  QString& error);
bool remove(const QString& name);

// Chopping a loop: where its hits start (sample positions, the first always 0), at most maxSlices of them.
// sensitivity 0..1: how faint a hit may be and still count.
std::vector<size_t> findHits(const dsp::SampleData& d, float sensitivity, int maxSlices);
std::vector<size_t> equalCuts(size_t frames, int parts);
// The piece of a sound between two cuts, with short fades so it starts and stops without a click.
dsp::SampleData slice(const dsp::SampleData& d, size_t from, size_t to);
// How many bars a loop of this length most likely is at this tempo: 1, 2, 4 or 8.
int guessBars(double seconds, double bpm);
bool rename(const QString& from, const QString& to, QString& error);

} // namespace samples
