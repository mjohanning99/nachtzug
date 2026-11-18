// The command line. Besides a song to open, it takes the runs used for testing and for pictures:
//
//   nachtzug [song.nachtzug]                 open the program, with that song
//   nachtzug --render out.wav [song]         write a song (or the demo song) to a WAV file without a window;
//                                            --arrange 0|1|2 arranges it first, --kit NAME swaps the drum kit
//   nachtzug --selftest                      drive every feature without a window and report problems
//   nachtzug --guitest                       drive the window's own controls and report problems
//   nachtzug --check-tooltips                list every visible control that has no tooltip
//   nachtzug --parity folder                 write what the song code does with fixed seeds (tools/parity.sh)
//   nachtzug --write-starters folder         make the starter songs afresh (assets/starters, see starters.h)
//   nachtzug --screenshot out.png [song]     save a picture of the window and quit; what it shows: --tab 0|1|2,
//                                            --track N, --theme NAME, --loops, --lines, --guide, --simple,
//                                            --arrange 0|1|2, --chord-keys, --lesson ID:STEP, --size WxH,
//                                            --timeline-beside, --no-inspector, --frames N (N pictures of the
//                                            song playing: out-000.png …), --dialog NAME, --geometry out.json
//                                            (where each part of the window is)
//   --lang de                                a language for this run, with any of the above
#pragma once
#include <QSize>
#include <QString>

class App;
class MainWindow;

struct Options {
	QString songPath; // a song to open, or to render

	// runs without a window
	QString renderPath, parityDir, startersDir;
	QString kitName;     // --render and pictures: the drum kit
	int arrangeLen = -1; // --render and pictures: arranged first (0 short, 1 medium, 2 long)
	int partIndex = -1;  // pictures, with --arrange: this named part selected (--part N, from 0)
	// --render and pictures: then continued for this many minutes after the end (--journey MINUTES[:CHARACTER], the
	// character 0 calm, 1 evolving, 2 restless)
	int journeyMin = 0, journeyChar = 1;
	bool selftest = false, guiTest = false, tipCheck = false;

	// pictures
	QString shotPath, themeName, langName, dialogName, geomPath;
	QString lessonArg; // "acid:3" shows step 3 of that lesson
	QSize winSize;     // the window at this size (as small as it allows)
	int tab = 0, trackIndex = -1;
	bool guide = false, loopsView = false, linesView = false, simpleView = false, chordKeys = false;
	bool symbolsOnly = false;    // --symbols: the keys show their symbols alone
	bool timelineBeside = false; // View > Timeline on top unticked
	bool noInspector = false;    // View > Inspector unticked
	int frames = 0;              // pictures of the song playing, about 12 a second (for the website's animation)

	bool windowless() const; // runs on Qt's window-less platform
	bool testRun() const;    // keeps its own settings and sample folder, so it neither depends on nor changes yours
};

Options parseOptions(int argc, char** argv);
int renderSong(const Options& o);       // --render: 0, or 1 if the song could not be read or the file written
int pictureOfWelcome(const Options& o); // --screenshot with --dialog welcome: the first-start window
// for pictures, in this order:
void preparePicture(App& app, const Options& o);                    // the song as the picture wants it
void setUpWindow(App& app, MainWindow& w, const Options& o);        // the tab, the Simple page, Chords mode, the size
void scheduleViews(MainWindow& w, const Options& o);                // the whole song in view, a lesson step
void scheduleScreenshot(App& app, MainWindow& w, const Options& o); // the picture itself, then quit
