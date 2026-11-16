// Makes the Nachtzug sample library: every sound is synthesized here, from oscillators, noise and filters.
// Nothing is recorded or copied from anywhere.
//
//   g++ -O2 -std=c++20 -Isrc tools/samplegen/samplegen.cpp -o build/samplegen && build/samplegen assets/samples
//
// The same program always writes the same files (the random numbers are seeded), so the library can be made again
// and changed sound by sound. Drums and one-shots are 48 kHz mono, the long textures 24 kHz stereo, all 16-bit.
#include "dsp.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace fs = std::filesystem;
static constexpr double TAU_D = 6.283185307179586;

// ---------------------------------------------------------------- building blocks

struct Rand {
	uint64_t s;
	explicit Rand(uint64_t seed) : s(seed * 0x9E3779B97F4A7C15ull + 1) {}
	double next() {
		s ^= s << 13;
		s ^= s >> 7;
		s ^= s << 17;
		return double(s >> 11) / double(1ull << 53);
	} // 0..1
	double bi() { return next() * 2.0 - 1.0; } // -1..1
};

struct Pink { // Paul Kellet's filter: white noise in, pink noise out
	double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
	double tick(double w) {
		b0 = 0.99886 * b0 + w * 0.0555179;
		b1 = 0.99332 * b1 + w * 0.0750759;
		b2 = 0.96900 * b2 + w * 0.1538520;
		b3 = 0.86650 * b3 + w * 0.3104856;
		b4 = 0.55000 * b4 + w * 0.5329522;
		b5 = -0.7616 * b5 - w * 0.0168980;
		const double p = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362;
		b6 = w * 0.115926;
		return p * 0.11;
	}
};

// state-variable filter whose cutoff may change every sample
struct Svf {
	double ic1 = 0, ic2 = 0, sr = 48000;
	explicit Svf(double rate = 48000) : sr(rate) {}
	enum Mode { LP, BP, HP };
	double tick(double x, double fc, double q, Mode m) {
		fc = std::clamp(fc, 10.0, sr * 0.45);
		const double g = std::tan(M_PI * fc / sr), k = 1.0 / q;
		const double a1 = 1.0 / (1.0 + g * (g + k)), a2 = g * a1, a3 = g * a2;
		const double v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
		ic1 = 2 * v1 - ic1;
		ic2 = 2 * v2 - ic2;
		return m == LP ? v2 : m == BP ? v1 * k : x - k * v1 - v2; // band-pass scaled to unity gain at the centre
	}
};

struct OnePole { // gentle low-pass, and a high-pass made from it
	double z = 0;
	double lp(double x, double fc, double sr) {
		const double a = 1.0 - std::exp(-TAU_D * fc / sr);
		z += (x - z) * a;
		return z;
	}
	double hp(double x, double fc, double sr) { return x - lp(x, fc, sr); }
};

static double square(double ph) {
	return std::fmod(ph, 1.0) < 0.5 ? 1.0 : -1.0;
}
static double saw(double ph) {
	return 2.0 * std::fmod(ph, 1.0) - 1.0;
}
static double tri(double ph) {
	const double p = std::fmod(ph, 1.0);
	return 1.0 - 4.0 * std::fabs(p - 0.5);
}
static double ex(double t, double tau) {
	return t < 0 ? 0.0 : std::exp(-t / tau);
}
static double attackDecay(double t, double att, double tau) {
	return t < 0 ? 0.0 : t < att ? t / att : std::exp(-(t - att) / tau);
}
static double soft(double x, double drive) {
	return std::tanh(x * drive) / std::tanh(drive);
}
static double midiHz(double m) {
	return 440.0 * std::pow(2.0, (m - 69.0) / 12.0);
}

// six square waves at the inharmonic pitches of analogue drum machines' metal sounds
static double metal(double t, double scale) {
	static const double fr[6] = {205.3, 304.4, 369.6, 522.7, 540.0, 800.0};
	double m = 0;
	for (double f : fr)
		m += square(t * f * scale);
	return m / 6.0;
}

struct Sound {
	std::vector<float> l, r;
	int sr = 48000;
	bool stereo = false;
	Sound(double seconds, int rate = 48000, bool st = false) : sr(rate), stereo(st) {
		l.assign(size_t(seconds * rate), 0.f);
		if (st) r = l;
	}
	size_t n() const { return l.size(); }
	double t(size_t i) const { return double(i) / sr; }
	template <class F> void fill(F f) {
		for (size_t i = 0; i < n(); i++)
			l[i] = float(f(t(i), i));
		if (stereo) r = l;
	}
	template <class F> void add(F f, double gain = 1.0) {
		for (size_t i = 0; i < n(); i++) {
			const float v = float(f(t(i), i) * gain);
			l[i] += v;
			if (stereo) r[i] += v;
		}
	}
	void mix(const Sound& o, double gain = 1.0, double offset = 0.0) {
		const long off = long(offset * sr);
		for (size_t i = 0; i < o.n(); i++) {
			const long j = long(i) + off;
			if (j < 0 || j >= long(n())) continue;
			l[size_t(j)] += float(o.l[i] * gain);
			if (stereo) r[size_t(j)] += float((o.stereo ? o.r[i] : o.l[i]) * gain);
		}
	}
	template <class F> void map(F f) {
		for (size_t i = 0; i < n(); i++) {
			l[i] = float(f(double(l[i]), t(i)));
			if (stereo) r[i] = float(f(double(r[i]), t(i)));
		}
	}
};

// a room or a hall around a sound, using the program's own reverb
static void reverb(Sound& s, double size, double damping, double wet, double dry = 1.0) {
	dsp::Reverb rv;
	rv.init(float(s.sr));
	rv.setSize(float(size), float(damping));
	const bool wasMono = !s.stereo;
	if (wasMono) {
		s.stereo = true;
		s.r = s.l;
	}
	for (size_t i = 0; i < s.n(); i++) {
		float ol, orr;
		rv.process(s.l[i], s.r[i], ol, orr);
		s.l[i] = float(s.l[i] * dry + ol * wet);
		s.r[i] = float(s.r[i] * dry + orr * wet);
	}
	if (wasMono) {
		for (size_t i = 0; i < s.n(); i++)
			s.l[i] = 0.5f * (s.l[i] + s.r[i]);
		s.r.clear();
		s.stereo = false;
	}
}

// ---------------------------------------------------------------- finishing and writing

struct Finish {
	double peakDb = -1.0;
	double rmsDb = 0.0;
	bool trim = true;
	double fadeIn = 0.0;
	double fadeOut = 0.004;
};

static int written = 0;
static double totalBytes = 0;
static std::string outDir;

static void save(const std::string& rel, Sound s, Finish f = {}) {
	const int ch = s.stereo ? 2 : 1;
	// take away any DC offset
	for (auto* v : {&s.l, &s.r}) {
		if (v->empty()) continue;
		OnePole hp;
		for (float& x : *v)
			x = float(hp.hp(x, 8.0, s.sr));
	}
	// end the file once it has gone quiet (-70 dB), keeping a short breath
	if (f.trim) {
		double pk = 0;
		for (size_t i = 0; i < s.n(); i++)
			pk = std::max(pk, double(std::fabs(s.l[i])) + (s.stereo ? std::fabs(s.r[i]) : 0.0));
		const double gate = pk * std::pow(10.0, -70.0 / 20.0);
		size_t end = s.n();
		while (end > 1 && std::fabs(s.l[end - 1]) < gate && (!s.stereo || std::fabs(s.r[end - 1]) < gate))
			end--;
		end = std::min(s.n(), end + size_t(0.02 * s.sr));
		s.l.resize(end);
		if (s.stereo) s.r.resize(end);
	}
	// a sound still ringing when its time is up fades out over its last stretch instead of stopping short
	if (f.trim) {
		double pk = 0, tail = 0;
		const size_t last = s.n() / 20; // the final 5 %
		for (size_t i = 0; i < s.n(); i++) {
			const double v = std::fabs(s.l[i]);
			pk = std::max(pk, v);
			if (i + last >= s.n()) tail = std::max(tail, v);
		}
		if (tail > pk * 0.001) { // louder than -60 dB at the end
			const size_t len = std::min(s.n() / 3, size_t(0.35 * s.sr));
			for (size_t i = 0; i < len; i++) {
				const double x = double(i) / double(len), g = 0.5 - 0.5 * std::cos(M_PI * x);
				s.l[s.n() - 1 - i] *= float(g);
				if (s.stereo) s.r[s.n() - 1 - i] *= float(g);
			}
		}
	}
	// soft edges, so nothing clicks
	const size_t fo = std::min(s.n() / 2, size_t(f.fadeOut * s.sr)), fi = std::min(s.n() / 2, size_t(f.fadeIn * s.sr));
	for (size_t i = 0; i < fo; i++) {
		const double g = double(i) / double(fo);
		s.l[s.n() - 1 - i] *= float(g);
		if (s.stereo) s.r[s.n() - 1 - i] *= float(g);
	}
	for (size_t i = 0; i < fi; i++) {
		const double g = double(i) / double(fi);
		s.l[i] *= float(g);
		if (s.stereo) s.r[i] *= float(g);
	}
	// level: peak at -1 dBFS for one-shots; textures by their average loudness, but never above the peak
	double pk = 0, sum = 0;
	for (size_t i = 0; i < s.n(); i++) {
		pk = std::max({pk, double(std::fabs(s.l[i])), s.stereo ? double(std::fabs(s.r[i])) : 0.0});
		sum += double(s.l[i]) * s.l[i] + (s.stereo ? double(s.r[i]) * s.r[i] : 0.0);
	}
	const double rms = std::sqrt(sum / double(s.n() * size_t(ch)));
	double gain = std::pow(10.0, f.peakDb / 20.0) / std::max(pk, 1e-9);
	if (f.rmsDb < 0) gain = std::min(gain, std::pow(10.0, f.rmsDb / 20.0) / std::max(rms, 1e-9));
	// 16-bit with triangular dither
	Rand d(0xD17E5 + written);
	std::vector<int16_t> pcm(s.n() * size_t(ch));
	for (size_t i = 0; i < s.n(); i++)
		for (int c = 0; c < ch; c++) {
			const double x = (c ? s.r[i] : s.l[i]) * gain * 32767.0 + (d.next() - d.next());
			pcm[i * size_t(ch) + size_t(c)] = int16_t(std::clamp(std::lround(x), -32768L, 32767L));
		}
	const fs::path path = fs::path(outDir) / (rel + ".wav");
	fs::create_directories(path.parent_path());
	FILE* fp = std::fopen(path.c_str(), "wb");
	if (!fp) {
		std::fprintf(stderr, "cannot write %s\n", path.c_str());
		std::exit(1);
	}
	auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, fp); };
	auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, fp); };
	const uint32_t bytes = uint32_t(pcm.size() * 2);
	std::fwrite("RIFF", 1, 4, fp);
	u32(36 + bytes);
	std::fwrite("WAVEfmt ", 1, 8, fp);
	u32(16);
	u16(1);
	u16(uint16_t(ch));
	u32(uint32_t(s.sr));
	u32(uint32_t(s.sr * ch * 2));
	u16(uint16_t(ch * 2));
	u16(16);
	std::fwrite("data", 1, 4, fp);
	u32(bytes);
	std::fwrite(pcm.data(), 2, pcm.size(), fp);
	std::fclose(fp);
	written++;
	totalBytes += 44 + bytes;
	std::printf("  %-34s %5.2f s  %s  peak %+.1f dB  %6.0f kB\n", rel.c_str(), double(s.n()) / s.sr,
	            s.stereo ? "stereo" : "mono  ", 20 * std::log10(pk * gain), (44 + bytes) / 1000.0);
}

// ---------------------------------------------------------------- kicks

// the basic kick: a sine whose pitch falls fast, with a click at the start
static Sound kick(double secs, double f0, double sweep, double sweepTau, double tau, double click, double drive,
                  uint64_t seed, double clickHz = 3500) {
	Sound s(secs);
	Rand rn(seed);
	Svf cf;
	double ph = 0;
	s.fill([&](double t, size_t) {
		const double f = f0 + sweep * ex(t, sweepTau);
		ph += f / s.sr;
		double body = std::sin(TAU_D * ph) * attackDecay(t, 0.0008, tau);
		const double c = cf.tick(rn.bi(), clickHz, 0.8, Svf::BP) * ex(t, 0.0018) * click;
		return soft(body + c, drive);
	});
	return s;
}

static void makeKicks() {
	std::printf("Kicks\n");
	{
		Sound s = kick(1.8, 46, 115, 0.04, 0.42, 0.25, 1.4, 1); // long, deep, round
		save("Kicks/Deep boom", s);
	}
	{
		Sound s = kick(0.6, 52, 240, 0.017, 0.15, 0.6, 2.2, 2, 4200);
		save("Kicks/Punch", s);
	}
	{
		Sound s(0.5);
		double ph = 0;
		OnePole lp;
		s.fill([&](double t, size_t) {
			ph += (58 + 85 * ex(t, 0.028)) / s.sr;
			return lp.lp((std::sin(TAU_D * ph) + 0.35 * tri(ph)) * attackDecay(t, 0.001, 0.11), 1800, s.sr);
		});
		save("Kicks/Analog thump", s);
	}
	{
		Sound s = kick(1.6, 50, 260, 0.014, 0.18, 0.5, 2.5, 4); // with a dark, rumbling tail
		Sound tail = s;
		reverb(tail, 0.62, 0.8, 1.0, 0.0);
		OnePole a, b;
		tail.map([&](double x, double) { return b.lp(a.lp(x, 160, 48000), 160, 48000); });
		s.mix(tail, 3.5);
		s.map([](double x, double) { return soft(x, 1.8); });
		save("Kicks/Rumble", s);
	}
	{
		Sound s = kick(0.4, 56, 330, 0.011, 0.105, 0.7, 4.0, 5, 5000);
		OnePole lp;
		s.map([&](double x, double) { return lp.lp(x, 7000, 48000); });
		save("Kicks/Hard", s);
	}
	{
		Sound s(0.8);
		double ph = 0;
		Rand rn(6);
		OnePole lp, nl;
		s.fill([&](double t, size_t) {
			ph += (50 + 38 * ex(t, 0.05)) / s.sr;
			const double felt = nl.lp(rn.bi(), 260, s.sr) * attackDecay(t, 0.002, 0.03) * 2.5;
			return lp.lp(std::sin(TAU_D * ph) * attackDecay(t, 0.004, 0.2) + felt, 700, s.sr);
		});
		save("Kicks/Soft felt", s);
	}
	{
		Sound s(2.8);
		double ph = 0;
		s.fill([&](double t, size_t) {
			ph += (37 + 55 * ex(t, 0.06)) / s.sr;
			const double x = std::sin(TAU_D * ph);
			return (x + 0.06 * x * x) * attackDecay(t, 0.002, 0.85);
		});
		save("Kicks/Sub boom", s);
	}
	{
		Sound s = kick(0.28, 68, 420, 0.007, 0.065, 1.2, 1.8, 8, 6000);
		save("Kicks/Click", s);
	}
}

// ---------------------------------------------------------------- snares and claps

static Sound snare(double secs, double fa, double fb, double bodyTau, double noiseTau, double hp, double lpf,
                   double bodyMix, double noiseMix, uint64_t seed) {
	Sound s(secs);
	Rand rn(seed);
	Svf h, l;
	double pa = 0, pb = 0;
	s.fill([&](double t, size_t) {
		const double bend = 1.0 + 0.25 * ex(t, 0.008);
		pa += fa * bend / s.sr;
		pb += fb * bend / s.sr;
		const double body = (std::sin(TAU_D * pa) + 0.6 * std::sin(TAU_D * pb)) * attackDecay(t, 0.0006, bodyTau);
		const double nz =
		    l.tick(h.tick(rn.bi(), hp, 0.7, Svf::HP), lpf, 0.7, Svf::LP) * attackDecay(t, 0.0005, noiseTau);
		return soft(body * bodyMix + nz * noiseMix, 1.5);
	});
	return s;
}

static Sound clap(double secs, double centre, double tailTau, uint64_t seed, int bursts = 4) {
	Sound s(secs);
	Rand rn(seed);
	Svf bp;
	const double gaps[6] = {0.0, 0.0085, 0.017, 0.0275, 0.036, 0.045};
	s.fill([&](double t, size_t) {
		double e = 0;
		for (int k = 0; k < bursts; k++)
			e = std::max(e, ex(t - gaps[k], 0.0035) * (t >= gaps[k] ? 1.0 : 0.0));
		const double last = gaps[bursts - 1];
		e = std::max(e, t >= last ? 0.8 * ex(t - last, tailTau) : 0.0);
		return bp.tick(rn.bi(), centre, 1.4, Svf::BP) * e;
	});
	return s;
}

static void makeSnaresAndClaps() {
	std::printf("Snares and claps\n");
	save("Snares/Tight", snare(0.4, 188, 330, 0.045, 0.085, 1600, 9000, 0.55, 0.9, 11));
	{
		Sound s = snare(0.9, 152, 245, 0.085, 0.17, 800, 6500, 0.7, 0.85, 12);
		reverb(s, 0.2, 0.5, 0.25);
		save("Snares/Fat", s);
	}
	{
		Sound s = snare(0.5, 175, 300, 0.055, 0.11, 1200, 8000, 0.6, 0.85, 13);
		OnePole lp;
		s.map([&](double x, double) { return lp.lp(soft(x, 2.6), 6500, 48000); });
		// a little wow, as from an old tape
		Sound w = s;
		for (size_t i = 0; i < s.n(); i++) {
			const double pos = double(i) * (1.0 + 0.004 * std::sin(TAU_D * 5.5 * s.t(i)));
			const size_t k = std::min(s.n() - 2, size_t(pos));
			const double fr = pos - double(k);
			w.l[i] = float(s.l[k] + (s.l[k + 1] - s.l[k]) * fr);
		}
		save("Snares/Tape", w);
	}
	{
		Sound s(0.6);
		Rand rn(14);
		Svf bp;
		s.fill([&](double t, size_t) {
			const double e = std::max(attackDecay(t, 0.018, 0.12), 0.5 * attackDecay(t - 0.16, 0.03, 0.12));
			return bp.tick(rn.bi(), 2000 + 3500 * std::min(1.0, t / 0.35), 1.1, Svf::BP) * e;
		});
		save("Snares/Brush", s, {.fadeIn = 0.003});
	}
	{
		Sound s(0.45);
		Rand rn(15);
		Svf hp, lp;
		double ph = 0;
		s.fill([&](double t, size_t) {
			ph += (180 + 900 * ex(t, 0.012)) / s.sr;
			const double nz = lp.tick(hp.tick(rn.bi(), 1700, 0.7, Svf::HP), 7500, 0.7, Svf::LP);
			return soft(nz * attackDecay(t, 0.0005, 0.1) + 0.9 * std::sin(TAU_D * ph) * ex(t, 0.035), 2.0);
		});
		save("Snares/Electro", s);
	}
	{
		Sound s = snare(0.32, 200, 350, 0.03, 0.06, 2000, 9000, 0.35, 0.5, 16);
		Rand rn(17);
		Svf ring;
		s.add([&](double t, size_t) { return ring.tick(rn.bi(), 1850, 9, Svf::BP) * ex(t, 0.018) * 1.6; });
		save("Snares/Rim snare", s);
	}
	save("Claps/Classic", clap(0.55, 1150, 0.11, 21));
	{
		Sound s = clap(1.8, 1100, 0.13, 22);
		reverb(s, 0.7, 0.35, 0.55);
		save("Claps/Big room", s);
	}
	{
		Sound s(0.22);
		Rand rn(23);
		Svf bp;
		s.fill([&](double t, size_t) {
			return bp.tick(rn.bi(), 2600, 4.5, Svf::BP) * ex(t, 0.011) * 1.8 +
			       std::sin(TAU_D * 2250 * t) * ex(t, 0.008) * 0.4;
		});
		save("Claps/Snap", s);
	}
	{
		Sound s(0.8);
		Rand rn(24);
		for (int k = 0; k < 7; k++)
			s.mix(clap(0.8, 850 + 160 * k, 0.07 + 0.008 * k, 30 + k, 3 + k % 2), 0.7 + 0.1 * rn.next(),
			      0.03 * rn.next());
		reverb(s, 0.3, 0.5, 0.2);
		save("Claps/Crowd", s);
	}
}

// ---------------------------------------------------------------- hats and cymbals

static Sound hat(double secs, double tau, double hpf, double metalMix, double scale, uint64_t seed,
                 double att = 0.0004) {
	Sound s(secs);
	Rand rn(seed);
	Svf h1, h2, lp;
	s.fill([&](double t, size_t) {
		const double src = metal(t, scale) * metalMix + rn.bi() * (1.0 - metalMix);
		return lp.tick(h2.tick(h1.tick(src, hpf, 0.7, Svf::HP), hpf, 0.7, Svf::HP), 15000, 0.7, Svf::LP) *
		       attackDecay(t, att, tau);
	});
	return s;
}

static Sound cymbal(double secs, double tau, uint64_t seed) {
	Sound s(secs);
	Rand rn(seed);
	Svf h1, h2, lp;
	static const double fr[8] = {263, 400, 421, 474, 587, 845, 1027, 1311};
	s.fill([&](double t, size_t) {
		double m = 0;
		for (double f : fr)
			m += square(t * f * 2.3);
		const double src = m / 8.0 * 0.55 + rn.bi() * 0.45;
		const double e = attackDecay(t, 0.002, tau) * 0.75 + ex(t, 0.035) * 0.45;
		return lp.tick(h2.tick(h1.tick(src, 4200, 0.7, Svf::HP), 4200, 0.7, Svf::HP), 13000, 0.6, Svf::LP) * e;
	});
	return s;
}

// struck metal: a handful of partials, each ringing for its own time
static Sound bell(double secs, double base, std::vector<std::pair<double, double>> partials, std::vector<double> taus,
                  double strike, uint64_t seed) {
	Sound s(secs);
	Rand rn(seed);
	Svf bp;
	std::vector<double> ph(partials.size(), 0.0);
	for (size_t k = 0; k < ph.size(); k++)
		ph[k] = rn.next();
	s.fill([&](double t, size_t) {
		double x = 0;
		for (size_t k = 0; k < partials.size(); k++) {
			ph[k] += base * partials[k].first / s.sr;
			x += std::sin(TAU_D * ph[k]) * partials[k].second * attackDecay(t, 0.001, taus[k]);
		}
		return x + bp.tick(rn.bi(), base * 4, 2, Svf::BP) * ex(t, 0.004) * strike;
	});
	return s;
}

static void makeHatsAndCymbals() {
	std::printf("Hats and cymbals\n");
	save("Hats/Closed", hat(0.22, 0.034, 7000, 0.5, 2.0, 41));
	save("Hats/Tight", hat(0.12, 0.014, 9000, 0.45, 2.1, 42));
	save("Hats/Open", hat(1.2, 0.32, 6500, 0.55, 2.0, 43));
	save("Hats/Sizzle", hat(2.4, 0.75, 6000, 0.5, 1.95, 44));
	{
		Sound s = hat(0.28, 0.055, 5200, 0.6, 2.0, 45, 0.003);
		save("Hats/Pedal", s);
	}
	{
		Sound s(0.3);
		Rand rn(46);
		Svf bp;
		s.fill([&](double t, size_t) {
			const double e =
			    std::exp(-std::pow((t - 0.025) / 0.014, 2.0)) + 0.8 * std::exp(-std::pow((t - 0.12) / 0.03, 2.0));
			return bp.tick(rn.bi(), 6200, 2.0, Svf::BP) * e;
		});
		save("Hats/Shaker", s, {.fadeIn = 0.002});
	}
	{
		Sound s(0.55);
		Rand rn(47);
		Svf hp;
		const double jf[5] = {3120, 4710, 6180, 7930, 9450};
		const double hits[5] = {0.0, 0.011, 0.024, 0.04, 0.062};
		s.fill([&](double t, size_t) {
			double x = 0;
			for (int h = 0; h < 5; h++) {
				const double tt = t - hits[h];
				if (tt < 0) continue;
				double j = 0;
				for (int k = 0; k < 5; k++)
					j += std::sin(TAU_D * jf[k] * (1 + 0.006 * h) * tt + k);
				x += j / 5 * ex(tt, 0.05) * (1.0 - 0.15 * h);
			}
			return x + hp.tick(rn.bi(), 7000, 0.7, Svf::HP) * ex(t, 0.08) * 0.4;
		});
		save("Hats/Tambourine", s, {.fadeIn = 0.0005});
	}
	save("Cymbals/Crash", cymbal(3.4, 0.95, 51));
	{
		Sound s = bell(3.0, 420, {{1, 0.5}, {2.155, 0.6}, {3.19, 0.45}, {5.5, 0.35}, {7.62, 0.3}, {11.2, 0.25}},
		               {1.6, 1.3, 1.0, 0.8, 0.6, 0.4}, 1.2, 52);
		Sound wash = hat(3.0, 0.9, 5000, 0.4, 2.4, 53);
		s.mix(wash, 0.35);
		save("Cymbals/Ride", s);
	}
	{
		Sound s = bell(2.4, 640, {{1, 1.0}, {2.4, 0.7}, {4.07, 0.5}, {5.9, 0.3}}, {1.5, 1.1, 0.7, 0.5}, 0.8, 54);
		Sound wash = hat(2.4, 0.5, 6000, 0.4, 2.4, 55);
		s.mix(wash, 0.15);
		save("Cymbals/Ride bell", s);
	}
	{
		Sound c = cymbal(2.2, 0.9, 56);
		Sound s(2.2);
		for (size_t i = 0; i < s.n(); i++)
			s.l[i] = c.l[c.n() - 1 - i];
		save("Cymbals/Reverse crash", s, {.trim = false, .fadeIn = 0.05, .fadeOut = 0.012});
	}
}

// ---------------------------------------------------------------- percussion

static Sound membrane(double secs, double f0, double tau, double slap, uint64_t seed) {
	Sound s(secs);
	Rand rn(seed);
	Svf bp;
	double p1 = 0, p2 = 0;
	s.fill([&](double t, size_t) {
		const double bend = 1.0 + 0.15 * ex(t, 0.015);
		p1 += f0 * bend / s.sr;
		p2 += f0 * 1.59 * bend / s.sr;
		return std::sin(TAU_D * p1) * attackDecay(t, 0.0008, tau) + 0.35 * std::sin(TAU_D * p2) * ex(t, tau * 0.3) +
		       bp.tick(rn.bi(), 2200, 1.2, Svf::BP) * ex(t, 0.006) * slap;
	});
	return s;
}

static Sound tom(double secs, double f0, double tau, uint64_t seed) {
	Sound s(secs);
	Rand rn(seed);
	OnePole lp;
	double ph = 0;
	s.fill([&](double t, size_t) {
		ph += f0 * (1.0 + 0.6 * ex(t, 0.05)) / s.sr;
		return soft(std::sin(TAU_D * ph) * attackDecay(t, 0.001, tau) + lp.lp(rn.bi(), 1500, s.sr) * ex(t, 0.01) * 0.8,
		            1.4);
	});
	return s;
}

static void makePercussion() {
	std::printf("Percussion\n");
	{
		Sound s(0.45);
		Svf bp;
		s.fill([&](double t, size_t) {
			const double x = square(540 * t) + square(800 * t);
			return bp.tick(x, 820, 2.6, Svf::BP) * (0.6 * ex(t, 0.012) + 0.55 * ex(t, 0.09));
		});
		save("Percussion/Cowbell", s);
	}
	{
		Sound s(0.16);
		Rand rn(61);
		Svf bp;
		s.fill([&](double t, size_t) {
			return std::sin(TAU_D * 2480 * t) * ex(t, 0.022) + bp.tick(rn.bi(), 3000, 3, Svf::BP) * ex(t, 0.002);
		});
		save("Percussion/Clave", s);
	}
	{
		Sound s(0.22);
		Rand rn(62);
		Svf bp;
		s.fill([&](double t, size_t) {
			return std::sin(TAU_D * 880 * t) * ex(t, 0.035) + 0.6 * std::sin(TAU_D * 1460 * t) * ex(t, 0.02) +
			       bp.tick(rn.bi(), 1800, 2, Svf::BP) * ex(t, 0.003);
		});
		save("Percussion/Woodblock", s);
	}
	save("Percussion/Conga high", membrane(0.5, 330, 0.17, 0.9, 63));
	save("Percussion/Conga low", membrane(0.7, 220, 0.24, 0.8, 64));
	save("Percussion/Bongo", membrane(0.35, 430, 0.11, 1.0, 65));
	save("Percussion/Tom low", tom(1.1, 88, 0.34, 66));
	save("Percussion/Tom mid", tom(0.95, 128, 0.29, 67));
	save("Percussion/Tom high", tom(0.8, 178, 0.24, 68));
	{
		Sound s(0.18);
		Rand rn(69);
		Svf ring;
		s.fill([&](double t, size_t) {
			return std::sin(TAU_D * 1720 * t) * ex(t, 0.01) * 0.7 +
			       ring.tick(rn.bi(), 1900, 8, Svf::BP) * ex(t, 0.02) * 2.2 +
			       std::sin(TAU_D * 410 * t) * ex(t, 0.014) * 0.8;
		});
		save("Percussion/Rimshot", s);
	}
	{
		Sound s = bell(2.6, 1310, {{1, 1.0}, {2.76, 0.6}, {5.40, 0.4}, {8.93, 0.25}}, {2.0, 1.4, 0.9, 0.5}, 0.3, 70);
		save("Percussion/Triangle", s);
	}
}

// ---------------------------------------------------------------- the train

// one wheel crossing a rail joint: a low thud and a short metallic knock
static Sound clack(double gain, uint64_t seed) {
	Sound s(0.35);
	Rand rn(seed);
	OnePole lp;
	Svf bp, r1, r2;
	s.fill([&](double t, size_t) {
		const double thud =
		    std::sin(TAU_D * (70 + 40 * ex(t, 0.01)) * t) * ex(t, 0.05) + lp.lp(rn.bi(), 400, s.sr) * ex(t, 0.03) * 1.5;
		const double knock = bp.tick(rn.bi(), 3100, 5, Svf::BP) * ex(t, 0.007) * 1.2;
		const double ring =
		    (r1.tick(rn.bi() * ex(t, 0.004), 1930, 30, Svf::BP) + r2.tick(rn.bi() * ex(t, 0.004), 2640, 30, Svf::BP)) *
		    3.0;
		return (thud + knock + ring) * gain;
	});
	return s;
}

static void makeTrain() {
	std::printf("Train\n");
	{
		Sound s(0.7);
		s.mix(clack(0.8, 81));
		s.mix(clack(1.0, 82), 1.0, 0.125); // ta-dum
		save("Train/Rail joint", s);
	}
	{
		Sound s(1.3);
		s.mix(clack(0.8, 83));
		s.mix(clack(1.0, 84), 1.0, 0.13);
		OnePole a, b;
		s.map([&](double x, double) { return b.lp(a.lp(x, 1100, 48000), 1400, 48000); });
		reverb(s, 0.45, 0.7, 0.45);
		save("Train/Rail joint far", s);
	}
	{
		Sound s(2.4); // two tones, high then low, as on a station or a train door
		auto tone = [&](double f, double at) {
			s.add([&, f, at](double t, size_t) {
				const double tt = t - at;
				if (tt < 0) return 0.0;
				return (std::sin(TAU_D * f * tt) + 0.25 * std::sin(TAU_D * f * 4.0 * tt) * ex(tt, 0.08) +
				        0.1 * std::sin(TAU_D * f * 2.0 * tt)) *
				       attackDecay(tt, 0.004, 0.9);
			});
		};
		tone(midiHz(79), 0.0);
		tone(midiHz(76), 0.5);
		reverb(s, 0.35, 0.5, 0.18);
		save("Train/Door chime", s);
	}
	{
		Sound s = bell(3.6, 620, {{0.5, 0.5}, {1, 0.8}, {1.19, 0.5}, {1.5, 0.35}, {2.0, 0.6}, {2.51, 0.3}, {3.01, 0.2}},
		               {2.8, 2.2, 1.6, 1.4, 1.2, 0.8, 0.6}, 1.0, 85);
		reverb(s, 0.5, 0.5, 0.25);
		save("Train/Station bell", s);
	}
	{
		Sound s(1.7);
		Rand rn(86);
		Pink pk;
		Svf bp;
		s.fill([&](double t, size_t) {
			const double e = attackDecay(t, 0.03, 0.9) * (t < 0.25 ? 1.0 : 1.0);
			return bp.tick(pk.tick(rn.bi()) * 3 + rn.bi() * 0.3, 3200 - 1600 * std::min(1.0, t / 1.4), 1.3, Svf::BP) *
			       e;
		});
		save("Train/Brake hiss", s, {.fadeIn = 0.01});
	}
	{
		Sound s(2.2);
		Svf lp; // a distant horn: three detuned saw tones in a soft chord
		const double notes[3] = {midiHz(57), midiHz(61), midiHz(64)};
		double ph[6] = {0.1, 0.4, 0.7, 0.2, 0.5, 0.9};
		s.fill([&](double t, size_t) {
			double x = 0;
			for (int k = 0; k < 3; k++)
				for (int d = 0; d < 2; d++) {
					ph[k * 2 + d] +=
					    notes[k] * (1.0 + (d ? 0.003 : -0.003)) * (1.0 + 0.002 * std::sin(TAU_D * 5 * t)) / s.sr;
					x += saw(ph[k * 2 + d]);
				}
			const double env = std::min(1.0, t / 0.08) * (t < 1.3 ? 1.0 : ex(t - 1.3, 0.25));
			return lp.tick(x / 6.0, 1400, 0.9, Svf::LP) * env;
		});
		reverb(s, 0.55, 0.6, 0.35);
		save("Train/Night horn", s);
	}
	{
		Sound s(0.5);
		Rand rn(87);
		OnePole lp, rumble;
		s.fill([&](double t, size_t) {
			return lp.lp(rn.bi(), 1400, s.sr) * attackDecay(t, 0.012, 0.13) * 1.5 +
			       rumble.lp(rn.bi(), 120, s.sr) * attackDecay(t, 0.02, 0.15) * 4.0;
		});
		save("Train/Steam puff", s, {.fadeIn = 0.004});
	}
	{
		Sound s = bell(1.0, 310, {{1, 0.6}, {2.39, 0.7}, {3.83, 0.6}, {6.04, 0.45}, {8.55, 0.3}},
		               {0.6, 0.45, 0.3, 0.2, 0.15}, 2.0, 88);
		s.mix(clack(1.2, 89));
		s.map([](double x, double) { return soft(x, 1.6); });
		save("Train/Coupling clank", s);
	}
	{
		Sound s(1.4);
		double ph = 0; // a wheel squealing in a curve
		s.fill([&](double t, size_t) {
			ph += 2550 * (1.0 + 0.03 * std::min(1.0, t / 1.2)) * (1.0 + 0.004 * std::sin(TAU_D * 7 * t)) / s.sr;
			const double env = std::min(1.0, t / 0.2) * (t < 0.9 ? 1.0 : ex(t - 0.9, 0.15));
			return (std::sin(TAU_D * ph) + 0.3 * std::sin(TAU_D * 2 * ph) + 0.1 * std::sin(TAU_D * 3 * ph)) * env * 0.7;
		});
		reverb(s, 0.4, 0.4, 0.25);
		save("Train/Wheel squeal", s, {.fadeIn = 0.01});
	}
}

// ---------------------------------------------------------------- tones

static void makeTones() {
	std::printf("Tones\n");
	{
		Sound s(1.5);
		double ph = 0;
		s.fill([&](double t, size_t) {
			ph += (30 + 95 * ex(t, 0.35)) / s.sr;
			return std::sin(TAU_D * ph) * attackDecay(t, 0.003, 0.55);
		});
		save("Tones/Sub drop", s);
	}
	{
		Sound s(0.3);
		Svf lp;
		double ph = 0;
		s.fill([&](double t, size_t) {
			ph += (70 + 2100 * ex(t, 0.03)) / s.sr;
			return lp.tick(square(ph), 300 + 9000 * ex(t, 0.05), 1.8, Svf::LP) * attackDecay(t, 0.001, 0.08);
		});
		save("Tones/Zap", s);
	}
	{
		Sound s(0.45);
		double pc = 0, pm = 0;
		s.fill([&](double t, size_t) {
			const double f = 300 + 1100 * ex(t, 0.08);
			pm += f * 1.5 / s.sr;
			pc += f / s.sr + 0.0;
			return std::sin(TAU_D * pc + 4.0 * ex(t, 0.1) * std::sin(TAU_D * pm)) * attackDecay(t, 0.002, 0.12);
		});
		save("Tones/Laser", s);
	}
	{
		Sound s(0.12);
		s.fill([&](double t, size_t) { return std::sin(TAU_D * 1318.5 * t) * attackDecay(t, 0.004, 0.025); });
		save("Tones/Blip", s);
	}
	{
		Sound s(0.5);
		dsp::Ladder f;
		double ph = 0; // one acid note, C2, through the program's own ladder filter
		s.fill([&](double t, size_t) {
			ph += midiHz(36) / s.sr;
			const double fc = 120 * std::pow(2.0, 5.2 * ex(t, 0.09)), g = std::tan(M_PI * std::min(fc, 18000.0) / s.sr);
			return soft(f.process(float(saw(ph) * 0.9), float(g), 3.4f, 0) * 2.2, 1.6) * attackDecay(t, 0.002, 0.16);
		});
		save("Tones/Acid stab", s);
	}
	{
		Sound s(1.2);
		Svf lp; // a minor chord, C minor, filtered down
		const double notes[4] = {midiHz(48), midiHz(51), midiHz(55), midiHz(60)};
		double ph[8] = {0, .3, .6, .1, .4, .7, .2, .5};
		s.fill([&](double t, size_t) {
			double x = 0;
			for (int k = 0; k < 4; k++)
				for (int d = 0; d < 2; d++) {
					ph[k * 2 + d] += notes[k] * (d ? 1.004 : 0.996) / s.sr;
					x += saw(ph[k * 2 + d]);
				}
			return lp.tick(x / 8, 700 + 6000 * ex(t, 0.12), 1.2, Svf::LP) * attackDecay(t, 0.002, 0.3);
		});
		reverb(s, 0.5, 0.5, 0.3);
		save("Tones/Chord stab", s);
	}
	{
		Sound s(2.8);
		double pc = 0, pm = 0; // two-operator FM bell, A5
		s.fill([&](double t, size_t) {
			pc += 880.0 / s.sr;
			pm += 880.0 * 3.5 / s.sr;
			return std::sin(TAU_D * pc + 3.0 * ex(t, 0.4) * std::sin(TAU_D * pm)) * attackDecay(t, 0.001, 0.9);
		});
		reverb(s, 0.45, 0.5, 0.2);
		save("Tones/FM bell", s);
	}
	{
		Sound s(1.8);
		Rand rn(97); // a plucked string, A3 (Karplus-Strong)
		const int period = int(48000 / 220.0 + 0.5);
		std::vector<double> line(static_cast<size_t>(period));
		OnePole ex1;
		for (double& v : line)
			v = ex1.lp(rn.bi(), 3500, 48000) * 2.0; // a softer pluck: darker noise in the string
		size_t at = 0;
		double last = 0;
		s.fill([&](double t, size_t) {
			const double v = line[at];
			const double nv = 0.5 * (v + last) * 0.997;
			last = v;
			line[at] = nv;
			at = (at + 1) % line.size();
			return v * (t < 1.7 ? 1.0 : 1.0);
		});
		save("Tones/Pluck", s);
	}
}

// ---------------------------------------------------------------- effects

static void makeEffects() {
	std::printf("Effects\n");
	{
		Sound s(3.0);
		Rand rn(101);
		Svf bp;
		double ph = 0;
		s.fill([&](double t, size_t) {
			const double x = t / 3.0;
			ph += (200 + 1800 * x * x) / s.sr;
			return (bp.tick(rn.bi(), 300 * std::pow(30.0, x), 2.0, Svf::BP) + 0.15 * std::sin(TAU_D * ph)) *
			       (0.05 + x * x);
		});
		save("Effects/Riser", s, {.trim = false, .fadeOut = 0.02});
	}
	{
		Sound s(2.2);
		Rand rn(102);
		Svf bp;
		s.fill([&](double t, size_t) {
			const double x = t / 2.2;
			return bp.tick(rn.bi(), 8000 * std::pow(0.03, x), 1.6, Svf::BP) * (1 - x) * (1 - x);
		});
		save("Effects/Downlifter", s, {.fadeIn = 0.003});
	}
	{
		Sound s(3.5);
		Rand rn(103);
		double ph = 0;
		OnePole lp;
		s.fill([&](double t, size_t) {
			ph += (34 + 70 * ex(t, 0.08)) / s.sr;
			return std::sin(TAU_D * ph) * attackDecay(t, 0.002, 0.9) * 1.2 + lp.lp(rn.bi(), 3000, s.sr) * ex(t, 0.12);
		});
		reverb(s, 0.85, 0.5, 0.6);
		s.map([](double x, double) { return soft(x, 1.3); });
		save("Effects/Impact", s);
	}
	{
		Sound s(1.6);
		Rand rn(104);
		Svf bp, lp; // a quick tape stop: a buzz that slows and darkens to nothing
		double ph = 0;
		s.fill([&](double t, size_t) {
			const double rate = std::max(0.0, 1.0 - t / 1.2);
			ph += 110 * rate * rate / s.sr;
			return lp.tick(saw(ph) * 0.6 + bp.tick(rn.bi(), 200 + 2000 * rate, 1, Svf::BP) * 0.3,
			               150 + 3500 * rate * rate, 0.8, Svf::LP) *
			       rate;
		});
		save("Effects/Power down", s, {.fadeIn = 0.003});
	}
}

// ---------------------------------------------------------------- textures (long, stereo)

static void makeTextures() {
	std::printf("Textures\n");
	const int SR = 24000; // the textures are noise and air: 24 kHz keeps them small
	{
		Sound s(7.0, SR, true);
		Rand rn(111);
		Pink pl, pr;
		OnePole ll, lr; // rain on the window
		for (size_t i = 0; i < s.n(); i++) {
			s.l[i] = float(ll.lp(pl.tick(rn.bi()), 5000, SR) * 0.5);
			s.r[i] = float(lr.lp(pr.tick(rn.bi()), 5000, SR) * 0.5);
		}
		for (int d = 0; d < 260; d++) { // drops, each a tiny ringing tick somewhere in the stereo field
			const size_t at = size_t(rn.next() * (s.n() - SR / 10));
			const double f = 2000 + 4500 * rn.next(), pan = rn.next(), g = 0.15 + 0.5 * rn.next() * rn.next();
			Svf bp(SR);
			for (size_t k = 0; k < size_t(SR / 25); k++) {
				const double v = bp.tick(k < 30 ? rn.bi() : 0.0, f, 7, Svf::BP) * g * 4;
				s.l[at + k] += float(v * (1 - pan));
				s.r[at + k] += float(v * pan);
			}
		}
		save("Textures/Night rain", s, {.peakDb = -1, .rmsDb = -22, .trim = false, .fadeIn = 0.3, .fadeOut = 0.3});
	}
	{
		Sound s(7.0, SR, true);
		Rand rn(112);
		OnePole hl, hr, rum; // an old record between the songs
		for (size_t i = 0; i < s.n(); i++) {
			const double hum = rum.lp(rn.bi(), 40, SR) * 3;
			s.l[i] = float(hl.lp(rn.bi(), 7000, SR) * 0.05 + hum);
			s.r[i] = float(hr.lp(rn.bi(), 7000, SR) * 0.05 + hum);
		}
		for (int c = 0; c < 900; c++) {
			const size_t at = size_t(rn.next() * (s.n() - 200));
			const double g = std::pow(rn.next(), 3.0) * 0.9 + 0.03;
			const bool pop = rn.next() < 0.04;
			const int len = pop ? 90 : 6;
			for (int k = 0; k < len; k++) {
				const double v = (pop ? std::sin(k * 0.12) * (1 - k / double(len)) : rn.bi()) * g;
				s.l[at + size_t(k)] += float(v);
				s.r[at + size_t(k)] += float(v * (0.7 + 0.3 * rn.next()));
			}
		}
		save("Textures/Vinyl crackle", s, {.peakDb = -1, .rmsDb = -26, .trim = false, .fadeIn = 0.2, .fadeOut = 0.2});
	}
	{
		Sound s(8.0, SR, true);
		Rand rn(113);
		Pink pl, pr;
		Svf bl(SR), br(SR), wl(SR); // wind through a gap in the carriage
		for (size_t i = 0; i < s.n(); i++) {
			const double t = s.t(i);
			const double c = 500 + 380 * std::sin(TAU_D * 0.11 * t) + 200 * std::sin(TAU_D * 0.27 * t + 1);
			const double whistle = wl.tick(rn.bi(), 1900 + 120 * std::sin(TAU_D * 0.2 * t), 40, Svf::BP) *
			                       std::max(0.0, std::sin(TAU_D * 0.13 * t - 1)) * 2;
			s.l[i] = float(bl.tick(pl.tick(rn.bi()), c, 1.5, Svf::BP) * 2 + whistle);
			s.r[i] = float(br.tick(pr.tick(rn.bi()), c * 1.07, 1.5, Svf::BP) * 2 + whistle * 0.8);
		}
		save("Textures/Wind in the carriage", s,
		     {.peakDb = -1, .rmsDb = -22, .trim = false, .fadeIn = 0.5, .fadeOut = 0.5});
	}
	{
		Sound s(8.8, SR, true);
		Rand rn(114);
		OnePole br, a, b; // a train far off in the night: rail joints every 1.1 s, and the rumble
		for (size_t i = 0; i < s.n(); i++) {
			const double r = br.lp(rn.bi(), 90, SR) * 2.2;
			s.l[i] = float(r);
			s.r[i] = float(r);
		}
		for (int k = 0; k < 8; k++) {
			Sound c(0.6);
			c.mix(clack(0.7, 120 + uint64_t(k)));
			c.mix(clack(0.9, 140 + uint64_t(k)), 1.0, 0.125);
			for (size_t j = 0; j < c.n(); j++) {
				const size_t at = size_t((k * 1.1 + 0.05) * SR) + j * SR / 48000;
				if (at < s.n()) {
					s.l[at] += c.l[j] * 0.5f;
					s.r[at] += c.l[j] * 0.45f;
				}
			}
		}
		s.map([&](double x, double) { return x; });
		for (size_t i = 0; i < s.n(); i++) {
			s.l[i] = float(a.lp(s.l[i], 900, SR));
			s.r[i] = float(b.lp(s.r[i], 950, SR));
		}
		reverb(s, 0.6, 0.6, 0.5);
		save("Textures/Rails at night", s,
		     {.peakDb = -1, .rmsDb = -20, .trim = false, .fadeIn = 0.05, .fadeOut = 0.05});
	}
}

int main(int argc, char** argv) {
	outDir = argc > 1 ? argv[1] : "assets/samples";
	std::printf("Writing the Nachtzug sample library to %s\n", outDir.c_str());
	makeKicks();
	makeSnaresAndClaps();
	makeHatsAndCymbals();
	makePercussion();
	makeTrain();
	makeTones();
	makeEffects();
	makeTextures();
	std::printf("%d samples, %.1f MB\n", written, totalBytes / 1e6);
}
