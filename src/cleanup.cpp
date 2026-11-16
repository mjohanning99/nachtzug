// Clean up: tidies a played-in pattern (onto the grid chosen, doubled notes merged, accidental blips removed, and
// across repeats missed notes filled in and wrong keys put right).
//
// What's here: cleanUpClip(), used by the Clean up key in the Notes tab and the tests.
// Changes in the song file: none.
#include "model.h"
#include <algorithm>
#include <map>
#include <set>
#include <tuple>

using namespace dsp;

CleanReport cleanUpClip(Clip& c, bool drums, int grid) {
	CleanReport r;
	const int L = c.loopLen;
	grid = std::clamp(grid, 6, 96);
	auto& ns = c.notes;
	auto heard = [&](const Note& n) { return n.start < L; };

	// a tiny note pressed by accident: much shorter than a note on the next row that starts at almost the same time
	if (!drums) {
		std::vector<bool> drop(ns.size(), false);
		for (size_t i = 0; i < ns.size(); i++) {
			const Note& a = ns[i];
			if (a.lock || !heard(a) || a.len > std::max(12, grid / 2)) continue;
			for (size_t j = 0; j < ns.size(); j++) {
				const Note& b = ns[j];
				if (i == j || drop[j] || std::abs(b.deg - a.deg) != 1 || std::abs(b.start - a.start) > grid / 2)
					continue;
				if (b.len >= 2 * a.len) {
					drop[i] = true;
					break;
				}
			}
		}
		for (size_t i = ns.size(); i-- > 0;)
			if (drop[i]) {
				ns.erase(ns.begin() + long(i));
				r.removed++;
			}
	}

	// onto the grid chosen beside the button: every note starts on it (the lengths stay, short notes are often meant)
	for (Note& n : ns) {
		if (n.lock || !heard(n)) continue;
		const int s = int(std::lround(double(n.start) / grid)) * grid % L;
		if (s != n.start) {
			n.start = s;
			r.snapped++;
		}
	}

	// doubled notes: one note, as long as the longest, accented if either was
	std::sort(ns.begin(), ns.end(), [](const Note& a, const Note& b) {
		return std::tie(a.start, a.deg, a.alt) < std::tie(b.start, b.deg, b.alt);
	});
	for (size_t i = 1; i < ns.size();) {
		Note& a = ns[i - 1];
		const Note& b = ns[i];
		if (a.start == b.start && a.deg == b.deg && (drums || a.alt == b.alt)) {
			a.len = std::max(a.len, b.len);
			a.acc |= b.acc;
			a.slide |= b.slide;
			a.lock |= b.lock;
			a.chance = std::max(a.chance, b.chance);
			ns.erase(ns.begin() + long(i));
			r.merged++;
		} else
			i++;
	}
	// a note that runs into the next one on its row stops where that one starts
	if (!drums)
		for (size_t i = 0; i < ns.size(); i++)
			for (size_t j = i + 1; j < ns.size(); j++)
				if (ns[j].deg == ns[i].deg && ns[j].alt == ns[i].alt && ns[j].start > ns[i].start) {
					if (!ns[i].slide && !ns[i].lock && ns[i].start + ns[i].len > ns[j].start)
						ns[i].len = std::max(6, ns[j].start - ns[i].start);
					break;
				}

	// the repeats: a period of one or two bars that comes round at least three times
	using Key = std::tuple<int, int, int>; // place in the period, row, semitone off the scale
	int bestP = 0;
	double bestAgree = 0.0;
	for (int P : {BAR, 2 * BAR}) {
		if (L % P || L / P < 3) continue;
		const int R = L / P;
		// how much of the pattern most of the repeats agree on
		std::map<Key, std::set<int>> reps;
		int all = 0;
		for (const Note& n : ns)
			if (heard(n)) {
				reps[{n.start % P, n.deg, n.alt}].insert(n.start / P);
				all++;
			}
		int agreed = 0;
		for (const auto& [k, in] : reps)
			if (in.size() >= 2 && 2 * int(in.size()) > R) agreed += int(in.size());
		const double agree = all ? double(agreed) / all : 0.0;
		if (agree > bestAgree) {
			bestAgree = agree;
			bestP = P;
		}
	}
	if (bestP && bestAgree >= 0.6) {
		const int P = bestP, R = L / P;
		std::map<Key, std::vector<int>> where; // which repeats have each note
		std::map<Key, size_t> example;         // a note to copy it from
		for (size_t i = 0; i < ns.size(); i++) {
			if (!heard(ns[i])) continue;
			const Key k{ns[i].start % P, ns[i].deg, ns[i].alt};
			where[k].push_back(ns[i].start / P);
			if (!example.count(k)) example[k] = i;
		}
		auto majority = [&](const Key& k) {
			const auto it = where.find(k);
			return it != where.end() && int(it->second.size()) >= 2 && 2 * int(it->second.size()) > R;
		};
		auto busyAt = [&](int rep, int pos) {
			return std::any_of(ns.begin(), ns.end(),
			                   [&](const Note& n) { return heard(n) && n.start / P == rep && n.start % P == pos; });
		};
		// a wrong key: a note only one repeat has, where most repeats have another note that this one lacks
		if (!drums)
			for (Note& n : ns) {
				if (n.lock || !heard(n)) continue;
				const int rep = n.start / P, pos = n.start % P;
				const Key k{pos, n.deg, n.alt};
				if (where[k].size() != 1) continue;
				for (const auto& [other, reps] : where) {
					if (std::get<0>(other) != pos || other == k || !majority(other)) continue;
					if (std::find(reps.begin(), reps.end(), rep) != reps.end()) continue;
					n.deg = std::get<1>(other);
					n.alt = std::get<2>(other);
					where[k].clear();
					where[other].push_back(rep);
					r.fixed++;
					break;
				}
			}
		// a missed note: most repeats have it, and this one has nothing else in its place
		std::vector<Note> add;
		for (const auto& [k, reps] : where) {
			if (!majority(k)) continue;
			for (int rep = 0; rep < R; rep++) {
				if (std::find(reps.begin(), reps.end(), rep) != reps.end()) continue;
				const int pos = std::get<0>(k);
				if (!drums && busyAt(rep, pos)) continue;
				if (drums && std::any_of(ns.begin(), ns.end(), [&](const Note& n) {
					    return n.start == rep * P + pos && n.deg == std::get<1>(k);
				    }))
					continue;
				Note n = ns[example[k]];
				n.start = rep * P + pos;
				n.sel = false;
				n.lock = false;
				add.push_back(n);
				r.filled++;
			}
		}
		// a stray: a short note only one repeat has, with nothing in that place in any other repeat (not in the last
		// repeat, where a change on the way round is usually meant)
		if (!drums)
			for (size_t i = ns.size(); i-- > 0;) {
				const Note& n = ns[i];
				if (n.lock || !heard(n) || n.len > grid) continue;
				const int rep = n.start / P, pos = n.start % P;
				if (rep == R - 1 || where[Key{pos, n.deg, n.alt}].size() != 1) continue;
				bool alone = true;
				for (int o = 0; o < R && alone; o++)
					if (o != rep && busyAt(o, pos)) alone = false;
				if (alone) {
					ns.erase(ns.begin() + long(i));
					r.removed++;
				}
			}
		ns.insert(ns.end(), add.begin(), add.end());
	}
	std::sort(ns.begin(), ns.end(),
	          [](const Note& a, const Note& b) { return std::tie(a.start, a.deg) < std::tie(b.start, b.deg); });
	return r;
}
