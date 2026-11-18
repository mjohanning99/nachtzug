// Starter songs: see starters.h.
//
// What's here: starters() (the list the start window shows), loadStarter() (from the resources), makeStarter() and
// writeStarters() (how the files in assets/starters/ were made: nachtzug --write-starters assets/starters).
// Changes in the song file: none (the starters are ordinary songs).

#include "starters.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>

const std::vector<Starter>& starters() {
	static const std::vector<Starter> list = {
	    {"acid", QT_TRANSLATE_NOOP("Model", "Acid"),
		 QT_TRANSLATE_NOOP("Model", "A squelchy bass line over busy acid house drums")},
	    {"berlin", QT_TRANSLATE_NOOP("Model", "Berlin School"),
		 QT_TRANSLATE_NOOP("Model", "Rolling sequences, string chords and a slow tune")},
	    {"motorik", QT_TRANSLATE_NOOP("Model", "Motorik"),
		 QT_TRANSLATE_NOOP("Model", "The steady krautrock beat, a driving bass and an arpeggio")},
	    {"electro", QT_TRANSLATE_NOOP("Model", "Electro"),
		 QT_TRANSLATE_NOOP("Model", "Broken electro drums, a bouncing bass and bell chords")},
	    {"halftime", QT_TRANSLATE_NOOP("Model", "Half time"),
		 QT_TRANSLATE_NOOP("Model", "Slow and deep: a half-time beat, a drone and a plucked arpeggio")},
	};
	return list;
}

bool loadStarter(int i, Song& out) {
	if (i < 0 || i >= int(starters().size())) return false;
	QFile f(QStringLiteral(":/starters/%1.nachtzug").arg(QLatin1String(starters()[size_t(i)].id)));
	if (!f.open(QIODevice::ReadOnly)) return false;
	Song s;
	if (!Song::fromJson(QJsonDocument::fromJson(f.readAll()).object(), s)) return false;
	resolveSamples(s);
	out = s;
	return true;
}

namespace {

struct Part {
	int role;
	const char* idea;  // the generator, by its name in generatorsFor()
	const char* sound; // a preset
	int octave;
	float density, vol;
};
struct Recipe {
	float bpm, swing;
	int root, scale;
	std::vector<int> chords; // in the 7-note shape, made to fit the scale with Song::q()
	int drumStyle;
	float energy, play;
	const char* kit;
	std::vector<Part> parts;
	unsigned seed;
};

const Recipe& recipe(int i) {
	// clang-format off
	static const Recipe r[] = {
	    // Acid: A Phrygian, one chord, a 303-style line over acid house drums, stabs on top
	    {126, 0.f, 9, 2, {0, 0, 0, 0}, 1, 0.72f, 0.5f, "Tight acid kit",
	     {{R_BASS, "Acid line", "Acid Squelch", 0, 0.7f, 0.64f},
	      {R_PAD, "Chord stabs", "Hollow Band", 1, 0.5f, 0.42f}}, 21},
	    // Berlin School: D minor, a rolling sequence and a bass run under string chords, a sparse lead
	    {112, 0.f, 2, 0, {0, 5, 3, 4}, 6, 0.45f, 0.3f, "Soft boom kit",
	     {{R_BASS, "Berlin sequence", "Deep Bass", 0, 0.6f, 0.68f},
	      {R_SEQ, "Berlin sequence", "Berlin Sequence", 1, 0.7f, 0.62f},
	      {R_PAD, "Held chord", "String Machine", 2, 0.6f, 0.5f},
	      {R_LEAD, "Sparse melody", "Mellow Lead", 2, 0.5f, 0.55f}}, 33},
	    // Motorik: E Dorian, the krautrock beat, eighths on the key note, an arpeggio and a choir
	    {132, 0.f, 4, 1, {0, 0, 3, 3}, 2, 0.6f, 0.35f, "Machine kit",
	     {{R_BASS, "Root pulse", "Rubber Square", 0, 0.6f, 0.7f},
	      {R_SEQ, "Arpeggio run", "Pulse Wanderer", 1, 0.6f, 0.58f},
	      {R_PAD, "Held chord", "Phase Choir", 2, 0.6f, 0.46f}}, 47},
	    // Electro: C minor, electro drums with a little swing, an octave bass, bell arpeggios and saw stabs
	    {118, 0.12f, 0, 0, {0, 0, 6, 5}, 5, 0.65f, 0.5f, "Electro kit",
	     {{R_BASS, "Octave bounce", "Zap Bass", 0, 0.6f, 0.7f},
	      {R_SEQ, "Arpeggio run", "FM Bell", 1, 0.5f, 0.5f},
	      {R_PAD, "Chord stabs", "Supersaw Pad", 2, 0.5f, 0.4f}}, 58},
	    // Half time: F minor, slow, a drone and a plucked arpeggio, a melody later
	    {96, 0.08f, 5, 0, {0, 5, 6, 4}, 4, 0.5f, 0.3f, "Night train kit",
	     {{R_BASS, "Root pulse", "Deep Bass", 0, 0.4f, 0.7f},
	      {R_SEQ, "Arpeggio run", "Glass Pluck", 1, 0.5f, 0.55f},
	      {R_PAD, "Held chord", "Night Drone", 2, 0.6f, 0.5f},
	      {R_LEAD, "Melody", "Wide Unison Lead", 2, 0.4f, 0.5f}}, 64},
	};
	// clang-format on
	return r[std::clamp(i, 0, int(std::size(r)) - 1)];
}

int ideaId(int role, const char* name) {
	const std::vector<GenDef> gens = generatorsFor(role);
	for (const GenDef& d : gens)
		if (qstrcmp(d.name, name) == 0) return d.id;
	return gens.front().id;
}

} // namespace

Song makeStarter(int i) {
	const Recipe& r = recipe(i);
	std::mt19937 g(r.seed);
	Song s;
	s.bpm = r.bpm;
	s.swing = r.swing;
	s.root = r.root;
	s.scale = r.scale;
	std::vector<int> chords;
	for (int d : r.chords)
		chords.push_back(s.q(d));
	s.setChords(chords);
	s.loopStart = 0;
	s.loopEnd = 8 * BAR;

	// the drums: the auto drummer in the style's own beat, with the style's kit
	{
		Track t = makeTrack(s, R_DRUMS);
		for (const DrumKit& k : drumKits())
			if (k.name == QLatin1String(r.kit)) t.kit = chooseKit(t.kit, k);
		Clip c;
		c.id = s.newId();
		c.len = 8 * BAR;
		c.loopLen = BAR;
		c.ad.on = true;
		c.ad.style = r.drumStyle;
		c.ad.intensity = r.energy;
		c.ad.complexity = r.play;
		c.ad.seed = unsigned(g());
		autoDrumFill(c);
		t.clips.push_back(c);
		s.tracks.push_back(t);
	}
	for (const Part& p : r.parts) {
		Track t = makeTrack(s, p.role);
		t.patch = presetByName(QLatin1String(p.sound));
		t.octave = p.octave;
		t.vol = p.vol;
		Clip c;
		c.id = s.newId();
		c.len = 8 * BAR;
		c.loopLen = p.role == R_LEAD ? 4 * BAR : BAR;
		generate(s, t, c, ideaId(p.role, p.idea), p.density, g);
		t.clips.push_back(c);
		s.tracks.push_back(t);
	}
	arrangeSong(s, 0, true, g); // a short song: intro, build-up, main part, break, main part again, outro
	return s;
}

int writeStarters(const QString& folder) {
	QDir().mkpath(folder);
	int written = 0;
	for (int i = 0; i < int(starters().size()); i++) {
		QFile f(QDir(folder).filePath(QStringLiteral("%1.nachtzug").arg(QLatin1String(starters()[size_t(i)].id))));
		if (!f.open(QIODevice::WriteOnly)) continue;
		f.write(QJsonDocument(makeStarter(i).toJson()).toJson(QJsonDocument::Compact));
		written++;
	}
	return written;
}
