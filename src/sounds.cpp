// The sounds: the presets, the drum kits, a rolled sound for each kind of track, a mutated one, and a kit chosen
// over the one a track has.
//
// What's here: presets(), presetGroup(), presetByName(), drumKits(), rollPatch(), mutatePatch(), chooseKit(); used
// by the Sound tab, the inspector, new tracks and Arrange.
// Changes in the song file: none (a track saves its sound's values, not which preset they came from).
#include "model.h"
#include "patterns.h"
#include "samples.h"
#include <QHash>
#include <algorithm>

using namespace dsp;

using namespace patterns;

// ---------------------------------------------------------------- sounds

static Patch mk(const char* name, std::initializer_list<std::pair<int, float>> v) {
	Patch p;
	p.name = QString::fromLatin1(name);
	for (int i = 0; i < P_COUNT; i++)
		p.p[i] = PARAMS[i].def;
	for (auto& kv : v)
		p.p[kv.first] = kv.second;
	return p;
}

const std::vector<Patch>& presets() {
	// clang-format off: one preset per line
	static const std::vector<Patch> list = {
		mk(QT_TRANSLATE_NOOP("Model", "Acid Squelch"), {{P_MIX, 0}, {P_CUTOFF, .30f}, {P_RES, .80f}, {P_ENVAMT, .62f}, {P_ACCENT, .8f}, {P_FD, .58f}, {P_FS, 0}, {P_AD, .7f}, {P_AS, .5f}, {P_AR, .3f}, {P_MONO, 1}, {P_DRIVE, .45f}, {P_DELAY, .22f}, {P_REVERB, .08f}}),
		mk(QT_TRANSLATE_NOOP("Model", "Rubber Square"), {{P_WAVE1, 1}, {P_MIX, 0}, {P_CUTOFF, .27f}, {P_RES, .7f}, {P_ENVAMT, .5f}, {P_ACCENT, .7f}, {P_FD, .62f}, {P_FS, 0}, {P_AD, .7f}, {P_AS, .5f}, {P_AR, .3f}, {P_MONO, 1}, {P_DRIVE, .3f}, {P_DELAY, .2f}, {P_REVERB, .1f}}),
		mk(QT_TRANSLATE_NOOP("Model", "Deep Bass"), {{P_WAVE2, 1}, {P_TUNE2, 0}, {P_MIX, .5f}, {P_SUB, .6f}, {P_CUTOFF, .3f}, {P_RES, .25f}, {P_ENVAMT, .4f}, {P_FD, .65f}, {P_MONO, 1}, {P_GLIDE, .15f}, {P_DELAY, .05f}, {P_REVERB, .05f}}),
		mk(QT_TRANSLATE_NOOP("Model", "Berlin Sequence"), {{P_WAVE2, 1}, {P_TUNE2, 0}, {P_DETUNE, .15f}, {P_MIX, .4f}, {P_CUTOFF, .42f}, {P_RES, .35f}, {P_ENVAMT, .45f}, {P_FD, .6f}, {P_FS, .1f}, {P_AD, .62f}, {P_AS, .3f}, {P_AR, .5f}, {P_CHORUS, .2f}, {P_DELAY, .55f}, {P_REVERB, .2f}}),
		mk(QT_TRANSLATE_NOOP("Model", "Glass Pluck"), {{P_WAVE1, 2}, {P_TUNE2, 3}, {P_MIX, .35f}, {P_CUTOFF, .55f}, {P_RES, .2f}, {P_ENVAMT, .5f}, {P_FD, .55f}, {P_FS, 0}, {P_AD, .6f}, {P_AS, 0}, {P_AR, .6f}, {P_CHORUS, .3f}, {P_DELAY, .5f}, {P_REVERB, .35f}}),
		mk(QT_TRANSLATE_NOOP("Model", "Pulse Wanderer"), {{P_WAVE1, 1}, {P_WAVE2, 1}, {P_PW, .55f}, {P_DETUNE, .3f}, {P_MIX, .45f}, {P_CUTOFF, .45f}, {P_RES, .3f}, {P_ENVAMT, .35f}, {P_FD, .62f}, {P_FS, .15f}, {P_AD, .64f}, {P_AS, .25f}, {P_AR, .55f}, {P_LFORATE, .2f}, {P_LFOCUT, .2f}, {P_PHASER, .35f}, {P_DELAY, .5f}, {P_REVERB, .25f}}),
		mk(QT_TRANSLATE_NOOP("Model", "String Machine"), {{P_DETUNE, .5f}, {P_MIX, .5f}, {P_CUTOFF, .55f}, {P_RES, .1f}, {P_ENVAMT, .15f}, {P_FA, .7f}, {P_FD, .8f}, {P_FS, .7f}, {P_AA, .72f}, {P_AD, .8f}, {P_AS, .9f}, {P_AR, .8f}, {P_DRIVE, .05f}, {P_PHASER, .45f}, {P_PHRATE, .2f}, {P_CHORUS, .7f}, {P_DELAY, .2f}, {P_REVERB, .5f}}),
		mk(QT_TRANSLATE_NOOP("Model", "Phase Choir"), {{P_WAVE1, 1}, {P_MIX, .5f}, {P_DETUNE, .4f}, {P_NOISE, .1f}, {P_CUTOFF, .45f}, {P_RES, .3f}, {P_ENVAMT, .2f}, {P_FA, .75f}, {P_FD, .8f}, {P_FS, .6f}, {P_AA, .75f}, {P_AS, .9f}, {P_AR, .82f}, {P_LFORATE, .15f}, {P_LFOCUT, .25f}, {P_PHASER, .7f}, {P_CHORUS, .5f}, {P_DELAY, .25f}, {P_REVERB, .6f}}),
		mk(QT_TRANSLATE_NOOP("Model", "Night Drone"), {{P_WAVE2, 1}, {P_TUNE2, 0}, {P_MIX, .5f}, {P_SUB, .5f}, {P_CUTOFF, .35f}, {P_RES, .4f}, {P_ENVAMT, .1f}, {P_AA, .8f}, {P_AS, 1}, {P_AR, .85f}, {P_LFORATE, .08f}, {P_LFOCUT, .5f}, {P_PHASER, .6f}, {P_PHRATE, .12f}, {P_CHORUS, .3f}, {P_DELAY, .3f}, {P_REVERB, .6f}}),
		mk(QT_TRANSLATE_NOOP("Model", "Mellow Lead"), {{P_WAVE1, 2}, {P_WAVE2, 3}, {P_TUNE2, 3}, {P_MIX, .25f}, {P_CUTOFF, .6f}, {P_RES, .15f}, {P_ENVAMT, .2f}, {P_AA, .35f}, {P_AS, .9f}, {P_AR, .6f}, {P_MONO, 1}, {P_GLIDE, .35f}, {P_LFORATE, .78f}, {P_LFOPITCH, .35f}, {P_DELAY, .5f}, {P_REVERB, .4f}}),
		mk(QT_TRANSLATE_NOOP("Model", "Saw Lead"), {{P_DETUNE, .3f}, {P_MIX, .5f}, {P_CUTOFF, .62f}, {P_RES, .3f}, {P_ENVAMT, .3f}, {P_FD, .7f}, {P_FS, .5f}, {P_AS, .9f}, {P_AR, .55f}, {P_MONO, 1}, {P_GLIDE, .25f}, {P_DRIVE, .35f}, {P_DELAY, .5f}, {P_REVERB, .3f}}),
		mk(QT_TRANSLATE_NOOP("Model", "Supersaw Pad"), {{P_UNISON, 4}, {P_UDETUNE, .45f}, {P_WIDTH, .8f}, {P_MIX, 0}, {P_CUTOFF, .58f}, {P_RES, .1f}, {P_ENVAMT, .1f}, {P_FA, .6f}, {P_FD, .8f}, {P_FS, .7f}, {P_AA, .65f}, {P_AS, .9f}, {P_AR, .8f}, {P_DRIVE, .05f}, {P_CHORUS, .4f}, {P_DELAY, .2f}, {P_REVERB, .55f}}),
		mk(QT_TRANSLATE_NOOP("Model", "Wide Unison Lead"), {{P_UNISON, 3}, {P_UDETUNE, .3f}, {P_MIX, 0}, {P_MONO, 1}, {P_GLIDE, .2f}, {P_CUTOFF, .62f}, {P_RES, .25f}, {P_ENVAMT, .3f}, {P_FD, .7f}, {P_FS, .5f}, {P_AS, .9f}, {P_AR, .5f}, {P_DRIVE, .3f}, {P_DELAY, .45f}, {P_REVERB, .3f}}),
		mk(QT_TRANSLATE_NOOP("Model", "FM Bell"), {{P_WAVE1, 3}, {P_WAVE2, 3}, {P_TUNE2, 3}, {P_MIX, 0}, {P_FM, .55f}, {P_CUTOFF, .8f}, {P_RES, 0}, {P_ENVAMT, 0}, {P_AD, .7f}, {P_AS, 0}, {P_AR, .7f}, {P_WIDTH, .5f}, {P_DRIVE, 0}, {P_CHORUS, .2f}, {P_DELAY, .4f}, {P_REVERB, .5f}}),
		mk(QT_TRANSLATE_NOOP("Model", "Zap Bass"), {{P_WAVE1, 1}, {P_MIX, 0}, {P_PENV, .35f}, {P_FD, .45f}, {P_FS, 0}, {P_CUTOFF, .35f}, {P_RES, .5f}, {P_ENVAMT, .5f}, {P_MONO, 1}, {P_AD, .6f}, {P_AS, .4f}, {P_AR, .3f}, {P_DRIVE, .4f}, {P_DELAY, .1f}, {P_REVERB, .05f}}),
		mk(QT_TRANSLATE_NOOP("Model", "Hollow Band"), {{P_SLOPE, 2}, {P_WAVE2, 1}, {P_MIX, .4f}, {P_CUTOFF, .5f}, {P_RES, .55f}, {P_ENVAMT, .35f}, {P_FD, .65f}, {P_FS, .2f}, {P_AD, .65f}, {P_AS, .5f}, {P_LFORATE, .25f}, {P_LFOCUT, .3f}, {P_DELAY, .5f}, {P_REVERB, .3f}}),
		mk(QT_TRANSLATE_NOOP("Model", "Thin Air"), {{P_SLOPE, 3}, {P_NOISE, .2f}, {P_DETUNE, .5f}, {P_MIX, .5f}, {P_UNISON, 1}, {P_WIDTH, .7f}, {P_CUTOFF, .45f}, {P_RES, .2f}, {P_ENVAMT, .1f}, {P_AA, .7f}, {P_AS, .9f}, {P_AR, .85f}, {P_CHORUS, .6f}, {P_DELAY, .3f}, {P_REVERB, .7f}}),
		mk(QT_TRANSLATE_NOOP("Model", "Sample and Hold"), {{P_WAVE1, 1}, {P_MIX, 0}, {P_CUTOFF, .4f}, {P_RES, .65f}, {P_ENVAMT, .2f}, {P_FD, .6f}, {P_AS, .7f}, {P_AR, .5f}, {P_LFOWAVE, 2}, {P_LFORATE, .72f}, {P_LFOCUT, .6f}, {P_DELAY, .45f}, {P_REVERB, .3f}}),
	};
	// clang-format on
	return list;
}

QString presetGroup(const QString& name) {
	static const QHash<QString, QString> groups = {{"Acid Squelch", "Bass"},
	                                               {"Rubber Square", "Bass"},
	                                               {"Deep Bass", "Bass"},
	                                               {"Zap Bass", "Bass"},
	                                               {"Berlin Sequence", "Sequence and pluck"},
	                                               {"Glass Pluck", "Sequence and pluck"},
	                                               {"Pulse Wanderer", "Sequence and pluck"},
	                                               {"Hollow Band", "Sequence and pluck"},
	                                               {"FM Bell", "Sequence and pluck"},
	                                               {"Sample and Hold", "Sequence and pluck"},
	                                               {"String Machine", "Pad"},
	                                               {"Supersaw Pad", "Pad"},
	                                               {"Phase Choir", "Pad"},
	                                               {"Night Drone", "Pad"},
	                                               {"Thin Air", "Pad"},
	                                               {"Mellow Lead", "Lead"},
	                                               {"Saw Lead", "Lead"},
	                                               {"Wide Unison Lead", "Lead"}};
	[[maybe_unused]] static const char* const shown[] = {
	    QT_TRANSLATE_NOOP("Model", "Sequence and pluck"),
	    QT_TRANSLATE_NOOP("Model", "Other")}; // group names, translated where shown
	return groups.value(name, QStringLiteral("Other"));
}

Patch presetByName(const QString& name) {
	for (const Patch& p : presets())
		if (p.name == name) return p;
	return presets()[0];
}

// a kit's equaliser flat and its low cut off
static void flatEq(DrumKit& k) {
	k.p[DP_EQCUT] = 0.f;
	for (int i = DP_EQLOW; i <= DP_EQHIGH; i++)
		k.p[i] = 0.5f;
}

const std::vector<DrumKit>& drumKits() {
	static const std::vector<DrumKit> list = [] {
		auto kit = [](const char* name, std::initializer_list<float> v, float drive, float delay, float reverb) {
			DrumKit k;
			k.name = QString::fromLatin1(name);
			int i = 0;
			for (float f : v)
				k.p[i++] = f;
			k.p[DP_DRIVE] = drive;
			k.p[DP_DELAY] = delay;
			k.p[DP_REVERB] = reverb;
			flatEq(k);
			return k;
		};
		auto sampled = [](const char* name, std::initializer_list<const char*> files,
		                  std::initializer_list<float> levels, float drive, float delay, float reverb) {
			DrumKit k;
			k.name = QString::fromLatin1(name);
			int i = 0;
			for (const char* f : files) {
				k.sample[i] = QStringLiteral("nz:") + QString::fromLatin1(f);
				k.p[i * 3 + 1] = 0.5f;
				k.p[i * 3 + 2] = 1.f;
				i++;
			}
			i = 0;
			for (float l : levels)
				k.p[3 * i++] = l;
			k.p[DP_DRIVE] = drive;
			k.p[DP_DELAY] = delay;
			k.p[DP_REVERB] = reverb;
			flatEq(k);
			return k;
		};
		// clang-format off: the levels line up under the drum names
		//                         kick           snare          clap           closed hat     open hat       tom            rim            cymbal
		return std::vector<DrumKit>{
			kit(QT_TRANSLATE_NOOP("Model", "Machine kit"), {.85f, .5f, .35f, .7f, .5f, .4f, .65f, .5f, .4f, .55f, .5f, .3f, .5f, .5f, .4f, .65f, .5f, .4f, .6f, .5f, .3f, .45f, .5f, .4f}, .2f, .05f, .12f),
			kit(QT_TRANSLATE_NOOP("Model", "Soft boom kit"), {.9f, .4f, .7f, .6f, .45f, .5f, .55f, .45f, .55f, .45f, .45f, .2f, .45f, .45f, .5f, .7f, .4f, .6f, .5f, .5f, .3f, .4f, .45f, .5f}, .05f, .1f, .25f),
			kit(QT_TRANSLATE_NOOP("Model", "Tight acid kit"), {.9f, .55f, .22f, .7f, .6f, .25f, .7f, .55f, .3f, .6f, .6f, .15f, .5f, .6f, .25f, .6f, .6f, .25f, .65f, .55f, .2f, .4f, .6f, .25f}, .5f, .08f, .08f),
		    // clang-format on
		    // kits that play sounds from the Nachtzug library: each drum at its own pitch, playing out in full
		    sampled(QT_TRANSLATE_NOOP("Model", "Night train kit"),
			        {"Kicks/Deep boom", "Snares/Tape", "Train/Rail joint", "Hats/Shaker", "Train/Steam puff",
			         "Percussion/Tom low", "Train/Coupling clank", "Train/Door chime"},
			        {.85f, .66f, .62f, .5f, .5f, .6f, .5f, .45f}, .1f, .1f, .2f),
		    sampled(QT_TRANSLATE_NOOP("Model", "Tape kit"),
			        {"Kicks/Punch", "Snares/Tape", "Claps/Classic", "Hats/Closed", "Hats/Open", "Percussion/Tom mid",
			         "Percussion/Rimshot", "Cymbals/Crash"},
			        {.85f, .66f, .6f, .52f, .48f, .6f, .55f, .45f}, .25f, .06f, .1f),
		    sampled(QT_TRANSLATE_NOOP("Model", "Electro kit"),
			        {"Kicks/Hard", "Snares/Electro", "Claps/Big room", "Hats/Tight", "Hats/Sizzle", "Tones/Zap",
			         "Percussion/Clave", "Cymbals/Crash"},
			        {.85f, .62f, .6f, .5f, .45f, .55f, .5f, .45f}, .3f, .08f, .08f),
		    sampled(QT_TRANSLATE_NOOP("Model", "Hand percussion kit"),
			        {"Kicks/Soft felt", "Percussion/Conga high", "Claps/Snap", "Hats/Shaker", "Hats/Tambourine",
			         "Percussion/Conga low", "Percussion/Clave", "Percussion/Cowbell"},
			        {.85f, .62f, .55f, .52f, .45f, .62f, .52f, .45f}, .05f, .05f, .18f),
		};
	}();
	return list;
}

Patch rollPatch(int role, std::mt19937& g) {
	const int kind = role == R_PAD ? 1 : role == R_LEAD ? 2 : 0; // pluck, pad, lead
	Patch p = mk(kind == 0   ? QT_TRANSLATE_NOOP("Model", "Rolled pluck")
	             : kind == 1 ? QT_TRANSLATE_NOOP("Model", "Rolled pad")
	                         : QT_TRANSLATE_NOOP("Model", "Rolled lead"),
	             {});
	float* v = p.p;
	v[P_WAVE1] = float(ri(g, 3));
	v[P_WAVE2] = float(ri(g, 4));
	v[P_TUNE2] = float(ri(g, 4));
	v[P_DETUNE] = rr(g, 0, .6f);
	v[P_MIX] = ch(g, .3f) ? 0 : rr(g, .15f, .6f);
	v[P_SUB] = ch(g, .4f) ? rr(g, .2f, .7f) : 0;
	v[P_NOISE] = ch(g, .2f) ? rr(g, .05f, .25f) : 0;
	v[P_PW] = ch(g, .5f) ? rr(g, 0, .8f) : 0;
	v[P_PHRATE] = rr(g, .1f, .6f);
	v[P_SLOPE] = ch(g, .2f) ? 1.f : ch(g, .1f) ? 2.f : 0.f;
	v[P_LFOWAVE] = ch(g, .15f) ? 2.f : 0.f;
	v[P_UNISON] = ch(g, kind == 1 ? .6f : .3f) ? float(1 + ri(g, kind == 1 ? 4 : 2)) : 0.f;
	v[P_UDETUNE] = rr(g, .15f, .5f);
	v[P_WIDTH] = kind == 1 ? rr(g, .4f, .9f) : rr(g, 0, .4f);
	v[P_FM] = ch(g, .15f) ? rr(g, .2f, .6f) : 0;
	v[P_PENV] = kind == 0 && ch(g, .15f) ? rr(g, .1f, .4f) : 0;
	v[P_LFOPW] = v[P_PW] > 0 && ch(g, .4f) ? rr(g, .2f, .6f) : 0;
	if (kind == 0) {
		v[P_CUTOFF] = rr(g, .2f, .5f);
		v[P_RES] = rr(g, .2f, .88f);
		v[P_ENVAMT] = rr(g, .35f, .75f);
		v[P_ACCENT] = rr(g, .4f, .9f);
		v[P_FA] = 0;
		v[P_FD] = rr(g, .48f, .7f);
		v[P_FS] = rr(g, 0, .2f);
		v[P_AA] = 0;
		v[P_AD] = rr(g, .55f, .75f);
		v[P_AS] = rr(g, 0, .6f);
		v[P_AR] = rr(g, .25f, .6f);
		v[P_MONO] = ch(g, role == R_BASS ? .85f : .4f) ? 1.f : 0.f;
		v[P_GLIDE] = ch(g, .25f) ? rr(g, .1f, .3f) : 0;
		v[P_LFORATE] = rr(g, .1f, .8f);
		v[P_LFOCUT] = ch(g, .3f) ? rr(g, .05f, .3f) : 0;
		v[P_LFOPITCH] = 0;
		v[P_DRIVE] = rr(g, .05f, .6f);
		v[P_PHASER] = ch(g, .25f) ? rr(g, .2f, .6f) : 0;
		v[P_CHORUS] = ch(g, .3f) ? rr(g, .2f, .5f) : 0;
		v[P_DELAY] = role == R_BASS ? rr(g, 0, .3f) : rr(g, .25f, .6f);
		v[P_REVERB] = rr(g, 0, .3f);
	} else if (kind == 1) {
		v[P_WAVE1] = float(ri(g, 2));
		v[P_DETUNE] = rr(g, .3f, .8f);
		v[P_MIX] = rr(g, .35f, .6f);
		v[P_CUTOFF] = rr(g, .4f, .65f);
		v[P_RES] = rr(g, .05f, .4f);
		v[P_ENVAMT] = rr(g, .05f, .3f);
		v[P_FA] = rr(g, .6f, .85f);
		v[P_FD] = rr(g, .7f, .9f);
		v[P_FS] = rr(g, .4f, .9f);
		v[P_AA] = rr(g, .6f, .82f);
		v[P_AD] = .8f;
		v[P_AS] = rr(g, .7f, 1);
		v[P_AR] = rr(g, .7f, .9f);
		v[P_MONO] = 0;
		v[P_GLIDE] = 0;
		v[P_LFOWAVE] = 0;
		v[P_LFORATE] = rr(g, .05f, .4f);
		v[P_LFOCUT] = rr(g, 0, .4f);
		v[P_LFOPITCH] = 0;
		v[P_DRIVE] = rr(g, 0, .2f);
		v[P_PHASER] = rr(g, 0, .8f);
		v[P_CHORUS] = rr(g, .3f, .9f);
		v[P_DELAY] = rr(g, .1f, .4f);
		v[P_REVERB] = rr(g, .35f, .75f);
	} else {
		v[P_CUTOFF] = rr(g, .5f, .7f);
		v[P_RES] = rr(g, .1f, .45f);
		v[P_ENVAMT] = rr(g, .1f, .4f);
		v[P_FA] = 0;
		v[P_FD] = rr(g, .6f, .8f);
		v[P_FS] = rr(g, .3f, .7f);
		v[P_AA] = rr(g, 0, .4f);
		v[P_AD] = .7f;
		v[P_AS] = rr(g, .7f, 1);
		v[P_AR] = rr(g, .5f, .7f);
		v[P_MONO] = 1;
		v[P_GLIDE] = rr(g, .15f, .45f);
		v[P_LFOWAVE] = 0;
		v[P_LFORATE] = rr(g, .74f, .82f);
		v[P_LFOPITCH] = rr(g, .15f, .45f);
		v[P_LFOCUT] = 0;
		v[P_DRIVE] = rr(g, .1f, .5f);
		v[P_PHASER] = 0;
		v[P_CHORUS] = rr(g, 0, .4f);
		v[P_DELAY] = rr(g, .35f, .65f);
		v[P_REVERB] = rr(g, .2f, .5f);
	}
	return p;
}

Patch mutatePatch(const Patch& src, std::mt19937& g) {
	Patch p = src;
	for (int i = 0; i < P_COUNT; i++) {
		// not the filter release or the equaliser: set by hand, or left alone
		if (i >= P_FR && i <= P_EQHIGH) continue;
		// the LFO's volume, pan and fade-in are nudged only once they are in use: off, they stay off
		if (i >= P_LFOAMP && i <= P_LFOFADE && p.p[i] <= 0.f) continue;
		if (PARAMS[i].kind != K_CHOICE) p.p[i] = std::clamp(p.p[i] + rr(g, -.09f, .09f), 0.f, 1.f);
	}
	if (!p.name.endsWith(" (mutated)")) p.name += " (mutated)";
	return p;
}

Patch keepEq(Patch next, const Patch& current) {
	for (int i = P_EQCUT; i <= P_EQHIGH; i++)
		next.p[i] = current.p[i];
	return next;
}

DrumKit keepEq(DrumKit next, const DrumKit& current) {
	for (int i = DP_EQCUT; i <= DP_EQHIGH; i++)
		next.p[i] = current.p[i];
	return next;
}

DrumKit chooseKit(const DrumKit& current, const DrumKit& preset) {
	DrumKit k = preset;
	for (int d = 0; d < D_COUNT; d++)
		if (k.sample[d].isEmpty() && !current.sample[d].isEmpty() && !samples::isLibrary(current.sample[d])) {
			k.sample[d] = current.sample[d];
			k.data[d] = current.data[d];
			for (int j = 0; j < 3; j++)
				k.p[d * 3 + j] = current.p[d * 3 + j]; // with the level, tuning and length it had
		}
	return k;
}
