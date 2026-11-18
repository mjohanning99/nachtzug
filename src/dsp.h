// Nachtzug DSP: oscillators, ladder filter, envelopes, synth voice, drum machine, effects.
// Header-only, no allocation after init(), safe to call from the audio thread.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace dsp {

constexpr float PI = 3.14159265358979f;
constexpr float TAU = 6.28318530717958f;
constexpr int CHUNK = 16; // control-rate block size in samples

inline float fastTanh(float x) {
	x = std::clamp(x, -3.f, 3.f);
	const float x2 = x * x;
	return x * (27.f + x2) / (27.f + 9.f * x2);
}
inline float coef(float seconds, float rate) {
	return 1.f - std::exp(-1.f / (std::max(seconds, 0.0004f) * rate));
}

struct Rng {
	uint32_t s = 0x2545F491u;
	inline float next() { // -1..1
		s ^= s << 13;
		s ^= s >> 17;
		s ^= s << 5;
		return float(int32_t(s)) * (1.f / 2147483648.f);
	}
};

inline float polyblep(float t, float dt) {
	if (t < dt) {
		t /= dt;
		return t + t - t * t - 1.f;
	}
	if (t > 1.f - dt) {
		t = (t - 1.f) / dt;
		return t * t + t + t + 1.f;
	}
	return 0.f;
}

struct Osc {
	float ph = 0.f;
	inline float tick(int wave, float dt, float pw) {
		ph += dt;
		if (ph >= 1.f) ph -= 1.f;
		switch (wave) {
		case 0:
			return 2.f * ph - 1.f - polyblep(ph, dt);
		case 1: {
			float v = ph < pw ? 1.f : -1.f;
			v += polyblep(ph, dt);
			float p2 = ph - pw;
			if (p2 < 0.f) p2 += 1.f;
			v -= polyblep(p2, dt);
			return v;
		}
		case 2:
			return 1.f - 4.f * std::fabs(ph - 0.5f);
		default:
			return std::sin(TAU * ph);
		}
	}
	// same, with the read position pushed by pm (in cycles): used for FM
	inline float tickPM(int wave, float dt, float pw, float pm) {
		ph += dt;
		if (ph >= 1.f) ph -= 1.f;
		float q = ph + pm;
		q -= std::floor(q);
		switch (wave) {
		case 0:
			return 2.f * q - 1.f - polyblep(q, dt);
		case 1:
			return q < pw ? 1.f : -1.f;
		case 2:
			return 1.f - 4.f * std::fabs(q - 0.5f);
		default:
			return std::sin(TAU * q);
		}
	}
};

// Four-pole zero-delay-feedback ladder with a saturating feedback path.
// mode: 0 low-pass 24 dB, 1 low-pass 12 dB, 2 band-pass, 3 high-pass, 4 notch (made by mixing the stage outputs).
struct Ladder {
	float s[4] = {0, 0, 0, 0};
	inline float process(float x, float g, float k, int mode) {
		const float G = g / (1.f + g), G2 = G * G, om = 1.f - G;
		const float S = (G2 * G * s[0] + G2 * s[1] + G * s[2] + s[3]) * om;
		const float u0 = fastTanh((x - k * S) / (1.f + k * G2 * G2));
		float y[4], u = u0;
		for (int i = 0; i < 4; i++) {
			const float v = (u - s[i]) * G;
			y[i] = v + s[i];
			s[i] = y[i] + v;
			u = y[i];
		}
		switch (mode) {
		case 1:
			return y[1];
		case 2:
			return 2.f * (y[0] - y[1]);
		case 3:
			return u0 - 2.f * y[0] + y[1];
		case 4: // the 12 dB high-pass and low-pass added: (s² + 1) / (s + 1)², nothing left right at the cutoff
			return u0 - 2.f * y[0] + 2.f * y[1];
		default:
			return y[3];
		}
	}
	void reset() { s[0] = s[1] = s[2] = s[3] = 0.f; }
};

struct Env {
	float v = 0.f;
	int st = 0; // 0 idle, 1 attack, 2 decay/sustain, 3 release
	inline float tick(float ca, float cd, float sus, float cr) {
		switch (st) {
		case 1:
			v += (1.3f - v) * ca;
			if (v >= 1.f) {
				v = 1.f;
				st = 2;
			}
			break;
		case 2:
			v += (sus - v) * cd;
			break;
		case 3:
			v += (0.f - v) * cr;
			if (v < 1e-4f) {
				v = 0.f;
				st = 0;
			}
			break;
		default:
			break;
		}
		return v;
	}
	void on() { st = 1; }
	void off() {
		if (st) st = 3;
	}
	bool active() const { return st != 0; }
};

// Simper state-variable filter
struct SVF {
	float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1;
	void set(float fc, float q, float sr) {
		const float g = std::tan(PI * std::min(fc, sr * 0.45f) / sr);
		k = 1.f / q;
		a1 = 1.f / (1.f + g * (g + k));
		a2 = g * a1;
		a3 = g * a2;
	}
	inline void tick(float x, float& lp, float& bp, float& hp) {
		const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
		ic1 = 2.f * v1 - ic1;
		ic2 = 2.f * v2 - ic2;
		lp = v2;
		bp = v1;
		hp = x - k * v1 - v2;
	}
};

struct DelayLine {
	std::vector<float> b;
	int w = 0;
	void init(int n) {
		b.assign(size_t(std::max(n, 4)), 0.f);
		w = 0;
	}
	void clear() { std::fill(b.begin(), b.end(), 0.f); } // no allocation: safe on the audio thread
	inline void write(float x) {
		b[size_t(w)] = x;
		if (++w >= int(b.size())) w = 0;
	}
	inline float read(float d) const { // d in samples, >= 1
		const int n = int(b.size());
		float p = float(w) - d;
		while (p < 0.f)
			p += float(n);
		int i = int(p);
		const float f = p - float(i);
		if (i >= n) i -= n;
		int j = i + 1;
		if (j >= n) j = 0;
		return b[size_t(i)] + (b[size_t(j)] - b[size_t(i)]) * f;
	}
	inline float tap(int d) const {
		int i = w - d;
		const int n = int(b.size());
		while (i < 0)
			i += n;
		return b[size_t(i)];
	}
};

// ---------------------------------------------------------------- equaliser

// A track's equaliser, on every synthesiser and drum kit: a low cut (12 dB an octave), a low shelf, a bell in the
// middle that can be moved, and a high shelf, each up to 12 dB either way. Its five settings are 0..1 like every knob,
// in this order: low cut (0 off), low, mid, the mid's pitch, high (0.5 flat).
constexpr float EQ_LOW_HZ = 150.f, EQ_HIGH_HZ = 5000.f, EQ_MID_Q = 0.9f;
inline float eqDb(float v) {
	return (v - 0.5f) * 24.f;
} // -12 .. +12 dB
inline float eqMidHz(float v) {
	return 150.f * std::pow(40.f, v);
} // 150 Hz .. 6 kHz, about 950 Hz in the middle
inline float lowCutHz(float v) {
	return 20.f * std::pow(40.f, v);
} // 20 .. 800 Hz (0 is off)
inline bool eqFlat(const float* v) {
	return v[0] <= 0.f && v[1] == 0.5f && v[2] == 0.5f && v[4] == 0.5f;
}

// One second-order filter for two channels (transposed direct form II), with the usual cookbook shapes.
struct Biquad {
	float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;
	float z1[2] = {}, z2[2] = {};
	inline float tick(float x, int c) {
		const float y = b0 * x + z1[c];
		z1[c] = b1 * x - a1 * y + z2[c];
		z2[c] = b2 * x - a2 * y;
		return y;
	}
	void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0.f; }
	void set(float B0, float B1, float B2, float A0, float A1, float A2) {
		const float k = 1.f / A0;
		b0 = B0 * k;
		b1 = B1 * k;
		b2 = B2 * k;
		a1 = A1 * k;
		a2 = A2 * k;
	}
	void lowShelf(float f, float db, float sr) { shelf(f, db, sr, 1.f); }
	void highShelf(float f, float db, float sr) { shelf(f, db, sr, -1.f); }
	void peak(float f, float db, float q, float sr) {
		const float A = std::pow(10.f, db / 40.f), w = TAU * std::min(f, sr * 0.45f) / sr, cs = std::cos(w),
		            al = std::sin(w) / (2.f * q);
		set(1.f + al * A, -2.f * cs, 1.f - al * A, 1.f + al / A, -2.f * cs, 1.f - al / A);
	}
	void highPass(float f, float sr) {
		const float w = TAU * std::min(f, sr * 0.45f) / sr, cs = std::cos(w), al = std::sin(w) * 0.70710678f;
		set((1.f + cs) * 0.5f, -(1.f + cs), (1.f + cs) * 0.5f, 1.f + al, -2.f * cs, 1.f - al);
	}
	// how loud a sine of frequency f comes out, in dB (for the picture)
	float gainDb(float f, float sr) const {
		const float w = TAU * f / sr, c1 = std::cos(w), s1 = std::sin(w), c2 = std::cos(2.f * w),
		            s2 = std::sin(2.f * w);
		const float nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2);
		const float dr = 1.f + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
		return 10.f * std::log10(std::max((nr * nr + ni * ni) / std::max(dr * dr + di * di, 1e-20f), 1e-12f));
	}

private:
	void shelf(float f, float db, float sr, float side) { // side 1: low shelf, -1: high shelf (slope 1)
		const float A = std::pow(10.f, db / 40.f), w = TAU * std::min(f, sr * 0.45f) / sr, cs = std::cos(w) * side,
		            sa = 2.f * std::sqrt(A) * std::sin(w) * 0.70710678f;
		set(A * ((A + 1.f) - (A - 1.f) * cs + sa), 2.f * side * A * ((A - 1.f) - (A + 1.f) * cs),
		    A * ((A + 1.f) - (A - 1.f) * cs - sa), (A + 1.f) + (A - 1.f) * cs + sa,
		    -2.f * side * ((A - 1.f) + (A + 1.f) * cs), (A + 1.f) + (A - 1.f) * cs - sa);
	}
};

// The equaliser itself. Click-free: its settings glide to where the knobs are, a little every chunk, and a band
// that is flat is skipped; with every band flat the sound passes untouched, sample for sample, so a track that never
// had its equaliser turned sounds exactly as it did before there was one. No allocation: safe on the audio thread.
class Eq {
public:
	void reset() { // silence at once; the settings stay
		for (Biquad& b : band)
			b.reset();
	}
	// for the picture: these settings at once (no gliding), and then how loud each frequency comes out, in dB
	void setNow(const float* v, float sr) {
		for (int i = 0; i < 5; i++)
			cur[i] = v[i];
		design(sr);
	}
	float responseDb(float f, float sr) const {
		float db = 0.f;
		for (int b = 0; b < 4; b++)
			if (on[b]) db += band[b].gainDb(f, sr);
		return db;
	}
	inline void process(float* l, float* r, int n, const float* v, float sr) {
		if (eqFlat(v) && eqFlat(cur)) return; // nothing to do, and nothing still gliding back to flat
		for (int i = 0; i < 5; i++) {
			const float d = v[i] - cur[i];
			if (d != 0.f) cur[i] = std::fabs(d) < 0.0005f ? v[i] : cur[i] + d * 0.12f; // about 3 ms
		}
		design(sr);
		for (int b = 0; b < 4; b++) {
			if (!on[b]) continue;
			Biquad& q = band[b];
			// in locals, so the compiler keeps them in registers rather than reading them back for every sample
			const float b0 = q.b0, b1 = q.b1, b2 = q.b2, a1 = q.a1, a2 = q.a2;
			float zl1 = q.z1[0], zl2 = q.z2[0], zr1 = q.z1[1], zr2 = q.z2[1];
			for (int i = 0; i < n; i++) {
				const float xl = l[i], xr = r[i];
				const float yl = b0 * xl + zl1, yr = b0 * xr + zr1;
				zl1 = b1 * xl - a1 * yl + zl2;
				zr1 = b1 * xr - a1 * yr + zr2;
				zl2 = b2 * xl - a2 * yl;
				zr2 = b2 * xr - a2 * yr;
				l[i] = yl;
				r[i] = yr;
			}
			q.z1[0] = zl1;
			q.z2[0] = zl2;
			q.z1[1] = zr1;
			q.z2[1] = zr2;
		}
	}

private:
	// new coefficients for the bands whose settings moved; a band that has become flat forgets what it held
	void design(float sr) {
		const bool want[4] = {cur[0] > 0.f, cur[1] != 0.5f, cur[2] != 0.5f, cur[4] != 0.5f};
		for (int b = 0; b < 4; b++) {
			if (!want[b]) {
				if (on[b]) band[b].reset();
				on[b] = false;
				continue;
			}
			const float a = b == 0 ? cur[0] : b == 1 ? cur[1] : b == 2 ? cur[2] : cur[4], f = b == 2 ? cur[3] : 0.f;
			if (on[b] && a == made[b][0] && f == made[b][1] && sr == madeSr) continue;
			switch (b) {
			case 0:
				band[b].highPass(lowCutHz(a), sr);
				break;
			case 1:
				band[b].lowShelf(EQ_LOW_HZ, eqDb(a), sr);
				break;
			case 2:
				band[b].peak(eqMidHz(f), eqDb(a), EQ_MID_Q, sr);
				break;
			default:
				band[b].highShelf(EQ_HIGH_HZ, eqDb(a), sr);
				break;
			}
			made[b][0] = a;
			made[b][1] = f;
			on[b] = true;
		}
		madeSr = sr;
	}
	float cur[5] = {0.f, 0.5f, 0.5f, 0.5f, 0.5f}; // the settings as they glide
	Biquad band[4];                               // low cut, low, mid, high
	bool on[4] = {};
	float made[4][2] = {}, madeSr = 0.f; // what each band's coefficients were made from
};

// ---------------------------------------------------------------- synth

enum P {
	P_WAVE1,
	P_WAVE2,
	P_TUNE2,
	P_DETUNE,
	P_MIX,
	P_SUB,
	P_NOISE,
	P_PW,
	P_CUTOFF,
	P_RES,
	P_ENVAMT,
	P_ACCENT,
	P_SLOPE,
	P_FA,
	P_FD,
	P_FS,
	P_AA,
	P_AD,
	P_AS,
	P_AR,
	P_MONO,
	P_GLIDE,
	P_LFOWAVE,
	P_LFORATE,
	P_LFOCUT,
	P_LFOPITCH,
	P_DRIVE,
	P_PHASER,
	P_PHRATE,
	P_CHORUS,
	P_DELAY,
	P_REVERB,
	P_UNISON,
	P_UDETUNE,
	P_WIDTH,
	P_FM,
	P_PENV,
	P_KEYTRK,
	P_LFOPW,
	// the LFO in time with the tempo, to the volume and the pan, and its slow fade-in (see lfoBeats(), lfoFadeSec())
	P_LFOSYNC,
	P_LFOAMP,
	P_LFOPAN,
	P_LFOFADE,
	// the filter envelope's own release and the track's equaliser (see Eq below)
	P_FR,     // 0: the filter envelope lets go with the amp's release, as before there was a knob for it
	P_EQCUT,  // low cut: 0 off, else from 20 Hz (lowCutHz)
	P_EQLOW,  // low shelf, 0.5 flat (eqDb)
	P_EQMID,  // bell in the middle, 0.5 flat
	P_EQFREQ, // where that bell sits (eqMidHz)
	P_EQHIGH, // high shelf, 0.5 flat
	P_COUNT
};

inline float envTime(float v) {
	return 0.001f * std::pow(8000.f, v);
} // 1 ms .. 8 s
inline float cutoffHz(float v) {
	return 40.f * std::exp2(v * 9.f);
} // 40 Hz .. 20 kHz
inline float lfoHz(float v) {
	return 0.05f * std::pow(400.f, v);
} // 0.05 .. 20 Hz
// The LFO in time: the rate knob picks a note value instead of a speed. Beats (quarter notes) per cycle, from eight
// bars down to a thirty-second note; the names are LFO_SYNC_NAMES in model.cpp.
constexpr int LFO_SYNC_STEPS = 11;
inline int lfoSyncStep(float v) {
	return std::clamp(int(v * float(LFO_SYNC_STEPS - 1) + 0.5f), 0, LFO_SYNC_STEPS - 1);
}
inline float lfoBeats(float v) {
	static const float beats[LFO_SYNC_STEPS] = {32.f,      16.f, 8.f,       4.f,   2.f,   1.f,
	                                            2.f / 3.f, 0.5f, 1.f / 3.f, 0.25f, 0.125f};
	return beats[lfoSyncStep(v)];
}
// How long the LFO takes to fade in after a note starts (a delayed vibrato): off at 0, up to 5 seconds
inline float lfoFadeSec(float v) {
	return v * v * 5.f;
}
inline float phaserHz(float v) {
	return 0.05f * std::pow(100.f, v);
}
inline float glideSec(float v) {
	return v * v * 0.6f;
}

class Synth {
public:
	static constexpr int NV = 8, NU = 7; // voices, and oscillator copies per voice for unison
	static constexpr int PARTS = 4;      // notes of a chord (Song::chordOffsets)

	void init(float sampleRate) {
		sr = sampleRate;
		chorus.init(int(sr * 0.06f));
		trK = coef(0.0025f, sr); // the tremolo's and the LFO pan's smoothing: a square wave without clicks
		static const float pans[NV] = {0.f, 0.5f, -0.5f, 0.8f, -0.8f, 0.3f, -0.3f, 1.f};
		for (int i = 0; i < NV; i++) {
			voices[i] = Voice();
			voices[i].pan = pans[i];
			for (int u = 1; u < NU; u++) {
				voices[i].o1[u].ph = rng.next() * 0.5f + 0.5f;
				voices[i].o2[u].ph = rng.next() * 0.5f + 0.5f;
			} // unison copies start out of step
		}
		heldN = 0;
	}
	void setPatch(const float* src) { std::memcpy(p, src, sizeof(p)); }
	// The song's tempo and, while it plays, where it is (in beats, from the song's start; -1 when stopped), for an
	// LFO in time. Set before each render(); while stopped the LFO keeps going at the tempo.
	void setClock(double beat, float bpm) {
		clockBeat = beat;
		clockBpm = bpm;
	}

	// part: which note of a chord this is (0 to 3; a single note is 0). A Mono sound gives each part a voice of its
	// own, so a chord plays as a chord and glides as one; with glide, a part glides from the same part's last note.
	void noteOn(int key, int midi, bool acc, int part = 0) {
		part = std::clamp(part, 0, PARTS - 1);
		const float f = 440.f * std::exp2(float(midi - 69) / 12.f);
		const float gl = glideSec(p[P_GLIDE]);
		if (p[P_MONO] > 0.5f) {
			Voice& v = voices[part];
			bool partHeld = false;
			for (int i = 0; i < heldN; i++)
				partHeld = partHeld || held[i].part == part;
			const bool legato = v.gate && partHeld;
			pushHeld(key, midi, acc, part);
			v.key = key;
			v.midi = midi;
			v.target = f;
			if (legato) { // a slide: no new attack, but an accent on the note slid into still counts, as on a 303
				v.glide = std::max(gl, 0.06f);
				v.acc = acc;
				if (acc) v.accEnv = 1.f;
			} else {
				v.glide = gl;
				if (gl < 0.003f || !v.ae.active()) v.freq = f;
				v.acc = acc;
				v.ae.on();
				v.fe.on();
				v.age = ++clock;
				if (acc) v.accEnv = 1.f;
				v.lfoAge = 0.f; // a new note, not a slide: the LFO's fade-in starts again
				lfoAgeAll = 0.f;
			}
			v.gate = true;
			return;
		}
		Voice* best = nullptr;
		for (auto& v : voices)
			if (!v.ae.active()) {
				best = &v;
				break;
			}
		if (!best) {
			for (auto& v : voices)
				if (!v.gate && (!best || v.age < best->age)) best = &v;
			if (!best) {
				best = &voices[0];
				for (auto& v : voices)
					if (v.age < best->age) best = &v;
			}
		}
		bool anyHeld = false;
		for (auto& o : voices)
			anyHeld |= o.gate;
		if (!anyHeld) lfoAgeAll = 0.f; // a new phrase: the LFO's fade-in starts again for the tremolo and pan
		Voice& v = *best;
		v.lfoAge = 0.f;
		v.key = key;
		v.midi = midi;
		v.target = f;
		v.glide = gl;
		if (gl < 0.003f)
			v.freq = f;
		else
			v.freq = lastFreq[part] > 0.f ? lastFreq[part] : f;
		lastFreq[part] = f;
		v.acc = acc;
		v.gate = true;
		v.ae.on();
		v.fe.on();
		v.age = ++clock;
		if (acc) v.accEnv = 1.f;
		if (v.ae.v < 0.001f) {
			v.f.reset();
			v.accCv = 0.f;
		}
	}

	void noteOff(int key) {
		// the mono mode's list of held notes forgets this one, whatever the mode is now
		int w = 0, part = -1;
		for (int i = 0; i < heldN; i++) {
			if (held[i].key != key)
				held[w++] = held[i];
			else
				part = held[i].part;
		}
		heldN = w;
		if (p[P_MONO] > 0.5f) {
			// voices still sounding from before the sound was switched to mono (a preset, Roll new sound, Play: Mono)
			for (int i = 1; i < NV; i++)
				if (i != part && voices[i].gate && voices[i].key == key) {
					voices[i].gate = false;
					voices[i].ae.off();
					voices[i].fe.off();
				}
			Voice& v = voices[std::max(part, 0)];
			if (v.key != key || !v.gate) return;
			int last = heldN - 1; // fall back to the part's note still held
			while (last >= 0 && held[last].part != std::max(part, 0))
				last--;
			if (last >= 0) {
				const Held& h = held[last];
				v.key = h.key;
				v.midi = h.midi;
				v.acc = h.acc;
				v.target = 440.f * std::exp2(float(h.midi - 69) / 12.f);
				v.glide = std::max(glideSec(p[P_GLIDE]), 0.06f);
			} else {
				v.gate = false;
				v.ae.off();
				v.fe.off();
			}
			return;
		}
		for (auto& v : voices)
			if (v.gate && v.key == key) {
				v.gate = false;
				v.ae.off();
				v.fe.off();
			}
	}

	void allOff() {
		heldN = 0;
		for (auto& v : voices) {
			v.gate = false;
			v.ae.off();
			v.fe.off();
		}
	}

	bool silent() const {
		for (auto& v : voices)
			if (v.ae.active()) return false;
		return true;
	}
	void reset() { // silence at once, no release tails
		heldN = 0;
		for (auto& v : voices) {
			v.gate = false;
			v.ae.v = 0.f;
			v.ae.st = 0;
			v.fe.v = 0.f;
			v.fe.st = 0;
			v.f.reset();
		}
		eq.reset();
	}

	// Renders n (<= CHUNK) samples of stereo output, replacing l and r.
	void render(float* l, float* r, int n) {
		float mono[CHUNK], side[CHUNK];
		for (int i = 0; i < n; i++)
			mono[i] = side[i] = 0.f;
		const float crate = sr / float(n);

		// LFO, once per chunk: at its own speed, or in time with the song
		if (p[P_LFOSYNC] < 0.5f) {
			lfoPh += lfoHz(p[P_LFORATE]) / crate;
			if (lfoPh >= 1.f) {
				lfoPh -= 1.f;
				sh = rng.next();
			}
		} else
			lfoInTime(crate);
		const float fadeT = lfoFadeSec(p[P_LFOFADE]); // 0: no fade-in, the LFO at full depth from the start
		const int lw = int(p[P_LFOWAVE] + 0.5f);
		const float lfo = lw == 0 ? 1.f - 4.f * std::fabs(lfoPh - 0.5f) : lw == 1 ? (lfoPh < 0.5f ? 1.f : -1.f) : sh;

		const int w1 = int(p[P_WAVE1] + 0.5f), w2 = int(p[P_WAVE2] + 0.5f);
		static const float tune2[4] = {-12.f, 0.f, 7.f, 12.f};
		const float ratio2 = std::exp2((tune2[std::clamp(int(p[P_TUNE2] + 0.5f), 0, 3)] + p[P_DETUNE] * 0.4f) / 12.f);
		const float pitchMod = std::exp2(lfo * p[P_LFOPITCH] * p[P_LFOPITCH] * 100.f / 1200.f);
		const float k = p[P_RES] * 4.15f;
		const float comp = 0.5f * (1.f + 0.7f * k); // resonance thins the bass of a ladder: make most of it up
		const float g1 = std::min(1.f, 2.f * (1.f - p[P_MIX])), g2 = std::min(1.f, 2.f * p[P_MIX]);
		const float gs = p[P_SUB], gn = p[P_NOISE] * 0.6f;
		const float pw = std::clamp(0.5f - 0.42f * p[P_PW] + lfo * p[P_LFOPW] * 0.3f, 0.05f, 0.95f);
		const int mode = std::clamp(int(p[P_SLOPE] + 0.5f), 0, 4);
		const float fm = p[P_FM] * p[P_FM] * 1.5f, width = p[P_WIDTH];
		const float caA = coef(envTime(p[P_AA]) / 1.47f, sr), cdA = coef(envTime(p[P_AD]) / 3.f, sr),
		            crA = coef(envTime(p[P_AR]) / 3.f, sr);
		const float caF = coef(envTime(p[P_FA]) / 1.47f, crate), cdF = coef(envTime(p[P_FD]) / 3.f, crate),
		            crF = coef(envTime(p[P_FR] > 0.f ? p[P_FR] : p[P_AR]) / 3.f, crate);
		const float susA = p[P_AS], susF = p[P_FS];
		const float accDecay = std::exp(-1.f / (0.2f * crate)), accRise = coef(0.012f, crate),
		            accFall = coef(0.35f, crate);
		// unison: several copies of both oscillators, spread in pitch
		static const int ucount[5] = {1, 2, 3, 5, 7};
		const int nu = ucount[std::clamp(int(p[P_UNISON] + 0.5f), 0, 4)];
		float urat[NU];
		for (int u = 0; u < nu; u++)
			urat[u] = std::exp2((nu > 1 ? float(u) / float(nu - 1) * 2.f - 1.f : 0.f) * p[P_UDETUNE] * 0.5f / 12.f);
		const float ugain = 1.f / std::sqrt(float(nu));
		const bool need2 = g2 > 0.f || fm > 0.f;

		for (auto& v : voices) {
			if (!v.ae.active()) continue;
			if (v.glide >= 0.003f)
				v.freq += (v.target - v.freq) * coef(v.glide / 3.f, crate);
			else
				v.freq = v.target;
			const float fenv = v.fe.tick(caF, cdF, susF, crF);
			const float accF = v.acc ? 1.f + p[P_ACCENT] * 0.3f : 1.f;
			// the 303's accent: a short, fixed envelope on each accented note, fed through a capacitor that charges
			// quickly and lets go slowly, so accents close together pile up and push the filter higher each time
			v.accEnv *= accDecay;
			v.accCv += (v.accEnv - v.accCv) * (v.accEnv > v.accCv ? accRise : accFall);
			const float accSweep = p[P_ACCENT] * v.accCv * (1.2f + 1.8f * p[P_RES]); // more wow with more resonance
			// the LFO as this voice hears it: faded in since its note started, when a fade-in is set
			float vl = lfo, vpitch = pitchMod, vpw = pw;
			if (fadeT > 0.f) {
				vl = lfo * std::min(1.f, v.lfoAge / fadeT);
				v.lfoAge = std::min(v.lfoAge + 1.f / crate, 1000.f);
				vpitch = std::exp2(vl * p[P_LFOPITCH] * p[P_LFOPITCH] * 100.f / 1200.f);
				vpw = std::clamp(0.5f - 0.42f * p[P_PW] + vl * p[P_LFOPW] * 0.3f, 0.05f, 0.95f);
			}
			const float oct = p[P_CUTOFF] * 9.f + p[P_ENVAMT] * 5.f * accF * fenv + accSweep + vl * p[P_LFOCUT] * 2.f +
			                  float(v.midi - 57) / 12.f * p[P_KEYTRK];
			const float fc = std::clamp(40.f * std::exp2(oct), 20.f, sr * 0.45f);
			const float gT = std::tan(PI * fc / sr);
			const float gInc = (gT - v.g) / float(n);
			const float base =
			    v.freq * vpitch * std::exp2(p[P_PENV] * fenv * 2.f) / sr; // pitch envelope: up to two octaves
			float d1[NU], d2[NU];
			for (int u = 0; u < nu; u++) {
				d1[u] = std::min(0.45f, base * urat[u]);
				d2[u] = std::min(0.45f, d1[u] * ratio2);
			}
			const float dts = std::min(0.45f, base * 0.5f);
			const float amp = 0.75f * (v.acc ? 1.f + p[P_ACCENT] * 0.5f : 1.f);
			for (int i = 0; i < n; i++) {
				v.g += gInc;
				float x = 0.f;
				for (int u = 0; u < nu; u++) {
					const float b = need2 ? v.o2[u].tick(w2, d2[u], vpw) : 0.f;
					const float a = fm > 0.f ? v.o1[u].tickPM(w1, d1[u], vpw, b * fm) : v.o1[u].tick(w1, d1[u], vpw);
					x += a * g1 + b * g2;
				}
				x *= ugain;
				if (gs > 0.f) x += v.sub.tick(2, dts, 0.5f) * gs;
				if (gn > 0.f) x += rng.next() * gn;
				const float y = v.f.process(x * comp, v.g, k, mode) * v.ae.tick(caA, cdA, susA, crA) * amp;
				mono[i] += y;
				side[i] += y * v.pan * width;
			}
		}

		// drive
		const float pre = 1.f + p[P_DRIVE] * p[P_DRIVE] * 24.f, post = 1.f / (1.f + p[P_DRIVE] * 1.8f);
		for (int i = 0; i < n; i++)
			mono[i] = fastTanh(mono[i] * pre) * post;

		// phaser
		if (p[P_PHASER] > 0.001f) {
			phPh += phaserHz(p[P_PHRATE]) / crate;
			if (phPh >= 1.f) phPh -= 1.f;
			const float f = 300.f * std::exp2(1.6f + 1.6f * std::sin(TAU * phPh));
			const float t = std::tan(PI * std::min(f, sr * 0.4f) / sr), a = (t - 1.f) / (t + 1.f);
			const float wet = p[P_PHASER], fb = 0.55f * wet;
			for (int i = 0; i < n; i++) {
				float x = mono[i] + phLast * fb;
				for (int s = 0; s < 4; s++) {
					const float y = a * x + apx[s] - a * apy[s];
					apx[s] = x;
					apy[s] = y;
					x = y;
				}
				phLast = x;
				mono[i] = mono[i] + x * wet;
			}
			const float norm = 1.f / (1.f + 0.5f * wet);
			for (int i = 0; i < n; i++)
				mono[i] *= norm;
		}

		// chorus (mono in, stereo out), then the stereo spread of the voices
		const float cw = p[P_CHORUS] * 0.9f;
		chPh1 += 0.6f / crate;
		if (chPh1 >= 1.f) chPh1 -= 1.f;
		chPh2 += 0.83f / crate;
		if (chPh2 >= 1.f) chPh2 -= 1.f;
		const float c1 = sr * (0.011f + 0.0028f * std::sin(TAU * chPh1)),
		            c2 = sr * (0.017f + 0.003f * std::sin(TAU * chPh2));
		const float dd1 = (c1 - chD1) / float(n), dd2 = (c2 - chD2) / float(n);
		for (int i = 0; i < n; i++) {
			chD1 += dd1;
			chD2 += dd2;
			chorus.write(mono[i]);
			l[i] = mono[i] + cw * chorus.read(chD1) - side[i] * post;
			r[i] = mono[i] + cw * chorus.read(chD2) + side[i] * post;
		}
		eq.process(l, r, n, p + P_EQCUT, sr); // as on a mixing desk's channel, before the pan
		lfoToVolumeAndPan(l, r, n, lfo, fadeT, crate);
	}

private:
	// The LFO in time with the song: its place in the cycle follows the song's position while it plays, so a
	// one-bar sweep starts on the bar; while stopped it runs on at the tempo.
	void lfoInTime(float crate) {
		if (clockBeat >= 0.0)
			lfoBeat = clockBeat;
		else
			lfoBeat += double(clockBpm) / 60.0 / double(crate);
		const double cycles = lfoBeat / double(lfoBeats(p[P_LFORATE]));
		const float ph = float(cycles - std::floor(cycles));
		if (ph < lfoPh) sh = rng.next(); // a new cycle: a new random step
		lfoPh = ph;
	}
	// The LFO to the volume (tremolo) and to the pan, after everything else. The gains glide over a few
	// milliseconds, so a square wave chops without clicks. Nothing is done while both are off and settled.
	void lfoToVolumeAndPan(float* l, float* r, int n, float lfo, float fadeT, float crate) {
		const float amp = p[P_LFOAMP], pan = p[P_LFOPAN];
		const float fade = fadeT > 0.f ? std::min(1.f, lfoAgeAll / fadeT) : 1.f;
		lfoAgeAll = std::min(lfoAgeAll + 1.f / crate, 1000.f);
		if (amp <= 0.f && pan <= 0.f) {
			if (trL == 1.f && trR == 1.f) return;
			if (std::fabs(trL - 1.f) < 1e-4f && std::fabs(trR - 1.f) < 1e-4f) {
				trL = trR = 1.f;
				return;
			}
		}
		const float g = 1.f - amp * fade * 0.5f * (1.f + lfo); // down on the LFO's peaks, full on its troughs
		const float x = pan * fade * lfo;
		const float wl = g * std::min(1.f, 1.f - x), wr = g * std::min(1.f, 1.f + x);
		for (int i = 0; i < n; i++) {
			trL += (wl - trL) * trK;
			trR += (wr - trR) * trK;
			l[i] *= trL;
			r[i] *= trR;
		}
	}
	struct Voice {
		Osc o1[NU], o2[NU], sub;
		Ladder f;
		Env ae, fe;
		float freq = 220.f, target = 220.f, glide = 0.f, g = 0.05f, pan = 0.f;
		float lfoAge = 0.f;              // seconds since this voice's note started, for the LFO's fade-in
		float accEnv = 0.f, accCv = 0.f; // the accent's envelope and the charge it leaves behind
		int key = -1, midi = 57;
		bool gate = false, acc = false;
		uint32_t age = 0;
	};
	struct Held {
		int key, midi;
		bool acc;
		int part;
	};
	void pushHeld(int key, int midi, bool acc, int part) {
		if (heldN == 16) {
			for (int i = 1; i < 16; i++)
				held[i - 1] = held[i];
			heldN = 15;
		}
		held[heldN++] = {key, midi, acc, part};
	}
	float sr = 48000.f, p[P_COUNT] = {};
	Voice voices[NV];
	Held held[16];
	int heldN = 0;
	uint32_t clock = 0;
	float lastFreq[PARTS] = {}; // each part's last note, for a Poly sound's glide
	Rng rng;
	float lfoPh = 0.f, sh = 0.f;
	double clockBeat = -1.0, lfoBeat = 0.0;
	float clockBpm = 120.f, lfoAgeAll = 0.f, trL = 1.f, trR = 1.f, trK = 0.01f;
	float phPh = 0.f, phLast = 0.f, apx[4] = {}, apy[4] = {};
	DelayLine chorus;
	float chPh1 = 0.f, chPh2 = 0.33f, chD1 = 500.f, chD2 = 800.f;
	Eq eq;
};

// ---------------------------------------------------------------- drums

// A sound file loaded into memory, for a drum to play instead of its own sound. Never changed once loaded.
struct SampleData {
	std::vector<float> l, r; // the same data in both for a mono file
	float rate = 48000.f;
	size_t frames() const { return l.size(); }
};

enum { D_KICK, D_SNARE, D_CLAP, D_CHAT, D_OHAT, D_TOM, D_RIM, D_CYM, D_COUNT };
// drum parameter layout: for each sound [level, tune, decay], then drive, echo send, reverb send, and the kit's
// equaliser (low cut, low, mid, the mid's pitch, high: as the synthesiser's P_EQCUT to P_EQHIGH)
enum { DP_DRIVE = D_COUNT * 3, DP_DELAY, DP_REVERB, DP_EQCUT, DP_EQLOW, DP_EQMID, DP_EQFREQ, DP_EQHIGH, DP_COUNT };

class Drums {
public:
	void init(float sampleRate) {
		sr = sampleRate;
		for (auto& v : t)
			v = -1.f;
		snHp.set(1800.f, 0.7f, sr);
		clBp.set(1100.f, 1.6f, sr);
		hatHp.set(7500.f, 0.9f, sr);
		ohHp.set(7000.f, 0.9f, sr);
		cyHp.set(5200.f, 0.8f, sr);
		rimBp.set(1900.f, 3.f, sr);
	}
	void setParams(const float* src) { std::memcpy(p, src, sizeof(p)); }
	// Which drums play a sound file instead of their own sound (nullptr: built in). A drum whose file changed stops,
	// so it never reads a file that may already be gone.
	void setSamples(const SampleData* const* s) {
		for (int d = 0; d < D_COUNT; d++)
			if (s[d] != smp[d]) {
				if (smp[d])
					t[d] = -1.f; // it was reading the old file: stop. A built-in hit may carry on as the new sample,
					             // from its start
				else
					spos[d] = 0.0;
				smp[d] = s[d];
			}
	}
	void trigger(int which, bool acc) {
		if (which < 0 || which >= D_COUNT) return;
		t[which] = 0.f;
		vel[which] = acc ? 1.f : 0.72f;
		ph[which] = 0.f;
		spos[which] = 0.0;
		if (which == D_CHAT) t[D_OHAT] = -1.f; // closed hat chokes the open hat
	}
	bool silent() const {
		for (float v : t)
			if (v >= 0.f) return false;
		return true;
	}
	void reset() {
		for (auto& v : t)
			v = -1.f;
		eq.reset();
	}

	void render(float* l, float* r, int n) {
		const float dt = 1.f / sr;
		for (int i = 0; i < n; i++) {
			float L = 0.f, R = 0.f, lp, bp, hp;
			auto lev = [&](int d) { return p[d * 3] * p[d * 3] * 0.95f * vel[d]; };
			auto tune = [&](int d) { return std::exp2((p[d * 3 + 1] - 0.5f) * 1.6f); };
			auto dec = [&](int d) { return p[d * 3 + 2]; };
			auto done = [&](int d, float tau) {
				t[d] += dt;
				if (t[d] > tau * 7.f) t[d] = -1.f;
			};

			// drums playing a sound file: tune changes its speed (an octave either way), decay shortens it
			for (int d = 0; d < D_COUNT; d++) {
				const SampleData* sd = smp[d];
				if (!sd || t[d] < 0.f) continue;
				const size_t nf = sd->frames();
				const size_t k = size_t(spos[d]);
				if (k + 1 >= nf) {
					t[d] = -1.f;
					continue;
				}
				const float fr = float(spos[d] - double(k));
				const float sl = sd->l[k] + (sd->l[k + 1] - sd->l[k]) * fr,
				            sr2 = sd->r[k] + (sd->r[k + 1] - sd->r[k]) * fr;
				const float dv = dec(d);
				float env = 1.f;
				if (dv < 0.97f) {
					const float tau = 0.02f * std::pow(150.f, dv);
					env = std::exp(-t[d] / tau);
					if (t[d] > tau * 7.f) {
						t[d] = -1.f;
						continue;
					}
				}
				const float g = lev(d) * env * 1.1f;
				L += sl * g;
				R += sr2 * g;
				spos[d] += double(sd->rate) / double(sr) * double(std::exp2((p[d * 3 + 1] - 0.5f) * 2.f));
				t[d] += dt;
			}

			if (t[D_KICK] >= 0.f && !smp[D_KICK]) {
				const float tt = t[D_KICK], tau = 0.07f + dec(D_KICK) * 0.5f;
				const float f =
				    46.f * tune(D_KICK) * (1.f + 4.5f * std::exp(-tt / 0.012f) + 0.7f * std::exp(-tt / 0.07f));
				ph[D_KICK] += f * dt;
				float s =
				    std::sin(TAU * ph[D_KICK]) * std::exp(-tt / tau) + rng.next() * 0.25f * std::exp(-tt / 0.0015f);
				s = fastTanh(s * 1.25f) * lev(D_KICK);
				L += s;
				R += s;
				done(D_KICK, tau);
			}
			if (t[D_SNARE] >= 0.f && !smp[D_SNARE]) {
				const float tt = t[D_SNARE], tau = 0.05f + dec(D_SNARE) * 0.28f, tn = tune(D_SNARE);
				ph[D_SNARE] += 185.f * tn * (1.f + 0.6f * std::exp(-tt / 0.01f)) * dt;
				const float tone = (std::sin(TAU * ph[D_SNARE]) + 0.6f * std::sin(TAU * ph[D_SNARE] * 1.78f)) *
				                   std::exp(-tt / 0.06f) * 0.6f;
				snHp.tick(rng.next(), lp, bp, hp);
				const float s = (tone + hp * 0.8f * std::exp(-tt / tau)) * lev(D_SNARE);
				L += s;
				R += s;
				done(D_SNARE, std::max(tau, 0.06f));
			}
			if (t[D_CLAP] >= 0.f && !smp[D_CLAP]) {
				const float tt = t[D_CLAP], tau = 0.04f + dec(D_CLAP) * 0.25f;
				clBp.tick(rng.next(), lp, bp, hp);
				const float e = tt < 0.03f ? std::exp(-std::fmod(tt, 0.01f) / 0.004f) : std::exp(-(tt - 0.03f) / tau);
				const float s = bp * e * 1.6f * lev(D_CLAP);
				L += s * 0.9f;
				R += s * 1.1f;
				done(D_CLAP, tau + 0.01f);
			}
			if ((t[D_CHAT] >= 0.f && !smp[D_CHAT]) || (t[D_OHAT] >= 0.f && !smp[D_OHAT])) {
				static const float fr[6] = {205.3f, 304.4f, 369.6f, 522.7f, 540.f, 800.f};
				// six square waves each, every one with its own phase kept in 0..1, so the pitch stays put however long
				// the song runs
				auto metal = [&](float* phs, float mul) {
					float m = 0.f;
					for (int k = 0; k < 6; k++) {
						phs[k] += fr[k] * mul * dt;
						phs[k] -= std::floor(phs[k]);
						m += phs[k] < 0.5f ? 1.f : -1.f;
					}
					return m;
				};
				if (t[D_CHAT] >= 0.f && !smp[D_CHAT]) {
					const float tt = t[D_CHAT], tau = 0.012f + dec(D_CHAT) * 0.07f, tn = tune(D_CHAT) * 2.f;
					const float m = metal(chPh, tn);
					hatHp.tick(m * 0.16f + rng.next() * 0.3f, lp, bp, hp);
					const float s = hp * std::exp(-tt / tau) * lev(D_CHAT);
					L += s * 1.15f;
					R += s * 0.85f;
					done(D_CHAT, tau);
				}
				if (t[D_OHAT] >= 0.f && !smp[D_OHAT]) {
					const float tt = t[D_OHAT], tau = 0.08f + dec(D_OHAT) * 0.6f, tn = tune(D_OHAT) * 2.f;
					const float m = metal(ohPh, tn * 1.01f);
					ohHp.tick(m * 0.16f + rng.next() * 0.25f, lp, bp, hp);
					const float s = hp * std::exp(-tt / tau) * lev(D_OHAT);
					L += s * 0.85f;
					R += s * 1.15f;
					done(D_OHAT, tau);
				}
			}
			if (t[D_TOM] >= 0.f && !smp[D_TOM]) {
				const float tt = t[D_TOM], tau = 0.1f + dec(D_TOM) * 0.45f;
				ph[D_TOM] += 92.f * tune(D_TOM) * (1.f + 0.9f * std::exp(-tt / 0.03f)) * dt;
				const float s = std::sin(TAU * ph[D_TOM]) * std::exp(-tt / tau) * lev(D_TOM);
				L += s * 1.2f;
				R += s * 0.8f;
				done(D_TOM, tau);
			}
			if (t[D_RIM] >= 0.f && !smp[D_RIM]) {
				const float tt = t[D_RIM], tau = 0.008f + dec(D_RIM) * 0.05f, tn = tune(D_RIM);
				ph[D_RIM] += 1700.f * tn * dt;
				rimBp.tick(rng.next() * std::exp(-tt / 0.002f), lp, bp, hp);
				const float s =
				    (std::sin(TAU * ph[D_RIM]) * 0.6f + std::sin(TAU * ph[D_RIM] * 0.29f) * 0.5f + bp * 2.f) *
				    std::exp(-tt / tau) * lev(D_RIM);
				L += s * 0.8f;
				R += s * 1.1f;
				done(D_RIM, tau);
			}
			if (t[D_CYM] >= 0.f && !smp[D_CYM]) {
				const float tt = t[D_CYM], tau = 0.25f + dec(D_CYM) * 1.6f, tn = tune(D_CYM) * 2.3f;
				static const float fr2[6] = {263.f, 400.f, 421.f, 474.f, 587.f, 845.f};
				float m = 0.f;
				for (int k = 0; k < 6; k++) {
					cyPh[k] += fr2[k] * tn * dt;
					cyPh[k] -= std::floor(cyPh[k]);
					m += cyPh[k] < 0.5f ? 1.f : -1.f;
				}
				cyHp.tick(m * 0.14f + rng.next() * 0.35f, lp, bp, hp);
				const float s = hp * (std::exp(-tt / tau) * 0.7f + std::exp(-tt / 0.03f) * 0.5f) * lev(D_CYM);
				L += s * 0.8f;
				R += s * 1.2f;
				done(D_CYM, tau);
			}
			const float pre = 1.f + p[DP_DRIVE] * p[DP_DRIVE] * 12.f, post = 1.f / (1.f + p[DP_DRIVE] * 1.2f);
			// half-scale into the shaper: clean at normal levels, drive only when the knob asks for it
			l[i] = fastTanh(L * pre * 0.5f) * post * 2.f;
			r[i] = fastTanh(R * pre * 0.5f) * post * 2.f;
		}
		eq.process(l, r, n, p + DP_EQCUT, sr);
	}

private:
	float sr = 48000.f, p[DP_COUNT] = {};
	float t[D_COUNT], vel[D_COUNT] = {}, ph[D_COUNT] = {};
	const SampleData* smp[D_COUNT] = {};
	double spos[D_COUNT] = {};
	float chPh[6] = {}, ohPh[6] = {}, cyPh[6] = {}; // phases of the metallic square waves
	SVF snHp, clBp, hatHp, ohHp, cyHp, rimBp;
	Rng rng;
	Eq eq;
};

// ---------------------------------------------------------------- global effects

// The shared echo. Ping-pong: the repeats bounce from the left to the right speaker; with spread 1 they all come
// from the middle instead. Each repeat goes through a gentle low-pass (tone: 0.32 is the classic) and high-pass, so
// the repeats grow darker and thinner, and through a soft limit that leaves anything below full scale untouched, so
// the echo can never run away. A small change of time (the tempo nudged) glides like a tape echo; a jump (another
// note value) crossfades from the old time to the new one, without a click or a swoop.
class PingPong {
public:
	void init(float sampleRate) {
		sr = sampleRate;
		// 2.6 seconds, as ever: the lines' length decides how a read across their end is rounded, so another length
		// would change older songs by a hair. They play every echo up to 2.5 seconds; a longer one (a half note
		// below 48 BPM, down to 3 seconds at 40 BPM, the slowest tempo) is read from a longer pair that is written
		// with the same samples, so the two hold the same echoes and a time moving across 2.5 seconds does not click.
		a.init(int(sr * 2.6f));
		b.init(int(sr * 2.6f));
		aLong.init(int(sr * 3.1f));
		bLong.init(int(sr * 3.1f));
		cur = sr * 0.3f;
	}
	inline void process(float in, float timeSamples, float fb, float& outL, float& outR) {
		process(in, timeSamples, fb, 0.32f, 0.f, outL, outR);
	}
	inline void process(float in, float timeSamples, float fb, float tone, float spread, float& outL, float& outR) {
		if (timeSamples != want) {
			if (started && std::fabs(timeSamples - want) > want * 0.04f) jump = true;
			want = timeSamples;
		}
		started = true;
		if (jump && fadeLeft == 0) { // crossfade from where the echo is now to the new time
			from = std::clamp(cur, 8.f, sr * 3.05f);
			cur = want;
			fadeLeft = FADE;
			jump = false;
		}
		cur += (want - cur) * 0.0004f;
		const float d = std::clamp(cur, 8.f, sr * 3.05f);
		float ra = readA(d), rb = readB(d);
		if (fadeLeft > 0) {
			const float x = float(fadeLeft--) / float(FADE); // 1 -> 0: the old time fades out
			ra += (readA(from) - ra) * x;
			rb += (readB(from) - rb) * x;
		}
		mono += (spread - mono) * 0.002f; // switching ping-pong glides over a few milliseconds
		if (std::fabs(mono - spread) < 1e-5f) mono = spread;
		const float src = rb + (ra - rb) * mono; // ping-pong feeds the right side back, a plain echo the left
		lp += (src - lp) * tone;
		hpS += (lp - hpS) * 0.02f;
		const float back = softLimit(in + (lp - hpS) * fb);
		a.write(back);
		b.write(ra);
		aLong.write(back);
		bLong.write(ra);
		const float mid = ra * 0.70710678f; // a plain echo in both speakers, as loud as one ping-pong side
		outL = ra + (mid - ra) * mono;
		outR = rb + (mid - rb) * mono;
	}
	void clear() { // the echoes fall silent at once
		a.clear();
		b.clear();
		aLong.clear();
		bLong.clear();
		lp = hpS = 0.f;
		fadeLeft = 0;
	}
	// below full scale nothing changes; above, it bends smoothly towards twice full scale and never further
	static inline float softLimit(float x) {
		const float ax = std::fabs(x);
		if (ax <= 1.f) return x;
		const float over = ax - 1.f;
		return std::copysign(1.f + over / (1.f + over), x);
	}

private:
	inline float readA(float d) const { return d <= sr * 2.5f ? a.read(d) : aLong.read(d); }
	inline float readB(float d) const { return d <= sr * 2.5f ? b.read(d) : bLong.read(d); }
	static constexpr int FADE = 2400; // samples a jump in time takes to crossfade (50 ms at 48 kHz)
	float sr = 48000.f, cur = 1000.f, lp = 0.f, hpS = 0.f;
	float want = 0.f, from = 0.f, mono = 0.f;
	int fadeLeft = 0;
	bool started = false, jump = false;
	DelayLine a, b, aLong, bLong;
};

// How dark the echo's repeats are: 0.5 is the classic tone (a gentle low-pass near 3 kHz), darker to the left (down
// to about 300 Hz), brighter to the right (almost open). Returns the low-pass's coefficient for PingPong::process.
inline float echoToneCoef(float v) {
	return std::min(1.f, 0.32f * std::exp2((v - 0.5f) * (v < 0.5f ? 6.f : 3.2f)));
}
// The note values the echo's time can take: beats (quarter notes) per repeat. The first five are those of older
// songs and keep their numbers; the rest were added after, so the list shows them in another order (model.cpp).
constexpr int ECHO_TIMES = 8;
inline float echoBeats(int i) {
	static const float beats[ECHO_TIMES] = {0.25f, 0.5f, 0.75f, 1.f, 1.5f, 1.f / 3.f, 2.f / 3.f, 2.f};
	return beats[std::clamp(i, 0, ECHO_TIMES - 1)];
}

// Eight-line feedback delay network with input diffusion.
class Reverb {
public:
	void init(float sampleRate) {
		sr = sampleRate;
		static const int len[8] = {1423, 1559, 1693, 1811, 1949, 2083, 2239, 2377};
		for (int i = 0; i < 8; i++) {
			n[i] = int(float(len[i]) * sr / 48000.f);
			line[i].init(n[i] + 8);
			damp[i] = 0.f;
		}
		static const int ap[4] = {142, 107, 379, 277};
		for (int i = 0; i < 4; i++) {
			apN[i] = int(float(ap[i]) * sr / 48000.f);
			apl[i].init(apN[i] + 4);
		}
		pre.init(int(sr * 0.03f));
		setSize(0.5f, 0.5f);
	}
	void setSize(float size, float damping) {
		const float t60 = 0.5f + size * size * 11.f;
		for (int i = 0; i < 8; i++)
			fbk[i] = std::pow(10.f, -3.f * float(n[i]) / (sr * t60));
		dc = 0.75f - damping * 0.62f;
	}
	inline void process(float inL, float inR, float& outL, float& outR) {
		pre.write((inL + inR) * 0.5f);
		float x = pre.tap(int(sr * 0.02f));
		float xl = x + (inL - inR) * 0.25f, xr = x - (inL - inR) * 0.25f;
		xl = allpass(0, allpass(1, xl));
		xr = allpass(2, allpass(3, xr));
		float v[8];
		for (int i = 0; i < 8; i++) {
			const float o = line[i].tap(n[i]);
			damp[i] += (o - damp[i]) * dc;
			v[i] = damp[i] * fbk[i];
		}
		// Hadamard mix
		for (int h = 1; h < 8; h <<= 1)
			for (int i = 0; i < 8; i += h << 1)
				for (int j = i; j < i + h; j++) {
					const float a = v[j], b = v[j + h];
					v[j] = a + b;
					v[j + h] = a - b;
				}
		const float s = 0.35355339f;
		for (int i = 0; i < 8; i++)
			line[i].write(v[i] * s + ((i & 1) ? xr : xl) * 0.5f);
		outL = (damp[0] - damp[2] + damp[4] - damp[6]) * 0.5f;
		outR = (damp[1] - damp[3] + damp[5] - damp[7]) * 0.5f;
	}
	void clear() { // the room falls silent at once
		for (auto& l : line)
			l.clear();
		for (auto& l : apl)
			l.clear();
		pre.clear();
		for (float& d : damp)
			d = 0.f;
	}

private:
	inline float allpass(int i, float x) {
		const float d = apl[i].tap(apN[i]);
		const float y = d - 0.6f * x;
		apl[i].write(x + 0.6f * y);
		return y;
	}
	float sr = 48000.f, fbk[8] = {}, damp[8] = {}, dc = 0.4f;
	int n[8] = {}, apN[4] = {};
	DelayLine line[8], apl[4], pre;
};

struct Limiter {
	float gain = 1.f;
	inline void process(float& l, float& r, float release) {
		const float pk = std::max(std::fabs(l), std::fabs(r)) * gain;
		if (pk > 0.9f)
			gain *= 0.9f / pk;
		else
			gain += (1.f - gain) * release;
		l = std::clamp(l * gain, -1.f, 1.f);
		r = std::clamp(r * gain, -1.f, 1.f);
	}
};

} // namespace dsp
