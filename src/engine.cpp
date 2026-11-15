#include "engine.h"
#include <QDir>
#include <QFile>
#include <climits>
#include <cstdint>
#include <cstring>
#include <functional>
#if defined(__SSE__)
#include <xmmintrin.h>
#endif

using namespace dsp;

static constexpr int LIVE_BASE = 100000;   // keys of notes played by hand
static constexpr int ARP_BASE = 200000000; // keys of notes the arpeggiator plays

Engine::Engine(float sampleRate) : sr(sampleRate) {
    for (auto& s : rt_) { s.synth.init(sr); s.drums.init(sr); }
    for (auto& p : trackPeak) p.store(0.f);
    phrSynth.init(sr); phrDrums.init(sr);
    delay.init(sr);
    reverb.init(sr);
}

void Engine::pushInput(const float* mono, int n) {
    unsigned w = inW_.load(std::memory_order_relaxed);
    const unsigned r = inR_.load(std::memory_order_acquire);
    for (int i = 0; i < n; i++) {
        if (w - r >= IN_N - 1) break;   // full: the audio thread is not reading (the song may be loading)
        inRing_[w % IN_N] = mono[i];
        w++;
    }
    inW_.store(w, std::memory_order_release);
}

void Engine::setSong(std::shared_ptr<const Song> s) {
    while (songLock_.test_and_set(std::memory_order_acquire)) {}
    song_.swap(s);
    songLock_.clear(std::memory_order_release);
}   // the song it replaced is let go here, on the GUI thread

bool Engine::push(const EngineEvent& e) {
    const unsigned h = qHead.load(std::memory_order_relaxed), next = (h + 1) % QN;
    if (next == qTail.load(std::memory_order_acquire)) return false;
    queue[h] = e;
    qHead.store(next, std::memory_order_release);
    return true;
}

void Engine::remap() {
    const Song& song = *cur_;
    // hand-played notes waiting for the grid must not start on a track that no longer exists
    for (int i = 0; i < nPend;) { if (song.track(pend[i].track)) i++; else pend[i] = pend[--nPend]; }
    for (auto& sl : rt_) {
        if (sl.id < 0) continue;
        if (!song.track(sl.id)) { // the track is gone: silence everything it was doing, at once
            sl.synth.reset(); sl.drums.reset();
            sl.nActive = 0; sl.nArp = 0; sl.arpKey = -1; sl.arpIdx = 0; sl.peak = 0.f; sl.id = -1;
        }
    }
    for (size_t i = 0; i < song.tracks.size() && i < size_t(MAX_TRACKS); i++) {
        int found = -1;
        for (int k = 0; k < MAX_TRACKS; k++) if (rt_[k].id == song.tracks[i].id) { found = k; break; }
        if (found < 0)
            for (int k = 0; k < MAX_TRACKS; k++) if (rt_[k].id < 0) { found = k; rt_[k].id = song.tracks[i].id; rt_[k].gl = rt_[k].gr = 0.f; break; }
        map_[i] = std::max(found, 0);
    }
}

void Engine::addActive(Slot& sl, const Track& tr, int key, double samples, bool seq) {
    if (sl.nActive == 64) { // drop the oldest
        noteIn(sl, tr, sl.active[0].key, 0, false, false);
        for (int i = 1; i < 64; i++) sl.active[i - 1] = sl.active[i];
        sl.nActive = 63;
    }
    sl.active[sl.nActive++] = {key, samples, seq};
}

void Engine::noteIn(Slot& sl, const Track& tr, int key, int deg, bool acc, bool on, int alt) {
    if (tr.isDrums()) { if (on) drumHit(sl, deg, acc); return; }
    if (on) {
        int offs[4], cnt = 0;
        cur_->chordOffsets(tr.chord, offs, cnt);
        for (int i = 0; i < cnt; i++) {
            const int k = key * 8 + i, d = deg + offs[i];
            if (tr.arp) {
                if (sl.nArp < 32) {
                    if (sl.nArp == 0) { sl.arpFree = 0.0; sl.arpIdx = 0; }
                    sl.arp[sl.nArp++] = {k, d, alt};
                }
            } else synOn(sl, k, cur_->midiOf(tr, d) + alt, acc);
        }
    } else {
        for (int i = 0; i < 4; i++) {
            const int k = key * 8 + i;
            int w = 0;
            for (int j = 0; j < sl.nArp; j++) if (sl.arp[j].key != k) sl.arp[w++] = sl.arp[j];
            sl.nArp = w;
            synOff(sl, k);
        }
    }
}

void Engine::releaseSequenced() {
    const Song& song = *cur_;
    for (size_t ti = 0; ti < song.tracks.size() && ti < size_t(MAX_TRACKS); ti++) {
        Slot& sl = rt_[map_[ti]];
        int w = 0;
        for (int i = 0; i < sl.nActive; i++) {
            if (sl.active[i].seq) noteIn(sl, song.tracks[ti], sl.active[i].key, 0, false, false);
            else sl.active[w++] = sl.active[i];
        }
        sl.nActive = w;
    }
}

void Engine::handle(const EngineEvent& e) {
    const Song& song = *cur_;
    switch (e.type) {
    case EngineEvent::Play: pos_ = std::max(0.0, e.tick); playing_ = true; pass_++; break;
    case EngineEvent::Stop: playing_ = false; nPend = 0; releaseSequenced(); break;
    case EngineEvent::Seek: pos_ = std::max(0.0, e.tick); nPend = 0; releaseSequenced(); break;
    case EngineEvent::Panic:
        nPend = 0;
        for (auto& sl : rt_) { sl.synth.allOff(); sl.nActive = 0; sl.nArp = 0; sl.arpKey = -1; }
        break;
    default: {
        const int ti = song.trackIndex(e.track);
        if (ti < 0 || ti >= MAX_TRACKS) return;
        const Track& tr = song.tracks[size_t(ti)];
        Slot& sl = rt_[map_[ti]];
        if (e.type == EngineEvent::NoteOn) {
            if (e.quant > 0 && playing_ && nPend < 16) {
                // a little late: play now. A little early: wait for the grid line.
                const double m = unswing(pos_, song.swing), phase = std::fmod(m, double(e.quant));
                if (phase > e.quant * 0.5) {
                    double due = m - phase + e.quant;
                    if (loop_ && due >= song.loopEnd) due = song.loopStart;
                    pend[nPend++] = {e.track, e.key, e.deg, e.quant, e.alt, due, e.acc, false};
                    return;
                }
            }
            noteIn(sl, tr, LIVE_BASE + e.key, e.deg, e.acc, true, e.alt);
        } else if (e.type == EngineEvent::NoteOff) {
            for (int i = 0; i < nPend; i++)
                if (pend[i].key == e.key && pend[i].track == e.track) { pend[i].off = true; return; }   // released before it sounded
            noteIn(sl, tr, LIVE_BASE + e.key, 0, false, false);
        } else { // Audition: a short note that ends by itself
            const int key = nextKey();
            noteIn(sl, tr, key, e.deg, e.acc, true, e.alt);
            if (!tr.isDrums()) addActive(sl, tr, key, double(sr) * 0.25, false);
        }
    }
    }
}

// sub: which of the note's quick repeats this is (0 for a note that plays once)
void Engine::trigger(Slot& sl, const Track& tr, const Clip& c, const Note& n, double tAbs, double clipEnd, int sub) {
    if (tr.isDrums()) { drumHit(sl, n.deg, n.acc); return; }
    const int rep = std::clamp(n.rep, 1, 4);
    const double step = double(n.repeatSpan(c.loopLen, false)) / rep;
    const bool last = sub == rep - 1;
    // repeats are short and separate; the last one takes what is left of the note, and its slide
    double len = rep > 1 ? (last ? step - 0.5 : std::max(1.0, step * 0.55)) : n.len - 0.5;
    if (n.slide && last) { // hold into the next note so a mono sound glides instead of retriggering
        int best = INT_MAX;
        for (const Note& o : c.notes) {
            if (&o == &n || o.start >= c.loopLen) continue;
            int d = o.start - n.start; if (d <= 0) d += c.loopLen;
            best = std::min(best, d);
        }
        if (best != INT_MAX && best <= 2 * BAR) len = best - sub * step + 3;
    }
    len = std::min(len, clipEnd - tAbs + (n.slide && last ? 3.0 : 0.0));
    const int live = liveChord.load(std::memory_order_relaxed);
    const int bar = int(tAbs) / BAR;
    const int deg = n.deg + (tr.follow ? (live != NO_CHORD ? live : cur_->chordAt(bar)) : 0);
    const int alt = n.alt + (tr.follow ? (live != NO_CHORD ? liveChordAlt.load(std::memory_order_relaxed) : cur_->chordAltAt(bar)) : 0);
    const int key = nextKey();
    noteIn(sl, tr, key, deg, n.acc, true, alt);
    // the note ends where its musical end falls on the swung playhead, so with swing a slide still reaches the
    // next note (which starts late) and a late note does not run into the one after it
    const double sw = cur_->swing;
    const double ticks = swingPos(tAbs + std::max(1.0, len), sw) - swingPos(tAbs, sw);
    addActive(sl, tr, key, std::max(1.0, ticks) / tps, true);
}

// Whether a note with a chance below 100% plays this time round. The same note in the same round always gets the
// same answer, so its repeats agree; each pass through the loop (and each repeat of the pattern) rolls again.
bool Engine::plays(const Clip& c, size_t idx, long iter) const {
    const int chance = c.notes[idx].chance;
    if (chance >= 100) return true;
    uint32_t h = pass_ * 0x9E3779B1u ^ uint32_t(c.id) * 0x85EBCA6Bu ^ uint32_t(idx) * 0xC2B2AE35u ^ uint32_t(iter) * 0x27D4EB2Fu;
    h ^= h >> 16; h *= 0x7FEB352Du; h ^= h >> 15; h *= 0x846CA68Bu; h ^= h >> 16;
    return int(h % 100u) < chance;
}

void Engine::arpStep(Slot& sl, const Track& tr, double stepSamples) {
    if (sl.arpKey >= 0) { synOff(sl, sl.arpKey); sl.arpKey = -1; }
    if (sl.nArp == 0) return;
    ArpNote list[32];
    for (int i = 0; i < sl.nArp; i++) list[i] = sl.arp[i];
    if (tr.arp != 5)
        for (int i = 1; i < sl.nArp; i++) { // insertion sort by pitch
            const ArpNote x = list[i]; int j = i - 1;
            while (j >= 0 && list[j].deg > x.deg) { list[j + 1] = list[j]; j--; }
            list[j + 1] = x;
        }
    const int len = sl.nArp * (std::clamp(tr.arpOct, 0, 2) + 1);
    int e;
    switch (tr.arp) {
    case 2: e = len - 1 - sl.arpIdx % len; break;
    case 3: { const int period = len > 1 ? 2 * len - 2 : 1; const int i = sl.arpIdx % period; e = i < len ? i : period - i; break; }
    case 4: e = int((rng.next() * 0.5f + 0.5f) * float(len)) % len; break;
    default: e = sl.arpIdx % len; break;
    }
    const int deg = list[e % sl.nArp].deg + (e / sl.nArp) * cur_->sc().n;
    sl.arpKey = ARP_BASE + (sl.arpIdx & 1023);
    synOn(sl, sl.arpKey, cur_->midiOf(tr, deg) + list[e % sl.nArp].alt, false);
    sl.arpOff = stepSamples * 0.6;
    sl.arpIdx++;
}

// Starts the notes of one clip that fall in [m0, m1). The clip covers [start, start + len) and repeats its pattern.
void Engine::fireClip(Slot& sl, const Track& tr, const Clip& c, double m0, double m1, double start, double len) {
    if (start >= m1 || start + len <= m0 || c.loopLen <= 0) return;
    const double ca = std::max(m0, start) - start + c.offset;
    const double cb = std::min(m1, start + len) - start + c.offset;
    for (long k = long(std::floor(ca / c.loopLen)); double(k) * c.loopLen < cb; k++) {
        const double base = double(k) * c.loopLen;
        const double lo = std::max(ca, base) - base, hi = std::min(cb, base + c.loopLen) - base;
        for (size_t i = 0; i < c.notes.size(); i++) {
            const Note& n = c.notes[i];
            if (n.start >= c.loopLen) continue;   // beyond a shortened pattern: kept, but not heard
            const int rep = std::clamp(n.rep, 1, 4);
            if (rep == 1) {
                if (n.start >= lo && n.start < hi && plays(c, i, k)) trigger(sl, tr, c, n, start + base + n.start - c.offset, start + len, 0);
                continue;
            }
            // a note played several times: its repeats are spread over its length, all inside this round of the pattern
            const double step = double(n.repeatSpan(c.loopLen, tr.isDrums())) / rep;
            for (int j = 0; j < rep; j++) {
                const double p = n.start + j * step;
                if (p >= lo && p < hi && plays(c, i, k)) trigger(sl, tr, c, n, start + base + p - c.offset, start + len, j);
            }
        }
    }
}

// Starts every note whose musical start time lies in [m0, m1).
void Engine::fire(double m0, double m1) {
    const Song& song = *cur_;
    for (int i = 0; i < nPend;) { // hand-played notes that waited for the grid
        const Pending p = pend[i];
        const bool stale = p.due < m0 && m0 - p.due < p.quant;
        if (!stale && !(p.due >= m0 && p.due < m1)) { i++; continue; }
        pend[i] = pend[--nPend];
        const int ti = song.trackIndex(p.track);
        if (ti < 0 || ti >= MAX_TRACKS) continue;
        Slot& sl = rt_[map_[ti]];
        noteIn(sl, song.tracks[size_t(ti)], LIVE_BASE + p.key, p.deg, p.acc, true, p.alt);
        if (p.off && !song.tracks[size_t(ti)].isDrums()) addActive(sl, song.tracks[size_t(ti)], LIVE_BASE + p.key, p.quant / tps * 0.9, false);
    }
    for (size_t ti = 0; ti < song.tracks.size() && ti < size_t(MAX_TRACKS); ti++) {
        const Track& tr = song.tracks[ti];
        Slot& sl = rt_[map_[ti]];
        if (song.liveMode) { // loop grid: the switched-on cell repeats, lined up with the bars
            if (const Clip* c = tr.liveCell >= 0 ? tr.cell(tr.liveCell) : nullptr) fireClip(sl, tr, *c, m0, m1, 0.0, 1e12);
        } else
            for (const Clip& c : tr.clips) fireClip(sl, tr, c, m0, m1, c.start, c.len);
        if (tr.arp && sl.nArp > 0 && !tr.isDrums()) {
            const int rate = RATE_TICKS[std::clamp(tr.arpRate, 0, 3)];
            const double first = std::ceil(m0 / rate) * rate;
            if (first < m1) arpStep(sl, tr, rate / tps);
        }
    }
    if (metronome.load(std::memory_order_relaxed)) {
        const double first = std::ceil(m0 / BEAT) * BEAT;
        if (first < m1) { clickEnv = 1.f; clickFreq = (long(first) % BAR == 0) ? 1700.f : 1100.f; }
    }
}

void Engine::process(float* out, int frames) {
    // flush denormals to zero: fading reverb tails must not slow the CPU down
#if defined(__SSE__)
    _mm_setcsr(_mm_getcsr() | 0x8040);
#elif defined(__aarch64__)
    { uint64_t fpcr; __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr)); __asm__ volatile("msr fpcr, %0" :: "r"(fpcr | (uint64_t(1) << 24))); }
#endif
    if (!songLock_.test_and_set(std::memory_order_acquire)) {   // busy: keep playing the song we have
        const bool changed = song_ != cur_;
        if (changed) cur_ = song_;
        songLock_.clear(std::memory_order_release);
        if (changed) remap();
    }
    if (!cur_) { std::memset(out, 0, sizeof(float) * size_t(frames) * 2); return; }
    const Song& song = *cur_;
    const int nt = std::min(int(song.tracks.size()), MAX_TRACKS);

    reverb.setSize(song.rsize, song.rdamp);
    tps = double(song.bpm) * PPQ / (60.0 * double(sr));
    bpmNow.store(song.bpm, std::memory_order_relaxed);
    const bool loop = !song.liveMode && song.loopOn && song.loopEnd > song.loopStart && !ignoreLoop.load(std::memory_order_relaxed);
    loop_ = loop;

    for (unsigned t = qTail.load(std::memory_order_relaxed); t != qHead.load(std::memory_order_acquire);) {
        handle(queue[t]);
        t = (t + 1) % QN;
        qTail.store(t, std::memory_order_release);
    }

    // sounds for this block, with recorded knob moves applied at the playhead
    const double mNow = unswing(pos_, song.swing);
    for (int ti = 0; ti < nt; ti++) {
        const Track& tr = song.tracks[size_t(ti)];
        Slot& sl = rt_[map_[ti]];
        float eff[P_COUNT];
        std::memcpy(eff, tr.patch.p, sizeof(eff));
        for (const AutoLane& l : tr.lanes)
            if (l.param >= 0 && l.param < P_COUNT && !l.pts.empty()) eff[l.param] = tr.automated(l.param, mNow, eff[l.param]);
        // clip transitions: fade the volume and/or sweep the filter at a clip's start and end
        float fade = 1.f, sweep = 0.f;
        bool inClip = false;
        if (!song.liveMode) for (const Clip& c : tr.clips) {
            if (mNow < c.start || mNow >= c.end()) continue;
            inClip = true;
            if (c.inKind && mNow < c.start + c.inLen) {
                const float x = float((mNow - c.start) / std::max(1, c.inLen));
                if (c.inKind & 1) fade *= x;
                if (c.inKind & 2) sweep -= (1.f - x) * 0.45f;
            }
            if (c.outKind && mNow >= c.end() - c.outLen) {
                const float x = float((c.end() - mNow) / std::max(1, c.outLen));
                if (c.outKind & 1) fade *= x;
                if (c.outKind & 2) sweep -= (1.f - x) * 0.45f;
            }
        }
        if (!inClip && !song.liveMode) // just after a faded ending: stay down for a bar so ringing tails do not jump back up
            for (const Clip& c : tr.clips)
                if (c.outKind && mNow >= c.end() && mNow < c.end() + BAR) { if (c.outKind & 1) fade = 0.f; if (c.outKind & 2) sweep = -0.45f; }
        eff[P_CUTOFF] = std::clamp(eff[P_CUTOFF] + sweep, 0.f, 1.f);
        sl.synth.setPatch(eff);
        sl.drums.setParams(tr.kit.p);
        if (tr.isDrums()) { const SampleData* sd[D_COUNT]; for (int d = 0; d < D_COUNT; d++) sd[d] = tr.kit.data[d].get(); sl.drums.setSamples(sd); }
        sl.vol = tr.automated(AP_VOLUME, mNow, tr.vol) * fade;
        sl.dsend = (tr.isDrums() ? tr.kit.p[DP_DELAY] : eff[P_DELAY]) * 0.8f;
        sl.rsend = (tr.isDrums() ? tr.kit.p[DP_REVERB] : eff[P_REVERB]) * 0.9f;
        sl.duck = tr.isDrums() ? 0.f : std::clamp(tr.duck, 0.f, 1.f);
    }

    // a phrase to listen to a sound with
    if (const Phrase* req = phraseReq.exchange(nullptr, std::memory_order_acquire)) {
        phr_ = req; phrT_ = 0.0; phrNext_ = 0; nPhrOn_ = 0;
        phrSynth.reset(); phrDrums.reset();
        phrSynth.setPatch(req->patch); phrDrums.setParams(req->kit);
        const SampleData* sd[D_COUNT]; for (int d = 0; d < D_COUNT; d++) sd[d] = req->samples[d].get();
        phrDrums.setSamples(sd);
        phrDelay_ = (req->drums ? req->kit[DP_DELAY] : req->patch[P_DELAY]) * 0.8f;
        phrReverb_ = (req->drums ? req->kit[DP_REVERB] : req->patch[P_REVERB]) * 0.9f;
    }
    if (stopPhrase.exchange(false, std::memory_order_relaxed)) { phr_ = nullptr; nPhrOn_ = 0; phrSynth.allOff(); }

    const bool anySolo = song.anySolo();
    for (int ti = 0; ti < nt; ti++) {
        const Track& tr = song.tracks[size_t(ti)];
        rt_[map_[ti]].audible = onlyTrack >= 0 ? tr.id == onlyTrack : !tr.mute && (!anySolo || tr.solo);
    }
    // Pump: after each kick the tracks dip and swell back up within about a third of a beat
    const float duckAttack = 0.004f, duckRelease = std::clamp(0.36f * 60.f / song.bpm, 0.07f, 0.4f);
    const float delaySamples = 60.f / song.bpm * DTIME_BEATS[std::clamp(song.dtime, 0, 4)] * sr;
    const float master = song.master * song.master * 1.3f;
    const float limRelease = 1.f - std::exp(-1.f / (0.25f * sr));

    for (int off = 0; off < frames;) {
        const int n = std::min(CHUNK, frames - off);
        chunkAt_ = off;

        // note-offs first, so back-to-back notes retrigger
        for (int ti = 0; ti < nt; ti++) {
            const Track& tr = song.tracks[size_t(ti)];
            Slot& sl = rt_[map_[ti]];
            int w = 0;
            for (int i = 0; i < sl.nActive; i++) {
                Active a = sl.active[i];
                a.samples -= n;
                if (a.samples <= 0.0) noteIn(sl, tr, a.key, 0, false, false); else sl.active[w++] = a;
            }
            sl.nActive = w;
            if (sl.arpKey >= 0 && (sl.arpOff -= n) <= 0.0) { synOff(sl, sl.arpKey); sl.arpKey = -1; }
        }

        if (playing_) {
            const double p0 = pos_;
            double p1 = p0 + n * tps;
            if (loop && p0 < song.loopEnd && p1 >= song.loopEnd) {
                fire(unswing(p0, song.swing), double(song.loopEnd));
                p1 = song.loopStart + (p1 - song.loopEnd);
                pass_++;   // notes with a chance roll again
                fire(double(song.loopStart), unswing(p1, song.swing));
            } else fire(unswing(p0, song.swing), unswing(p1, song.swing));
            pos_ = p1;
        } else {
            for (int ti = 0; ti < nt; ti++) { // free-running arpeggiators
                const Track& tr = song.tracks[size_t(ti)];
                Slot& sl = rt_[map_[ti]];
                if (!tr.arp || tr.isDrums() || sl.nArp == 0) continue;
                const int rate = RATE_TICKS[std::clamp(tr.arpRate, 0, 3)];
                const double f0 = sl.arpFree, f1 = f0 + n * tps;
                if (std::ceil(f0 / rate) * rate < f1) arpStep(sl, tr, rate / tps);
                sl.arpFree = f1;
            }
        }

        float mL[CHUNK] = {}, mR[CHUNK] = {}, dS[CHUNK] = {}, rL[CHUNK] = {}, rR[CHUNK] = {};
        float duckE[CHUNK];   // how far down the kick holds the pumping tracks, 0..1
        for (int i = 0; i < n; i++) {
            const float t = duckT_;
            if (t < duckAttack) duckE[i] = t / duckAttack;
            else if (t < duckAttack + duckRelease) { const float y = (t - duckAttack) / duckRelease; duckE[i] = 1.f - y * y * (3.f - 2.f * y); }
            else duckE[i] = 0.f;
            duckT_ = std::min(t + 1.f / sr, 10.f);
        }
        if (eventsOnly) { off += n; continue; }
        for (int ti = 0; ti < nt; ti++) {
            const Track& tr = song.tracks[size_t(ti)];
            Slot& sl = rt_[map_[ti]];
            float l[CHUNK], r[CHUNK];
            if (tr.isDrums()) sl.drums.render(l, r, n); else sl.synth.render(l, r, n);
            const float g = sl.audible ? sl.vol * sl.vol * 1.15f : 0.f;
            const float tl = g * std::min(1.f, 1.f - tr.pan), trr = g * std::min(1.f, 1.f + tr.pan);
            const float ds = sl.dsend, rs = sl.rsend, dk = sl.duck * 0.9f;
            float pk = sl.peak * 0.97f;
            for (int i = 0; i < n; i++) {
                sl.gl += (tl - sl.gl) * 0.01f; sl.gr += (trr - sl.gr) * 0.01f;
                const float dg = 1.f - dk * duckE[i];
                const float a = l[i] * sl.gl * dg, b = r[i] * sl.gr * dg;
                mL[i] += a; mR[i] += b;
                dS[i] += (a + b) * 0.5f * ds;
                rL[i] += a * rs; rR[i] += b * rs;
                pk = std::max(pk, std::max(std::fabs(a), std::fabs(b)));
            }
            sl.peak = pk;
            trackPeak[ti].store(pk, std::memory_order_relaxed);
        }

        // the live input through its filter, drive and sends; it keeps only a little in hand, so it stays close to
        // what is played, and plays silence for what has not arrived
        if (inputOn.load(std::memory_order_relaxed)) {
            const LiveInput& li = song.input;
            unsigned r = inR_.load(std::memory_order_relaxed);
            const unsigned w = inW_.load(std::memory_order_acquire);
            const unsigned keep = unsigned(std::max(2048.f, sr * 0.02f));
            if (w - r > keep + unsigned(n) * 4) r = w - keep;   // too far behind: catch up
            const float pre = li.gain * li.gain * 4.f, k = li.res * 3.9f;
            const float dpre = 1.f + li.drive * li.drive * 20.f, dpost = 1.f / (1.f + li.drive * 1.6f);
            const bool heard = !li.mute && !anySolo && onlyTrack < 0;   // a soloed track, or a file being written, leaves it out
            const float g = heard ? li.vol * li.vol * 1.15f : 0.f;
            const float tl = g * std::min(1.f, 1.f - li.pan), trr = g * std::min(1.f, 1.f + li.pan);
            const float rel = std::exp(-1.f / (0.12f * sr));
            for (int i = 0; i < n; i++) {
                float x = 0.f;
                if (r != w) { x = inRing_[r % IN_N] * pre; r++; }
                inEnv_ = std::max(std::fabs(x), inEnv_ * rel);
                const float oct = li.cutoff * 9.f + li.wah * std::min(1.f, inEnv_ * 3.f) * 4.5f - li.wah * 2.f;
                const float gT = std::tan(PI * std::clamp(40.f * std::exp2(oct), 20.f, sr * 0.45f) / sr);
                inG_ += (gT - inG_) * 0.02f;
                float y = inFilter_.process(x * (0.5f * (1.f + 0.7f * k)), inG_, k, 0);
                y = fastTanh(y * dpre) * dpost * (1.f - li.duck * 0.9f * duckE[i]);
                inGl_ += (tl - inGl_) * 0.01f; inGr_ += (trr - inGr_) * 0.01f;
                const float a = y * inGl_, b = y * inGr_;
                mL[i] += a; mR[i] += b;
                dS[i] += (a + b) * 0.5f * li.echo * 0.8f;
                rL[i] += a * li.reverb * 0.9f; rR[i] += b * li.reverb * 0.9f;
                inPk_ = std::max(inPk_ * 0.9995f, std::fabs(y) * g);
            }
            inR_.store(r, std::memory_order_release);
            inputPeak.store(inPk_, std::memory_order_relaxed);
        }

        // the phrase: notes that are due start, notes that are over end, and the sound joins the mix
        if (phr_) {
            const double t1 = phrT_ + double(n) / sr;
            for (int i = 0; i < nPhrOn_;) {
                if (phrOn_[i].end <= t1) { phrSynth.noteOff(phrOn_[i].key); phrOn_[i] = phrOn_[--nPhrOn_]; } else i++;
            }
            while (phrNext_ < phr_->notes.size() && phr_->notes[phrNext_].at < t1) {
                const PhraseNote& pn = phr_->notes[phrNext_];
                if (phr_->drums) phrDrums.trigger(pn.note, pn.acc);
                else {
                    const int key = 300000000 + int(phrNext_);
                    phrSynth.noteOn(key, pn.note, pn.acc);
                    if (nPhrOn_ < 16) phrOn_[nPhrOn_++] = {key, pn.at + pn.len};
                }
                phrNext_++;
            }
            phrT_ = t1;
            if (phrNext_ >= phr_->notes.size() && nPhrOn_ == 0) phr_ = nullptr;   // all played: the tails ring out below
        }
        if (phr_ || !phrSynth.silent() || !phrDrums.silent()) {
            float l[CHUNK], r[CHUNK];
            const bool drumsNow = phr_ ? phr_->drums : !phrDrums.silent();
            if (drumsNow) phrDrums.render(l, r, n); else phrSynth.render(l, r, n);
            const float g = 0.62f;
            for (int i = 0; i < n; i++) {
                const float a = l[i] * g, b = r[i] * g;
                mL[i] += a; mR[i] += b;
                dS[i] += (a + b) * 0.5f * phrDelay_;
                rL[i] += a * phrReverb_; rR[i] += b * phrReverb_;
            }
        }

        if (ping.exchange(false, std::memory_order_relaxed)) { clickEnv = 1.f; clickFreq = 880.f; pingLeft = int(sr * 0.6f); }
    if (const SampleData* req = previewReq.exchange(nullptr, std::memory_order_acquire)) { prev_ = req; prevPos_ = 0.0; }
    if (stopPreview.exchange(false, std::memory_order_relaxed)) prev_ = nullptr;
    int sw = scopeW.load(std::memory_order_relaxed);
        for (int i = 0; i < n; i++) {
            float dl, dr, vl, vr;
            delay.process(dS[i], delaySamples, song.dfb * 0.85f, dl, dr);
            reverb.process(rL[i] + dl * 0.15f, rR[i] + dr * 0.15f, vl, vr);
            float L = mL[i] + dl + vl * 0.8f, R = mR[i] + dr + vr * 0.8f;
            if (clickEnv > 0.001f) {
                clickPh += clickFreq / sr; if (clickPh >= 1.f) clickPh -= 1.f;
                const float c = std::sin(TAU * clickPh) * clickEnv * 0.35f;
                clickEnv *= pingLeft > 0 ? 0.99993f : 0.9985f;   // the test tone rings longer than a metronome tick
                if (pingLeft > 0 && --pingLeft == 0) clickEnv *= 0.5f;
                L += c; R += c;
            }
            if (prev_) {   // a sample being listened to in the settings
                const size_t k = size_t(prevPos_);
                if (k + 1 < prev_->frames()) {
                    const float fr = float(prevPos_ - double(k));
                    L += (prev_->l[k] + (prev_->l[k + 1] - prev_->l[k]) * fr) * 0.8f;
                    R += (prev_->r[k] + (prev_->r[k + 1] - prev_->r[k]) * fr) * 0.8f;
                    prevPos_ += double(prev_->rate) / double(sr);
                } else prev_ = nullptr;
            }
            L *= master; R *= master;
            if (limGainIn) { const float g = limGainIn[off + i]; L = std::clamp(L * g, -1.f, 1.f); R = std::clamp(R * g, -1.f, 1.f); }
            else { limiter.process(L, R, limRelease); if (limGainOut) limGainOut[off + i] = limiter.gain; }
            out[(off + i) * 2] = L; out[(off + i) * 2 + 1] = R;
            pkL = std::max(pkL * 0.9997f, std::fabs(L)); pkR = std::max(pkR * 0.9997f, std::fabs(R));
            scope[sw].store((L + R) * 0.5f, std::memory_order_relaxed); sw = (sw + 1) % SCOPE_N;
        }
        scopeW.store(sw, std::memory_order_relaxed);
        off += n;
    }
    clock_ += frames;
    if (eventsOnly) std::memset(out, 0, sizeof(float) * size_t(frames) * 2);
    pos.store(pos_, std::memory_order_relaxed);
    playing.store(playing_, std::memory_order_relaxed);
    peakL.store(pkL, std::memory_order_relaxed);
    peakR.store(pkR, std::memory_order_relaxed);
}

// ---------------------------------------------------------------- offline render

static void renderSpan(const Song& song, const RenderOptions& opt, int& from, int& to) {
    if (opt.loopOnly && song.loopEnd > song.loopStart) { from = song.loopStart; to = song.loopEnd; }
    else { from = 0; to = std::max(BAR, song.endTick()); }
}

double renderLength(const Song& song, const RenderOptions& opt) {
    int from = 0, to = 0;
    renderSpan(song, opt, from, to);
    const int reps = opt.loopOnly ? std::max(1, opt.repeats) : 1;
    return double(to - from) * reps * 60.0 / (double(song.bpm) * PPQ);
}

RenderResult renderSong(const Song& song, const QString& path, const RenderOptions& opt, const std::function<bool(float)>& progress) {
    RenderResult res;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return res;
    const float sr = opt.sampleRate;
    const int bytesPer = opt.bits == 24 ? 3 : 2;
    int from = 0, to = 0;
    renderSpan(song, opt, from, to);
    Song s = song;
    s.liveMode = false;
    Engine e(sr);
    e.onlyTrack = opt.onlyTrack;
    if (opt.loopOnly) { s.loopOn = true; s.loopStart = from; s.loopEnd = to; }   // the engine loops it for us
    else e.ignoreLoop.store(true);
    e.setSong(std::make_shared<const Song>(s));
    // a stem: the whole mix runs alongside, only to say how far the master limiter turns things down at each moment
    std::unique_ptr<Engine> mix;
    if (opt.onlyTrack >= 0) {
        mix = std::make_unique<Engine>(sr);
        if (!opt.loopOnly) mix->ignoreLoop.store(true);
        mix->setSong(std::make_shared<const Song>(s));
    }

    const long musicFrames = long(renderLength(s, opt) * double(sr));
    const long tailFrames = opt.fadeOut ? 0 : long(opt.maxTail * double(sr));
    const long fadeFrames = opt.fadeOut ? std::min(musicFrames / 2, long(std::min(4.0 * BAR * 60.0 / (double(s.bpm) * PPQ), 12.0) * double(sr))) : 0;
    const long total = musicFrames + tailFrames;

    QByteArray header(44, 0);
    f.write(header);
    EngineEvent play; play.type = EngineEvent::Play; play.tick = from;
    e.push(play);
    if (mix) mix->push(play);
    const int block = 1024;
    std::vector<float> buf(size_t(block) * 2), mixBuf(mix ? size_t(block) * 2 : 0), gains(mix ? size_t(block) : 0);
    if (mix) { mix->limGainOut = gains.data(); e.limGainIn = gains.data(); }
    QByteArray pcm(block * 2 * bytesPer, 0);
    dsp::Rng dither;
    bool stopped = false, ok = true;
    long quiet = 0, written = 0;
    const long quietNeeded = long(0.4 * double(sr));
    for (long done = 0; done < total; done += block) {
        if (!stopped && done >= musicFrames) { EngineEvent st; st.type = EngineEvent::Stop; e.push(st); if (mix) mix->push(st); stopped = true; }
        const int n = int(std::min<long>(block, total - done));
        if (mix) mix->process(mixBuf.data(), n);
        e.process(buf.data(), n);
        for (int i = 0; i < n; i++) {
            const long at = done + i;
            float g = 1.f;
            if (fadeFrames > 0 && at >= musicFrames - fadeFrames) { const float x = float(musicFrames - at) / float(fadeFrames); g = x * x; }   // an ear-friendly curve
            float l = buf[size_t(i) * 2] * g, r = buf[size_t(i) * 2 + 1] * g;
            buf[size_t(i) * 2] = l; buf[size_t(i) * 2 + 1] = r;
            if (stopped) { if (std::max(std::fabs(l), std::fabs(r)) < 0.0001f) quiet++; else quiet = 0; }   // below -80 dB
        }
        for (int i = 0; i < n * 2; i++) {
            const float x = std::clamp(buf[size_t(i)], -1.f, 1.f);
            char* o = pcm.data() + i * bytesPer;
            if (bytesPer == 2) {
                const float d = (dither.next() + dither.next()) * 0.5f;   // triangular dither, one bit wide
                const int v = std::clamp(int(std::lround(x * 32767.f + d)), -32768, 32767);
                o[0] = char(v & 0xff); o[1] = char((v >> 8) & 0xff);
            } else {
                const int v = std::clamp(int(std::lround(double(x) * 8388607.0)), -8388608, 8388607);
                o[0] = char(v & 0xff); o[1] = char((v >> 8) & 0xff); o[2] = char((v >> 16) & 0xff);
            }
        }
        // once the ringing out has been quiet for a while, end the file there
        int keep = n;
        if (stopped && quiet >= quietNeeded) keep = std::max(0, n - int(std::min<long>(quiet - quietNeeded, n)));
        if (f.write(pcm.constData(), qint64(keep) * 2 * bytesPer) < 0) { ok = false; break; }
        written += keep;
        if (stopped && quiet >= quietNeeded) break;
        if (progress && (done / block) % 32 == 0 && !progress(float(done) / float(total))) { ok = false; break; }
    }
    const quint32 dataBytes = quint32(f.size() - 44), rate = quint32(sr);
    auto put32 = [&](int at, quint32 v) { for (int i = 0; i < 4; i++) header[at + i] = char((v >> (8 * i)) & 0xff); };
    auto put16 = [&](int at, quint16 v) { header[at] = char(v & 0xff); header[at + 1] = char(v >> 8); };
    std::memcpy(header.data(), "RIFF", 4); put32(4, 36 + dataBytes); std::memcpy(header.data() + 8, "WAVEfmt ", 8);
    put32(16, 16); put16(20, 1); put16(22, 2); put32(24, rate); put32(28, rate * 2 * quint32(bytesPer)); put16(32, quint16(2 * bytesPer)); put16(34, quint16(opt.bits == 24 ? 24 : 16));
    std::memcpy(header.data() + 36, "data", 4); put32(40, dataBytes);
    f.seek(0); f.write(header); f.close();
    if (!ok) { f.remove(); return res; }
    res.ok = true; res.seconds = double(written) / double(sr); res.bytes = f.size();
    return res;
}

std::vector<int> audibleTracks(const Song& song) {
    std::vector<int> out;
    const bool solo = song.anySolo();
    for (const Track& t : song.tracks) if (!t.mute && (!solo || t.solo) && !t.clips.empty()) out.push_back(t.id);   // an empty track has nothing to write
    return out;
}

QString stemFileName(const Song& song, int trackId, int number) {
    const Track* t = song.track(trackId);
    QString name = t ? ui(t->name) : QStringLiteral("Track");
    static const QString bad = QStringLiteral("/\\:*?\"<>|");
    for (QChar& ch : name) if (bad.contains(ch) || ch.unicode() < 32) ch = QLatin1Char('-');
    name = name.trimmed().left(60);   // file systems take 255 bytes; a long name is cut, not refused
    if (name.isEmpty()) name = QStringLiteral("Track");
    return QStringLiteral("%1 %2.wav").arg(number, 2, 10, QLatin1Char('0')).arg(name);
}

RenderResult renderStems(const Song& song, const QString& folder, const RenderOptions& opt, const std::function<bool(float)>& progress, QStringList* files) {
    RenderResult total;
    const std::vector<int> ids = audibleTracks(song);
    if (ids.empty() || !QDir().mkpath(folder)) return total;
    QStringList written;
    for (size_t i = 0; i < ids.size(); i++) {
        RenderOptions o = opt;
        o.onlyTrack = ids[i];
        const QString file = QDir(folder).filePath(stemFileName(song, ids[i], int(i) + 1));
        const RenderResult r = renderSong(song, file, o, [&](float f) { return !progress || progress((float(i) + f) / float(ids.size())); });
        if (!r.ok) { for (const QString& w : written) QFile::remove(w); return RenderResult{}; }
        written << file;
        total.seconds = std::max(total.seconds, r.seconds);
        total.bytes += r.bytes;
    }
    if (files) *files = written;
    total.ok = true;
    return total;
}
