// Small helpers the pattern makers share (ideas.cpp, drummer.cpp, sounds.cpp): random numbers from a seeded
// generator, and putting a note into a pattern. Not for use outside the song code.
#pragma once
#include "model.h"
#include <algorithm>
#include <random>

namespace patterns {

inline float rr(std::mt19937& g, float a, float b) {
	return std::uniform_real_distribution<float>(a, b)(g);
}
inline int ri(std::mt19937& g, int n) {
	return n <= 1 ? 0 : std::uniform_int_distribution<int>(0, n - 1)(g);
}
inline bool ch(std::mt19937& g, float p) {
	return rr(g, 0.f, 1.f) < p;
}

inline void add(Clip& c, int start, int len, int deg, bool acc = false, bool slide = false) {
	if (start >= c.loopLen) return;
	Note n;
	n.start = start;
	n.len = std::max(1, std::min(len, c.loopLen - start + (slide ? 4 : 0)));
	n.deg = deg;
	n.acc = acc;
	n.slide = slide;
	c.notes.push_back(n);
}

} // namespace patterns
