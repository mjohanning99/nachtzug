// The ideas: the pattern generators for each kind of track, Vary, chord progressions, new tracks with a sound and a
// pattern, the demo song, and Arrange for me.
//
// What's here: generatorsFor(), generate(), varyClip(), randomProgression(), makeTrack(), addTrackWithClip(),
// demoSong(), arrangePlan(), arrangeSong(); used by the New idea menu, Vary, Add track, Arrange and the tests.
// Every random choice comes from the generator it is given, so the same seed gives the same pattern (on one
// system: AGENTS.md).
// Changes in the song file: none of its own; Arrange for me writes the song's named parts ("parts").
#include "model.h"
#include "patterns.h"
#include <QCoreApplication>
#include <algorithm>

using namespace dsp;

using namespace patterns;

// ---------------------------------------------------------------- generators

enum {
	G_ACID,
	G_BERLIN,
	G_OCTAVE,
	G_PULSE,
	G_ARPRUN,
	G_CHORD,
	G_STABS,
	G_MELODY,
	G_SPARSE,
	G_D_FOUR,
	G_D_ACID,
	G_D_MOTORIK,
	G_D_BREAK
};

std::vector<GenDef> generatorsFor(int role) {
	switch (role) {
	case R_BASS:
		return {{QT_TRANSLATE_NOOP("Model", "Acid line"), G_ACID,
		         QT_TRANSLATE_NOOP("Model", "busy sixteenths with accents and slides")},
		        {QT_TRANSLATE_NOOP("Model", "Octave bounce"), G_OCTAVE,
		         QT_TRANSLATE_NOOP("Model", "the key note jumping between two octaves")},
		        {QT_TRANSLATE_NOOP("Model", "Root pulse"), G_PULSE,
		         QT_TRANSLATE_NOOP("Model", "steady eighths on the key note, for a driving groove")},
		        {QT_TRANSLATE_NOOP("Model", "Berlin sequence"), G_BERLIN,
		         QT_TRANSLATE_NOOP("Model", "a repeating run of chord notes")}};
	case R_SEQ:
		return {{QT_TRANSLATE_NOOP("Model", "Berlin sequence"), G_BERLIN,
		         QT_TRANSLATE_NOOP("Model", "the classic rolling run of chord notes")},
		        {QT_TRANSLATE_NOOP("Model", "Arpeggio run"), G_ARPRUN,
		         QT_TRANSLATE_NOOP("Model", "chord notes up and down, like an arpeggiator")},
		        {QT_TRANSLATE_NOOP("Model", "Acid line"), G_ACID,
		         QT_TRANSLATE_NOOP("Model", "busy sixteenths with accents and slides")}};
	case R_PAD:
		return {
		    {QT_TRANSLATE_NOOP("Model", "Held chord"), G_CHORD, QT_TRANSLATE_NOOP("Model", "one long chord per bar")},
		    {QT_TRANSLATE_NOOP("Model", "Chord stabs"), G_STABS,
			 QT_TRANSLATE_NOOP("Model", "short chords on a rhythm")}};
	case R_LEAD:
		return {{QT_TRANSLATE_NOOP("Model", "Melody"), G_MELODY,
		         QT_TRANSLATE_NOOP("Model", "a tune that moves between chord notes")},
		        {QT_TRANSLATE_NOOP("Model", "Sparse melody"), G_SPARSE,
		         QT_TRANSLATE_NOOP("Model", "a few long notes, with room to breathe")},
		        {QT_TRANSLATE_NOOP("Model", "Arpeggio run"), G_ARPRUN,
		         QT_TRANSLATE_NOOP("Model", "chord notes up and down")}};
	default:
		return {
		    {QT_TRANSLATE_NOOP("Model", "Four on the floor"), G_D_FOUR,
			 QT_TRANSLATE_NOOP("Model", "kick on every beat, claps and open hats")},
		    {QT_TRANSLATE_NOOP("Model", "Acid house"), G_D_ACID, QT_TRANSLATE_NOOP("Model", "busy hats and rimshots")},
		    {QT_TRANSLATE_NOOP("Model", "Motorik"), G_D_MOTORIK,
			 QT_TRANSLATE_NOOP("Model", "the steady krautrock beat")},
		    {QT_TRANSLATE_NOOP("Model", "Broken beat"), G_D_BREAK,
			 QT_TRANSLATE_NOOP("Model", "syncopated, off-kilter kicks")}};
	}
}

static void drumRow(Clip& c, int bar, int drum, const char* pat) {
	for (int i = 0; i < 16 && pat[i]; i++)
		if (pat[i] != '.') add(c, bar * BAR + i * 24, 12, drum, pat[i] == 'X');
}

static void generateNotes(const Song& s, const Track& t, Clip& c, int genId, float density, std::mt19937& g);

// A new idea keeps the notes you marked Keep: the new pattern makes room for them.
void generate(const Song& s, const Track& t, Clip& c, int genId, float density, std::mt19937& g) {
	std::vector<Note> kept;
	for (const Note& n : c.notes)
		if (n.lock) kept.push_back(n);
	generateNotes(s, t, c, genId, density, g);
	if (kept.empty()) return;
	std::erase_if(c.notes, [&](const Note& n) {
		for (const Note& k : kept) {
			if (k.start >= c.loopLen) continue;
			if (t.isDrums() ? (n.deg == k.deg && n.start == k.start)
			                : (n.start < k.start + k.len && k.start < n.start + n.len))
				return true;
		}
		return false;
	});
	for (const Note& k : kept)
		if (k.start < c.loopLen) c.notes.push_back(k);
	std::sort(c.notes.begin(), c.notes.end(), [](const Note& a, const Note& b) { return a.start < b.start; });
}

static void generateNotes(const Song& s, const Track& t, Clip& c, int genId, float density, std::mt19937& g) {
	c.notes.clear();
	c.ad.on = false;
	for (const GenDef& d : generatorsFor(t.role))
		if (d.id == genId) c.name = QString::fromLatin1(d.name);
	const int n = s.sc().n, bars = std::max(1, (c.loopLen + BAR - 1) / BAR);
	auto q = [&](int k) { return s.q(k); };
	auto pick = [&](const std::vector<int>& v) { return v[size_t(ri(g, int(v.size())))]; };
	// tracks that do not follow the chord lane get the chord root added here instead
	auto chordAt = [&](int bar) { return t.follow ? 0 : s.chordAt((c.start + bar * BAR) / BAR); };

	switch (genId) {
	case G_ACID: {
		const std::vector<int> pool = {0, 0, 0, 0, 0, n, n, q(2), q(4), q(4), q(6), n + q(2)};
		struct St {
			bool on;
			int deg;
			bool acc, sld;
		} st[16];
		for (int i = 0; i < 16; i++)
			st[i] = {i == 0 || ch(g, .5f + .45f * density), pick(pool), ch(g, .3f), ch(g, .22f)};
		for (int b = 0; b < bars; b++) {
			if (b > 0 && b == bars - 1)
				for (int k = 0; k < 3; k++) {
					St& x = st[ri(g, 16)];
					x.deg = pick(pool);
					x.on = true;
					x.sld = ch(g, .4f);
				}
			for (int i = 0; i < 16; i++)
				if (st[i].on)
					add(c, b * BAR + i * 24, st[i].sld ? 24 : 18, st[i].deg + chordAt(b), st[i].acc, st[i].sld);
		}
		break;
	}
	case G_BERLIN: {
		const std::vector<int> pool = {0, q(2), q(4), q(4), n, n, n + q(2), n + q(4)};
		int deg[8], prev = 0;
		const int rest = ch(g, .7f - .6f * density) ? 1 + ri(g, 7) : -1, rat = ch(g, density) ? 1 + ri(g, 7) : -1;
		for (int i = 0; i < 8; i++) {
			int d = 0;
			if (i) do
					d = pick(pool);
				while (d == prev);
			deg[i] = prev = d;
		}
		for (int i = 0; i * 24 < c.loopLen; i++) {
			const int k = i % 8, b = i / 16;
			if (k == rest) continue;
			if (k == rat) {
				add(c, i * 24, 9, deg[k] + chordAt(b));
				add(c, i * 24 + 12, 9, deg[k] + chordAt(b));
			} else
				add(c, i * 24, 15, deg[k] + chordAt(b), k == 0);
		}
		break;
	}
	case G_OCTAVE:
		for (int i = 0; i * 48 < c.loopLen; i++)
			add(c, i * 48, 36, (i % 2 ? n : 0) + chordAt(i / 8), i % 2 == 0);
		break;
	case G_PULSE: {
		const int last = c.loopLen / 48 - 1;
		for (int i = 0; i * 48 < c.loopLen; i++)
			add(c, i * 48, 34, (i == last && ch(g, .7f) ? pick({q(4), q(6), n}) : 0) + chordAt(i / 8), i % 2 == 0);
		break;
	}
	case G_ARPRUN: {
		const std::vector<std::vector<int>> shapes = {{0, q(2), q(4), n, n + q(2), n + q(4), n + q(2), n},
		                                              {0, q(4), n, q(4), n + q(2), n, q(4), n},
		                                              {n + q(4), n + q(2), n, q(4), q(2), 0, q(2), q(4)}};
		const auto& sh = shapes[size_t(ri(g, int(shapes.size())))];
		for (int i = 0; i * 24 < c.loopLen; i++)
			add(c, i * 24, 18, sh[size_t(i % 8)] + chordAt(i / 16), i % 8 == 0);
		break;
	}
	case G_CHORD:
		for (int b = 0; b < bars; b++)
			add(c, b * BAR, BAR - 6, chordAt(b));
		break;
	case G_STABS: {
		const std::vector<std::vector<int>> rh = {{0, 6, 12}, {4, 12}, {2, 6, 10, 14}, {0, 3, 6, 10, 12}};
		const auto& r = rh[size_t(density < .3f ? 1 : density > .75f ? 3 : ri(g, int(rh.size())))];
		for (int b = 0; b < bars; b++)
			for (int st : r)
				add(c, b * BAR + st * 24, 20, chordAt(b), st == 0);
		break;
	}
	case G_MELODY:
	case G_SPARSE: {
		// rhythm templates: pairs of (start, length) in 16ths
		const std::vector<std::vector<int>> dense = {{0, 3, 3, 1, 4, 2, 6, 2, 8, 4, 12, 2, 14, 2},
		                                             {0, 2, 2, 2, 4, 4, 8, 2, 10, 2, 12, 4},
		                                             {0, 6, 6, 2, 8, 3, 11, 1, 12, 4},
		                                             {2, 2, 4, 2, 6, 2, 8, 6, 14, 2}};
		const std::vector<std::vector<int>> sparse = {{0, 8, 8, 8}, {0, 12, 12, 4}, {0, 6, 8, 8}, {4, 12}, {0, 16}};
		const float d = genId == G_MELODY ? density : density * 0.4f;
		int prev = n;
		for (int b = 0; b < bars; b++) {
			const auto& tpl = ch(g, d * 1.3f) ? dense : sparse; // busier settings pick the dense rhythms more often
			const auto& r = tpl[size_t(ri(g, int(tpl.size())))];
			const int root = chordAt(b);
			for (size_t i = 0; i + 1 < r.size(); i += 2) {
				int d;
				if (r[i] % 4 == 0 || ch(g, .35f)) {
					d = root + pick({0, q(2), q(4)}) + n;
					while (d - prev > n / 2 + 1)
						d -= n;
					while (prev - d > n / 2 + 1)
						d += n;
				} else
					d = prev + pick({-2, -1, -1, 1, 1, 2});
				d = std::clamp(d, q(2), 2 * n + q(4));
				add(c, b * BAR + r[i] * 24, r[i + 1] * 24 - 4, d);
				prev = d;
			}
		}
		break;
	}
	case G_D_FOUR:
		for (int b = 0; b < bars; b++) {
			drumRow(c, b, D_KICK, "X...X...X...X...");
			drumRow(c, b, D_CLAP, "....X.......X...");
			drumRow(c, b, D_CHAT, "x.x.x.x.x.x.x.x.");
			drumRow(c, b, D_OHAT, "..X...X...X...X.");
			if (b == bars - 1 && bars > 1) drumRow(c, b, D_SNARE, "............x.xX");
		}
		break;
	case G_D_ACID:
		for (int b = 0; b < bars; b++) {
			drumRow(c, b, D_KICK, "X...X...X...X.x.");
			drumRow(c, b, D_SNARE, "....X.......X...");
			drumRow(c, b, D_CHAT, "xxXxxxXxxxXxxxXx");
			drumRow(c, b, D_OHAT, "..x...x...x...x.");
			drumRow(c, b, D_RIM, ch(g, .5f) ? "...x..x....x..x." : ".x....x..x......");
			if (b == bars - 1 && bars > 1) drumRow(c, b, D_TOM, "..........x.x.xx");
		}
		break;
	case G_D_MOTORIK:
		for (int b = 0; b < bars; b++) {
			drumRow(c, b, D_KICK, "X...x...X...x...");
			drumRow(c, b, D_SNARE, "....X.......X...");
			drumRow(c, b, D_CHAT, "X.x.X.x.X.x.X.x.");
			if (b % 4 == 0) drumRow(c, b, D_CYM, "X...............");
		}
		break;
	default:
		for (int b = 0; b < bars; b++) {
			drumRow(c, b, D_KICK, "X.....x...X.....");
			drumRow(c, b, D_SNARE, "....X..x.x..X...");
			drumRow(c, b, D_CHAT, "x.x.x.x.x.x.x.xx");
			drumRow(c, b, D_OHAT, "......x.......x.");
		}
		break;
	}
}

void varyClip(const Song& s, const Track& t, Clip& c, std::mt19937& g) {
	if (c.ad.on) {
		c.ad.seed = unsigned(g());
		return;
	} // a new take from the drummer
	if (t.isDrums()) {
		// add or remove one ghost hit; kept hits stay, and nothing is added on top of one
		for (int tries = 0; tries < 8; tries++) {
			const int step = ri(g, std::max(1, c.loopLen / 24)) * 24,
			          drum = std::vector<int>{D_CHAT, D_CHAT, D_SNARE, D_RIM, D_KICK, D_OHAT}[size_t(ri(g, 6))];
			auto it = std::find_if(c.notes.begin(), c.notes.end(),
			                       [&](const Note& n) { return n.start == step && n.deg == drum; });
			if (it != c.notes.end()) {
				if (it->lock) continue;
				c.notes.erase(it);
				return;
			}
			add(c, step, 12, drum);
			return;
		}
		return;
	}
	(void)s;
	std::vector<size_t> free;
	for (size_t i = 0; i < c.notes.size(); i++)
		if (!c.notes[i].lock) free.push_back(i);
	if (free.empty()) return;
	const int count = 1 + ri(g, 2);
	for (int k = 0; k < count; k++) {
		Note& n = c.notes[free[size_t(ri(g, int(free.size())))]];
		const float r = rr(g, 0, 1);
		if (r < .65f)
			n.deg = std::clamp(n.deg + std::vector<int>{-2, -1, 1, 2}[size_t(ri(g, 4))], DEG_MIN, DEG_MAX);
		else if (r < .85f)
			n.acc = !n.acc;
		else
			n.slide = !n.slide;
	}
}

std::vector<int> randomProgression(const Song& s, std::mt19937& g) {
	const bool major = s.degToSemis(s.q(2)) == 4;
	static const std::vector<std::vector<int>> minorP = {{0, 0, 5, 6},
	                                                     {0, 5, 2, 6},
	                                                     {0, 3, 5, 4},
	                                                     {0, 6, 5, 6},
	                                                     {0, 0, 3, 3},
	                                                     {0, 5, 3, 4},
	                                                     {0, 0, 0, 0, 5, 5, 6, 6},
	                                                     {0, 2, 5, 4},
	                                                     {0, 0, 6, 6, 5, 5, 6, 6}};
	static const std::vector<std::vector<int>> majorP = {
	    {0, 4, 5, 3}, {0, 5, 3, 4}, {0, 3, 0, 4}, {0, 0, 3, 4}, {0, 0, 0, 0, 3, 3, 4, 4}};
	const auto& list = major ? majorP : minorP;
	std::vector<int> out = list[size_t(ri(g, int(list.size())))];
	for (int& d : out)
		d = s.q(d);
	return out;
}

// ---------------------------------------------------------------- tracks

Track makeTrack(Song& s, int role) {
	Track t;
	t.id = s.newId();
	t.role = role;
	t.name = QString::fromLatin1(ROLE_NAMES[role]);
	static const char* sound[R_COUNT] = {"Acid Squelch", "Berlin Sequence", "String Machine", "Mellow Lead",
	                                     "Acid Squelch"};
	static const int oct[R_COUNT] = {0, 1, 2, 2, 0};
	t.patch = presetByName(QString::fromLatin1(sound[role]));
	t.kit = drumKits()[0];
	t.octave = oct[role];
	t.chord = role == R_PAD ? 3 : 0;
	t.follow = role != R_LEAD && role != R_DRUMS;
	t.vol = role == R_PAD ? 0.6f : role == R_DRUMS ? 0.8f : 0.72f;
	int same = 0;
	for (auto& o : s.tracks)
		if (o.role == role) same++;
	if (same) t.name += " " + QString::number(same + 1);
	return t;
}

int addTrackWithClip(Song& s, int role, std::mt19937& rng) {
	if (int(s.tracks.size()) >= MAX_TRACKS) return -1;
	Track t = makeTrack(s, role);
	Clip c;
	c.id = s.newId();
	c.start = s.loopStart;
	c.len = s.loopEnd > s.loopStart ? s.loopEnd - s.loopStart : 8 * BAR;
	c.loopLen = std::min(c.len, role == R_LEAD ? 4 * BAR : BAR);
	if (role == R_DRUMS) {
		c.ad.on = true;
		c.ad.seed = unsigned(rng());
		autoDrumFill(c);
	} // drums start with the auto drummer
	else
		generate(s, t, c, generatorsFor(role)[0].id, 0.6f, rng);
	t.clips.push_back(c);
	s.tracks.push_back(t);
	return t.id;
}

Song demoSong() {
	Song s;
	std::mt19937 g(11);
	s.bpm = 122;
	s.chords = {0, 0, 5, 6};
	addTrackWithClip(s, R_DRUMS, g);
	addTrackWithClip(s, R_BASS, g);
	addTrackWithClip(s, R_SEQ, g);
	addTrackWithClip(s, R_PAD, g);
	s.tracks.push_back(makeTrack(s, R_LEAD));
	return s;
}

// ---------------------------------------------------------------- arranging

enum { A_INTRO, A_BUILD, A_MAIN, A_BREAK, A_MAIN2, A_OUTRO };

std::vector<ArrangePart> arrangePlan(int length) {
	const char* intro = QT_TRANSLATE_NOOP("Model", "Intro");
	const char* build = QT_TRANSLATE_NOOP("Model", "Build-up");
	const char* main1 = QT_TRANSLATE_NOOP("Model", "Main part");
	const char* brk = QT_TRANSLATE_NOOP("Model", "Break");
	const char* main2 = QT_TRANSLATE_NOOP("Model", "Main part 2");
	const char* outro = QT_TRANSLATE_NOOP("Model", "Outro");
	switch (length) {
	case 0:
		return {{intro, A_INTRO, 8}, {build, A_BUILD, 8},  {main1, A_MAIN, 16},
		        {brk, A_BREAK, 8},   {main2, A_MAIN2, 16}, {outro, A_OUTRO, 8}};
	case 2:
		return {{intro, A_INTRO, 16}, {build, A_BUILD, 16}, {main1, A_MAIN, 32}, {brk, A_BREAK, 16},
		        {build, A_BUILD, 8},  {main2, A_MAIN2, 32}, {outro, A_OUTRO, 16}};
	default:
		return {{intro, A_INTRO, 16}, {build, A_BUILD, 8},  {main1, A_MAIN, 24}, {brk, A_BREAK, 8},
		        {build, A_BUILD, 8},  {main2, A_MAIN2, 24}, {outro, A_OUTRO, 16}};
	}
}

std::vector<ArrangePart> arrangeSong(Song& s, int length, bool writeLead, std::mt19937& g) {
	const std::vector<ArrangePart> plan = arrangePlan(length);
	s.liveMode = false;
	for (Track& t : s.tracks) {
		// the pattern to build from: the clip at the start of the loop, or else the earliest one
		const Clip* src = nullptr;
		for (const Clip& c : t.clips)
			if (c.start <= s.loopStart && s.loopStart < c.end()) {
				src = &c;
				break;
			}
		if (!src)
			for (const Clip& c : t.clips)
				if (!src || c.start < src->start) src = &c;
		if (!src && !t.cells.empty())
			src = t.cell(t.liveCell) ? t.cell(t.liveCell) : &t.cells[0]; // made in the loop grid only
		Clip source = src ? *src : Clip{};
		if (!src) {
			source.len = source.loopLen = t.role == R_LEAD ? 4 * BAR : BAR;
		}
		bool has = src && (!src->notes.empty() || src->ad.on);
		if (!has && t.role == R_LEAD && writeLead) { // a melody for the lead, so the second main part has something new
			source.loopLen = source.len = 4 * BAR;
			source.notes.clear();
			source.ad.on = false;
			generate(s, t, source, generatorsFor(R_LEAD)[0].id, 0.5f, g);
			has = !source.notes.empty();
		}
		if (!has && t.isDrums()) {
			source.ad = AutoDrum{};
			source.ad.on = true;
			source.ad.seed = unsigned(g());
			has = true;
		}
		t.clips.clear();
		if (!has) continue;
		const AutoDrum drummer = source.ad;
		int at = 0;
		for (const ArrangePart& part : plan) {
			const int len = part.bars * BAR;
			bool on = true;
			int in = 0, out = 0, inLen = BAR, outLen = BAR;
			switch (t.role) {
			case R_DRUMS:
				on = part.type != A_BREAK && (part.type != A_INTRO || drummer.on);
				if (part.type == A_OUTRO) {
					out = 1;
					outLen = std::min(len, 4 * BAR);
				}
				break;
			case R_BASS:
				on = part.type != A_INTRO && part.type != A_BREAK;
				if (part.type == A_BUILD) {
					in = 2;
					inLen = len;
				} // the filter opens over the build-up
				if (part.type == A_OUTRO) {
					out = 3;
					outLen = len;
				}
				break;
			case R_SEQ:
				if (part.type == A_INTRO) {
					in = 3;
					inLen = len;
				} // fades in with the filter opening
				if (part.type == A_OUTRO) {
					out = 3;
					outLen = len;
				}
				break;
			case R_PAD:
				if (part.type == A_INTRO) {
					in = 1;
					inLen = std::min(len, 8 * BAR);
				}
				if (part.type == A_OUTRO) {
					out = 1;
					outLen = len;
				}
				break;
			case R_LEAD:
				on = part.type == A_MAIN2 || part.type == A_BREAK;
				if (part.type == A_BREAK) {
					in = 1;
					inLen = std::min(len, 4 * BAR);
				}
				break;
			default:
				break;
			}
			if (on) {
				Clip c = source;
				c.id = s.newId();
				c.scene = -1;
				c.start = at;
				c.len = len;
				c.offset = 0;
				c.adKey = 0;
				c.inKind = in;
				c.inLen = std::min(inLen, len);
				c.outKind = out;
				c.outLen = std::min(outLen, len);
				if (c.ad.on) { // the drummer plays each part its own way
					AutoDrum& a = c.ad;
					a = drummer;
					a.seed = drummer.seed + unsigned(at / BAR);
					switch (part.type) {
					case A_INTRO:
						a.intensity = std::max(0.15f, drummer.intensity * 0.45f);
						a.complexity *= 0.5f;
						a.crash = false;
						a.shape = 0;
						break;
					case A_BUILD:
						a.shape = 1;
						a.fillEvery = 4;
						break;
					case A_MAIN2:
						a.intensity = std::min(1.f, drummer.intensity + 0.1f);
						a.complexity = std::min(1.f, drummer.complexity + 0.15f);
						break;
					case A_OUTRO:
						a.shape = 2;
						break;
					default:
						break;
					}
				} else if (part.type == A_MAIN2 && (t.role == R_SEQ || t.role == R_BASS)) {
					varyClip(s, t, c, g); // the second time round, the patterns change a little
					varyClip(s, t, c, g);
				}
				t.clips.push_back(c);
			}
			at += len;
		}
	}
	// the loop goes around the first main part, switched off so the whole song plays
	int at = 0;
	for (const ArrangePart& part : plan) {
		if (part.type == A_MAIN) {
			s.loopStart = at;
			s.loopEnd = at + part.bars * BAR;
			break;
		}
		at += part.bars * BAR;
	}
	s.loopOn = false;
	// the parts it made, named on the timeline (in English, shown translated)
	std::vector<SongPart> named;
	int bar = 0;
	for (const ArrangePart& part : plan) {
		named.push_back({bar, QString::fromLatin1(part.name)});
		bar += part.bars;
	}
	s.setParts(std::move(named));
	refreshAutoDrums(s);
	return plan;
}
