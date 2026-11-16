// Song data model. A Song is a plain value: copying it is how undo and the audio snapshot work.
#pragma once
#include "dsp.h"
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <memory>
#include <random>
#include <vector>

constexpr int PPQ = 96; // ticks per quarter note
constexpr int BEAT = PPQ;
constexpr int BAR = PPQ * 4; // 4/4 throughout
constexpr int MAX_TRACKS = 12;
constexpr int DEG_MIN = -7, DEG_MAX = 21; // note editor range, in scale degrees
constexpr int AP_VOLUME = 62;             // automation "parameter" number of the track volume

struct ScaleDef {
	const char* name;
	int n;
	int iv[7];
};
extern const ScaleDef SCALES[];
extern const int NUM_SCALES;
extern const char* const NOTE_NAMES[12];
extern const char* const DRUM_NAMES[dsp::D_COUNT];
constexpr int NUM_DRUM_STYLES = 7;
extern const char* const DRUM_STYLES[NUM_DRUM_STYLES];
extern const char* const CHORD_NAMES[7];
extern const char* const ARP_NAMES[6];
extern const char* const RATE_NAMES[4];
extern const int RATE_TICKS[4];
extern const char* const DTIME_NAMES[5];
extern const float DTIME_BEATS[5];

enum ParamKind { K_PCT, K_TIME, K_CUTOFF, K_LFO, K_PHASER, K_CENTS, K_GLIDE, K_CHOICE };
struct ParamDef {
	const char* id;
	const char* label;
	float def;
	ParamKind kind;
	const char* choices;
};
extern const ParamDef PARAMS[dsp::P_COUNT];
QString formatParam(int param, float v);
// Text from the tables here is kept in English in the song; ui() translates it where it is shown.
QString ui(const char* text);
QString ui(const QString& text);
QString pitchName(int pc);     // C, C♯, … B; in German the last one is H, as German musicians name it
QString paramTip(int param);   // what a synth setting does, in a sentence or two
QString paramLabel(int param); // also knows AP_VOLUME

struct Note {
	int start = 0, len = 24, deg = 0; // deg: scale degree (synth tracks) or drum index (drum tracks)
	bool acc = false, slide = false;
	bool lock = false; // kept: Vary and New idea leave it alone
	int chance = 100;  // how often it plays, in percent (100, 75, 50 or 25)
	int rep = 1;       // played this many times in quick succession over its length (1 to 4)
	int alt = 0;       // -1 or +1: played a semitone below or above its row, a passing note outside the scale
	// the stretch the repeats are spread over: the note's length (at least a sixteenth for a drum hit, whose length is
	// only nominal), cut at the end of the pattern so every repeat belongs to the same round
	int repeatSpan(int loopLen, bool drum) const {
		return std::max(1, std::min(drum ? std::max(len, 24) : len, loopLen - start));
	}
	bool sel = false; // editor selection, not saved
};

// Settings of an auto-drummer clip: the notes are written by the program from these.
struct AutoDrum {
	bool on = false;
	int style = 0;
	float intensity = 0.6f, complexity = 0.4f;
	int fillEvery = 4; // a fill at the end of every N bars, 0 = never
	unsigned seed = 1;
	int backbeat = 0;    // 0 style's own, 1 clap, 2 snare, 3 rim, 4 none
	int hats = 0;        // 0 style's own, 1 eighths, 2 sixteenths, 3 offbeat only, 4 none
	int shape = 0;       // 0 steady, 1 builds up over the clip, 2 winds down
	float shuffle = 0.f; // delays every second sixteenth
	bool crash = true;   // cymbal crash after a fill
};

struct Clip {
	int id = 0;
	int start = 0, len = BAR;      // position on the timeline
	int loopLen = BAR, offset = 0; // the notes repeat every loopLen ticks; offset shifts where the repeat starts
	std::vector<Note> notes;
	QString name; // the idea it came from, shown on the clip; empty for hand-made clips
	AutoDrum ad;
	unsigned adKey = 0; // what the notes were last generated from
	// transitions: bit 1 = volume fade, bit 2 = filter sweep; lengths in ticks
	int inKind = 0, inLen = BAR, outKind = 0, outLen = BAR;
	int scene = -1; // loop-grid cells only: which column the cell sits in
	int end() const { return start + len; }
};

struct AutoPoint {
	int tick;
	float v;
};
// A knob's line over time: recorded knob moves or points drawn by hand, sorted by tick.
// loop > 0: the shape is loop ticks long and repeats; otherwise ticks are song positions.
struct AutoLane {
	int param = 0, loop = 0;
	std::vector<AutoPoint> pts;
	float valueAt(double tick, float fallback) const;
};

enum Role { R_BASS, R_SEQ, R_PAD, R_LEAD, R_DRUMS, R_COUNT };
extern const char* const ROLE_NAMES[R_COUNT];

struct Patch {
	QString name;
	float p[dsp::P_COUNT];
};
struct DrumKit {
	QString name;
	float p[dsp::DP_COUNT];
	QString sample[dsp::D_COUNT]; // your own sound for a drum (a file in the sample library), empty = built in
	std::shared_ptr<const dsp::SampleData> data[dsp::D_COUNT]; // those files, loaded; not saved (see resolveSamples)
};

struct Track {
	int id = 0;
	QString name;
	int role = R_BASS;
	Patch patch;
	DrumKit kit;
	float vol = 0.75f, pan = 0.f;
	float duck = 0.f; // Pump: how far the track dips on every kick, 0..1
	bool mute = false, solo = false, follow = true;
	int octave = 0, chord = 0, arp = 0, arpRate = 2, arpOct = 0;
	std::vector<Clip> clips; // on the song timeline
	std::vector<Clip> cells; // in the loop grid: patterns that repeat for as long as they are switched on
	int liveCell = -1;       // column of the cell that is playing in Loops mode, -1 = none; not saved
	std::vector<AutoLane> lanes;
	quint64 bypass = 0;            // bit per parameter: recorded moves paused by hand, not saved
	bool laneOpen = false;         // the line lane under this track is shown in the timeline
	int laneParam = dsp::P_CUTOFF; // which knob that lane shows
	bool isDrums() const { return role == R_DRUMS; }
	const Clip* cell(int scene) const {
		for (auto& c : cells)
			if (c.scene == scene) return &c;
		return nullptr;
	}
	Clip* cell(int scene) {
		for (auto& c : cells)
			if (c.scene == scene) return &c;
		return nullptr;
	}
	const AutoLane* lane(int param) const {
		for (auto& l : lanes)
			if (l.param == param) return &l;
		return nullptr;
	}
	AutoLane* lane(int param) {
		for (auto& l : lanes)
			if (l.param == param) return &l;
		return nullptr;
	}
	bool hasMoves(int param) const {
		const AutoLane* l = lane(param);
		return l && !l->pts.empty();
	}
	float automated(int param, double tick, float fallback) const; // value of a parameter at a song position
};

// The sound card's input played live through Nachtzug: a guitar, a voice, another synthesizer. It goes through a
// filter (which can open with the loudness, like a wah pedal), drive, echo, reverb and Pump. Never in an export.
struct LiveInput {
	float gain = 0.5f, cutoff = 1.f, res = 0.f, wah = 0.f, drive = 0.f;
	float echo = 0.f, reverb = 0.1f, duck = 0.f, vol = 0.75f, pan = 0.f;
	bool mute = false;
	bool operator==(const LiveInput&) const = default;
};

struct Song {
	float bpm = 118.f, swing = 0.f, master = 0.8f;
	LiveInput input;
	int root = 9, scale = 0;
	int dtime = 2;
	float dfb = 0.45f, rsize = 0.55f, rdamp = 0.45f;
	int loopStart = 0, loopEnd = 8 * BAR;
	bool loopOn = true;
	bool liveMode = false;      // true: the loop grid plays instead of the timeline; not saved
	int scenes = 8;             // columns in the loop grid
	std::vector<int> chords{0}; // chord root per bar as a scale degree; repeats across the song
	// a chord moved a semitone down (-1) or up (+1) out of the scale, per bar of chords; shorter or empty: none
	std::vector<int> chordAlts;
	std::vector<Track> tracks;
	int nextId = 1;

	const ScaleDef& sc() const { return SCALES[std::clamp(scale, 0, NUM_SCALES - 1)]; }
	int newId() { return nextId++; }
	int chordAt(int bar) const {
		const int n = int(chords.size());
		return n ? chords[size_t(((bar % n) + n) % n)] : 0;
	}
	int chordAltAt(int bar) const {
		const int n = int(chords.size());
		if (!n) return 0;
		const size_t i = size_t(((bar % n) + n) % n);
		return i < chordAlts.size() ? chordAlts[i] : 0;
	}
	void setChord(int bar, int deg, int alt = 0); // changes that one bar only
	void setChords(std::vector<int> prog) {
		chords = std::move(prog);
		chordAlts.clear();
	}
	// the semitone a note on this degree is bent by to become a "right wrong note": the step below it when that is
	// outside the scale (a note leaning into the next one), otherwise the step above; 0 when both are in the scale
	int approachAlt(int deg) const;
	int degToSemis(int deg) const; // scale degree -> semitones above the root
	int degreeNear(int semis)
	    const; // the scale degree nearest these semitones above the root (the lower one when two are as near)
	int midiOf(const Track& t, int deg) const;
	QString noteName(const Track& t, int deg, int alt = 0) const;
	QString chordName(int deg, int alt = 0) const;
	int q(int k) const { return int(std::lround(double(k) * sc().n / 7.0)); } // 7-note shape -> this scale
	void chordOffsets(int chord, int out[4], int& count) const;
	Track* track(int id);
	const Track* track(int id) const;
	Clip* clip(int id, Track** owner = nullptr);
	int trackIndex(int id) const;
	int endTick() const;
	bool anySolo() const;

	QJsonObject toJson() const;
	static bool fromJson(const QJsonObject& o, Song& out);
};

// swing: maps a linear playhead position to the musical position (delays every second 16th)
double unswing(double p, float swing);
double swingPos(double m, float swing); // the reverse: where a musical position falls on the playhead

const std::vector<Patch>& presets();
QString presetGroup(const QString& name); // "Bass", "Sequence and pluck", "Pad" or "Lead", for grouping the list
const std::vector<DrumKit>& drumKits();
// A kit preset chosen over the current kit: the preset brings its own sounds, but a drum playing one of your own
// samples (not one from the Nachtzug library) keeps it.
DrumKit chooseKit(const DrumKit& current, const DrumKit& preset);
Patch presetByName(const QString& name);
Patch rollPatch(int role, std::mt19937& rng);
Patch mutatePatch(const Patch& p, std::mt19937& rng);

struct GenDef {
	const char* name;
	int id;
	const char* about = "";
}; // about: one line for the Ideas menu
std::vector<GenDef> generatorsFor(int role);
// density 0..1: how busy the result is
void generate(const Song& s, const Track& t, Clip& c, int genId, float density, std::mt19937& rng);
void varyClip(const Song& s, const Track& t, Clip& c, std::mt19937& rng); // leaves kept (locked) notes alone
std::vector<int> randomProgression(const Song& s, std::mt19937& rng);

// Tidies a played-in pattern: notes off the grid go onto it, doubled notes become one, a tiny note pressed by
// accident right beside a real one goes, and when the pattern repeats (three times or more, mostly the same) a note
// missed in one repeat is filled in, a one-off wrong key is put right and a stray short note is removed. Kept
// (locked) notes are left alone. Says how many notes it changed in each way.
struct CleanReport {
	int snapped = 0, merged = 0, removed = 0, filled = 0, fixed = 0;
	int total() const { return snapped + merged + removed + filled + fixed; }
};
CleanReport cleanUpClip(Clip& c, bool drums, int grid);

void autoDrumFill(Clip& c);     // writes the notes of an auto-drummer clip
bool refreshAutoDrums(Song& s); // rewrites auto-drummer clips whose settings or length changed

Track makeTrack(Song& s, int role);                         // new track with a fitting sound
int addTrackWithClip(Song& s, int role, std::mt19937& rng); // track + generated clip over the loop; returns track id
Song demoSong();
// "Arrange for me": lays the song out as intro, build-up, main part, break, second main part and outro, from each
// track's current pattern (the clip at the loop). length: 0 short, 1 medium, 2 long. Returns the parts, in order,
// as (name, bars). With writeLead, an empty lead track gets a melody for the second main part.
struct ArrangePart {
	const char* name;
	int type;
	int bars;
};
std::vector<ArrangePart> arrangePlan(int length);
std::vector<ArrangePart> arrangeSong(Song& s, int length, bool writeLead, std::mt19937& rng);

// Loads the samples the drum kits name from the sample library; returns the names it could not find.
QStringList resolveSamples(Song& s);
