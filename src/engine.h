// Audio engine: plays a Song snapshot, runs the synths, drums and effects.
// The GUI thread talks to it only through setSong(), push() and the atomics.
#pragma once
#include "model.h"
#include <atomic>
#include <functional>
#include <memory>

struct EngineEvent {
	// Hush: every sound, echo and reverb silent at once, after a fade of a few milliseconds
	enum Type { NoteOn, NoteOff, Audition, Play, Stop, Seek, Panic, Hush } type = NoteOn;
	int track = 0; // track id
	int deg = 0;
	int key = 0; // identifies a held note between NoteOn and NoteOff
	bool acc = false;
	int alt = 0; // NoteOn, Audition: semitones off the scale note (a passing note)
	double tick = 0;
	int quant = 0; // NoteOn: if > 0 and the song is playing, the note waits for this grid (in ticks). Seek: if > 0
	               // and the song is playing, the jump waits for the next bar line, so the music stays in time
};

// A note that sounded, written down by an engine that is asked to (see Engine::log): for the MIDI export and the tests.
struct NoteEvent {
	double frame; // sample position, counted from the first process() call
	int track;    // track id
	int key;      // matches a note-on with its note-off
	int note;     // MIDI note number, or the drum (0..7) for a drum hit
	bool on, drum, acc;
};

// A short tune played with a sound that is on no track yet: what a preset or kit sounds like, heard while
// the mouse rests on it in a list. Times in seconds; note is a MIDI note, or the drum for a drum kit.
struct PhraseNote {
	float at, len;
	int note;
	bool acc;
};
struct Phrase {
	bool drums = false;
	float patch[dsp::P_COUNT] = {};
	float kit[dsp::DP_COUNT] = {};
	std::shared_ptr<const dsp::SampleData> samples[dsp::D_COUNT]; // kept alive here; the engine reads them only
	std::vector<PhraseNote> notes;                                // sorted by time
	float length = 0.f;
};

class Engine {
public:
	explicit Engine(float sampleRate);
	void setSong(std::shared_ptr<const Song> s);
	bool push(const EngineEvent& e);      // GUI thread
	void process(float* out, int frames); // audio thread, interleaved stereo
	bool synthsSilent() const {
		for (const auto& sl : rt_)
			if (!sl.synth.silent()) return false;
		return true;
	} // tests: no voice sounds

	std::atomic<double> pos{0.0};     // playhead in ticks
	std::atomic<float> bpmNow{120.f}; // the song's tempo, for threads that must not touch the song (MIDI clock)
	std::atomic<bool> playing{false}, metronome{false}, ignoreLoop{false}, ping{false}; // ping: a short test tone
	// a sound file to listen to once (the caller keeps it alive); stopPreview cuts it off
	std::atomic<const dsp::SampleData*> previewReq{nullptr};
	std::atomic<bool> stopPreview{false};
	// a phrase to play once (the caller keeps it alive for a while); stopPhrase ends it
	std::atomic<const Phrase*> phraseReq{nullptr};
	std::atomic<bool> stopPhrase{false};
	// a chord played on the keys in Chords mode: replaces the song's chords for tracks that follow them (NO_CHORD:
	// none)
	static constexpr int NO_CHORD = -1000;
	std::atomic<int> liveChord{NO_CHORD};
	std::atomic<int> liveChordAlt{0}; // that chord moved a semitone out of the scale (-1 or +1)
	std::atomic<float> peakL{0.f}, peakR{0.f};
	std::atomic<float> trackPeak[MAX_TRACKS];
	// the live input: the capture thread writes into the ring, the audio thread reads it (see pushInput)
	// the song snapshot process() plays now, so the GUI thread knows which older ones it may let go (only compared,
	// never read through)
	std::atomic<const Song*> songNow{nullptr};
	// only while no sound device runs (its callback is then the only other user): lets go of the snapshot the engine
	// played last, here on the GUI thread, so the App may free it and the next callback takes the newest
	void dropPlaying() {
		cur_.reset();
		songNow.store(nullptr, std::memory_order_release);
	}
	std::atomic<bool> inputOn{false};
	std::atomic<float> inputPeak{0.f};
	void pushInput(const float* mono, int n); // capture thread
	static constexpr int SCOPE_N = 1024;
	std::atomic<float> scope[SCOPE_N] = {}; // what the display draws; written by the audio thread
	std::atomic<int> scopeW{0};
	float sampleRate() const { return sr; }

	// for writing files: only this track is heard (-1: every track as the mixer says), and with eventsOnly the
	// engine works out the notes but makes no sound (much faster); log collects every note that sounds
	int onlyTrack = -1;
	bool eventsOnly = false;
	std::vector<NoteEvent>* log = nullptr;
	// the master limiter: limGainOut gets the gain it used for each sample of a process() call; with limGainIn the
	// engine uses those gains instead of its own, so a single track (a stem) is turned down exactly as in the mix
	float* limGainOut = nullptr;
	const float* limGainIn = nullptr;

private:
	struct Active {
		int key;
		double samples;
		bool seq;
	};
	struct ArpNote {
		int key, deg, alt;
	};
	struct Slot {
		int id = -1;
		dsp::Synth synth;
		dsp::Drums drums;
		Active active[64];
		int nActive = 0;
		ArpNote arp[32];
		int nArp = 0, arpIdx = 0, arpKey = -1;
		double arpOff = 0, arpFree = 0;
		float gl = 0.f, gr = 0.f, peak = 0.f;
		float vol = 0.75f, dsend = 0.f, rsend = 0.f; // after recorded knob moves are applied
		float duck = 0.f;                            // how far the kick pushes it down (Pump)
		bool audible = true;                         // heard in this block (mute, solo)
	};
	struct Pending {
		int track, key, deg, quant, alt;
		double due;
		bool acc, off;
	}; // hand-played note waiting for the grid

	void remap();
	void handle(const EngineEvent& e);
	void noteIn(Slot& sl, const Track& tr, int key, int deg, bool acc, bool on, int alt = 0);
	void addActive(Slot& sl, const Track& tr, int key, double samples, bool seq);
	void releaseSequenced();
	void silenceAll(); // every sound, echo and reverb cut at once (at the end of a Hush's fade)
	void fire(double m0, double m1);
	void fireClip(Slot& sl, const Track& tr, const Clip& c, double m0, double m1, double start, double len);
	void trigger(Slot& sl, const Track& tr, const Clip& c, const Note& n, double tAbs, double clipEnd, int sub);
	bool plays(const Clip& c, size_t idx, long iter) const;
	// every note that sounds goes through these three
	void synOn(Slot& sl, int key, int midi, bool acc, int part = 0) { // part: which note of a chord
		sl.synth.noteOn(key, midi, acc, part);
		if (log && sl.audible) log->push_back({clock_ + chunkAt_, sl.id, key, midi, true, false, acc});
	}
	void synOff(Slot& sl, int key) {
		sl.synth.noteOff(key);
		if (log && sl.audible) log->push_back({clock_ + chunkAt_, sl.id, key, 0, false, false, false});
	}
	void drumHit(Slot& sl, int drum, bool acc) {
		sl.drums.trigger(drum, acc);
		if (drum == dsp::D_KICK) duckT_ = 0.f; // every kick pumps the tracks that ask for it, heard or not
		if (log && sl.audible) log->push_back({clock_ + chunkAt_, sl.id, 0, drum, true, true, acc});
	}
	void arpStep(Slot& sl, const Track& tr, double stepSamples);
	int nextKey() {
		if (++keyCounter > 90000) keyCounter = 1;
		return keyCounter;
	}

	float sr;
	// the newest song from the GUI thread; a tiny lock the audio thread only ever tries, so it never waits
	// (std::atomic<std::shared_ptr> would be neater, but Apple's C++ library does not have it)
	std::shared_ptr<const Song> song_;
	std::atomic_flag songLock_ = ATOMIC_FLAG_INIT;
	std::shared_ptr<const Song> cur_;
	Slot rt_[MAX_TRACKS];
	int map_[MAX_TRACKS] = {}; // track index -> slot index

	static constexpr unsigned QN = 1024;
	EngineEvent queue[QN];
	std::atomic<unsigned> qHead{0}, qTail{0};

	Pending pend[16];
	int nPend = 0;
	bool playing_ = false, loop_ = false;
	double pos_ = 0.0, tps = 0.01;        // ticks per sample
	double jumpAt_ = -1.0, jumpTo_ = 0.0; // a jump waiting for the bar line at jumpAt_ (-1: none), and where to
	int keyCounter = 0;
	uint32_t pass_ = 0;  // counts plays and passes through the loop, for notes with a chance
	double clock_ = 0.0; // samples processed before this block
	int chunkAt_ = 0;    // where in the block the current chunk starts
	float duckT_ = 10.f; // seconds since the last kick
	int hushLeft_ = -1;  // a Hush fading out: samples left of the fade (0: faded, silence at the chunk's end); -1 none
	dsp::PingPong delay;
	dsp::Reverb reverb;
	dsp::Limiter limiter;
	dsp::Rng rng;
	float clickEnv = 0.f, clickPh = 0.f, clickFreq = 1000.f;
	int pingLeft = 0;
	const dsp::SampleData* prev_ = nullptr;
	const Phrase* phr_ = nullptr; // the phrase being played, until its last note has started and ended
	double phrT_ = 0.0;
	size_t phrNext_ = 0;
	struct PhraseOn {
		int key;
		double end;
	} phrOn_[16];
	int nPhrOn_ = 0;
	dsp::Synth phrSynth;
	dsp::Drums phrDrums;
	float phrDelay_ = 0.f, phrReverb_ = 0.f;
	static constexpr unsigned IN_N = 1 << 15;
	float inRing_[IN_N] = {};
	std::atomic<unsigned> inW_{0}, inR_{0};
	dsp::Ladder inFilter_;
	float inEnv_ = 0.f, inG_ = 0.05f, inGl_ = 0.f, inGr_ = 0.f, inPk_ = 0.f;
	double prevPos_ = 0.0;
	float pkL = 0.f, pkR = 0.f;
};

// How a song is written to a sound file.
struct RenderOptions {
	float sampleRate = 48000.f;
	int bits = 16; // 16 (with dither) or 24
	bool loopOnly =
	    false; // false: the whole song from bar 1, ignoring the loop. true: the loop, played `repeats` times
	int repeats = 1;
	bool fadeOut = false; // fade over the last bars instead of letting the echoes ring out
	double maxTail = 8.0; // seconds of ringing out after the music, cut short once it has gone quiet
	int onlyTrack = -1;   // a track id: only that track is heard (for stems); -1 every track, as the mixer says
};
struct RenderResult {
	bool ok = false;
	double seconds = 0.0;
	qint64 bytes = 0;
};
// Writes a stereo WAV file. progress gets 0..1 and returns false to cancel (the file is then removed).
RenderResult renderSong(const Song& song, const QString& path, const RenderOptions& opt,
                        const std::function<bool(float)>& progress);
// The whole song, 48 kHz 16-bit, as before.
inline bool renderToWav(const Song& song, const QString& path, float sampleRate,
                        const std::function<bool(float)>& progress) {
	RenderOptions o;
	o.sampleRate = sampleRate;
	return renderSong(song, path, o, progress).ok;
}
// Length in seconds of what renderSong would write before the ringing out.
double renderLength(const Song& song, const RenderOptions& opt);
// The tracks that are heard in the song (not muted; only the soloed ones while any is soloed; not empty), in order.
std::vector<int> audibleTracks(const Song& song);
// One file per heard track ("01 Drums.wav", "02 Bass.wav", …) in folder; together they add up to the song.
// files gets the names written. progress gets 0..1 over all of them and returns false to cancel (nothing is left
// behind).
RenderResult renderStems(const Song& song, const QString& folder, const RenderOptions& opt,
                         const std::function<bool(float)>& progress, QStringList* files = nullptr);
QString stemFileName(const Song& song, int trackId, int number);
