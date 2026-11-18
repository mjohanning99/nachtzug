// The song: its tables (scales, chords, synth settings and their names and explanations), the song, its tracks,
// knob lines and swing, and loading the samples a song names.
//
// What's here: SCALES and the other tables, formatParam(), paramTip(), paramLabel(), the Song, Track and AutoLane
// functions, unswing(), swingPos(), pitchName(), ui(), resolveSamples(). Saving and opening are in songfile.cpp,
// the sounds in sounds.cpp, the ideas and Arrange in ideas.cpp, the long journey in journey.cpp, the drummer in
// drummer.cpp, Clean up in cleanup.cpp.
// Changes in the song file: none here (songfile.cpp).
#include "model.h"
#include <QRegularExpression>
#include <QLocale>
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
// the echo's times: the first five keep the numbers older songs saved them under, the triplets and the half note
// came later (their lengths are dsp::echoBeats())
const char* const DTIME_NAMES[ECHO_TIMES] = {
    QT_TRANSLATE_NOOP("Model", "1/16"),        QT_TRANSLATE_NOOP("Model", "1/8"),
    QT_TRANSLATE_NOOP("Model", "Dotted 1/8"),  QT_TRANSLATE_NOOP("Model", "1/4"),
    QT_TRANSLATE_NOOP("Model", "Dotted 1/4"),  QT_TRANSLATE_NOOP("Model", "1/8 triplet"),
    QT_TRANSLATE_NOOP("Model", "1/4 triplet"), QT_TRANSLATE_NOOP("Model", "1/2")};
const int DTIME_ORDER[ECHO_TIMES] = {0, 5, 1, 6, 2, 3, 4, 7};
// the LFO's note values when it runs in time (dsp::lfoBeats())
const char* const LFO_SYNC_NAMES[LFO_SYNC_STEPS] = {
    QT_TRANSLATE_NOOP("Model", "8 bars"),      QT_TRANSLATE_NOOP("Model", "4 bars"),
    QT_TRANSLATE_NOOP("Model", "2 bars"),      QT_TRANSLATE_NOOP("Model", "1 bar"),
    QT_TRANSLATE_NOOP("Model", "1/2"),         QT_TRANSLATE_NOOP("Model", "1/4"),
    QT_TRANSLATE_NOOP("Model", "1/4 triplet"), QT_TRANSLATE_NOOP("Model", "1/8"),
    QT_TRANSLATE_NOOP("Model", "1/8 triplet"), QT_TRANSLATE_NOOP("Model", "1/16"),
    QT_TRANSLATE_NOOP("Model", "1/32")};
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
	 QT_TRANSLATE_NOOP("Model", "Low-pass 24 dB|Low-pass 12 dB|Band-pass|High-pass|Notch")},
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
    // the LFO in time, to the volume and the pan, and its fade-in
    {"lfoSync", QT_TRANSLATE_NOOP("Model", "LFO timing"), 0, K_CHOICE, QT_TRANSLATE_NOOP("Model", "Free|In time")},
    {"lfoAmp", QT_TRANSLATE_NOOP("Model", "LFO to volume"), 0, K_PCT, nullptr},
    {"lfoPan", QT_TRANSLATE_NOOP("Model", "LFO to pan"), 0, K_PCT, nullptr},
    {"lfoFade", QT_TRANSLATE_NOOP("Model", "LFO fade-in"), 0, K_FADE, nullptr},
    // the filter envelope's release and the equaliser
    {"fR", QT_TRANSLATE_NOOP("Model", "Release"), 0, K_FRELEASE, nullptr},
    {"eqCut", QT_TRANSLATE_NOOP("Model", "Low cut"), 0, K_LOWCUT, nullptr},
    {"eqLow", QT_TRANSLATE_NOOP("Model", "Low"), 0.5f, K_DB, nullptr},
    {"eqMid", QT_TRANSLATE_NOOP("Model", "Mid"), 0.5f, K_DB, nullptr},
    {"eqFreq", QT_TRANSLATE_NOOP("Model", "Mid pitch"), 0.5f, K_MIDHZ, nullptr},
    {"eqHigh", QT_TRANSLATE_NOOP("Model", "High"), 0.5f, K_DB, nullptr},
};

// a number and its unit, joined by a narrow no-break space; decimals as the language writes them (2,5 in German)
static QString withUnit(double v, int decimals, const char* unit) {
	QLocale loc;
	loc.setNumberOptions(QLocale::OmitGroupSeparator);
	return loc.toString(v, 'f', decimals) + QChar(0x202F) + QLatin1String(unit);
}

static QString hz(float h) {
	if (h < 10.f) return withUnit(double(h), 2, "Hz");
	if (h < 1000.f) return withUnit(std::round(double(h)), 0, "Hz");
	return withUnit(double(h) / 1000.0, 1, "kHz");
}

QString formatParam(int param, float v) {
	const ParamDef& d = PARAMS[param];
	switch (d.kind) {
	case K_TIME: {
		const float s = envTime(v);
		return s < 1.f ? withUnit(std::round(double(s) * 1000.0), 0, "ms") : withUnit(double(s), 1, "s");
	}
	case K_CUTOFF:
		return hz(std::min(cutoffHz(v), 20000.f));
	case K_LFO:
		return hz(lfoHz(v));
	case K_PHASER:
		return hz(phaserHz(v));
	case K_CENTS:
		return ui(QT_TRANSLATE_NOOP("Model", "%1 cents")).arg(int(std::lround(v * 40.f)));
	case K_GLIDE:
		return withUnit(std::round(double(glideSec(v)) * 1000.0), 0, "ms");
	case K_CHOICE:
		return ui(d.choices).split('|').value(int(v + 0.5f));
	case K_FADE:
		return v <= 0.f ? ui(QT_TRANSLATE_NOOP("Model", "Off")) : withUnit(double(lfoFadeSec(v)), 1, "s");
	case K_FRELEASE:
		return v > 0.f ? formatParam(P_AR, v) : ui(QT_TRANSLATE_NOOP("Model", "As amp"));
	case K_DB: {
		const float db = eqDb(v);
		if (std::fabs(db) < 0.05f) return withUnit(0, 0, "dB");
		return (db > 0 ? QStringLiteral("+") : QStringLiteral("−")) + withUnit(double(std::fabs(db)), 1, "dB");
	}
	case K_MIDHZ:
		return hz(eqMidHz(v));
	case K_LOWCUT:
		return v > 0.f ? hz(lowCutHz(v)) : ui(QT_TRANSLATE_NOOP("Model", "Off"));
	default:
		return QString::number(int(std::lround(v * 100.f))) + "%";
	}
}

QString lfoRateText(float v, bool inTime) {
	return inTime ? ui(LFO_SYNC_NAMES[lfoSyncStep(v)]) : hz(lfoHz(v));
}

QString formatParam(int param, float v, const Patch& patch) {
	if (param == P_LFORATE) return lfoRateText(v, patch.p[P_LFOSYNC] > 0.5f);
	return formatParam(param, v);
}

QString echoToneText(float v) { // the repeats' low-pass, as a frequency (at 48 kHz)
	const float a = echoToneCoef(v);
	if (a >= 0.999f) return ui(QT_TRANSLATE_NOOP("Model", "open"));
	return hz(-std::log(1.f - a) * 48000.f / TAU);
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
		                           "high-pass removes the lows; notch takes out a narrow band and keeps the rest."),
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
	    QT_TRANSLATE_NOOP("Model", "How fast the LFO moves. In time, how long one sweep lasts, as a note value."),
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
	    QT_TRANSLATE_NOOP("Model",
		                  "Free: the LFO moves at its own speed. In time: it follows the song’s tempo, and the "
		                  "rate knob picks a note value, from eight bars to a thirty-second note."),
	    QT_TRANSLATE_NOOP("Model", "How much the LFO moves the volume: a gentle throb, or a chopped, stuttering sound "
		                           "with the square shape."),
	    QT_TRANSLATE_NOOP("Model", "How much the LFO moves the sound between the left and the right speaker."),
	    QT_TRANSLATE_NOOP("Model", "How slowly the LFO fades in after a note starts, so a held note begins still and "
		                           "then starts to sway: a delayed vibrato."),
	    // the filter envelope's release and the equaliser
	    QT_TRANSLATE_NOOP("Model", "How quickly the filter closes after a note ends. All the way down, it follows the "
		                           "amp envelope’s release, as it always did."),
	    QT_TRANSLATE_NOOP("Model", "Takes away the deepest notes and rumble below this pitch, so the track leaves room "
		                           "for the bass and the kick. Off all the way down."),
	    QT_TRANSLATE_NOOP("Model", "More or less of the low end: weight and warmth. Flat in the middle."),
	    QT_TRANSLATE_NOOP("Model", "More or less of the middle, where a sound is hollow or honking. Flat in the "
		                           "middle; Mid pitch says where."),
	    QT_TRANSLATE_NOOP("Model", "Where the Mid knob lifts or lowers: low for body, high for bite."),
	    QT_TRANSLATE_NOOP("Model", "More or less of the top end: air and sparkle, or a softer, darker sound. Flat in "
		                           "the middle."),
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
	if (param == P_FR) return ui(QT_TRANSLATE_NOOP("Model", "Filter release"));
	if (param >= P_EQCUT && param <= P_EQHIGH) { // the equaliser's knobs, named as the timeline's lines show them
		static const char* const eq[5] = {QT_TRANSLATE_NOOP("Model", "EQ low cut"),
		                                  QT_TRANSLATE_NOOP("Model", "EQ low"), QT_TRANSLATE_NOOP("Model", "EQ mid"),
		                                  QT_TRANSLATE_NOOP("Model", "EQ mid pitch"),
		                                  QT_TRANSLATE_NOOP("Model", "EQ high")};
		return ui(eq[param - P_EQCUT]);
	}
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
void Song::setParts(std::vector<SongPart> list) {
	for (SongPart& pt : list)
		pt.bar = std::max(0, pt.bar);
	std::stable_sort(list.begin(), list.end(), [](const SongPart& a, const SongPart& b) { return a.bar < b.bar; });
	// one part to a bar: of two at the same bar the later one wins, as addPart() would have it
	std::vector<SongPart> kept;
	for (SongPart& pt : list) {
		if (!kept.empty() && kept.back().bar == pt.bar)
			kept.back() = std::move(pt);
		else
			kept.push_back(std::move(pt));
	}
	parts = std::move(kept);
}
void Song::addPart(int bar, const QString& name) {
	bar = std::max(0, bar);
	if (const int i = partIndex(bar); i >= 0) {
		parts[size_t(i)].name = name;
		return;
	}
	const auto at =
	    std::upper_bound(parts.begin(), parts.end(), bar, [](int b, const SongPart& pt) { return b < pt.bar; });
	parts.insert(at, SongPart{bar, name});
}
bool Song::removePart(int bar) {
	const int i = partIndex(bar);
	if (i < 0) return false;
	parts.erase(parts.begin() + i);
	return true;
}
int Song::partAt(int bar) const {
	int found = -1;
	for (size_t i = 0; i < parts.size() && parts[i].bar <= bar; i++)
		found = int(i);
	return found;
}
int Song::partIndex(int bar) const {
	for (size_t i = 0; i < parts.size(); i++)
		if (parts[i].bar == bar) return int(i);
	return -1;
}
int Song::partEnd(size_t i) const {
	if (i + 1 < parts.size()) return parts[i + 1].bar;
	return std::max(i < parts.size() ? parts[i].bar + 1 : 1, (endTick() + BAR - 1) / BAR);
}
int Song::repeatPart(size_t i) {
	if (i >= parts.size()) return -1;
	const int a = parts[i].bar, b = partEnd(i), n = b - a;
	if (n <= 0) return -1;
	const int ta = a * BAR, tb = b * BAR, shift = n * BAR;
	// the chords repeat across the song, so write them out bar by bar first; then the copy's bars go in
	if (chords.size() > 1 || !chordAlts.empty()) {
		const int want = std::max(b, (std::max(endTick(), loopEnd) + BAR - 1) / BAR);
		std::vector<int> all(static_cast<size_t>(want)), alts(static_cast<size_t>(want));
		for (int k = 0; k < want; k++) {
			all[size_t(k)] = chordAt(k);
			alts[size_t(k)] = chordAltAt(k);
		}
		const std::vector<int> allPart(all.begin() + a, all.begin() + b), altsPart(alts.begin() + a, alts.begin() + b);
		all.insert(all.begin() + b, allPart.begin(), allPart.end());
		alts.insert(alts.begin() + b, altsPart.begin(), altsPart.end());
		chords = std::move(all);
		chordAlts = std::move(alts);
	}
	// a clip that runs across the part's start or end is cut in two there first (as App::splitClip cuts one, the
	// pattern carrying on where it was), so the part's own bars are copied, and nothing outside it is doubled
	for (Track& t : tracks)
		for (const int cut : {ta, tb})
			for (size_t k = 0; k < t.clips.size(); k++) {
				Clip& c = t.clips[k];
				if (cut <= c.start || cut >= c.end()) continue;
				Clip right = c;
				right.id = newId();
				right.start = cut;
				right.len = c.end() - cut;
				right.offset = c.loopLen > 0 ? (c.offset + (cut - c.start)) % c.loopLen : c.offset;
				right.inKind = 0; // the transitions stay at the outer ends
				c.outKind = 0;
				c.len = cut - c.start;
				t.clips.push_back(right); // c is invalid from here on
			}
	for (Track& t : tracks) {
		std::vector<Clip> copies;
		for (Clip& c : t.clips) {
			if (c.start >= tb)
				c.start += shift;
			else if (c.start >= ta) {
				Clip d = c;
				d.id = newId();
				d.start += shift;
				copies.push_back(std::move(d));
			}
		}
		t.clips.insert(t.clips.end(), copies.begin(), copies.end());
		for (AutoLane& l : t.lanes) {
			if (l.loop > 0) continue; // a shape that repeats on its own stays as it is
			if (!l.pts.empty()) {
				// pin the line at the part's two ends first, so the copy starts where the part starts and the part
				// still ends where it ended, whichever points happen to lie around them
				const float va = l.valueAt(ta, 0.f), vb = l.valueAt(tb - 1, 0.f);
				auto pin = [&l](int tick, float v) {
					for (const AutoPoint& p : l.pts)
						if (p.tick == tick) return;
					l.pts.push_back({tick, v});
				};
				pin(ta, va);
				pin(tb - 1, vb);
				std::stable_sort(l.pts.begin(), l.pts.end(),
				                 [](const AutoPoint& x, const AutoPoint& y) { return x.tick < y.tick; });
			}
			std::vector<AutoPoint> out;
			for (const AutoPoint& p : l.pts) {
				out.push_back({p.tick >= tb ? p.tick + shift : p.tick, p.v});
				if (p.tick >= ta && p.tick < tb) out.push_back({p.tick + shift, p.v});
			}
			std::stable_sort(out.begin(), out.end(),
			                 [](const AutoPoint& x, const AutoPoint& y) { return x.tick < y.tick; });
			l.pts = std::move(out);
		}
	}
	if (loopStart >= tb) loopStart += shift;
	if (loopEnd > tb) loopEnd += shift;
	for (SongPart& pt : parts)
		if (pt.bar >= b) pt.bar += n;
	// the name: without a number at its end, then the first number from 2 that no part has
	QString base = parts[i].name;
	static const QRegularExpression tail(QStringLiteral("\\s+\\d+$"));
	base.remove(tail);
	int num = 2;
	for (bool taken = true; taken;) {
		taken = false;
		for (const SongPart& pt : parts)
			if (pt.name == base + QLatin1Char(' ') + QString::number(num)) taken = true;
		if (taken) num++;
	}
	addPart(b, base + QLatin1Char(' ') + QString::number(num));
	return partIndex(b);
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
QString partName(const QString& name) {
	const QString whole = ui(name);
	if (whole != name) return whole;
	// a copy of a suggested part ("Rising 2", from Repeat this part): the name in the program's language, its number
	static const QRegularExpression numbered(QStringLiteral("^(.*\\S)\\s+(\\d+)$"));
	const QRegularExpressionMatch m = numbered.match(name);
	if (!m.hasMatch()) return name;
	const QString base = ui(m.captured(1));
	return base == m.captured(1) ? name : base + QLatin1Char(' ') + m.captured(2);
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
