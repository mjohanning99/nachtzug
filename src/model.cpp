// The song: its tables (scales, chords, synth settings and their names and explanations), the song, its tracks,
// knob lines and swing, and loading the samples a song names.
//
// What's here: SCALES and the other tables, formatParam(), paramTip(), paramLabel(), the Song, Track and AutoLane
// functions, unswing(), swingPos(), pitchName(), ui(), resolveSamples(). Saving and opening are in songfile.cpp,
// the sounds in sounds.cpp, the ideas and Arrange in ideas.cpp, the drummer in drummer.cpp, Clean up in
// cleanup.cpp.
// Changes in the song file: none here (songfile.cpp).
#include "model.h"
#include "samples.h"
#include <QCoreApplication>
#include <algorithm>
#include <cmath>

using namespace dsp;

const ScaleDef SCALES[] = {
    {QT_TRANSLATE_NOOP("Model", "Natural minor"), 7, {0, 2, 3, 5, 7, 8, 10}},
    {QT_TRANSLATE_NOOP("Model", "Dorian"), 7, {0, 2, 3, 5, 7, 9, 10}},
    {QT_TRANSLATE_NOOP("Model", "Phrygian"), 7, {0, 1, 3, 5, 7, 8, 10}},
    {QT_TRANSLATE_NOOP("Model", "Harmonic minor"), 7, {0, 2, 3, 5, 7, 8, 11}},
    {QT_TRANSLATE_NOOP("Model", "Phrygian dominant"), 7, {0, 1, 4, 5, 7, 8, 10}},
    {QT_TRANSLATE_NOOP("Model", "Minor pentatonic"), 5, {0, 3, 5, 7, 10, 0, 0}},
    {QT_TRANSLATE_NOOP("Model", "Major"), 7, {0, 2, 4, 5, 7, 9, 11}},
    {QT_TRANSLATE_NOOP("Model", "Lydian"), 7, {0, 2, 4, 6, 7, 9, 11}},
};
const int NUM_SCALES = int(sizeof(SCALES) / sizeof(SCALES[0]));
const char* const NOTE_NAMES[12] = {"C", "C♯", "D", "D♯", "E", "F", "F♯", "G", "G♯", "A", "A♯", "B"};
const char* const DRUM_NAMES[D_COUNT] = {
    QT_TRANSLATE_NOOP("Model", "Kick"),     QT_TRANSLATE_NOOP("Model", "Snare"),
    QT_TRANSLATE_NOOP("Model", "Clap"),     QT_TRANSLATE_NOOP("Model", "Closed hat"),
    QT_TRANSLATE_NOOP("Model", "Open hat"), QT_TRANSLATE_NOOP("Model", "Tom"),
    QT_TRANSLATE_NOOP("Model", "Rim"),      QT_TRANSLATE_NOOP("Model", "Cymbal")};
const char* const DRUM_STYLES[NUM_DRUM_STYLES] = {
    QT_TRANSLATE_NOOP("Model", "Four on the floor"), QT_TRANSLATE_NOOP("Model", "Acid house"),
    QT_TRANSLATE_NOOP("Model", "Motorik"),           QT_TRANSLATE_NOOP("Model", "Broken beat"),
    QT_TRANSLATE_NOOP("Model", "Half time"),         QT_TRANSLATE_NOOP("Model", "Electro"),
    QT_TRANSLATE_NOOP("Model", "Berlin pulse")};
const char* const CHORD_NAMES[7] = {
    QT_TRANSLATE_NOOP("Model", "Single note"),    QT_TRANSLATE_NOOP("Model", "Octaves"),
    QT_TRANSLATE_NOOP("Model", "Fifths"),         QT_TRANSLATE_NOOP("Model", "Triad"),
    QT_TRANSLATE_NOOP("Model", "Seventh chord"),  QT_TRANSLATE_NOOP("Model", "Open triad"),
    QT_TRANSLATE_NOOP("Model", "Stacked fourths")};
const char* const ARP_NAMES[6] = {QT_TRANSLATE_NOOP("Model", "Off"),    QT_TRANSLATE_NOOP("Model", "Up"),
                                  QT_TRANSLATE_NOOP("Model", "Down"),   QT_TRANSLATE_NOOP("Model", "Up and down"),
                                  QT_TRANSLATE_NOOP("Model", "Random"), QT_TRANSLATE_NOOP("Model", "As played")};
const char* const RATE_NAMES[4] = {"1/8", "1/8 triplet", "1/16", "1/32"};
const int RATE_TICKS[4] = {48, 32, 24, 12};
const char* const DTIME_NAMES[5] = {QT_TRANSLATE_NOOP("Model", "1/16"), QT_TRANSLATE_NOOP("Model", "1/8"),
                                    QT_TRANSLATE_NOOP("Model", "Dotted 1/8"), QT_TRANSLATE_NOOP("Model", "1/4"),
                                    QT_TRANSLATE_NOOP("Model", "Dotted 1/4")};
const float DTIME_BEATS[5] = {0.25f, 0.5f, 0.75f, 1.f, 1.5f};
const char* const ROLE_NAMES[R_COUNT] = {QT_TRANSLATE_NOOP("Model", "Bass"), QT_TRANSLATE_NOOP("Model", "Sequence"),
                                         QT_TRANSLATE_NOOP("Model", "Pad"), QT_TRANSLATE_NOOP("Model", "Lead"),
                                         QT_TRANSLATE_NOOP("Model", "Drums")};

const ParamDef PARAMS[P_COUNT] = {
    {"wave1", QT_TRANSLATE_NOOP("Model", "Wave 1"), 0, K_CHOICE, QT_TRANSLATE_NOOP("Model", "Saw|Pulse|Triangle|Sine")},
    {"wave2", QT_TRANSLATE_NOOP("Model", "Wave 2"), 0, K_CHOICE, QT_TRANSLATE_NOOP("Model", "Saw|Pulse|Triangle|Sine")},
    {"tune2", QT_TRANSLATE_NOOP("Model", "Osc 2 pitch"), 1, K_CHOICE,
	 QT_TRANSLATE_NOOP("Model", "-1 octave|Unison|+ fifth|+1 octave")},
    {"detune", QT_TRANSLATE_NOOP("Model", "Detune"), 0.2f, K_CENTS, nullptr},
    {"mix", QT_TRANSLATE_NOOP("Model", "Osc mix"), 0.3f, K_PCT, nullptr},
    {"sub", QT_TRANSLATE_NOOP("Model", "Sub"), 0, K_PCT, nullptr},
    {"noise", QT_TRANSLATE_NOOP("Model", "Noise"), 0, K_PCT, nullptr},
    {"pw", QT_TRANSLATE_NOOP("Model", "Pulse width"), 0, K_PCT, nullptr},
    {"cutoff", QT_TRANSLATE_NOOP("Model", "Cutoff"), 0.5f, K_CUTOFF, nullptr},
    {"res", QT_TRANSLATE_NOOP("Model", "Resonance"), 0.3f, K_PCT, nullptr},
    {"envAmt", QT_TRANSLATE_NOOP("Model", "Env amount"), 0.4f, K_PCT, nullptr},
    {"accent", QT_TRANSLATE_NOOP("Model", "Accent"), 0.5f, K_PCT, nullptr},
    {"slope", QT_TRANSLATE_NOOP("Model", "Type"), 0, K_CHOICE,
	 QT_TRANSLATE_NOOP("Model", "Low-pass 24 dB|Low-pass 12 dB|Band-pass|High-pass")},
    {"fA", QT_TRANSLATE_NOOP("Model", "Attack"), 0, K_TIME, nullptr},
    {"fD", QT_TRANSLATE_NOOP("Model", "Decay"), 0.5f, K_TIME, nullptr},
    {"fS", QT_TRANSLATE_NOOP("Model", "Sustain"), 0.2f, K_PCT, nullptr},
    {"aA", QT_TRANSLATE_NOOP("Model", "Attack"), 0, K_TIME, nullptr},
    {"aD", QT_TRANSLATE_NOOP("Model", "Decay"), 0.5f, K_TIME, nullptr},
    {"aS", QT_TRANSLATE_NOOP("Model", "Sustain"), 0.8f, K_PCT, nullptr},
    {"aR", QT_TRANSLATE_NOOP("Model", "Release"), 0.35f, K_TIME, nullptr},
    {"mono", QT_TRANSLATE_NOOP("Model", "Voices"), 0, K_CHOICE, QT_TRANSLATE_NOOP("Model", "Poly|Mono")},
    {"glide", QT_TRANSLATE_NOOP("Model", "Glide"), 0, K_GLIDE, nullptr},
    {"lfoWave", QT_TRANSLATE_NOOP("Model", "LFO shape"), 0, K_CHOICE,
	 QT_TRANSLATE_NOOP("Model", "Triangle|Square|Random")},
    {"lfoRate", QT_TRANSLATE_NOOP("Model", "LFO rate"), 0.4f, K_LFO, nullptr},
    {"lfoCut", QT_TRANSLATE_NOOP("Model", "LFO to filter"), 0, K_PCT, nullptr},
    {"lfoPitch", QT_TRANSLATE_NOOP("Model", "Vibrato"), 0, K_PCT, nullptr},
    {"drive", QT_TRANSLATE_NOOP("Model", "Drive"), 0.15f, K_PCT, nullptr},
    {"phaser", QT_TRANSLATE_NOOP("Model", "Phaser"), 0, K_PCT, nullptr},
    {"phRate", QT_TRANSLATE_NOOP("Model", "Phaser rate"), 0.3f, K_PHASER, nullptr},
    {"chorus", QT_TRANSLATE_NOOP("Model", "Chorus"), 0, K_PCT, nullptr},
    {"delay", QT_TRANSLATE_NOOP("Model", "Echo"), 0.2f, K_PCT, nullptr},
    {"reverb", QT_TRANSLATE_NOOP("Model", "Reverb"), 0.15f, K_PCT, nullptr},
    {"unison", QT_TRANSLATE_NOOP("Model", "Voices"), 0, K_CHOICE, QT_TRANSLATE_NOOP("Model", "1 (off)|2|3|5|7")},
    {"uDetune", QT_TRANSLATE_NOOP("Model", "Unison detune"), 0.3f, K_PCT, nullptr},
    {"width", QT_TRANSLATE_NOOP("Model", "Stereo spread"), 0, K_PCT, nullptr},
    {"fm", QT_TRANSLATE_NOOP("Model", "FM"), 0, K_PCT, nullptr},
    {"pitchEnv", QT_TRANSLATE_NOOP("Model", "To pitch"), 0, K_PCT, nullptr},
    {"keyTrack", QT_TRANSLATE_NOOP("Model", "Key tracking"), 0.35f, K_PCT, nullptr},
    {"lfoPw", QT_TRANSLATE_NOOP("Model", "LFO to width"), 0, K_PCT, nullptr},
};

static QString hz(float h) {
	if (h < 10.f) return QString::number(double(h), 'f', 2) + " Hz";
	if (h < 1000.f) return QString::number(int(std::lround(h))) + " Hz";
	return QString::number(double(h) / 1000.0, 'f', 1) + " kHz";
}

QString formatParam(int param, float v) {
	const ParamDef& d = PARAMS[param];
	switch (d.kind) {
	case K_TIME: {
		const float s = envTime(v);
		return s < 1.f ? QString::number(int(std::lround(s * 1000.f))) + " ms"
		               : QString::number(double(s), 'f', 1) + " s";
	}
	case K_CUTOFF:
		return hz(std::min(cutoffHz(v), 20000.f));
	case K_LFO:
		return hz(lfoHz(v));
	case K_PHASER:
		return hz(phaserHz(v));
	case K_CENTS:
		return QString::number(int(std::lround(v * 40.f))) + " cents";
	case K_GLIDE:
		return QString::number(int(std::lround(glideSec(v) * 1000.f))) + " ms";
	case K_CHOICE:
		return ui(d.choices).split('|').value(int(v + 0.5f));
	default:
		return QString::number(int(std::lround(v * 100.f))) + "%";
	}
}

QString paramTip(int param) {
	static const char* const tips[P_COUNT] = {
	    QT_TRANSLATE_NOOP(
	        "Model", "The shape of oscillator 1. Saw is bright and buzzy, pulse hollow, triangle soft, sine pure."),
	    QT_TRANSLATE_NOOP("Model", "The shape of oscillator 2, which is mixed with oscillator 1."),
	    QT_TRANSLATE_NOOP(
	        "Model",
	        "How oscillator 2 is tuned against oscillator 1: an octave down, the same, a fifth up or an octave up."),
	    QT_TRANSLATE_NOOP(
	        "Model", "Pulls oscillator 2 slightly out of tune, so the two beat against each other and sound wider."),
	    QT_TRANSLATE_NOOP("Model", "The balance between oscillator 1 (left) and oscillator 2 (right)."),
	    QT_TRANSLATE_NOOP("Model", "Adds a soft tone one octave below the note, for weight in the low end."),
	    QT_TRANSLATE_NOOP("Model", "Adds hiss, for breathy or percussive sounds."),
	    QT_TRANSLATE_NOOP(
	        "Model",
	        "Makes the pulse wave thinner and more nasal. Only changes the sound when a pulse wave is chosen."),
	    QT_TRANSLATE_NOOP("Model", "Where the filter cuts: low is dark and muffled, high is bright and open. The most "
		                           "important knob for acid sounds."),
	    QT_TRANSLATE_NOOP("Model", "Adds a peak at the cutoff: the squelchy, singing sound of acid basslines."),
	    QT_TRANSLATE_NOOP("Model", "How far each note opens the filter, following the filter envelope."),
	    QT_TRANSLATE_NOOP("Model", "How much louder and brighter accented notes are."),
	    QT_TRANSLATE_NOOP("Model", "The filter type. Low-pass removes the highs; band-pass keeps a narrow middle band; "
		                           "high-pass removes the lows."),
	    QT_TRANSLATE_NOOP("Model", "How quickly the filter opens when a note starts."),
	    QT_TRANSLATE_NOOP(
	        "Model", "How quickly the filter closes again after opening. Short gives plucks, long gives slow sweeps."),
	    QT_TRANSLATE_NOOP("Model", "How far open the filter stays while a note is held."),
	    QT_TRANSLATE_NOOP("Model", "How quickly a note gets loud. Short for plucks, long for slow swells."),
	    QT_TRANSLATE_NOOP("Model", "How quickly the loudness falls after the start of a note."),
	    QT_TRANSLATE_NOOP("Model", "How loud a note stays while it is held."),
	    QT_TRANSLATE_NOOP("Model", "How long a note rings after it ends."),
	    QT_TRANSLATE_NOOP(
	        "Model",
	        "Poly plays chords; Mono plays one note at a time, as acid basslines do, and slides between notes."),
	    QT_TRANSLATE_NOOP("Model", "How long the pitch takes to slide from one note to the next."),
	    QT_TRANSLATE_NOOP(
	        "Model",
	        "The shape of the LFO’s slow movement: smooth up and down, a jump between two values, or random steps."),
	    QT_TRANSLATE_NOOP("Model", "How fast the LFO moves."),
	    QT_TRANSLATE_NOOP("Model", "How much the LFO moves the filter: slow sweeps or a wobble."),
	    QT_TRANSLATE_NOOP("Model", "How much the LFO moves the pitch: vibrato."),
	    QT_TRANSLATE_NOOP("Model", "Distortion: warms the sound up at first, makes it grittier further up."),
	    QT_TRANSLATE_NOOP("Model", "A swirling, sweeping effect."),
	    QT_TRANSLATE_NOOP("Model", "How fast the phaser sweeps."),
	    QT_TRANSLATE_NOOP("Model", "Thickens the sound with slightly delayed, wavering copies."),
	    QT_TRANSLATE_NOOP("Model", "How much of this sound goes to the shared echo."),
	    QT_TRANSLATE_NOOP("Model", "How much of this sound goes to the shared reverb."),
	    QT_TRANSLATE_NOOP("Model", "Stacks several detuned copies of both oscillators, for a big, wide sound."),
	    QT_TRANSLATE_NOOP("Model", "How far apart the stacked copies are tuned."),
	    QT_TRANSLATE_NOOP("Model", "Spreads the notes across the left and right speakers."),
	    QT_TRANSLATE_NOOP("Model", "Lets oscillator 2 bend oscillator 1, for bells, metallic and glassy tones."),
	    QT_TRANSLATE_NOOP(
	        "Model",
	        "Bends the pitch with the filter envelope at the start of each note: zaps, lasers, drum-like hits."),
	    QT_TRANSLATE_NOOP("Model", "Makes higher notes brighter and lower notes darker, as on acoustic instruments."),
	    QT_TRANSLATE_NOOP("Model",
		                  "How much the LFO moves the pulse width, for a slowly shifting, chorus-like pulse sound."),
	};
	return param >= 0 && param < P_COUNT ? ui(tips[param]) : QString();
}

QString paramLabel(int param) {
	if (param == AP_VOLUME) return ui(QT_TRANSLATE_NOOP("Model", "Volume"));
	if (param < 0 || param >= P_COUNT) return QString();
	// the two envelopes share knob names, so say which one is meant
	static const char* const env[7] = {
	    QT_TRANSLATE_NOOP("Model", "Filter attack"),  QT_TRANSLATE_NOOP("Model", "Filter decay"),
	    QT_TRANSLATE_NOOP("Model", "Filter sustain"), QT_TRANSLATE_NOOP("Model", "Amp attack"),
	    QT_TRANSLATE_NOOP("Model", "Amp decay"),      QT_TRANSLATE_NOOP("Model", "Amp sustain"),
	    QT_TRANSLATE_NOOP("Model", "Amp release")};
	if (param >= P_FA && param <= P_FS) return ui(env[param - P_FA]);
	if (param >= P_AA && param <= P_AR) return ui(env[3 + param - P_AA]);
	return ui(PARAMS[param].label);
}

float Track::automated(int param, double tick, float fallback) const {
	if (param < 64 && (bypass >> param) & 1) return fallback;
	const AutoLane* l = lane(param);
	return l ? l->valueAt(tick, fallback) : fallback;
}

float AutoLane::valueAt(double tick, float fallback) const {
	if (pts.empty()) return fallback;
	const auto& p = pts;
	if (loop > 0) tick = std::fmod(std::max(tick, 0.0), double(loop));
	if (tick <= p.front().tick) return p.front().v;
	if (tick >= p.back().tick) return p.back().v;
	size_t lo = 0, hi = p.size() - 1;
	while (hi - lo > 1) {
		const size_t mid = (lo + hi) / 2;
		if (p[mid].tick <= tick)
			lo = mid;
		else
			hi = mid;
	}
	const double span = double(p[hi].tick - p[lo].tick);
	return span <= 0.0 ? p[hi].v : p[lo].v + (p[hi].v - p[lo].v) * float((tick - p[lo].tick) / span);
}

// ---------------------------------------------------------------- Song

void Song::setChord(int bar, int deg, int alt) {
	if (bar < 0) return;
	// write the chords out bar by bar for the whole song first, so only this bar changes
	const int want = std::max(bar + 1, (std::max(endTick(), loopEnd) + BAR - 1) / BAR);
	if (int(chords.size()) < want) {
		std::vector<int> all(static_cast<size_t>(want)), alts(static_cast<size_t>(want));
		for (int i = 0; i < want; i++) {
			all[size_t(i)] = chordAt(i);
			alts[size_t(i)] = chordAltAt(i);
		}
		chords = all;
		chordAlts = alts;
	}
	if (alt != 0 || bar < int(chordAlts.size())) {
		if (int(chordAlts.size()) < int(chords.size())) chordAlts.resize(chords.size(), 0);
		chordAlts[size_t(bar)] = std::clamp(alt, -1, 1);
	}
	chords[size_t(bar)] = deg;
}

int Song::approachAlt(int deg) const {
	const int semis = degToSemis(deg);
	auto inScale = [&](int x) { return degToSemis(degreeNear(x)) == x; };
	if (!inScale(semis - 1)) return -1;
	if (!inScale(semis + 1)) return 1;
	return 0;
}

int Song::degToSemis(int deg) const {
	const ScaleDef& s = sc();
	const int o = int(std::floor(double(deg) / s.n));
	return 12 * o + s.iv[((deg % s.n) + s.n) % s.n];
}
int Song::degreeNear(int semis) const {
	int best = 0, dist = 1 << 20;
	for (int d = -6 * sc().n; d <= 6 * sc().n; d++) {
		const int x = std::abs(degToSemis(d) - semis);
		if (x < dist) {
			dist = x;
			best = d;
		}
	}
	return best;
}
int Song::midiOf(const Track& t, int deg) const {
	return 33 + ((root + 3) % 12) + 12 * t.octave + degToSemis(deg);
}

QString Song::noteName(const Track& t, int deg, int alt) const {
	if (t.isDrums()) return ui(DRUM_NAMES[std::clamp(deg, 0, D_COUNT - 1)]);
	const int m = midiOf(t, deg) + alt;
	return pitchName(((m % 12) + 12) % 12) + QString::number(m / 12 - 1);
}

QString Song::chordName(int deg, int alt) const {
	const int base = degToSemis(deg);
	const int pc = (((root + base + alt) % 12) + 12) % 12; // a moved chord keeps its shape, a semitone off
	const int third = degToSemis(deg + q(2)) - base, fifth = degToSemis(deg + q(4)) - base;
	QString n = pitchName(pc);
	if (third == 3)
		n += fifth == 6 ? "dim" : "m";
	else if (third == 4 && fifth == 8)
		n += "aug";
	else if (third == 2)
		n += "sus2";
	else if (third == 5)
		n += "sus4";
	return n;
}

void Song::chordOffsets(int chord, int out[4], int& count) const {
	static const int shapes[7][4] = {{0, -1, -1, -1}, {0, 7, -1, -1}, {0, 4, -1, -1}, {0, 2, 4, -1},
	                                 {0, 2, 4, 6},    {0, 4, 9, -1},  {0, 3, 6, -1}};
	const int* sh = shapes[std::clamp(chord, 0, 6)];
	const int n = sc().n;
	count = 0;
	for (int i = 0; i < 4 && sh[i] >= 0; i++)
		out[count++] = sh[i] == 7 ? n : sh[i] == 9 ? n + q(2) : q(sh[i]);
}

Track* Song::track(int id) {
	for (auto& t : tracks)
		if (t.id == id) return &t;
	return nullptr;
}
const Track* Song::track(int id) const {
	for (auto& t : tracks)
		if (t.id == id) return &t;
	return nullptr;
}
int Song::trackIndex(int id) const {
	for (size_t i = 0; i < tracks.size(); i++)
		if (tracks[i].id == id) return int(i);
	return -1;
}
Clip* Song::clip(int id, Track** owner) {
	for (auto& t : tracks) {
		for (auto& c : t.clips)
			if (c.id == id) {
				if (owner) *owner = &t;
				return &c;
			}
		for (auto& c : t.cells)
			if (c.id == id) {
				if (owner) *owner = &t;
				return &c;
			}
	}
	return nullptr;
}
int Song::endTick() const {
	int e = 0;
	for (auto& t : tracks)
		for (auto& c : t.clips)
			e = std::max(e, c.end());
	return e;
}
bool Song::anySolo() const {
	for (auto& t : tracks)
		if (t.solo) return true;
	return false;
}

double unswing(double p, float swing) {
	const double s = double(swing) * 12.0;
	if (s <= 0.01) return p;
	const double base = std::floor(p / 48.0) * 48.0, r = p - base;
	return base + (r < 24.0 + s ? r * 24.0 / (24.0 + s) : 24.0 + (r - 24.0 - s) * 24.0 / (24.0 - s));
}

double swingPos(double m, float swing) {
	const double s = double(swing) * 12.0;
	if (s <= 0.01) return m;
	const double base = std::floor(m / 48.0) * 48.0, r = m - base;
	return base + (r < 24.0 ? r * (24.0 + s) / 24.0 : 24.0 + s + (r - 24.0) * (24.0 - s) / 24.0);
}

QString pitchName(int pc) {
	pc = ((pc % 12) + 12) % 12;
	if (pc == 11)
		return QCoreApplication::translate("Model", "B", "note name: the note a semitone below C (German: H)");
	return QString::fromUtf8(NOTE_NAMES[pc]);
}

QString ui(const char* text) {
	return QCoreApplication::translate("Model", text);
}
QString ui(const QString& text) {
	return text.isEmpty() ? text : QCoreApplication::translate("Model", text.toUtf8().constData());
}

QStringList resolveSamples(Song& s) {
	QStringList missing;
	for (Track& t : s.tracks) {
		if (!t.isDrums()) continue;
		for (int i = 0; i < D_COUNT; i++) {
			if (t.kit.sample[i].isEmpty()) {
				t.kit.data[i].reset();
				continue;
			}
			auto d = samples::get(t.kit.sample[i]); // the same loaded file every time, unless the file itself changed
			if (d != t.kit.data[i]) t.kit.data[i] = std::move(d);
			if (!t.kit.data[i]) missing << t.kit.sample[i];
		}
	}
	missing.removeDuplicates();
	return missing;
}
