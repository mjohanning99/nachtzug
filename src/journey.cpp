// A long journey: “Continue for … minutes” lets the song go on for many minutes, the way a Berlin School piece
// does. The whole piece follows one arc: it holds back at first (fewer parts, darker filters), builds to a single
// peak about two thirds of the way in, after the one big breakdown, and then settles. Along the way the sequences
// change a note at a time and the steps add up, the filters travel over minutes with local sweeps on top, parts
// come in and leave at phrase lines, the drummer thins out and comes back, a part goes off on a new idea for a while
// and comes home, and the key and the chords stay as they are.
//
// What's here: journeyBars(), journeyPlan(), continueJourney(); used by the Continue dialog (mainwindow.cpp),
// --journey on the command line, the parity record and the tests. Every random choice comes from the generator it
// is given, so the same seed gives the same journey (on one system: AGENTS.md).
// Changes in the song file: none (the journey is written as ordinary clips, knob lines and named parts).
#include "model.h"
#include "patterns.h"
#include <QCoreApplication>
#include <algorithm>
#include <cmath>

using namespace dsp;
using namespace patterns;

namespace {

constexpr int SEG = 8; // bars: parts come and go, and patterns change, only on these phrase lines

const char* phaseName(int type) {
	switch (type) {
	case J_DRIFT:
		return QT_TRANSLATE_NOOP("Model", "Drifting in");
	case J_RISE:
		return QT_TRANSLATE_NOOP("Model", "Rising");
	case J_CRUISE:
		return QT_TRANSLATE_NOOP("Model", "Cruising");
	case J_SHIFT:
		return QT_TRANSLATE_NOOP("Model", "New ground");
	case J_BREAK:
		return QT_TRANSLATE_NOOP("Model", "Breakdown");
	case J_RETURN:
		return QT_TRANSLATE_NOOP("Model", "Return");
	case J_PEAK:
		return QT_TRANSLATE_NOOP("Model", "Peak");
	case J_LULL:
		return QT_TRANSLATE_NOOP("Model", "Breather");
	case J_GLOW:
		return QT_TRANSLATE_NOOP("Model", "Afterglow");
	default:
		return QT_TRANSLATE_NOOP("Model", "Fading out");
	}
}

// a length in bars for a character, a whole number of phrases
int scaled(int bars, int character) {
	const float m = character == J_CALM ? 1.5f : character == J_RESTLESS ? 0.75f : 1.f;
	return std::max(SEG, int(std::lround(bars * m / SEG)) * SEG);
}

// One track as the journey sees it: the pattern it came with (home), the one it plays now (cur), and the one the
// small steps wander away from (base: home, or a new idea).
struct Layer {
	Track* t = nullptr;
	Clip home, base, cur;
	bool has = false;           // something to play
	bool atEnd = false;         // it was playing at the bar where the journey starts
	bool fresh = false;         // its pattern was written for the journey (an empty track)
	int sinceStep = 0;          // phrases since the pattern last changed
	float thr = 0.f;            // how high the arc must be before this part plays (0: always)
	Clip kept, keptBase;        // the pattern it had before going off on a new idea
	std::vector<char> on;       // per phrase: playing
	std::vector<AutoDrum> drum; // per phrase: how the drummer plays
	int role() const { return t->role; }
	bool drums() const { return t->isDrums(); }
};

bool sameNotes(const Clip& a, const Clip& b) {
	if (a.loopLen != b.loopLen || a.notes.size() != b.notes.size()) return false;
	for (size_t i = 0; i < a.notes.size(); i++) {
		const Note &x = a.notes[i], &y = b.notes[i];
		if (x.start != y.start || x.len != y.len || x.deg != y.deg || x.acc != y.acc || x.slide != y.slide ||
		    x.alt != y.alt || x.chance != y.chance || x.rep != y.rep)
			return false;
	}
	return true;
}

bool sameDrummer(const AutoDrum& a, const AutoDrum& b) {
	return a.on == b.on && a.style == b.style && a.intensity == b.intensity && a.complexity == b.complexity &&
	       a.fillEvery == b.fillEvery && a.backbeat == b.backbeat && a.hats == b.hats && a.shape == b.shape &&
	       a.shuffle == b.shuffle && a.crash == b.crash;
}

// One small step away from the pattern, a note at a time: a note moves a step or two along the scale (never more
// than an octave from where it began), an accent or a slide comes or goes, or a note that had wandered goes home.
void stepPattern(const Song& s, Layer& L, std::mt19937& g) {
	Clip& c = L.cur;
	if (L.drums()) {
		varyClip(s, *L.t, c, g); // one ghost hit more or less
		return;
	}
	std::vector<size_t> free;
	for (size_t i = 0; i < c.notes.size(); i++)
		if (!c.notes[i].lock) free.push_back(i);
	if (free.empty()) return;
	const bool aligned = L.base.notes.size() == c.notes.size();
	const int n = s.sc().n;
	const float r = rr(g, 0.f, 1.f);
	if (aligned && r >= .93f) { // now and then a note that had wandered goes home; mostly the steps add up
		std::vector<size_t> moved;
		for (size_t i : free)
			if (c.notes[i].deg != L.base.notes[i].deg || c.notes[i].acc != L.base.notes[i].acc ||
			    c.notes[i].slide != L.base.notes[i].slide)
				moved.push_back(i);
		if (!moved.empty()) {
			const size_t i = moved[size_t(ri(g, int(moved.size())))];
			c.notes[i] = L.base.notes[i];
			return;
		}
	}
	Note& x = c.notes[free[size_t(ri(g, int(free.size())))]];
	const size_t xi = size_t(&x - c.notes.data());
	if (r < .6f || L.role() == R_LEAD) {
		static const int steps[4] = {-2, -1, 1, 2};
		int d = x.deg + steps[ri(g, 4)];
		if (aligned) {
			const int from = L.base.notes[xi].deg;
			d = std::clamp(d, from - n, from + n);
		}
		x.deg = std::clamp(d, DEG_MIN, DEG_MAX);
	} else if (r < .75f || L.role() == R_PAD)
		x.acc = !x.acc;
	else
		x.slide = !x.slide;
}

// a new idea for a track, from a generator other than the one its pattern came from
void newIdea(const Song& s, Layer& L, int startTick, std::mt19937& g, float density) {
	const std::vector<GenDef> gens = generatorsFor(L.role());
	std::vector<int> other;
	for (const GenDef& d : gens)
		if (QString::fromLatin1(d.name) != L.cur.name) other.push_back(d.id);
	if (other.empty())
		for (const GenDef& d : gens)
			other.push_back(d.id);
	Clip c = L.cur;
	c.start = startTick;
	for (Note& n : c.notes)
		n.lock = false; // the new idea is a whole new pattern; the kept notes stay in the home pattern
	generate(s, *L.t, c, other[size_t(ri(g, int(other.size())))], density, g);
	if (c.notes.empty()) return;
	L.cur = L.base = c;
}

// a pattern for a track that had none: a melody for the lead (it comes in on new ground), the first idea otherwise
Clip firstIdea(const Song& s, const Track& t, int startTick, int character, std::mt19937& g) {
	Clip c;
	c.start = startTick;
	const int period = int(s.chords.size());
	c.loopLen = c.len = t.role == R_LEAD ? (period >= 4 && period <= 8 ? period : 4) * BAR : BAR;
	if (t.isDrums()) {
		c.ad.on = true;
		c.ad.seed = unsigned(g());
		return c;
	}
	const int gen = t.role == R_LEAD && character == J_CALM ? generatorsFor(R_LEAD)[1].id : generatorsFor(t.role)[0].id;
	generate(s, t, c, gen, 0.55f, g);
	return c;
}

// the clip a track's journey grows from: the one playing just before the start, else the last one before it, else
// the first one at all, else a cell of the loop grid
const Clip* sourceClip(const Track& t, int fromTick) {
	const Clip* best = nullptr;
	for (const Clip& c : t.clips)
		if (c.start < fromTick && c.end() >= fromTick && (!c.notes.empty() || c.ad.on)) return &c;
	for (const Clip& c : t.clips)
		if (c.end() <= fromTick && (!c.notes.empty() || c.ad.on) && (!best || c.end() > best->end())) best = &c;
	if (best) return best;
	for (const Clip& c : t.clips)
		if ((!c.notes.empty() || c.ad.on) && (!best || c.start < best->start)) best = &c;
	if (best) return best;
	if (const Clip* c = t.cell(t.liveCell)) return c;
	for (const Clip& c : t.cells)
		if (!c.notes.empty() || c.ad.on) return &c;
	return nullptr;
}

// how strongly each phase moves the knob lines, as offsets from where each knob stood when the journey began:
// at the phase's start (only for the first phase), its middle and its end
struct Shape {
	float mid, end;
};
Shape cutoffShape(int type, std::mt19937& g) {
	switch (type) {
	case J_DRIFT:
		return {-.3f, -.2f};
	case J_RISE:
		return {-.1f, 0.f};
	case J_CRUISE: // one long sweep up and back over the phase
		return {rr(g, .06f, .24f), rr(g, -.1f, .05f)};
	case J_SHIFT:
		return {rr(g, .04f, .18f), .1f};
	case J_BREAK: // closes right down, then opens again for the return
		return {-.28f, .02f};
	case J_RETURN:
		return {.1f, .05f};
	case J_PEAK:
		return {.2f, .25f};
	case J_LULL:
		return {-.12f, 0.f};
	case J_GLOW:
		return {rr(g, -.04f, .12f), -.04f};
	default:
		return {-.15f, -.4f};
	}
}
Shape resShape(int type, std::mt19937& g) {
	switch (type) {
	case J_BREAK:
		return {.2f, .1f};
	case J_CRUISE:
		return {rr(g, -.03f, .08f), 0.f};
	case J_PEAK:
		return {.08f, .06f};
	case J_FADE:
		return {.05f, .1f};
	case J_LULL:
		return {.08f, 0.f};
	default:
		return {0.f, 0.f};
	}
}
Shape echoShape(int type, std::mt19937& g) {
	switch (type) {
	case J_DRIFT:
		return {.12f, .08f};
	case J_BREAK:
		return {.25f, .15f};
	case J_SHIFT:
		return {rr(g, 0.f, .06f), .03f};
	case J_FADE:
		return {.12f, .22f};
	case J_LULL:
		return {.1f, .04f};
	case J_GLOW:
		return {.05f, .08f};
	default:
		return {0.f, 0.f};
	}
}

// A knob line for one knob of one track over the journey. Two movements add up: the arc of the whole piece (macro:
// how far the knob travels between the sparse start and the peak; below zero it moves the other way, as the echo
// does) and each phase's own sweep (local), plus a long sweep of its own in the phases listed in features. The
// offsets are eased from one to the next, a point every four bars, so the line reads as a curve and stays easy to
// edit, kept within lo..hi. What came before the journey sounds as before: a track without a line gets one that
// holds the knob where it was until the journey begins.
void writeLine(Track& t, int param, int fromTick, const std::vector<JourneyPhase>& plan, bool continues, float local,
               float macro, float lo, float hi, Shape (*shape)(int, std::mt19937&), float startOffset,
               const std::vector<int>& features, std::mt19937& g) {
	AutoLane* lane = t.lane(param);
	if (lane && lane->loop > 0) return; // a repeating line drawn by hand stays as it is
	const float base = lane && !lane->pts.empty() ? lane->valueAt(fromTick, t.patch.p[param]) : t.patch.p[param];
	hi = std::max(hi, base);
	lo = std::min(lo, base);
	const float own = rr(g, .75f, 1.25f); // each track moves a little its own way, so the lines don't move in step
	const float e0 = plan.empty() ? 0.6f : plan.front().energy;
	// the arc from the start (-0.8) to the peak (+1.2), times how far this knob travels
	const auto arc = [&](float e) { return macro * ((e - e0) / std::max(.2f, 1.f - e0) * 2.f - .8f); };
	struct Key {
		int tick;
		float off;
	};
	std::vector<Key> keys;
	keys.push_back({fromTick, continues ? 0.f : startOffset});
	for (size_t i = 0; i < plan.size(); i++) {
		const JourneyPhase& ph = plan[i];
		const Shape sh = shape(ph.type, g);
		const int a = ph.bar * BAR, len = ph.bars * BAR;
		const float eEnd = i + 1 < plan.size() ? (ph.energy + plan[i + 1].energy) / 2 : ph.energy;
		const float feature = std::find(features.begin(), features.end(), int(i)) != features.end() ? .3f : 0.f;
		keys.push_back({a + len / 2, sh.mid * own * local + arc(ph.energy) + feature + rr(g, -.02f, .02f)});
		keys.push_back({a + len, sh.end * own * local + arc(eEnd)});
	}
	if (!lane) {
		t.lanes.push_back(AutoLane{param, 0, {}});
		lane = &t.lanes.back();
	}
	std::erase_if(lane->pts, [&](const AutoPoint& p) { return p.tick >= fromTick; });
	if (lane->pts.empty() && fromTick > 0) lane->pts.push_back({0, base});
	if (fromTick > 0 && keys.front().off != 0.f) lane->pts.push_back({fromTick - 1, base}); // no slope before
	const auto value = [&](float off) { return std::clamp(base + off, lo, hi); };
	for (size_t k = 0; k + 1 < keys.size(); k++) {
		const Key &a = keys[k], &b = keys[k + 1];
		for (int tick = a.tick; tick < b.tick; tick += 4 * BAR) {
			const float x = float(tick - a.tick) / float(b.tick - a.tick);
			const float ease = 0.5f - 0.5f * std::cos(x * 3.14159265f);
			lane->pts.push_back({tick, value(a.off + (b.off - a.off) * ease)});
		}
	}
	lane->pts.push_back({keys.back().tick, value(keys.back().off)});
}

// the journey's arc at x (0 the start, 1 the end before the fade): from e0 up to the peak at peakAt, then settling
float arcEnergy(float x, float peakAt, float e0) {
	const auto smooth = [](float v) {
		v = std::clamp(v, 0.f, 1.f);
		return v * v * (3.f - 2.f * v);
	};
	if (x < peakAt) return e0 + (1.f - e0) * std::pow(smooth(x / peakAt), 1.3f);
	return 1.f - 0.4f * smooth((x - peakAt) / std::max(0.05f, 1.f - peakAt));
}

// how long a phase lasts: calm journeys lean to long phases, restless ones to short
int phaseLength(int type, int character, std::mt19937& g) {
	if (type == J_LULL) return ch(g, .5f) ? SEG : 2 * SEG;
	static const int calm[5] = {24, 32, 32, 48, 48}, evolving[5] = {16, 24, 32, 32, 48},
	                 restless[5] = {16, 16, 24, 24, 32};
	const int* pick = character == J_CALM ? calm : character == J_RESTLESS ? restless : evolving;
	const int len = pick[ri(g, 5)];
	return type == J_RETURN ? std::min(len, 32) : len;
}

// A stretch of phases between two landmarks, chosen as it goes: cruising or new ground, now and then a breather
// (a part or two steps out) and a return after it. after: past the peak, where cruising becomes the afterglow.
void walk(std::vector<JourneyPhase>& plan, int len, bool after, int character, std::mt19937& g) {
	const size_t first = plan.size();
	int prev = plan.empty() ? J_RISE : plan.back().type, sinceLull = 2;
	const int cruise = after ? J_GLOW : J_CRUISE;
	while (len >= 2 * SEG) {
		const float r = rr(g, 0.f, 1.f);
		const bool lullOk = sinceLull >= 2;
		int type;
		switch (prev) {
		case J_CRUISE:
		case J_GLOW:
			type = lullOk && r < .35f ? J_LULL : J_SHIFT;
			break;
		case J_SHIFT:
			type = lullOk && r < .45f ? J_LULL : cruise;
			break;
		case J_LULL:
			type = r < .6f ? J_RETURN : cruise;
			break;
		default:
			type = r < .6f ? cruise : J_SHIFT;
			break;
		}
		int n = phaseLength(type, character, g);
		if (len - n < 2 * SEG) n = len;
		plan.push_back({0, n, type, phaseName(type), 0.f, type == J_LULL ? ri(g, 3) : 0});
		len -= n;
		prev = type;
		sinceLull = type == J_LULL ? 0 : sinceLull + 1;
	}
	if (plan.size() == first && len > 0)
		plan.push_back({0, len, cruise, phaseName(cruise), 0.f, 0});
	else if (len > 0)
		plan.back().bars += len;
	// a breather never leads straight into a landmark (the breakdown, the peak, the end)
	if (plan.size() > first && plan.back().type == J_LULL) {
		plan.back().type = cruise;
		plan.back().name = phaseName(cruise);
	}
}

// No group of two to four phases comes straight after the same group (the same kinds with the same lengths): the
// later one is made a phrase longer or shorter, or another kind.
void unrepeat(std::vector<JourneyPhase>& plan) {
	const auto fixed = [](int type) {
		return type == J_DRIFT || type == J_RISE || type == J_BREAK || type == J_PEAK || type == J_FADE;
	};
	for (int tries = 0; tries < 200; tries++) {
		bool found = false;
		for (size_t k = 2; k <= 4 && !found; k++)
			for (size_t i = 0; i + 2 * k <= plan.size() && !found; i++) {
				bool same = true;
				for (size_t j = 0; j < k; j++)
					same &= plan[i + j].type == plan[i + k + j].type && plan[i + j].bars == plan[i + k + j].bars;
				if (!same) continue;
				found = true;
				JourneyPhase &a = plan[i + k], &b = plan[i + k + 1];
				if (b.bars >= 3 * SEG && !fixed(b.type)) {
					a.bars += SEG;
					b.bars -= SEG;
				} else if (a.bars >= 3 * SEG && !fixed(a.type)) {
					a.bars -= SEG;
					b.bars += SEG;
				} else {
					const size_t ci = fixed(a.type) ? i + k + 1 : i + k;
					bool afterPeak = false;
					for (size_t m = 0; m < ci; m++)
						afterPeak |= plan[m].type == J_PEAK;
					JourneyPhase& c = plan[ci];
					c.type = c.type == J_SHIFT ? (afterPeak ? J_GLOW : J_CRUISE) : J_SHIFT;
					c.name = phaseName(c.type);
				}
			}
		if (!found) return;
	}
}

} // namespace

int journeyBars(const Song& s, int minutes) {
	const double bars = std::max(1, minutes) * double(s.bpm) / 4.0;
	return std::max(4 * SEG, int(std::lround(bars / SEG)) * SEG);
}

std::vector<JourneyPhase> journeyPlan(int bars, int character, bool continues, bool ending, unsigned seed) {
	std::mt19937 g(seed);
	bars = std::max(4 * SEG, bars / SEG * SEG);
	// the opening: drifting in and rising from nothing, or only rising when the song is already under way
	int drift = continues ? 0 : scaled(32, character), rise = scaled(16, character);
	int fade = ending ? (bars >= 192 ? 32 : 16) : 0;
	while (drift + rise + fade + 2 * SEG > bars) { // a short journey: everything shorter
		if (drift > SEG)
			drift -= SEG;
		else if (fade > SEG)
			fade -= SEG;
		else if (rise > SEG)
			rise -= SEG;
		else if (drift > 0)
			drift = 0;
		else
			break;
	}
	std::vector<JourneyPhase> plan;
	if (drift) plan.push_back({0, drift, J_DRIFT, phaseName(J_DRIFT), 0.f, 0});
	plan.push_back({0, rise, J_RISE, phaseName(J_RISE), 0.f, 0});
	// the middle: a stretch that builds, the one big breakdown and the peak about two thirds of the way in, and a
	// stretch that settles; a short journey just goes along
	const int middle = bars - drift - rise - fade;
	const int peakLen = scaled(32, character), breakLen = middle >= 160 ? 24 : 16;
	if (middle >= peakLen + breakLen + 2 * SEG) {
		const int peakStart = int(std::lround(bars * rr(g, .6f, .75f) / SEG)) * SEG;
		const int pre = std::clamp(peakStart - breakLen - drift - rise, 2 * SEG, middle - peakLen - breakLen);
		int post = middle - pre - breakLen - peakLen;
		walk(plan, pre, false, character, g);
		plan.push_back({0, breakLen, J_BREAK, phaseName(J_BREAK), 0.f, 0});
		plan.push_back({0, peakLen + (post < 2 * SEG ? post : 0), J_PEAK, phaseName(J_PEAK), 0.f, 0});
		if (post >= 2 * SEG) walk(plan, post, true, character, g);
	} else
		walk(plan, middle, false, character, g);
	if (fade) plan.push_back({0, fade, J_FADE, phaseName(J_FADE), 0.f, 0});
	unrepeat(plan);
	// where each phase starts, and where it stands on the arc
	int at = 0, peakMid = -1;
	for (JourneyPhase& ph : plan) {
		ph.bar = at;
		if (ph.type == J_PEAK) peakMid = at + ph.bars / 2;
		at += ph.bars;
	}
	const float body = float(std::max(SEG, bars - fade)), e0 = continues ? .45f : .3f;
	const float peakAt = peakMid >= 0 ? std::clamp(float(peakMid) / body, .3f, .95f) : 1.f;
	for (JourneyPhase& ph : plan) {
		const float x = float(ph.bar + ph.bars / 2) / body;
		ph.energy = ph.type == J_PEAK    ? 1.f
		            : ph.type == J_BREAK ? .2f
		            : ph.type == J_FADE  ? .3f
		            : ph.type == J_DRIFT ? e0 * .7f
		                                 : std::min(.95f, arcEnergy(x, peakAt, e0));
		if (ph.type == J_LULL) ph.energy *= .8f;
	}
	return plan;
}

std::vector<JourneyPhase> continueJourney(Song& s, int fromBar, int minutes, int character, bool ending,
                                          std::mt19937& g) {
	if (s.tracks.empty()) return {};
	character = std::clamp(character, 0, 2);
	fromBar = std::max(0, fromBar);
	const int fromTick = fromBar * BAR;
	bool continues = false;
	for (const Track& t : s.tracks)
		for (const Clip& c : t.clips)
			continues |= c.start < fromTick && (!c.notes.empty() || c.ad.on);
	const int bars = journeyBars(s, minutes);
	const unsigned planSeed = unsigned(g());
	std::vector<JourneyPhase> plan = journeyPlan(bars, character, continues, ending, planSeed);
	for (JourneyPhase& ph : plan)
		ph.bar += fromBar;
	const int segs = bars / SEG;
	s.liveMode = false;

	// what each track brings along, before the old clips after the start are taken away
	std::vector<Layer> layers;
	for (Track& t : s.tracks) {
		Layer L;
		L.t = &t;
		if (const Clip* src = sourceClip(t, fromTick)) {
			L.home = *src;
			L.has = true;
			for (const Clip& c : t.clips)
				L.atEnd |= c.start < fromTick && c.end() >= fromTick && (!c.notes.empty() || c.ad.on);
		} else {
			L.home = firstIdea(s, t, fromTick, character, g);
			L.has = !L.home.notes.empty() || L.home.ad.on;
			L.fresh = true;
		}
		L.home.inKind = L.home.outKind = 0;
		L.home.scene = -1;
		if (!L.drums()) L.home.ad.on = false;
		if (L.home.loopLen <= 0) L.home.loopLen = BAR;
		L.base = L.cur = L.home;
		L.on.assign(size_t(segs), 0);
		L.drum.assign(size_t(segs), L.home.ad);
		layers.push_back(std::move(L));
	}
	// everything from the start on makes way for the journey
	for (Track& t : s.tracks) {
		std::erase_if(t.clips, [&](const Clip& c) { return c.start >= fromTick; });
		for (Clip& c : t.clips)
			if (c.end() > fromTick) {
				c.len = fromTick - c.start;
				c.inLen = std::min(c.inLen, c.len);
				c.outLen = std::min(c.outLen, c.len);
			}
	}

	// the anchor plays whenever everything else rests: a sequence if there is one, the bass, a pad, the lead, drums
	Layer* anchor = nullptr;
	for (int role : {R_SEQ, R_BASS, R_PAD, R_LEAD, R_DRUMS}) {
		for (Layer& L : layers)
			if (!anchor && L.has && L.role() == role && !L.t->mute && !L.fresh) anchor = &L;
		for (Layer& L : layers)
			if (!anchor && L.has && L.role() == role && !L.t->mute) anchor = &L;
	}
	if (!anchor) {
		for (Layer& L : layers)
			if (!anchor && L.has) anchor = &L;
		if (!anchor) return {}; // nothing at all to play
	}
	// how high the arc must stand before each part plays: the pad first, then the bass, the drums, a second sequence
	for (Layer& L : layers) {
		const int r = L.role();
		L.thr = &L == anchor   ? 0.f
		        : r == R_PAD   ? rr(g, .15f, .4f)
		        : r == R_BASS  ? rr(g, .3f, .5f)
		        : r == R_DRUMS ? rr(g, .4f, .6f)
		        : r == R_LEAD  ? 2.f
		                       : rr(g, .45f, .7f);
	}
	// when the lead plays: always at the peak, otherwise now and then on new ground, more often as the arc rises,
	// and never two phases running, so it is missed before it comes back
	std::vector<char> leadIn(plan.size(), 0);
	{
		const float lo = character == J_CALM ? .15f : character == J_RESTLESS ? .35f : .25f;
		int since = 2;
		for (size_t i = 0; i < plan.size(); i++) {
			const JourneyPhase& ph = plan[i];
			bool play = ph.type == J_PEAK;
			if (!play && since >= 2 && ph.type == J_SHIFT) play = ch(g, lo + .5f * ph.energy);
			if (!play && since >= 2 && ph.type == J_RETURN && character == J_RESTLESS) play = ch(g, .4f);
			leadIn[i] = play;
			since = play ? 0 : since + 1;
		}
	}
	const auto thin = [](AutoDrum& ad) {
		ad.intensity *= 0.5f;
		ad.complexity *= 0.5f;
		ad.hats = 3;
		ad.crash = false;
	};

	// who plays in each phrase
	const int breathe = character == J_CALM ? 0 : character == J_EVOLVING ? 1 : 2; // rests per cruising phase
	for (size_t pi = 0; pi < plan.size(); pi++) {
		const JourneyPhase& ph = plan[pi];
		const int s0 = (ph.bar - fromBar) / SEG, n = ph.bars / SEG;
		const float e = ph.energy;
		// in the rising phase the parts not yet playing come in one by one, as far as the arc allows
		std::vector<Layer*> entering;
		if (ph.type == J_RISE) {
			for (int role : {R_PAD, R_SEQ, R_BASS, R_DRUMS})
				for (Layer& L : layers)
					if (L.has && L.role() == role) {
						const bool already = s0 > 0 ? L.on[size_t(s0 - 1)] : L.atEnd;
						if (!already && &L != anchor && L.thr <= e + .2f) entering.push_back(&L);
					}
		}
		// in a cruising phase, now and then a part rests for a phrase (the drummer only thins out)
		std::vector<std::pair<Layer*, int>> rests;
		if ((ph.type == J_CRUISE || ph.type == J_GLOW) && n >= 3)
			for (int k = 0; k < breathe; k++) {
				std::vector<Layer*> cand;
				for (Layer& L : layers)
					if (L.has && &L != anchor && L.role() != R_LEAD && L.role() != R_PAD) cand.push_back(&L);
				if (!cand.empty()) rests.push_back({cand[size_t(ri(g, int(cand.size())))], 1 + ri(g, n - 2)});
			}
		// and in a long one the pad may leave for its last two phrases, so it can come back fresh
		const bool padRests = (ph.type == J_CRUISE || ph.type == J_GLOW) && n >= 4 && ch(g, .5f);
		for (int k = 0; k < n; k++) {
			const size_t seg = size_t(s0 + k);
			for (Layer& L : layers) {
				if (!L.has) continue;
				const int r = L.role();
				const bool isAnchor = &L == anchor, otherSeq = r == R_SEQ && !isAnchor;
				const auto above = [&](float slack) { return isAnchor || e + slack >= L.thr; };
				bool on = false;
				AutoDrum ad = L.home.ad; // the drummer plays harder and busier as the arc rises
				ad.intensity = std::clamp(L.home.ad.intensity * (.55f + .6f * e), .1f, 1.f);
				ad.complexity = std::clamp(L.home.ad.complexity * (.6f + .6f * e), 0.f, 1.f);
				switch (ph.type) {
				case J_DRIFT:
					on = isAnchor || r == R_PAD || (otherSeq && k >= n / 2);
					break;
				case J_RISE: {
					const bool already = (s0 > 0 ? L.on[size_t(s0 - 1)] : L.atEnd) || isAnchor;
					const auto it = std::find(entering.begin(), entering.end(), &L);
					if (already)
						on = true;
					else if (it != entering.end()) {
						const int i = int(it - entering.begin()), entry = i * n / int(entering.size());
						on = k >= entry;
						if (k == entry) { // a drummer coming in builds up over its first phrase
							ad.shape = 1;
							ad.intensity *= 0.8f;
						}
					}
					if (r == R_LEAD) on = L.atEnd && !L.fresh && k < n / 2;
					break;
				}
				case J_CRUISE:
				case J_GLOW:
					on = r != R_LEAD && above(0.f) && !(r == R_PAD && padRests && k >= n - 2);
					for (auto [who, at] : rests)
						if (who == &L && at == k) {
							if (L.drums())
								thin(ad);
							else
								on = false;
						}
					break;
				case J_SHIFT: // new ground: in a long one the pad steps aside for the first half; the lead may come in
					on = r == R_LEAD  ? leadIn[pi] && k >= (n > 2 ? 1 : 0)
					     : r == R_PAD ? above(.05f) && (n < 4 || k >= n / 2)
					                  : above(.05f);
					break;
				case J_LULL: // a breather: one or two parts step out for a moment
					on = r != R_LEAD && above(0.f);
					if (ph.variant == 0 && L.drums()) on = false;
					if (ph.variant == 1 && r == R_BASS) on = false;
					if (ph.variant == 2 && r == R_PAD) on = false;
					if (ph.variant != 0 && L.drums()) thin(ad);
					break;
				case J_BREAK: // the one big breakdown: the anchor alone, the pad joining it halfway
					on = isAnchor || (r == R_PAD && k >= n / 2);
					if (character == J_RESTLESS && r == R_LEAD && k == n - 1) on = true;
					break;
				case J_RETURN:
					on = r == R_LEAD ? bool(leadIn[pi]) : above(.15f);
					ad.crash = true;
					break;
				case J_PEAK:
					on = r != R_LEAD || leadIn[pi];
					ad.intensity = std::min(1.f, L.home.ad.intensity + 0.15f);
					ad.complexity = std::min(1.f, L.home.ad.complexity + 0.15f);
					break;
				default: // fading out: the lead first, then the drums, then the bass; the rest to the end
					on = r == R_LEAD    ? false
					     : r == R_DRUMS ? k < std::max(1, n / 4 + (n > 2 ? 1 : 0))
					     : r == R_BASS  ? k < std::max(1, n / 2)
					                    : true;
					ad.shape = k == std::max(1, n / 4 + (n > 2 ? 1 : 0)) - 1 ? 2 : 0;
					break;
				}
				if (L.t->mute) on = false; // a muted track stays out of the counting below
				L.on[seg] = on;
				L.drum[seg] = ad;
			}
			// never a phrase with nothing playing
			bool any = false;
			for (Layer& L : layers)
				any |= L.on[seg] && !L.t->mute;
			if (!any) anchor->on[seg] = 1;
		}
	}

	// Once in the first half, a part goes off on a new idea (with Restless the sequence itself, otherwise another
	// part where there is one) and comes home at the peak, or at the next return.
	int away = -1, home = -1;
	Layer* rover = nullptr;
	if (bars >= 160) {
		std::vector<int> cand;
		for (size_t i = 0; i < plan.size(); i++)
			if (plan[i].type == J_SHIFT && plan[i].bar - fromBar < bars * 11 / 20) cand.push_back(int(i));
		std::vector<Layer*> who;
		for (Layer& L : layers)
			if (L.has && !L.drums() && L.role() != R_PAD && L.role() != R_LEAD && &L != anchor) who.push_back(&L);
		if (character == J_RESTLESS || who.empty())
			rover = character == J_CALM && who.empty() ? nullptr : anchor;
		else
			rover = who[size_t(ri(g, int(who.size())))];
		if (rover && rover->drums()) rover = nullptr;
		if (rover && !cand.empty()) {
			away = cand[size_t(ri(g, int(cand.size())))];
			for (size_t i = size_t(away) + 1; i < plan.size() && home < 0; i++)
				if (plan[i].type == J_PEAK) home = int(i);
			for (size_t i = size_t(away) + 1; i < plan.size() && home < 0; i++)
				if (plan[i].type == J_RETURN || plan[i].type == J_FADE) home = int(i);
		}
	}

	// the patterns change slowly as the journey goes, and the steps add up: a small step every few phrases while a
	// part plays, a bigger one on new ground, a varied phrase each time the lead comes back
	const int every = character == J_CALM ? 4 : character == J_EVOLVING ? 2 : 1;
	std::vector<std::vector<Clip>> pattern(layers.size(), std::vector<Clip>(size_t(segs)));
	bool leadHeard = false;
	for (size_t pi = 0; pi < plan.size(); pi++) {
		const JourneyPhase& ph = plan[pi];
		const int s0 = (ph.bar - fromBar) / SEG, n = ph.bars / SEG;
		for (int k = 0; k < n; k++) {
			const size_t seg = size_t(s0 + k);
			for (size_t li = 0; li < layers.size(); li++) {
				Layer& L = layers[li];
				if (!L.has) continue;
				const int r = L.role();
				if (k == 0 && int(pi) == away && &L == rover) {
					L.kept = L.cur;
					L.keptBase = L.base;
					newIdea(s, L, fromTick + int(seg) * SEG * BAR, g, 0.65f);
					L.sinceStep = 0;
				} else if (k == 0 && int(pi) == home && &L == rover) {
					L.cur = L.kept; // home again, as it was when it left
					L.base = L.keptBase;
					L.sinceStep = 0;
				} else if (k == 0 && ph.type == J_SHIFT && r != R_PAD && r != R_LEAD && !L.drums()) {
					for (int i = 0; i < (character == J_CALM ? 1 : 2); i++)
						stepPattern(s, L, g);
					L.sinceStep = 0;
				} else if (k == 0 && r == R_LEAD && leadIn[pi]) {
					if (leadHeard)
						for (int i = 0; i < 2; i++)
							stepPattern(s, L, g);
					leadHeard = true;
				} else if (L.on[seg] && r != R_PAD && !(L.drums() && L.home.ad.on)) {
					// the bass changes half as often as the rest: it holds the ground
					const int due = r == R_BASS ? 2 * every : every;
					if (++L.sinceStep >= due && ph.type != J_FADE) {
						stepPattern(s, L, g);
						L.sinceStep = 0;
					}
				}
				pattern[li][seg] = L.cur;
			}
		}
	}
	// the clips: one per phrase, joined where nothing changes, faded or swept in and out where a part comes or goes
	for (size_t li = 0; li < layers.size(); li++) {
		Layer& L = layers[li];
		if (!L.has) continue;
		Track& t = *L.t;
		const int r = L.role();
		const bool drummer = L.drums() && L.home.ad.on;
		for (int seg = 0; seg < segs; seg++) {
			if (!L.on[size_t(seg)]) continue;
			const bool before = seg > 0 ? L.on[size_t(seg - 1)] : L.atEnd;
			const bool after = seg + 1 < segs && L.on[size_t(seg + 1)];
			const bool last = seg + 1 == segs;
			const int start = fromTick + seg * SEG * BAR, len = SEG * BAR;
			int type = J_CRUISE;
			for (const JourneyPhase& ph : plan)
				if (ph.bar * BAR <= start) type = ph.type;
			Clip c = pattern[li][size_t(seg)];
			c.id = 0;
			c.start = start;
			c.len = len;
			c.adKey = 0;
			c.inKind = c.outKind = 0;
			c.inLen = c.outLen = BAR;
			if (drummer) {
				c.ad = L.drum[size_t(seg)];
				c.ad.seed = unsigned(g());
				c.notes.clear();
			} else
				c.offset = c.loopLen > 0 ? (start - fromTick) % c.loopLen : 0;
			if (L.drums() && !drummer && c.ad.hats == 3) // a drum part played by hand thins out to kick and hats
				std::erase_if(c.notes, [](const Note& x) { return x.deg != D_KICK && x.deg != D_CHAT; });
			if (!before && !L.drums()) { // coming in
				c.inKind = r == R_BASS ? 2 : (r == R_SEQ && type == J_DRIFT) ? 3 : 1;
				c.inLen = r == R_BASS ? len : r == R_PAD || type == J_DRIFT ? std::min(len, 8 * BAR) : 4 * BAR;
			}
			if (last && ending) { // the end of the journey: everything still playing fades away
				c.outKind = r == R_SEQ || r == R_BASS ? 3 : 1;
				c.outLen = len;
			} else if (!after && !last) { // leaving at a phrase line
				if (type == J_FADE) {
					c.outKind = r == R_BASS ? 3 : 1;
					c.outLen = r == R_DRUMS ? 2 * BAR : len;
				} else if (r == R_PAD || r == R_SEQ || r == R_LEAD) {
					c.outKind = 1;
					c.outLen = r == R_LEAD ? 2 * BAR : 4 * BAR;
				}
			}
			// joined to the clip before when nothing changes between them
			Clip* prev = t.clips.empty() ? nullptr : &t.clips.back();
			if (prev && seg > 0 && prev->end() == start && prev->start >= fromTick && prev->outKind == 0 &&
			    c.inKind == 0 && prev->name == c.name &&
			    (drummer ? sameDrummer(prev->ad, c.ad) : sameNotes(*prev, c))) {
				prev->len += len;
				prev->outKind = c.outKind;
				prev->outLen = c.outLen;
				continue;
			}
			c.id = s.newId();
			t.clips.push_back(c);
		}
	}

	// The knob lines: the filters travel with the arc over many minutes (dark at the start, brightest at the peak),
	// each phase sweeps on top, resonance rises towards the peak and in the breakdown, the echo grows where it is
	// sparse; and a few long sweeps of one track alone stand out as features.
	const float depth = character == J_CALM ? 0.6f : character == J_RESTLESS ? 1.2f : .9f;
	const float travel = character == J_CALM ? .28f : character == J_RESTLESS ? .38f : .33f;
	std::vector<std::vector<int>> features(layers.size());
	{
		std::vector<int> where;
		for (size_t i = 0; i < plan.size(); i++)
			if (plan[i].type == J_CRUISE || plan[i].type == J_SHIFT || plan[i].type == J_GLOW) where.push_back(int(i));
		std::vector<size_t> who;
		for (size_t li = 0; li < layers.size(); li++)
			if (layers[li].has && !layers[li].drums() && layers[li].role() != R_PAD) who.push_back(li);
		const int count = plan.size() >= 6 ? (character == J_CALM ? 1 : character == J_EVOLVING ? 2 : 3) : 0;
		for (int k = 0; k < count && !where.empty() && !who.empty(); k++)
			features[who[size_t(ri(g, int(who.size())))]].push_back(where[size_t(ri(g, int(where.size())))]);
	}
	for (size_t li = 0; li < layers.size(); li++) {
		Layer& L = layers[li];
		if (!L.has || L.drums()) continue;
		const int r = L.role();
		writeLine(*L.t, P_CUTOFF, fromTick, plan, continues, r == R_PAD ? depth * .6f : depth,
		          r == R_PAD ? travel * .7f : travel, 0.06f, 0.95f, cutoffShape, -0.3f * depth, features[li], g);
		if (r == R_SEQ || r == R_BASS)
			writeLine(*L.t, P_RES, fromTick, plan, continues, depth, .12f, 0.f, 0.82f, resShape, 0.f, {}, g);
		if (r == R_SEQ || r == R_LEAD || r == R_PAD)
			writeLine(*L.t, P_DELAY, fromTick, plan, continues, depth, -.15f, 0.f, 0.6f, echoShape, 0.1f, {}, g);
	}
	if (!anchor->drums()) {
		anchor->t->laneOpen = true; // the anchor's filter line shows, so the slow sweep can be seen
		anchor->t->laneParam = P_CUTOFF;
	}
	s.loopOn = false; // the whole journey plays, not the loop
	// the phases as named parts on the timeline (in English, shown translated), after the parts named before the
	// journey began; parts from its start on went with the clips there. Two phases of the same kind in a row are one
	// part.
	std::vector<SongPart> named;
	for (const SongPart& pt : s.parts)
		if (pt.bar < fromBar) named.push_back(pt);
	const size_t before = named.size();
	for (const JourneyPhase& ph : plan)
		if (named.size() == before || named.back().name != QLatin1String(ph.name))
			named.push_back({ph.bar, QString::fromLatin1(ph.name)});
	s.setParts(std::move(named));
	refreshAutoDrums(s);
	return plan;
}
