// Application core: owns the song, undo history, selection, clipboard, transport and the audio device.
#pragma once
#include "engine.h"
#include <QElapsedTimer>
#include <atomic>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <functional>
#include <memory>
#include <random>

class App : public QObject {
	Q_OBJECT
public:
	explicit App(bool openAudio, QObject* parent = nullptr);
	~App() override;

	Song song;
	int selTrack = -1; // the track the computer keyboard plays and records to
	QSet<int> selClips;
	int editClip = -1; // clip shown in the note editor
	QString filePath;
	bool dirty = false;
	enum Focus { FocusArrange, FocusEditor };
	Focus focus = FocusArrange; // which view Edit-menu commands act on
	bool recording = false;     // Record: played notes and knob moves are written into the song
	bool quantize = true;       // recorded notes snap to the grid
	bool inTime = true;         // hand-played notes wait for the grid while the song plays
	int grid = 24;              // editing grid in ticks
	float density = 0.6f;       // how busy generated ideas are
	std::mt19937 rng{std::random_device{}()};
	Engine* engine = nullptr;
	QString audioError;

	// the sound card. Settings are kept between runs; an empty name means the system's default.
	struct AudioSetup {
		QString driver, device;
		int rate = 48000, buffer = 256;
	};
	static QStringList audioDrivers();          // e.g. pipewire, pulseaudio, alsa, jack / coreaudio / wasapi
	QStringList audioDevices(bool input) const; // names of the outputs (or inputs) of the running driver
	AudioSetup audioSetup() const { return setup_; }
	QString audioDriver() const; // the driver actually in use
	int audioRate() const { return int(engine->sampleRate()); }
	int audioBuffer() const { return gotBuffer_; }
	bool audioRunning() const { return audioDev != 0; }
	bool enginePlays() const {
		return audioDev != 0 || handDriven_;
	} // something runs the engine, so a sound handed to it is taken
	bool openAudio(const AudioSetup& s); // closes the device and opens it again; false (and audioError) if it fails
	void testTone() { engine->ping.store(true); }
	void previewSample(const QString& name); // plays a sample from the library once; empty name stops it
	void previewData(
	    std::shared_ptr<const dsp::SampleData> d); // the same for a sound in memory (a slice); nullptr stops it
	// Chop a loop across the drums: the slices between the cuts are saved in the sample library and put on the
	// drums of the selected drum track (one is made if there is none), and with pattern a clip of `bars` bars
	// plays them back in their order, so the loop sounds as it was, at the song's tempo. Returns the track, or -1.
	int chopOntoDrums(const dsp::SampleData& src, const std::vector<size_t>& cuts, const QString& name, int bars,
	                  bool pattern, QString& error);
	// plays a short tune with a sound before it is chosen, in the song's key and tempo: a preset for the selected
	// track's role, or a drum kit (its samples are loaded). Both null: stop.
	void previewSound(const Patch* patch, const DrumKit* kit);
	// playing the input live through the mixer's Input strip (the device chosen in the settings)
	bool startLiveInput(QString& error);
	void stopLiveInput();
	bool liveInput() const { return liveDev != 0; }
	// recording from an input, for making samples
	bool startCapture(const QString& device, QString& error);
	std::vector<float> stopCapture(int& rate); // mono, at the rate returned
	float captureLevel() const { return capLevel_.load(std::memory_order_relaxed); }
	double captureSeconds() const;

	// editing: begin() saves an undo step, touch() sends the song to the engine and repaints.
	// what names the step ("New idea"); with announce, a note offering Undo pops up once the change is made
	void begin(const QString& what = QString(), bool announce = false);
	void touch();
	void edit(const std::function<void(Song&)>& f, const QString& what = QString(), bool announce = false) {
		begin(what, announce);
		f(song);
		touch();
	}
	// like edit(), but while the song plays the change waits for the next bar line
	void onBar(const std::function<void(Song&)>& f, bool undoable = true, const QString& what = QString(),
	           bool announce = false);
	int pendingChanges() const { return int(pending_.size()); }
	void undo();
	void redo();
	bool canUndo() const { return !undo_.empty(); }
	int editCount(const QString& what) const {
		return editCount_.value(what);
	} // how many changes of this name were made (for the lessons)
	bool canRedo() const { return !redo_.empty(); }
	QString undoName() const {
		return undoWhat_.empty() ? QString() : undoWhat_.back();
	} // empty if the step has no name
	QString redoName() const { return redoWhat_.empty() ? QString() : redoWhat_.back(); }
	void setSong(const Song& s, const QString& path);
	bool save(const QString& path);
	bool load(const QString& path);

	Track* curTrack() { return song.track(selTrack); }
	Clip* curClip(Track** owner = nullptr) { return song.clip(editClip, owner); }
	void selectTrack(int id);
	void selectClip(int id, bool add = false);
	void openClip(int id);
	void refreshSelection() {
		fixSelection();
		emit selectionChanged();
	} // after clips were replaced wholesale

	void play();
	void stop();
	void togglePlay();
	void seek(double tick);
	double pos() const { return engine->pos.load(std::memory_order_relaxed); }
	double musicalPos() const { return unswing(pos(), song.swing); }
	bool isPlaying() const { return engine->playing.load(std::memory_order_relaxed); }

	// bent: Shift was held, for the "right wrong note": a semitone outside the scale that leans into the key's own
	// note (see Song::approachAlt), or in Chords mode the key's chord a semitone lower
	void keyDown(int code, int row, int col, bool bent = false);
	bool shiftHeld = false; // Shift is down: the pads show the bent notes
	// a note from a MIDI keyboard: the nearest note of the song's scale on the selected track (a drum on a drum
	// track, by the General MIDI drum notes), or a chord in Chords mode; loud ones are accented
	void midiNote(int note, int velocity, bool on);
	class Midi* midi = nullptr; // MIDI devices, when the program has them (see midi.h)
	bool keyHeld(int code) const { return down_.contains(code) || chordDown_.contains(code); }
	void keyUp(int code);
	void releaseAllKeys();
	void audition(int trackId, int deg, bool acc = false, int alt = 0);

	// Chords mode: the letter keys change the chord instead of playing notes, and every track that follows the
	// chords moves with it at once, the way the sequencer of an old synthesizer is transposed from its keyboard.
	// The chord stays after the key is let go. With Record on it is written into the chord row bar by bar.
	void setChordKeys(bool on);
	bool chordKeys() const { return chordKeys_; }
	void playChord(int deg,
	               int alt = 0); // as if the key for this chord was pressed; alt moves it a semitone out of the scale
	void releaseChord();         // back to the song's own chords
	int liveChord() const { return liveChord_; } // Engine::NO_CHORD when none
	int liveChordAlt() const { return liveChordAlt_; }
	static int keyDegree(int row, int col, int scaleSize) { return col + scaleSize * (row - 1); }
	// knobs: with Record on and the song playing, moves are written along the timeline
	void paramPress(int trackId, int param);
	void paramMove(int trackId, int param, float v);
	void paramRelease();
	float paramValue(const Track& t, int param) const; // what the knob should show right now
	void forgetMoves(int trackId, int param);          // param < 0: all of the track's recorded moves
	void resumeMoves(int trackId);

	int addTrack(int role);
	void removeTrack(int id);
	int createClip(int trackId, int start);
	void splitClip(int clipId, int tick);
	void splitAtPlayhead();
	void deleteSelection();
	void duplicateSelection();
	void copy();
	void cut();
	void paste();
	void selectAll();
	void loopSelection(); // put the loop around the selected clips

	// loop grid: cells that repeat while switched on; switching lands on the next bar
	void setLiveMode(bool on);
	int newCell(int trackId, int scene); // a cell with a fresh idea in it
	void launch(int trackId, int scene); // scene -1 stops the track
	void launchScene(int scene);         // a whole column at once
	void stopAllCells();
	void deleteCell(int cellId);
	void cellToTimeline(int cellId);
	int queuedCell(int trackId) const {
		return queued_.value(trackId, -2);
	} // column waiting for the bar line, -2 = nothing waiting

signals:
	void changed();
	void selectionChanged();
	void status(const QString& text);
	void announced(const QString& what); // a big change was just made: offer to undo it
	void chordKeysChanged(bool on);
	// the engine is replaced when the sound card changes its rate: anything holding it lets go before, and takes the
	// new one after
	void engineReplacing();
	void engineReplaced();
	void undone(const QString& what, bool redo);

private:
	struct Down {
		int track, deg;
		double tick;
		qint64 ms;
		bool rec, acc;
		int alt;
	};
	void publish();
	void fixSelection();
	void recordNote(const Down& d, qint64 durMs);
	void pressNote(int code, int deg, bool acc, int alt = 0);
	void writePoint(float v);
	void applyLaunch(Song& s, int trackId, int scene);
	void closeRecorded(int trackId, int tick);
	void tick();
	static float baseValue(const Track& t, int param) { return param == AP_VOLUME ? t.vol : t.patch.p[param]; }
	static void audioCallback(void* user, unsigned char* stream, int len);
	static void captureCallback(void* user, unsigned char* stream, int len);
	static void liveCallback(void* user, unsigned char* stream, int len);
	unsigned liveDev = 0;
	bool handDriven_ = false;
	bool liveWasOn = false; // live input paused while a sample is recorded
	AudioSetup setup_;
	std::shared_ptr<const dsp::SampleData>
	    previewKeep_[4];                          // samples handed to the engine to preview, kept alive a while
	std::shared_ptr<const Phrase> phraseKeep_[4]; // the same for phrases
	int phraseIdx_ = 0;
	int previewIdx_ = 0;
	int gotBuffer_ = 0;
	unsigned capDev = 0;
	int capRate_ = 48000;
	std::vector<float> capBuf_; // filled by the capture thread, up to capMax_ samples
	std::atomic<size_t> capLen_{0};
	size_t capMax_ = 0;
	std::atomic<float> capLevel_{0.f};

	std::vector<Song> undo_, redo_;
	std::vector<QString> undoWhat_, redoWhat_; // names of the steps, side by side with undo_ and redo_
	QHash<QString, int> editCount_;
	std::shared_ptr<const Song> keep_[16];
	int keepIdx = 0;
	struct PendingChange {
		std::function<void(Song&)> f;
		bool undoable;
		QString what;
		bool announce;
	};
	std::vector<PendingChange> pending_;
	QTimer timer_;
	QHash<int, Down> down_;
	QSet<int> chordDown_; // keys held in Chords mode
	bool chordKeys_ = false;
	int liveChord_ = Engine::NO_CHORD;
	int liveChordAlt_ = 0;
	int chordFromBar_ = 0;      // recording chords: the first bar to write
	bool chordTake_ = false;    // an undo step for this recording of chords was made
	bool chordWriting_ = false; // the chord last pressed is being written into the bars
	int chordLastBar_ = -1;     // the last bar written, to notice the loop coming round
	QHash<int, int> queued_;    // track id -> column that starts on the next bar
	QHash<int, int> recOpen_;   // track id -> timeline clip being written by a launched cell
	QElapsedTimer clock_;
	std::vector<std::pair<bool, Clip>> clipClips_; // (is a drum clip, clip)
	std::vector<Note> clipNotes_;
	int wTrack = -1, wParam = -1, wLast = 0; // knob currently held
	bool writing = false;
	float wValue = 0.f;
	unsigned audioDev = 0;
	bool sdlUp = false;
};
