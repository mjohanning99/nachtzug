// The auto drummer: writes a drum clip's hits from its settings (style, energy, playfulness, fills, backbeat,
// hi-hats, shape, shuffle), and writes them again when the settings or the clip's length change.
//
// What's here: autoDrumFill(), refreshAutoDrums(); used by the drummer's settings, opening a song and the tests.
// Changes in the song file: none (a clip saves its drummer settings, AutoDrum, in songfile.cpp).
#include "model.h"
#include "patterns.h"
#include <algorithm>
#include <cmath>

using namespace dsp;

using namespace patterns;

// ---------------------------------------------------------------- auto drummer

static unsigned autoKey(const Clip& c) {
	unsigned h = 2166136261u;
	for (unsigned v :
	     {unsigned(c.ad.style), unsigned(c.ad.intensity * 1000.f), unsigned(c.ad.complexity * 1000.f),
	      unsigned(c.ad.fillEvery), c.ad.seed, unsigned(c.len), unsigned(c.ad.backbeat), unsigned(c.ad.hats),
	      unsigned(c.ad.shape), unsigned(c.ad.shuffle * 1000.f), unsigned(c.ad.crash)})
		h = (h ^ v) * 16777619u;
	return h | 1u;
}

static void autoDrumFillNotes(Clip& c);

// The drummer writes the clip again; hits you marked Keep stay, and the drummer's own hit on the same drum at the
// same moment makes way for them.
void autoDrumFill(Clip& c) {
	std::vector<Note> kept;
	for (const Note& n : c.notes)
		if (n.lock) kept.push_back(n);
	autoDrumFillNotes(c);
	if (kept.empty()) return;
	std::erase_if(c.notes, [&](const Note& n) {
		for (const Note& k : kept)
			if (k.start == n.start && k.deg == n.deg) return true;
		return false;
	});
	for (const Note& k : kept)
		if (k.start < c.loopLen) c.notes.push_back(k);
}

static void autoDrumFillNotes(Clip& c) {
	c.notes.clear();
	c.loopLen = std::max(c.len, 24);
	c.offset = 0;
	const AutoDrum& a = c.ad;
	const float X = a.complexity;
	const int bars = std::max(1, (c.len + BAR - 1) / BAR);
	const int late = int(a.shuffle * 8.f); // shuffle: every second sixteenth comes a little late
	auto hit = [&](int bar, int step, int drum, bool acc) {
		add(c, bar * BAR + step * 24 + (step % 2 ? late : 0), 12, drum, acc);
	};
	for (int b = 0; b < bars; b++) {
		std::mt19937 g(a.seed * 7919u + unsigned(b) * 104729u + 17u);
		// how hard the drummer plays in this bar
		const float along = bars > 1 ? float(b) / float(bars - 1) : 1.f;
		const float I = a.intensity * (a.shape == 1 ? 0.3f + 0.7f * along : a.shape == 2 ? 1.f - 0.7f * along : 1.f);
		// fills come every few bars and always lead out of the clip into whatever follows
		const bool fill = a.fillEvery > 0 && ((b + 1) % a.fillEvery == 0 || (b == bars - 1 && bars > 1)) && I > 0.12f;
		// the regular beat plays up to this step: the harder the drummer plays, the earlier the fill starts (last beat
		// .. last two)
		const int stop = fill ? std::clamp(14 - int(std::lround(I * 8.f)), 8, 13) : 16;
		if (a.crash && I > 0.45f && b > 0 && a.fillEvery > 0 && b % a.fillEvery == 0)
			hit(b, 0, D_CYM, true); // crash after a fill
		if (a.crash && a.style == 6 && b % 8 == 0 && I > 0.3f) hit(b, 0, D_CYM, false);
		for (int st = 0; st < stop; st++) {
			const bool beat = st % 4 == 0, off8 = st % 4 == 2, odd = st % 2 == 1;
			// kick
			bool kick = false, kAcc = true;
			switch (a.style) {
			case 0:
				kick = beat || (I > .75f && st == 14 && ch(g, X * .6f));
				break;
			case 1:
				kick = beat || (I > .5f && st == 14) || (I > .6f && (st == 7 || st == 11) && ch(g, X * .4f));
				kAcc = beat;
				break;
			case 2:
				kick = st % 8 == 0 || (I > .3f && st % 8 == 4) || (I > .7f && st == 10);
				kAcc = st % 8 == 0;
				break;
			case 3:
				kick = st == 0 || st == 10 || (I > .4f && st == 6) || ((st == 3 || st == 13) && ch(g, X * .5f));
				kAcc = st == 0 || st == 10;
				break;
			case 4:
				kick = st == 0 || (I > .4f && st == 10) || (st == 7 && ch(g, X * .4f));
				kAcc = st == 0;
				break;
			case 5:
				kick = st == 0 || st == 10 || (I > .3f && st == 6) || (I > .7f && st == 13 && ch(g, X * .6f));
				kAcc = st == 0 || st == 10;
				break;
			default:
				kick = beat;
				kAcc = st == 0;
				break;
			}
			if (kick) hit(b, st, D_KICK, kAcc);
			// backbeat
			const int own = a.style == 0 ? D_CLAP : a.style == 6 ? (I > .7f ? D_RIM : -1) : D_SNARE;
			const int bb = a.backbeat == 1   ? D_CLAP
			               : a.backbeat == 2 ? D_SNARE
			               : a.backbeat == 3 ? D_RIM
			               : a.backbeat == 4 ? -1
			                                 : own;
			if (I > .2f && bb >= 0) {
				if (a.style == 4) {
					if (st == 8) hit(b, st, bb, true);
				} else if (st == 4 || st == 12) {
					hit(b, st, bb, true);
					if (I > .8f && a.backbeat == 0 && a.style != 6) hit(b, st, bb == D_CLAP ? D_SNARE : D_CLAP, false);
				}
				if (a.style == 3 && (st == 7 || st == 9) && ch(g, X * .6f)) hit(b, st, bb, false);
			}
			// hats
			if (a.hats == 1) {
				if (!odd) hit(b, st, D_CHAT, beat);
			} else if (a.hats == 2)
				hit(b, st, D_CHAT, !odd);
			else if (a.hats == 3) {
				if (off8) hit(b, st, D_OHAT, true);
			} else if (a.hats == 0) {
				const bool open = (a.style <= 1 && I >= .45f && off8) || (a.style == 3 && I > .5f && st == 14) ||
				                  (a.style == 4 && I > .6f && st == 6) || (a.style == 5 && I > .6f && st == 14);
				const bool sixteenths = I >= .65f || (a.style == 6 && I >= .4f);
				if (open)
					hit(b, st, D_OHAT, true);
				else if (sixteenths)
					hit(b, st, D_CHAT, !odd);
				else if (I >= .25f && !odd)
					hit(b, st, D_CHAT, beat);
				else if (I >= .1f && beat)
					hit(b, st, D_CHAT, false);
			}
			// extras, more of them the harder the drummer plays, so every bit of Energy is heard, with or without hats
			const float more = std::clamp((I - .2f) / .8f, 0.f, 1.f); // 0 at 20 % energy .. 1 at full
			if (more > 0.f && odd && ch(g, (.08f + X * .3f) * more)) hit(b, st, D_RIM, false);
			if (more > 0.f && (st == 11 || st == 15) && ch(g, (.05f + X * .25f) * more)) hit(b, st, D_TOM, false);
			if (bb >= 0 && bb != D_RIM && more > 0.f && (st == 7 || st == 10 || st == 15) &&
			    ch(g, (.1f + X * .35f) * more))
				hit(b, st, D_SNARE, false); // ghost notes
			if (!kick && more > .3f && (st == 6 || st == 11 || st == 14) && ch(g, (more - .3f) * (.25f + X * .4f)))
				hit(b, st, D_KICK, false); // pushed kicks
		}
		if (fill) {
			if (I < .2f) {
				hit(b, 14, D_SNARE, false);
				hit(b, 15, D_SNARE, true);
			} else
				for (int st = stop; st < 16; st++) {
					const bool lastBeat = st >= 12;
					if (!lastBeat && st % 2 == 1 && !ch(g, X)) continue; // eighths first, then sixteenths
					if (I < .45f && st % 2 == 1 && st < 14 && !ch(g, I))
						continue; // a lighter fill when the drummer plays softly
					hit(b, st, I > .6f && lastBeat && st % 2 == 0 ? D_TOM : D_SNARE, st >= 14 || st % 4 == 0);
					if (st % 4 == 0) hit(b, st, D_KICK, true);
				}
		}
	}
	c.adKey = autoKey(c);
}

bool refreshAutoDrums(Song& s) {
	bool any = false;
	for (Track& t : s.tracks) {
		if (!t.isDrums()) continue;
		for (Clip& c : t.clips)
			if (c.ad.on && c.adKey != autoKey(c)) {
				autoDrumFill(c);
				any = true;
			}
		for (Clip& c : t.cells)
			if (c.ad.on && c.adKey != autoKey(c)) {
				autoDrumFill(c);
				any = true;
			}
	}
	return any;
}
