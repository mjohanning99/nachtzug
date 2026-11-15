#include "model.h"
#include "samples.h"
#include <QCoreApplication>
#include <QHash>
#include <QJsonArray>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <tuple>

using namespace dsp;

const ScaleDef SCALES[] = {
    {QT_TRANSLATE_NOOP("Model", "Natural minor"), 7, {0, 2, 3, 5, 7, 8, 10}},
    {QT_TRANSLATE_NOOP("Model", "Dorian"), 7, {0, 2, 3, 5, 7, 9, 10}},
    {QT_TRANSLATE_NOOP("Model", "Phrygian"), 7, {0, 1, 3, 5, 7, 8, 10}},
    {QT_TRANSLATE_NOOP("Model", "Harmonic minor"), 7, {0, 2, 3, 5, 7, 8, 11}},
    {QT_TRANSLATE_NOOP("Model", "Phrygian dominant"), 7, {0, 1, 4, 5, 7, 8, 10}},
    {QT_TRANSLATE_NOOP("Model", "Minor pentatonic"), 5, {0, 3, 5, 7, 10, 0, 0}},
    {QT_TRANSLATE_NOOP("Model", "Major"), 7, {0, 2, 4, 5, 7, 9, 11}},
    {QT_TRANSLATE_NOOP("Model", "Lydian"), 7, {0, 2, 4, 6, 7, 9, 11}},
};
const int NUM_SCALES = int(sizeof(SCALES) / sizeof(SCALES[0]));
const char* const NOTE_NAMES[12] = {"C", "C♯", "D", "D♯", "E", "F", "F♯", "G", "G♯", "A", "A♯", "B"};
const char* const DRUM_NAMES[D_COUNT] = {QT_TRANSLATE_NOOP("Model", "Kick"), QT_TRANSLATE_NOOP("Model", "Snare"), QT_TRANSLATE_NOOP("Model", "Clap"), QT_TRANSLATE_NOOP("Model", "Closed hat"), QT_TRANSLATE_NOOP("Model", "Open hat"), QT_TRANSLATE_NOOP("Model", "Tom"), QT_TRANSLATE_NOOP("Model", "Rim"), QT_TRANSLATE_NOOP("Model", "Cymbal")};
const char* const DRUM_STYLES[NUM_DRUM_STYLES] = {QT_TRANSLATE_NOOP("Model", "Four on the floor"), QT_TRANSLATE_NOOP("Model", "Acid house"), QT_TRANSLATE_NOOP("Model", "Motorik"), QT_TRANSLATE_NOOP("Model", "Broken beat"), QT_TRANSLATE_NOOP("Model", "Half time"), QT_TRANSLATE_NOOP("Model", "Electro"), QT_TRANSLATE_NOOP("Model", "Berlin pulse")};
const char* const CHORD_NAMES[7] = {QT_TRANSLATE_NOOP("Model", "Single note"), QT_TRANSLATE_NOOP("Model", "Octaves"), QT_TRANSLATE_NOOP("Model", "Fifths"), QT_TRANSLATE_NOOP("Model", "Triad"), QT_TRANSLATE_NOOP("Model", "Seventh chord"), QT_TRANSLATE_NOOP("Model", "Open triad"), QT_TRANSLATE_NOOP("Model", "Stacked fourths")};
const char* const ARP_NAMES[6] = {QT_TRANSLATE_NOOP("Model", "Off"), QT_TRANSLATE_NOOP("Model", "Up"), QT_TRANSLATE_NOOP("Model", "Down"), QT_TRANSLATE_NOOP("Model", "Up and down"), QT_TRANSLATE_NOOP("Model", "Random"), QT_TRANSLATE_NOOP("Model", "As played")};
const char* const RATE_NAMES[4] = {"1/8", "1/8 triplet", "1/16", "1/32"};
const int RATE_TICKS[4] = {48, 32, 24, 12};
const char* const DTIME_NAMES[5] = {QT_TRANSLATE_NOOP("Model", "1/16"), QT_TRANSLATE_NOOP("Model", "1/8"), QT_TRANSLATE_NOOP("Model", "Dotted 1/8"), QT_TRANSLATE_NOOP("Model", "1/4"), QT_TRANSLATE_NOOP("Model", "Dotted 1/4")};
const float DTIME_BEATS[5] = {0.25f, 0.5f, 0.75f, 1.f, 1.5f};
const char* const ROLE_NAMES[R_COUNT] = {QT_TRANSLATE_NOOP("Model", "Bass"), QT_TRANSLATE_NOOP("Model", "Sequence"), QT_TRANSLATE_NOOP("Model", "Pad"), QT_TRANSLATE_NOOP("Model", "Lead"), QT_TRANSLATE_NOOP("Model", "Drums")};

const ParamDef PARAMS[P_COUNT] = {
    {"wave1", QT_TRANSLATE_NOOP("Model", "Wave 1"), 0, K_CHOICE, QT_TRANSLATE_NOOP("Model", "Saw|Pulse|Triangle|Sine")},
    {"wave2", QT_TRANSLATE_NOOP("Model", "Wave 2"), 0, K_CHOICE, QT_TRANSLATE_NOOP("Model", "Saw|Pulse|Triangle|Sine")},
    {"tune2", QT_TRANSLATE_NOOP("Model", "Osc 2 pitch"), 1, K_CHOICE, QT_TRANSLATE_NOOP("Model", "-1 octave|Unison|+ fifth|+1 octave")},
    {"detune", QT_TRANSLATE_NOOP("Model", "Detune"), 0.2f, K_CENTS, nullptr},
    {"mix", QT_TRANSLATE_NOOP("Model", "Osc mix"), 0.3f, K_PCT, nullptr},
    {"sub", QT_TRANSLATE_NOOP("Model", "Sub"), 0, K_PCT, nullptr},
    {"noise", QT_TRANSLATE_NOOP("Model", "Noise"), 0, K_PCT, nullptr},
    {"pw", QT_TRANSLATE_NOOP("Model", "Pulse width"), 0, K_PCT, nullptr},
    {"cutoff", QT_TRANSLATE_NOOP("Model", "Cutoff"), 0.5f, K_CUTOFF, nullptr},
    {"res", QT_TRANSLATE_NOOP("Model", "Resonance"), 0.3f, K_PCT, nullptr},
    {"envAmt", QT_TRANSLATE_NOOP("Model", "Env amount"), 0.4f, K_PCT, nullptr},
    {"accent", QT_TRANSLATE_NOOP("Model", "Accent"), 0.5f, K_PCT, nullptr},
    {"slope", QT_TRANSLATE_NOOP("Model", "Type"), 0, K_CHOICE, QT_TRANSLATE_NOOP("Model", "Low-pass 24 dB|Low-pass 12 dB|Band-pass|High-pass")},
    {"fA", QT_TRANSLATE_NOOP("Model", "Attack"), 0, K_TIME, nullptr},
    {"fD", QT_TRANSLATE_NOOP("Model", "Decay"), 0.5f, K_TIME, nullptr},
    {"fS", QT_TRANSLATE_NOOP("Model", "Sustain"), 0.2f, K_PCT, nullptr},
    {"aA", QT_TRANSLATE_NOOP("Model", "Attack"), 0, K_TIME, nullptr},
    {"aD", QT_TRANSLATE_NOOP("Model", "Decay"), 0.5f, K_TIME, nullptr},
    {"aS", QT_TRANSLATE_NOOP("Model", "Sustain"), 0.8f, K_PCT, nullptr},
    {"aR", QT_TRANSLATE_NOOP("Model", "Release"), 0.35f, K_TIME, nullptr},
    {"mono", QT_TRANSLATE_NOOP("Model", "Voices"), 0, K_CHOICE, QT_TRANSLATE_NOOP("Model", "Poly|Mono")},
    {"glide", QT_TRANSLATE_NOOP("Model", "Glide"), 0, K_GLIDE, nullptr},
    {"lfoWave", QT_TRANSLATE_NOOP("Model", "LFO shape"), 0, K_CHOICE, QT_TRANSLATE_NOOP("Model", "Triangle|Square|Random")},
    {"lfoRate", QT_TRANSLATE_NOOP("Model", "LFO rate"), 0.4f, K_LFO, nullptr},
    {"lfoCut", QT_TRANSLATE_NOOP("Model", "LFO to filter"), 0, K_PCT, nullptr},
    {"lfoPitch", QT_TRANSLATE_NOOP("Model", "Vibrato"), 0, K_PCT, nullptr},
    {"drive", QT_TRANSLATE_NOOP("Model", "Drive"), 0.15f, K_PCT, nullptr},
    {"phaser", QT_TRANSLATE_NOOP("Model", "Phaser"), 0, K_PCT, nullptr},
    {"phRate", QT_TRANSLATE_NOOP("Model", "Phaser rate"), 0.3f, K_PHASER, nullptr},
    {"chorus", QT_TRANSLATE_NOOP("Model", "Chorus"), 0, K_PCT, nullptr},
    {"delay", QT_TRANSLATE_NOOP("Model", "Echo"), 0.2f, K_PCT, nullptr},
    {"reverb", QT_TRANSLATE_NOOP("Model", "Reverb"), 0.15f, K_PCT, nullptr},
    {"unison", QT_TRANSLATE_NOOP("Model", "Voices"), 0, K_CHOICE, QT_TRANSLATE_NOOP("Model", "1 (off)|2|3|5|7")},
    {"uDetune", QT_TRANSLATE_NOOP("Model", "Unison detune"), 0.3f, K_PCT, nullptr},
    {"width", QT_TRANSLATE_NOOP("Model", "Stereo spread"), 0, K_PCT, nullptr},
    {"fm", QT_TRANSLATE_NOOP("Model", "FM"), 0, K_PCT, nullptr},
    {"pitchEnv", QT_TRANSLATE_NOOP("Model", "To pitch"), 0, K_PCT, nullptr},
    {"keyTrack", QT_TRANSLATE_NOOP("Model", "Key tracking"), 0.35f, K_PCT, nullptr},
    {"lfoPw", QT_TRANSLATE_NOOP("Model", "LFO to width"), 0, K_PCT, nullptr},
};

static QString hz(float h) {
    if (h < 10.f) return QString::number(double(h), 'f', 2) + " Hz";
    if (h < 1000.f) return QString::number(int(std::lround(h))) + " Hz";
    return QString::number(double(h) / 1000.0, 'f', 1) + " kHz";
}

QString formatParam(int param, float v) {
    const ParamDef& d = PARAMS[param];
    switch (d.kind) {
    case K_TIME: { const float s = envTime(v); return s < 1.f ? QString::number(int(std::lround(s * 1000.f))) + " ms" : QString::number(double(s), 'f', 1) + " s"; }
    case K_CUTOFF: return hz(std::min(cutoffHz(v), 20000.f));
    case K_LFO: return hz(lfoHz(v));
    case K_PHASER: return hz(phaserHz(v));
    case K_CENTS: return QString::number(int(std::lround(v * 40.f))) + " cents";
    case K_GLIDE: return QString::number(int(std::lround(glideSec(v) * 1000.f))) + " ms";
    case K_CHOICE: return ui(d.choices).split('|').value(int(v + 0.5f));
    default: return QString::number(int(std::lround(v * 100.f))) + "%";
    }
}

QString paramTip(int param) {
    static const char* const tips[P_COUNT] = {
        QT_TRANSLATE_NOOP("Model", "The shape of oscillator 1. Saw is bright and buzzy, pulse hollow, triangle soft, sine pure."),
        QT_TRANSLATE_NOOP("Model", "The shape of oscillator 2, which is mixed with oscillator 1."),
        QT_TRANSLATE_NOOP("Model", "How oscillator 2 is tuned against oscillator 1: an octave down, the same, a fifth up or an octave up."),
        QT_TRANSLATE_NOOP("Model", "Pulls oscillator 2 slightly out of tune, so the two beat against each other and sound wider."),
        QT_TRANSLATE_NOOP("Model", "The balance between oscillator 1 (left) and oscillator 2 (right)."),
        QT_TRANSLATE_NOOP("Model", "Adds a soft tone one octave below the note, for weight in the low end."),
        QT_TRANSLATE_NOOP("Model", "Adds hiss, for breathy or percussive sounds."),
        QT_TRANSLATE_NOOP("Model", "Makes the pulse wave thinner and more nasal. Only changes the sound when a pulse wave is chosen."),
        QT_TRANSLATE_NOOP("Model", "Where the filter cuts: low is dark and muffled, high is bright and open. The most important knob for acid sounds."),
        QT_TRANSLATE_NOOP("Model", "Adds a peak at the cutoff: the squelchy, singing sound of acid basslines."),
        QT_TRANSLATE_NOOP("Model", "How far each note opens the filter, following the filter envelope."),
        QT_TRANSLATE_NOOP("Model", "How much louder and brighter accented notes are."),
        QT_TRANSLATE_NOOP("Model", "The filter type. Low-pass removes the highs; band-pass keeps a narrow middle band; high-pass removes the lows."),
        QT_TRANSLATE_NOOP("Model", "How quickly the filter opens when a note starts."),
        QT_TRANSLATE_NOOP("Model", "How quickly the filter closes again after opening. Short gives plucks, long gives slow sweeps."),
        QT_TRANSLATE_NOOP("Model", "How far open the filter stays while a note is held."),
        QT_TRANSLATE_NOOP("Model", "How quickly a note gets loud. Short for plucks, long for slow swells."),
        QT_TRANSLATE_NOOP("Model", "How quickly the loudness falls after the start of a note."),
        QT_TRANSLATE_NOOP("Model", "How loud a note stays while it is held."),
        QT_TRANSLATE_NOOP("Model", "How long a note rings after it ends."),
        QT_TRANSLATE_NOOP("Model", "Poly plays chords; Mono plays one note at a time, as acid basslines do, and slides between notes."),
        QT_TRANSLATE_NOOP("Model", "How long the pitch takes to slide from one note to the next."),
        QT_TRANSLATE_NOOP("Model", "The shape of the LFO’s slow movement: smooth up and down, a jump between two values, or random steps."),
        QT_TRANSLATE_NOOP("Model", "How fast the LFO moves."),
        QT_TRANSLATE_NOOP("Model", "How much the LFO moves the filter: slow sweeps or a wobble."),
        QT_TRANSLATE_NOOP("Model", "How much the LFO moves the pitch: vibrato."),
        QT_TRANSLATE_NOOP("Model", "Distortion: warms the sound up at first, makes it grittier further up."),
        QT_TRANSLATE_NOOP("Model", "A swirling, sweeping effect."),
        QT_TRANSLATE_NOOP("Model", "How fast the phaser sweeps."),
        QT_TRANSLATE_NOOP("Model", "Thickens the sound with slightly delayed, wavering copies."),
        QT_TRANSLATE_NOOP("Model", "How much of this sound goes to the shared echo."),
        QT_TRANSLATE_NOOP("Model", "How much of this sound goes to the shared reverb."),
        QT_TRANSLATE_NOOP("Model", "Stacks several detuned copies of both oscillators, for a big, wide sound."),
        QT_TRANSLATE_NOOP("Model", "How far apart the stacked copies are tuned."),
        QT_TRANSLATE_NOOP("Model", "Spreads the notes across the left and right speakers."),
        QT_TRANSLATE_NOOP("Model", "Lets oscillator 2 bend oscillator 1, for bells, metallic and glassy tones."),
        QT_TRANSLATE_NOOP("Model", "Bends the pitch with the filter envelope at the start of each note: zaps, lasers, drum-like hits."),
        QT_TRANSLATE_NOOP("Model", "Makes higher notes brighter and lower notes darker, as on acoustic instruments."),
        QT_TRANSLATE_NOOP("Model", "How much the LFO moves the pulse width, for a slowly shifting, chorus-like pulse sound."),
    };
    return param >= 0 && param < P_COUNT ? ui(tips[param]) : QString();
}

QString paramLabel(int param) {
    if (param == AP_VOLUME) return ui(QT_TRANSLATE_NOOP("Model", "Volume"));
    if (param < 0 || param >= P_COUNT) return QString();
    // the two envelopes share knob names, so say which one is meant
    static const char* const env[7] = {QT_TRANSLATE_NOOP("Model", "Filter attack"), QT_TRANSLATE_NOOP("Model", "Filter decay"), QT_TRANSLATE_NOOP("Model", "Filter sustain"),
                                       QT_TRANSLATE_NOOP("Model", "Amp attack"), QT_TRANSLATE_NOOP("Model", "Amp decay"), QT_TRANSLATE_NOOP("Model", "Amp sustain"), QT_TRANSLATE_NOOP("Model", "Amp release")};
    if (param >= P_FA && param <= P_FS) return ui(env[param - P_FA]);
    if (param >= P_AA && param <= P_AR) return ui(env[3 + param - P_AA]);
    return ui(PARAMS[param].label);
}

float Track::automated(int param, double tick, float fallback) const {
    if (param < 64 && (bypass >> param) & 1) return fallback;
    const AutoLane* l = lane(param);
    return l ? l->valueAt(tick, fallback) : fallback;
}

float AutoLane::valueAt(double tick, float fallback) const {
    if (pts.empty()) return fallback;
    const auto& p = pts;
    if (loop > 0) tick = std::fmod(std::max(tick, 0.0), double(loop));
    if (tick <= p.front().tick) return p.front().v;
    if (tick >= p.back().tick) return p.back().v;
    size_t lo = 0, hi = p.size() - 1;
    while (hi - lo > 1) { const size_t mid = (lo + hi) / 2; if (p[mid].tick <= tick) lo = mid; else hi = mid; }
    const double span = double(p[hi].tick - p[lo].tick);
    return span <= 0.0 ? p[hi].v : p[lo].v + (p[hi].v - p[lo].v) * float((tick - p[lo].tick) / span);
}

// ---------------------------------------------------------------- Song

void Song::setChord(int bar, int deg, int alt) {
    if (bar < 0) return;
    // write the chords out bar by bar for the whole song first, so only this bar changes
    const int want = std::max(bar + 1, (std::max(endTick(), loopEnd) + BAR - 1) / BAR);
    if (int(chords.size()) < want) {
        std::vector<int> all(static_cast<size_t>(want)), alts(static_cast<size_t>(want));
        for (int i = 0; i < want; i++) { all[size_t(i)] = chordAt(i); alts[size_t(i)] = chordAltAt(i); }
        chords = all;
        chordAlts = alts;
    }
    if (alt != 0 || bar < int(chordAlts.size())) {
        if (int(chordAlts.size()) < int(chords.size())) chordAlts.resize(chords.size(), 0);
        chordAlts[size_t(bar)] = std::clamp(alt, -1, 1);
    }
    chords[size_t(bar)] = deg;
}

int Song::approachAlt(int deg) const {
    const int semis = degToSemis(deg);
    auto inScale = [&](int x) { return degToSemis(degreeNear(x)) == x; };
    if (!inScale(semis - 1)) return -1;
    if (!inScale(semis + 1)) return 1;
    return 0;
}

int Song::degToSemis(int deg) const {
    const ScaleDef& s = sc();
    const int o = int(std::floor(double(deg) / s.n));
    return 12 * o + s.iv[((deg % s.n) + s.n) % s.n];
}
int Song::degreeNear(int semis) const {
    int best = 0, dist = 1 << 20;
    for (int d = -6 * sc().n; d <= 6 * sc().n; d++) {
        const int x = std::abs(degToSemis(d) - semis);
        if (x < dist) { dist = x; best = d; }
    }
    return best;
}
int Song::midiOf(const Track& t, int deg) const { return 33 + ((root + 3) % 12) + 12 * t.octave + degToSemis(deg); }

QString Song::noteName(const Track& t, int deg, int alt) const {
    if (t.isDrums()) return ui(DRUM_NAMES[std::clamp(deg, 0, D_COUNT - 1)]);
    const int m = midiOf(t, deg) + alt;
    return pitchName(((m % 12) + 12) % 12) + QString::number(m / 12 - 1);
}

QString Song::chordName(int deg, int alt) const {
    const int base = degToSemis(deg);
    const int pc = (((root + base + alt) % 12) + 12) % 12;   // a moved chord keeps its shape, a semitone off
    const int third = degToSemis(deg + q(2)) - base, fifth = degToSemis(deg + q(4)) - base;
    QString n = pitchName(pc);
    if (third == 3) n += fifth == 6 ? "dim" : "m";
    else if (third == 4 && fifth == 8) n += "aug";
    else if (third == 2) n += "sus2";
    else if (third == 5) n += "sus4";
    return n;
}

void Song::chordOffsets(int chord, int out[4], int& count) const {
    static const int shapes[7][4] = {{0, -1, -1, -1}, {0, 7, -1, -1}, {0, 4, -1, -1}, {0, 2, 4, -1}, {0, 2, 4, 6}, {0, 4, 9, -1}, {0, 3, 6, -1}};
    const int* sh = shapes[std::clamp(chord, 0, 6)];
    const int n = sc().n;
    count = 0;
    for (int i = 0; i < 4 && sh[i] >= 0; i++) out[count++] = sh[i] == 7 ? n : sh[i] == 9 ? n + q(2) : q(sh[i]);
}

Track* Song::track(int id) { for (auto& t : tracks) if (t.id == id) return &t; return nullptr; }
const Track* Song::track(int id) const { for (auto& t : tracks) if (t.id == id) return &t; return nullptr; }
int Song::trackIndex(int id) const { for (size_t i = 0; i < tracks.size(); i++) if (tracks[i].id == id) return int(i); return -1; }
Clip* Song::clip(int id, Track** owner) {
    for (auto& t : tracks) {
        for (auto& c : t.clips) if (c.id == id) { if (owner) *owner = &t; return &c; }
        for (auto& c : t.cells) if (c.id == id) { if (owner) *owner = &t; return &c; }
    }
    return nullptr;
}
int Song::endTick() const { int e = 0; for (auto& t : tracks) for (auto& c : t.clips) e = std::max(e, c.end()); return e; }
bool Song::anySolo() const { for (auto& t : tracks) if (t.solo) return true; return false; }

double unswing(double p, float swing) {
    const double s = double(swing) * 12.0;
    if (s <= 0.01) return p;
    const double base = std::floor(p / 48.0) * 48.0, r = p - base;
    return base + (r < 24.0 + s ? r * 24.0 / (24.0 + s) : 24.0 + (r - 24.0 - s) * 24.0 / (24.0 - s));
}

double swingPos(double m, float swing) {
    const double s = double(swing) * 12.0;
    if (s <= 0.01) return m;
    const double base = std::floor(m / 48.0) * 48.0, r = m - base;
    return base + (r < 24.0 ? r * (24.0 + s) / 24.0 : 24.0 + s + (r - 24.0) * (24.0 - s) / 24.0);
}

// ---------------------------------------------------------------- JSON

static QJsonObject clipToJson(const Clip& c) {
    QJsonObject cj;
    cj["id"] = c.id; cj["start"] = c.start; cj["len"] = c.len; cj["loopLen"] = c.loopLen; cj["offset"] = c.offset;
    if (c.scene >= 0) cj["column"] = c.scene;
    if (!c.name.isEmpty()) cj["name"] = c.name;
    if (c.inKind) { cj["inKind"] = c.inKind; cj["inLen"] = c.inLen; }
    if (c.outKind) { cj["outKind"] = c.outKind; cj["outLen"] = c.outLen; }
    if (c.ad.on) { // the drummer writes these notes again when the song is opened
        QJsonObject aj;
        aj["style"] = c.ad.style; aj["intensity"] = double(c.ad.intensity); aj["complexity"] = double(c.ad.complexity);
        aj["fill"] = c.ad.fillEvery; aj["seed"] = double(c.ad.seed);
        aj["backbeat"] = c.ad.backbeat; aj["hats"] = c.ad.hats; aj["shape"] = c.ad.shape;
        aj["shuffle"] = double(c.ad.shuffle); aj["crash"] = c.ad.crash;
        cj["auto"] = aj;
        QJsonArray kept;   // hits marked Keep: the drummer writes around them, so they are saved with it
        for (const Note& n : c.notes) if (n.lock) kept.append(QJsonArray{n.start, n.len, n.deg, (n.acc ? 1 : 0) | 4, n.chance, n.rep});
        if (!kept.isEmpty()) cj["kept"] = kept;
    } else {
        QJsonArray ns;
        for (const Note& n : c.notes) {
            QJsonArray a{n.start, n.len, n.deg, (n.acc ? 1 : 0) | (n.slide ? 2 : 0) | (n.lock ? 4 : 0) | (n.alt < 0 ? 8 : n.alt > 0 ? 16 : 0)};
            if (n.chance != 100 || n.rep != 1) { a.append(n.chance); a.append(n.rep); }   // left out when plain, as older songs have it
            ns.append(a);
        }
        cj["notes"] = ns;
    }
    return cj;
}

static Clip clipFromJson(const QJsonObject& cj) {
    Clip c;
    c.id = cj["id"].toInt(); c.start = std::max(0, cj["start"].toInt()); c.len = std::max(12, cj["len"].toInt(BAR));
    c.loopLen = std::max(24, cj["loopLen"].toInt(BAR)); c.offset = std::max(0, cj["offset"].toInt()) % c.loopLen;
    c.scene = cj["column"].toInt(-1);
    c.name = cj["name"].toString();
    c.inKind = std::clamp(cj["inKind"].toInt(), 0, 3); c.inLen = std::max(24, cj["inLen"].toInt(BAR));
    c.outKind = std::clamp(cj["outKind"].toInt(), 0, 3); c.outLen = std::max(24, cj["outLen"].toInt(BAR));
    QJsonArray noteList = cj["notes"].toArray();
    for (const auto& kv : cj["kept"].toArray()) noteList.append(kv);   // a drummer clip's kept hits
    for (const auto& nv : noteList) {
        const QJsonArray a = nv.toArray();
        if (a.size() < 3) continue;
        Note n; n.start = std::max(0, a[0].toInt()); n.len = std::max(1, a[1].toInt()); n.deg = a[2].toInt();
        const int fl = a.size() > 3 ? a[3].toInt() : 0; n.acc = fl & 1; n.slide = fl & 2; n.lock = fl & 4;
        n.alt = fl & 8 ? -1 : fl & 16 ? 1 : 0;
        if (a.size() > 4) n.chance = std::clamp(a[4].toInt(100), 1, 100);
        if (a.size() > 5) n.rep = std::clamp(a[5].toInt(1), 1, 4);
        c.notes.push_back(n);
    }
    if (cj.contains("auto")) {
        const QJsonObject aj = cj["auto"].toObject();
        c.ad.on = true;
        c.ad.style = std::clamp(aj["style"].toInt(), 0, NUM_DRUM_STYLES - 1);
        c.ad.backbeat = std::clamp(aj["backbeat"].toInt(), 0, 4);
        c.ad.hats = std::clamp(aj["hats"].toInt(), 0, 4);
        c.ad.shape = std::clamp(aj["shape"].toInt(), 0, 2);
        c.ad.shuffle = float(std::clamp(aj["shuffle"].toDouble(0), 0.0, 1.0));
        c.ad.crash = aj["crash"].toBool(true);
        c.ad.intensity = float(std::clamp(aj["intensity"].toDouble(0.6), 0.0, 1.0));
        c.ad.complexity = float(std::clamp(aj["complexity"].toDouble(0.4), 0.0, 1.0));
        c.ad.fillEvery = std::clamp(aj["fill"].toInt(4), 0, 16);
        c.ad.seed = unsigned(aj["seed"].toDouble(1));
    }
    return c;
}

QJsonObject Song::toJson() const {
    QJsonObject o;
    o["format"] = "nachtzug-1";
    o["bpm"] = double(bpm); o["swing"] = double(swing); o["master"] = double(master);
    o["root"] = root; o["scale"] = scale; o["dtime"] = dtime;
    o["dfb"] = double(dfb); o["rsize"] = double(rsize); o["rdamp"] = double(rdamp);
    o["loopStart"] = loopStart; o["loopEnd"] = loopEnd; o["loopOn"] = loopOn; o["nextId"] = nextId;
    o["columns"] = scenes;
    if (!(input == LiveInput{})) {
        QJsonObject ij;
        ij["gain"] = double(input.gain); ij["cutoff"] = double(input.cutoff); ij["res"] = double(input.res); ij["wah"] = double(input.wah);
        ij["drive"] = double(input.drive); ij["echo"] = double(input.echo); ij["reverb"] = double(input.reverb); ij["pump"] = double(input.duck);
        ij["vol"] = double(input.vol); ij["pan"] = double(input.pan); ij["mute"] = input.mute;
        o["input"] = ij;
    }
    QJsonArray ch; for (int c : chords) ch.append(c); o["chords"] = ch;
    if (std::any_of(chordAlts.begin(), chordAlts.end(), [](int a) { return a != 0; })) {
        QJsonArray ca; for (size_t i = 0; i < chords.size(); i++) ca.append(i < chordAlts.size() ? chordAlts[i] : 0);
        o["chordAlts"] = ca;
    }
    QJsonArray ts;
    for (const Track& t : tracks) {
        QJsonObject j;
        j["id"] = t.id; j["name"] = t.name; j["role"] = t.role; j["vol"] = double(t.vol); j["pan"] = double(t.pan);
        j["mute"] = t.mute; j["solo"] = t.solo; j["follow"] = t.follow;
        if (t.duck > 0.f) j["pump"] = double(t.duck);
        j["octave"] = t.octave; j["chord"] = t.chord; j["arp"] = t.arp; j["arpRate"] = t.arpRate; j["arpOct"] = t.arpOct;
        QJsonObject pj; for (int i = 0; i < P_COUNT; i++) pj[PARAMS[i].id] = double(t.patch.p[i]);
        j["patchName"] = t.patch.name; j["patch"] = pj;
        QJsonArray kj; for (int i = 0; i < DP_COUNT; i++) kj.append(double(t.kit.p[i]));
        j["kitName"] = t.kit.name; j["kit"] = kj;
        QJsonObject sj;
        for (int i = 0; i < D_COUNT; i++) if (!t.kit.sample[i].isEmpty()) sj[QString::number(i)] = t.kit.sample[i];
        if (!sj.isEmpty()) j["samples"] = sj;
        QJsonArray cs, gs;
        for (const Clip& c : t.clips) cs.append(clipToJson(c));
        for (const Clip& c : t.cells) gs.append(clipToJson(c));
        j["cells"] = gs;
        j["clips"] = cs;
        QJsonArray ls;
        for (const AutoLane& l : t.lanes) {
            if (l.pts.empty()) continue;
            QJsonObject lj;
            lj["param"] = l.param == AP_VOLUME ? QStringLiteral("volume") : QString::fromLatin1(PARAMS[l.param].id);
            QJsonArray pts;
            for (const AutoPoint& p : l.pts) pts.append(QJsonArray{p.tick, double(p.v)});
            lj["points"] = pts;
            lj["repeat"] = l.loop;
            ls.append(lj);
        }
        j["moves"] = ls;
        j["lineOpen"] = t.laneOpen; j["lineKnob"] = t.laneParam;
        ts.append(j);
    }
    o["tracks"] = ts;
    return o;
}

bool Song::fromJson(const QJsonObject& o, Song& s) {
    if (!o.contains("tracks")) return false;
    s = Song();
    s.bpm = float(std::clamp(o["bpm"].toDouble(118), 40.0, 240.0));
    s.swing = float(std::clamp(o["swing"].toDouble(0), 0.0, 1.0));
    s.master = float(std::clamp(o["master"].toDouble(0.8), 0.0, 1.0));
    s.root = std::clamp(o["root"].toInt(9), 0, 11);
    s.scale = std::clamp(o["scale"].toInt(0), 0, NUM_SCALES - 1);
    s.dtime = std::clamp(o["dtime"].toInt(2), 0, 4);
    s.dfb = float(std::clamp(o["dfb"].toDouble(0.45), 0.0, 1.0));
    s.rsize = float(std::clamp(o["rsize"].toDouble(0.55), 0.0, 1.0));
    s.rdamp = float(std::clamp(o["rdamp"].toDouble(0.45), 0.0, 1.0));
    s.loopStart = std::max(0, o["loopStart"].toInt(0));
    s.loopEnd = std::max(s.loopStart + BAR, o["loopEnd"].toInt(8 * BAR));
    s.loopOn = o["loopOn"].toBool(true);
    s.scenes = std::clamp(o["columns"].toInt(8), 4, 32);
    if (o.contains("input")) {
        const QJsonObject ij = o["input"].toObject();
        auto f = [&](const char* k, float def, double lo = 0.0) { return float(std::clamp(ij[k].toDouble(def), lo, 1.0)); };
        s.input.gain = f("gain", 0.5f); s.input.cutoff = f("cutoff", 1.f); s.input.res = f("res", 0.f); s.input.wah = f("wah", 0.f);
        s.input.drive = f("drive", 0.f); s.input.echo = f("echo", 0.f); s.input.reverb = f("reverb", 0.1f); s.input.duck = f("pump", 0.f);
        s.input.vol = f("vol", 0.75f); s.input.pan = f("pan", 0.f, -1.0); s.input.mute = ij["mute"].toBool();
    }
    s.chords.clear();
    for (const auto& v : o["chords"].toArray()) s.chords.push_back(std::clamp(v.toInt(), -42, 42));   // Chords mode and MIDI keys reach far up and down
    if (s.chords.empty()) s.chords.push_back(0);
    s.chordAlts.clear();
    for (const auto& v : o["chordAlts"].toArray()) s.chordAlts.push_back(std::clamp(v.toInt(), -1, 1));
    int maxId = 0;
    for (const auto& tv : o["tracks"].toArray()) {
        if (int(s.tracks.size()) >= MAX_TRACKS) break;
        const QJsonObject j = tv.toObject();
        Track t;
        t.id = j["id"].toInt(); t.name = j["name"].toString(QStringLiteral("Track")); t.role = std::clamp(j["role"].toInt(), 0, R_COUNT - 1);
        t.vol = float(std::clamp(j["vol"].toDouble(0.75), 0.0, 1.0)); t.pan = float(std::clamp(j["pan"].toDouble(0), -1.0, 1.0));
        t.mute = j["mute"].toBool(); t.solo = j["solo"].toBool(); t.follow = j["follow"].toBool(true);
        t.duck = float(std::clamp(j["pump"].toDouble(0), 0.0, 1.0));
        t.octave = std::clamp(j["octave"].toInt(), -2, 4); t.chord = std::clamp(j["chord"].toInt(), 0, 6);
        t.arp = std::clamp(j["arp"].toInt(), 0, 5); t.arpRate = std::clamp(j["arpRate"].toInt(2), 0, 3); t.arpOct = std::clamp(j["arpOct"].toInt(), 0, 2);
        t.patch.name = j["patchName"].toString();
        t.laneOpen = j["lineOpen"].toBool(false);
        t.laneParam = j["lineKnob"].toInt(P_CUTOFF);
        if (t.laneParam != AP_VOLUME && (t.laneParam < 0 || t.laneParam >= P_COUNT || PARAMS[t.laneParam].kind == K_CHOICE)) t.laneParam = P_CUTOFF;
        const QJsonObject pj = j["patch"].toObject();
        for (int i = 0; i < P_COUNT; i++) {
            const double top = PARAMS[i].kind == K_CHOICE ? double(QString::fromLatin1(PARAMS[i].choices).count('|')) : 1.0;
            t.patch.p[i] = float(std::clamp(pj[PARAMS[i].id].toDouble(double(PARAMS[i].def)), 0.0, top));
        }
        t.kit = drumKits()[0];
        t.kit.name = j["kitName"].toString(t.kit.name);
        const QJsonArray kj = j["kit"].toArray();
        for (int i = 0; i < DP_COUNT && i < kj.size(); i++) t.kit.p[i] = float(std::clamp(kj[i].toDouble(), 0.0, 1.0));
        const QJsonObject sj = j["samples"].toObject();
        for (int i = 0; i < D_COUNT; i++) t.kit.sample[i] = sj[QString::number(i)].toString();
        for (const auto& cv : j["clips"].toArray()) { Clip c = clipFromJson(cv.toObject()); c.scene = -1; maxId = std::max(maxId, c.id); t.clips.push_back(std::move(c)); }
        for (const auto& cv : j["cells"].toArray()) {
            Clip c = clipFromJson(cv.toObject());
            if (c.scene < 0 || c.scene >= 64 || t.cell(c.scene)) continue;
            c.start = 0; c.offset = 0; c.len = c.loopLen;
            maxId = std::max(maxId, c.id);
            t.cells.push_back(std::move(c));
        }
        for (const auto& lv : j["moves"].toArray()) {
            const QJsonObject lj = lv.toObject();
            const QString pid = lj["param"].toString();
            AutoLane l; l.param = -1; l.loop = std::max(0, lj["repeat"].toInt());
            if (pid == "volume") l.param = AP_VOLUME;
            else for (int i = 0; i < P_COUNT; i++) if (pid == QLatin1String(PARAMS[i].id)) l.param = i;
            if (l.param < 0) continue;
            for (const auto& pv : lj["points"].toArray()) {
                const QJsonArray a = pv.toArray();
                if (a.size() == 2) l.pts.push_back({std::max(0, a[0].toInt()), float(std::clamp(a[1].toDouble(), 0.0, 1.0))});
            }
            std::stable_sort(l.pts.begin(), l.pts.end(), [](const AutoPoint& a, const AutoPoint& b) { return a.tick < b.tick; });
            if (!l.pts.empty()) t.lanes.push_back(std::move(l));
        }
        maxId = std::max(maxId, t.id);
        s.tracks.push_back(std::move(t));
    }
    s.nextId = std::max(o["nextId"].toInt(1), maxId + 1);
    refreshAutoDrums(s);
    return true;
}

// ---------------------------------------------------------------- sounds

static Patch mk(const char* name, std::initializer_list<std::pair<int, float>> v) {
    Patch p; p.name = QString::fromLatin1(name);
    for (int i = 0; i < P_COUNT; i++) p.p[i] = PARAMS[i].def;
    for (auto& kv : v) p.p[kv.first] = kv.second;
    return p;
}

const std::vector<Patch>& presets() {
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
    return list;
}

QString presetGroup(const QString& name) {
    static const QHash<QString, QString> groups = {
        {"Acid Squelch", "Bass"}, {"Rubber Square", "Bass"}, {"Deep Bass", "Bass"}, {"Zap Bass", "Bass"},
        {"Berlin Sequence", "Sequence and pluck"}, {"Glass Pluck", "Sequence and pluck"}, {"Pulse Wanderer", "Sequence and pluck"},
        {"Hollow Band", "Sequence and pluck"}, {"FM Bell", "Sequence and pluck"}, {"Sample and Hold", "Sequence and pluck"},
        {"String Machine", "Pad"}, {"Supersaw Pad", "Pad"}, {"Phase Choir", "Pad"}, {"Night Drone", "Pad"}, {"Thin Air", "Pad"},
        {"Mellow Lead", "Lead"}, {"Saw Lead", "Lead"}, {"Wide Unison Lead", "Lead"}};
    [[maybe_unused]] static const char* const shown[] = {QT_TRANSLATE_NOOP("Model", "Sequence and pluck"), QT_TRANSLATE_NOOP("Model", "Other")};   // group names, translated where shown
    return groups.value(name, QStringLiteral("Other"));
}

Patch presetByName(const QString& name) {
    for (const Patch& p : presets()) if (p.name == name) return p;
    return presets()[0];
}

const std::vector<DrumKit>& drumKits() {
    static const std::vector<DrumKit> list = [] {
        auto kit = [](const char* name, std::initializer_list<float> v, float drive, float delay, float reverb) {
            DrumKit k; k.name = QString::fromLatin1(name);
            int i = 0; for (float f : v) k.p[i++] = f;
            k.p[DP_DRIVE] = drive; k.p[DP_DELAY] = delay; k.p[DP_REVERB] = reverb;
            return k;
        };
        auto sampled = [](const char* name, std::initializer_list<const char*> files, std::initializer_list<float> levels, float drive, float delay, float reverb) {
            DrumKit k; k.name = QString::fromLatin1(name);
            int i = 0; for (const char* f : files) { k.sample[i] = QStringLiteral("nz:") + QString::fromLatin1(f); k.p[i * 3 + 1] = 0.5f; k.p[i * 3 + 2] = 1.f; i++; }
            i = 0; for (float l : levels) k.p[3 * i++] = l;
            k.p[DP_DRIVE] = drive; k.p[DP_DELAY] = delay; k.p[DP_REVERB] = reverb;
            return k;
        };
        //                         kick           snare          clap           closed hat     open hat       tom            rim            cymbal
        return std::vector<DrumKit>{
            kit(QT_TRANSLATE_NOOP("Model", "Machine kit"), {.85f, .5f, .35f, .7f, .5f, .4f, .65f, .5f, .4f, .55f, .5f, .3f, .5f, .5f, .4f, .65f, .5f, .4f, .6f, .5f, .3f, .45f, .5f, .4f}, .2f, .05f, .12f),
            kit(QT_TRANSLATE_NOOP("Model", "Soft boom kit"), {.9f, .4f, .7f, .6f, .45f, .5f, .55f, .45f, .55f, .45f, .45f, .2f, .45f, .45f, .5f, .7f, .4f, .6f, .5f, .5f, .3f, .4f, .45f, .5f}, .05f, .1f, .25f),
            kit(QT_TRANSLATE_NOOP("Model", "Tight acid kit"), {.9f, .55f, .22f, .7f, .6f, .25f, .7f, .55f, .3f, .6f, .6f, .15f, .5f, .6f, .25f, .6f, .6f, .25f, .65f, .55f, .2f, .4f, .6f, .25f}, .5f, .08f, .08f),
            // kits that play sounds from the Nachtzug library: each drum at its own pitch, playing out in full
            sampled(QT_TRANSLATE_NOOP("Model", "Night train kit"), {"Kicks/Deep boom", "Snares/Tape", "Train/Rail joint", "Hats/Shaker", "Train/Steam puff", "Percussion/Tom low", "Train/Coupling clank", "Train/Door chime"},
                    {.85f, .66f, .62f, .5f, .5f, .6f, .5f, .45f}, .1f, .1f, .2f),
            sampled(QT_TRANSLATE_NOOP("Model", "Tape kit"), {"Kicks/Punch", "Snares/Tape", "Claps/Classic", "Hats/Closed", "Hats/Open", "Percussion/Tom mid", "Percussion/Rimshot", "Cymbals/Crash"},
                    {.85f, .66f, .6f, .52f, .48f, .6f, .55f, .45f}, .25f, .06f, .1f),
            sampled(QT_TRANSLATE_NOOP("Model", "Electro kit"), {"Kicks/Hard", "Snares/Electro", "Claps/Big room", "Hats/Tight", "Hats/Sizzle", "Tones/Zap", "Percussion/Clave", "Cymbals/Crash"},
                    {.85f, .62f, .6f, .5f, .45f, .55f, .5f, .45f}, .3f, .08f, .08f),
            sampled(QT_TRANSLATE_NOOP("Model", "Hand percussion kit"), {"Kicks/Soft felt", "Percussion/Conga high", "Claps/Snap", "Hats/Shaker", "Hats/Tambourine", "Percussion/Conga low", "Percussion/Clave", "Percussion/Cowbell"},
                    {.85f, .62f, .55f, .52f, .45f, .62f, .52f, .45f}, .05f, .05f, .18f),
        };
    }();
    return list;
}

static float rr(std::mt19937& g, float a, float b) { return std::uniform_real_distribution<float>(a, b)(g); }
static int ri(std::mt19937& g, int n) { return n <= 1 ? 0 : std::uniform_int_distribution<int>(0, n - 1)(g); }
static bool ch(std::mt19937& g, float p) { return rr(g, 0.f, 1.f) < p; }

Patch rollPatch(int role, std::mt19937& g) {
    const int kind = role == R_PAD ? 1 : role == R_LEAD ? 2 : 0; // pluck, pad, lead
    Patch p = mk(kind == 0 ? QT_TRANSLATE_NOOP("Model", "Rolled pluck") : kind == 1 ? QT_TRANSLATE_NOOP("Model", "Rolled pad") : QT_TRANSLATE_NOOP("Model", "Rolled lead"), {});
    float* v = p.p;
    v[P_WAVE1] = float(ri(g, 3)); v[P_WAVE2] = float(ri(g, 4)); v[P_TUNE2] = float(ri(g, 4));
    v[P_DETUNE] = rr(g, 0, .6f); v[P_MIX] = ch(g, .3f) ? 0 : rr(g, .15f, .6f);
    v[P_SUB] = ch(g, .4f) ? rr(g, .2f, .7f) : 0; v[P_NOISE] = ch(g, .2f) ? rr(g, .05f, .25f) : 0;
    v[P_PW] = ch(g, .5f) ? rr(g, 0, .8f) : 0; v[P_PHRATE] = rr(g, .1f, .6f);
    v[P_SLOPE] = ch(g, .2f) ? 1.f : ch(g, .1f) ? 2.f : 0.f; v[P_LFOWAVE] = ch(g, .15f) ? 2.f : 0.f;
    v[P_UNISON] = ch(g, kind == 1 ? .6f : .3f) ? float(1 + ri(g, kind == 1 ? 4 : 2)) : 0.f;
    v[P_UDETUNE] = rr(g, .15f, .5f); v[P_WIDTH] = kind == 1 ? rr(g, .4f, .9f) : rr(g, 0, .4f);
    v[P_FM] = ch(g, .15f) ? rr(g, .2f, .6f) : 0; v[P_PENV] = kind == 0 && ch(g, .15f) ? rr(g, .1f, .4f) : 0;
    v[P_LFOPW] = v[P_PW] > 0 && ch(g, .4f) ? rr(g, .2f, .6f) : 0;
    if (kind == 0) {
        v[P_CUTOFF] = rr(g, .2f, .5f); v[P_RES] = rr(g, .2f, .88f); v[P_ENVAMT] = rr(g, .35f, .75f); v[P_ACCENT] = rr(g, .4f, .9f);
        v[P_FA] = 0; v[P_FD] = rr(g, .48f, .7f); v[P_FS] = rr(g, 0, .2f); v[P_AA] = 0; v[P_AD] = rr(g, .55f, .75f); v[P_AS] = rr(g, 0, .6f); v[P_AR] = rr(g, .25f, .6f);
        v[P_MONO] = ch(g, role == R_BASS ? .85f : .4f) ? 1.f : 0.f; v[P_GLIDE] = ch(g, .25f) ? rr(g, .1f, .3f) : 0;
        v[P_LFORATE] = rr(g, .1f, .8f); v[P_LFOCUT] = ch(g, .3f) ? rr(g, .05f, .3f) : 0; v[P_LFOPITCH] = 0;
        v[P_DRIVE] = rr(g, .05f, .6f); v[P_PHASER] = ch(g, .25f) ? rr(g, .2f, .6f) : 0; v[P_CHORUS] = ch(g, .3f) ? rr(g, .2f, .5f) : 0;
        v[P_DELAY] = role == R_BASS ? rr(g, 0, .3f) : rr(g, .25f, .6f); v[P_REVERB] = rr(g, 0, .3f);
    } else if (kind == 1) {
        v[P_WAVE1] = float(ri(g, 2)); v[P_DETUNE] = rr(g, .3f, .8f); v[P_MIX] = rr(g, .35f, .6f);
        v[P_CUTOFF] = rr(g, .4f, .65f); v[P_RES] = rr(g, .05f, .4f); v[P_ENVAMT] = rr(g, .05f, .3f);
        v[P_FA] = rr(g, .6f, .85f); v[P_FD] = rr(g, .7f, .9f); v[P_FS] = rr(g, .4f, .9f);
        v[P_AA] = rr(g, .6f, .82f); v[P_AD] = .8f; v[P_AS] = rr(g, .7f, 1); v[P_AR] = rr(g, .7f, .9f);
        v[P_MONO] = 0; v[P_GLIDE] = 0; v[P_LFOWAVE] = 0; v[P_LFORATE] = rr(g, .05f, .4f); v[P_LFOCUT] = rr(g, 0, .4f); v[P_LFOPITCH] = 0;
        v[P_DRIVE] = rr(g, 0, .2f); v[P_PHASER] = rr(g, 0, .8f); v[P_CHORUS] = rr(g, .3f, .9f); v[P_DELAY] = rr(g, .1f, .4f); v[P_REVERB] = rr(g, .35f, .75f);
    } else {
        v[P_CUTOFF] = rr(g, .5f, .7f); v[P_RES] = rr(g, .1f, .45f); v[P_ENVAMT] = rr(g, .1f, .4f);
        v[P_FA] = 0; v[P_FD] = rr(g, .6f, .8f); v[P_FS] = rr(g, .3f, .7f);
        v[P_AA] = rr(g, 0, .4f); v[P_AD] = .7f; v[P_AS] = rr(g, .7f, 1); v[P_AR] = rr(g, .5f, .7f);
        v[P_MONO] = 1; v[P_GLIDE] = rr(g, .15f, .45f); v[P_LFOWAVE] = 0; v[P_LFORATE] = rr(g, .74f, .82f); v[P_LFOPITCH] = rr(g, .15f, .45f); v[P_LFOCUT] = 0;
        v[P_DRIVE] = rr(g, .1f, .5f); v[P_PHASER] = 0; v[P_CHORUS] = rr(g, 0, .4f); v[P_DELAY] = rr(g, .35f, .65f); v[P_REVERB] = rr(g, .2f, .5f);
    }
    return p;
}

Patch mutatePatch(const Patch& src, std::mt19937& g) {
    Patch p = src;
    for (int i = 0; i < P_COUNT; i++)
        if (PARAMS[i].kind != K_CHOICE) p.p[i] = std::clamp(p.p[i] + rr(g, -.09f, .09f), 0.f, 1.f);
    if (!p.name.endsWith(" (mutated)")) p.name += " (mutated)";
    return p;
}

// ---------------------------------------------------------------- generators

enum { G_ACID, G_BERLIN, G_OCTAVE, G_PULSE, G_ARPRUN, G_CHORD, G_STABS, G_MELODY, G_SPARSE, G_D_FOUR, G_D_ACID, G_D_MOTORIK, G_D_BREAK };

std::vector<GenDef> generatorsFor(int role) {
    switch (role) {
    case R_BASS: return {{QT_TRANSLATE_NOOP("Model", "Acid line"), G_ACID, QT_TRANSLATE_NOOP("Model", "busy sixteenths with accents and slides")}, {QT_TRANSLATE_NOOP("Model", "Octave bounce"), G_OCTAVE, QT_TRANSLATE_NOOP("Model", "the key note jumping between two octaves")},
                         {QT_TRANSLATE_NOOP("Model", "Root pulse"), G_PULSE, QT_TRANSLATE_NOOP("Model", "steady eighths on the key note, for a driving groove")}, {QT_TRANSLATE_NOOP("Model", "Berlin sequence"), G_BERLIN, QT_TRANSLATE_NOOP("Model", "a repeating run of chord notes")}};
    case R_SEQ: return {{QT_TRANSLATE_NOOP("Model", "Berlin sequence"), G_BERLIN, QT_TRANSLATE_NOOP("Model", "the classic rolling run of chord notes")}, {QT_TRANSLATE_NOOP("Model", "Arpeggio run"), G_ARPRUN, QT_TRANSLATE_NOOP("Model", "chord notes up and down, like an arpeggiator")},
                        {QT_TRANSLATE_NOOP("Model", "Acid line"), G_ACID, QT_TRANSLATE_NOOP("Model", "busy sixteenths with accents and slides")}};
    case R_PAD: return {{QT_TRANSLATE_NOOP("Model", "Held chord"), G_CHORD, QT_TRANSLATE_NOOP("Model", "one long chord per bar")}, {QT_TRANSLATE_NOOP("Model", "Chord stabs"), G_STABS, QT_TRANSLATE_NOOP("Model", "short chords on a rhythm")}};
    case R_LEAD: return {{QT_TRANSLATE_NOOP("Model", "Melody"), G_MELODY, QT_TRANSLATE_NOOP("Model", "a tune that moves between chord notes")}, {QT_TRANSLATE_NOOP("Model", "Sparse melody"), G_SPARSE, QT_TRANSLATE_NOOP("Model", "a few long notes, with room to breathe")},
                         {QT_TRANSLATE_NOOP("Model", "Arpeggio run"), G_ARPRUN, QT_TRANSLATE_NOOP("Model", "chord notes up and down")}};
    default: return {{QT_TRANSLATE_NOOP("Model", "Four on the floor"), G_D_FOUR, QT_TRANSLATE_NOOP("Model", "kick on every beat, claps and open hats")}, {QT_TRANSLATE_NOOP("Model", "Acid house"), G_D_ACID, QT_TRANSLATE_NOOP("Model", "busy hats and rimshots")},
                     {QT_TRANSLATE_NOOP("Model", "Motorik"), G_D_MOTORIK, QT_TRANSLATE_NOOP("Model", "the steady krautrock beat")}, {QT_TRANSLATE_NOOP("Model", "Broken beat"), G_D_BREAK, QT_TRANSLATE_NOOP("Model", "syncopated, off-kilter kicks")}};
    }
}

static void add(Clip& c, int start, int len, int deg, bool acc = false, bool slide = false) {
    if (start >= c.loopLen) return;
    Note n; n.start = start; n.len = std::max(1, std::min(len, c.loopLen - start + (slide ? 4 : 0))); n.deg = deg; n.acc = acc; n.slide = slide;
    c.notes.push_back(n);
}

static void drumRow(Clip& c, int bar, int drum, const char* pat) {
    for (int i = 0; i < 16 && pat[i]; i++)
        if (pat[i] != '.') add(c, bar * BAR + i * 24, 12, drum, pat[i] == 'X');
}

static void generateNotes(const Song& s, const Track& t, Clip& c, int genId, float density, std::mt19937& g);

// A new idea keeps the notes you marked Keep: the new pattern makes room for them.
void generate(const Song& s, const Track& t, Clip& c, int genId, float density, std::mt19937& g) {
    std::vector<Note> kept;
    for (const Note& n : c.notes) if (n.lock) kept.push_back(n);
    generateNotes(s, t, c, genId, density, g);
    if (kept.empty()) return;
    std::erase_if(c.notes, [&](const Note& n) {
        for (const Note& k : kept) {
            if (k.start >= c.loopLen) continue;
            if (t.isDrums() ? (n.deg == k.deg && n.start == k.start) : (n.start < k.start + k.len && k.start < n.start + n.len)) return true;
        }
        return false;
    });
    for (const Note& k : kept) if (k.start < c.loopLen) c.notes.push_back(k);
    std::sort(c.notes.begin(), c.notes.end(), [](const Note& a, const Note& b) { return a.start < b.start; });
}

static void generateNotes(const Song& s, const Track& t, Clip& c, int genId, float density, std::mt19937& g) {
    c.notes.clear();
    c.ad.on = false;
    for (const GenDef& d : generatorsFor(t.role)) if (d.id == genId) c.name = QString::fromLatin1(d.name);
    const int n = s.sc().n, bars = std::max(1, (c.loopLen + BAR - 1) / BAR);
    auto q = [&](int k) { return s.q(k); };
    auto pick = [&](const std::vector<int>& v) { return v[size_t(ri(g, int(v.size())))]; };
    // tracks that do not follow the chord lane get the chord root added here instead
    auto chordAt = [&](int bar) { return t.follow ? 0 : s.chordAt((c.start + bar * BAR) / BAR); };

    switch (genId) {
    case G_ACID: {
        const std::vector<int> pool = {0, 0, 0, 0, 0, n, n, q(2), q(4), q(4), q(6), n + q(2)};
        struct St { bool on; int deg; bool acc, sld; } st[16];
        for (int i = 0; i < 16; i++) st[i] = {i == 0 || ch(g, .5f + .45f * density), pick(pool), ch(g, .3f), ch(g, .22f)};
        for (int b = 0; b < bars; b++) {
            if (b > 0 && b == bars - 1) for (int k = 0; k < 3; k++) { St& x = st[ri(g, 16)]; x.deg = pick(pool); x.on = true; x.sld = ch(g, .4f); }
            for (int i = 0; i < 16; i++) if (st[i].on) add(c, b * BAR + i * 24, st[i].sld ? 24 : 18, st[i].deg + chordAt(b), st[i].acc, st[i].sld);
        }
        break;
    }
    case G_BERLIN: {
        const std::vector<int> pool = {0, q(2), q(4), q(4), n, n, n + q(2), n + q(4)};
        int deg[8], prev = 0; const int rest = ch(g, .7f - .6f * density) ? 1 + ri(g, 7) : -1, rat = ch(g, density) ? 1 + ri(g, 7) : -1;
        for (int i = 0; i < 8; i++) { int d = 0; if (i) do d = pick(pool); while (d == prev); deg[i] = prev = d; }
        for (int i = 0; i * 24 < c.loopLen; i++) {
            const int k = i % 8, b = i / 16;
            if (k == rest) continue;
            if (k == rat) { add(c, i * 24, 9, deg[k] + chordAt(b)); add(c, i * 24 + 12, 9, deg[k] + chordAt(b)); }
            else add(c, i * 24, 15, deg[k] + chordAt(b), k == 0);
        }
        break;
    }
    case G_OCTAVE:
        for (int i = 0; i * 48 < c.loopLen; i++) add(c, i * 48, 36, (i % 2 ? n : 0) + chordAt(i / 8), i % 2 == 0);
        break;
    case G_PULSE: {
        const int last = c.loopLen / 48 - 1;
        for (int i = 0; i * 48 < c.loopLen; i++) add(c, i * 48, 34, (i == last && ch(g, .7f) ? pick({q(4), q(6), n}) : 0) + chordAt(i / 8), i % 2 == 0);
        break;
    }
    case G_ARPRUN: {
        const std::vector<std::vector<int>> shapes = {
            {0, q(2), q(4), n, n + q(2), n + q(4), n + q(2), n}, {0, q(4), n, q(4), n + q(2), n, q(4), n}, {n + q(4), n + q(2), n, q(4), q(2), 0, q(2), q(4)}};
        const auto& sh = shapes[size_t(ri(g, int(shapes.size())))];
        for (int i = 0; i * 24 < c.loopLen; i++) add(c, i * 24, 18, sh[size_t(i % 8)] + chordAt(i / 16), i % 8 == 0);
        break;
    }
    case G_CHORD:
        for (int b = 0; b < bars; b++) add(c, b * BAR, BAR - 6, chordAt(b));
        break;
    case G_STABS: {
        const std::vector<std::vector<int>> rh = {{0, 6, 12}, {4, 12}, {2, 6, 10, 14}, {0, 3, 6, 10, 12}};
        const auto& r = rh[size_t(density < .3f ? 1 : density > .75f ? 3 : ri(g, int(rh.size())))];
        for (int b = 0; b < bars; b++) for (int st : r) add(c, b * BAR + st * 24, 20, chordAt(b), st == 0);
        break;
    }
    case G_MELODY:
    case G_SPARSE: {
        // rhythm templates: pairs of (start, length) in 16ths
        const std::vector<std::vector<int>> dense = {{0, 3, 3, 1, 4, 2, 6, 2, 8, 4, 12, 2, 14, 2}, {0, 2, 2, 2, 4, 4, 8, 2, 10, 2, 12, 4}, {0, 6, 6, 2, 8, 3, 11, 1, 12, 4}, {2, 2, 4, 2, 6, 2, 8, 6, 14, 2}};
        const std::vector<std::vector<int>> sparse = {{0, 8, 8, 8}, {0, 12, 12, 4}, {0, 6, 8, 8}, {4, 12}, {0, 16}};
        const float d = genId == G_MELODY ? density : density * 0.4f;
        int prev = n;
        for (int b = 0; b < bars; b++) {
            const auto& tpl = ch(g, d * 1.3f) ? dense : sparse;   // busier settings pick the dense rhythms more often
            const auto& r = tpl[size_t(ri(g, int(tpl.size())))];
            const int root = chordAt(b);
            for (size_t i = 0; i + 1 < r.size(); i += 2) {
                int d;
                if (r[i] % 4 == 0 || ch(g, .35f)) { d = root + pick({0, q(2), q(4)}) + n; while (d - prev > n / 2 + 1) d -= n; while (prev - d > n / 2 + 1) d += n; }
                else d = prev + pick({-2, -1, -1, 1, 1, 2});
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

// ---------------------------------------------------------------- auto drummer

static unsigned autoKey(const Clip& c) {
    unsigned h = 2166136261u;
    for (unsigned v : {unsigned(c.ad.style), unsigned(c.ad.intensity * 1000.f), unsigned(c.ad.complexity * 1000.f), unsigned(c.ad.fillEvery), c.ad.seed, unsigned(c.len),
                       unsigned(c.ad.backbeat), unsigned(c.ad.hats), unsigned(c.ad.shape), unsigned(c.ad.shuffle * 1000.f), unsigned(c.ad.crash)}) h = (h ^ v) * 16777619u;
    return h | 1u;
}

static void autoDrumFillNotes(Clip& c);

// The drummer writes the clip again; hits you marked Keep stay, and the drummer's own hit on the same drum at the
// same moment makes way for them.
void autoDrumFill(Clip& c) {
    std::vector<Note> kept;
    for (const Note& n : c.notes) if (n.lock) kept.push_back(n);
    autoDrumFillNotes(c);
    if (kept.empty()) return;
    std::erase_if(c.notes, [&](const Note& n) { for (const Note& k : kept) if (k.start == n.start && k.deg == n.deg) return true; return false; });
    for (const Note& k : kept) if (k.start < c.loopLen) c.notes.push_back(k);
}

static void autoDrumFillNotes(Clip& c) {
    c.notes.clear();
    c.loopLen = std::max(c.len, 24); c.offset = 0;
    const AutoDrum& a = c.ad;
    const float X = a.complexity;
    const int bars = std::max(1, (c.len + BAR - 1) / BAR);
    const int late = int(a.shuffle * 8.f);                               // shuffle: every second sixteenth comes a little late
    auto hit = [&](int bar, int step, int drum, bool acc) { add(c, bar * BAR + step * 24 + (step % 2 ? late : 0), 12, drum, acc); };
    for (int b = 0; b < bars; b++) {
        std::mt19937 g(a.seed * 7919u + unsigned(b) * 104729u + 17u);
        // how hard the drummer plays in this bar
        const float along = bars > 1 ? float(b) / float(bars - 1) : 1.f;
        const float I = a.intensity * (a.shape == 1 ? 0.3f + 0.7f * along : a.shape == 2 ? 1.f - 0.7f * along : 1.f);
        // fills come every few bars and always lead out of the clip into whatever follows
        const bool fill = a.fillEvery > 0 && ((b + 1) % a.fillEvery == 0 || (b == bars - 1 && bars > 1)) && I > 0.12f;
        // the regular beat plays up to this step: the harder the drummer plays, the earlier the fill starts (last beat .. last two)
        const int stop = fill ? std::clamp(14 - int(std::lround(I * 8.f)), 8, 13) : 16;
        if (a.crash && I > 0.45f && b > 0 && a.fillEvery > 0 && b % a.fillEvery == 0) hit(b, 0, D_CYM, true);   // crash after a fill
        if (a.crash && a.style == 6 && b % 8 == 0 && I > 0.3f) hit(b, 0, D_CYM, false);
        for (int st = 0; st < stop; st++) {
            const bool beat = st % 4 == 0, off8 = st % 4 == 2, odd = st % 2 == 1;
            // kick
            bool kick = false, kAcc = true;
            switch (a.style) {
            case 0: kick = beat || (I > .75f && st == 14 && ch(g, X * .6f)); break;
            case 1: kick = beat || (I > .5f && st == 14) || (I > .6f && (st == 7 || st == 11) && ch(g, X * .4f)); kAcc = beat; break;
            case 2: kick = st % 8 == 0 || (I > .3f && st % 8 == 4) || (I > .7f && st == 10); kAcc = st % 8 == 0; break;
            case 3: kick = st == 0 || st == 10 || (I > .4f && st == 6) || ((st == 3 || st == 13) && ch(g, X * .5f)); kAcc = st == 0 || st == 10; break;
            case 4: kick = st == 0 || (I > .4f && st == 10) || (st == 7 && ch(g, X * .4f)); kAcc = st == 0; break;
            case 5: kick = st == 0 || st == 10 || (I > .3f && st == 6) || (I > .7f && st == 13 && ch(g, X * .6f)); kAcc = st == 0 || st == 10; break;
            default: kick = beat; kAcc = st == 0; break;
            }
            if (kick) hit(b, st, D_KICK, kAcc);
            // backbeat
            const int own = a.style == 0 ? D_CLAP : a.style == 6 ? (I > .7f ? D_RIM : -1) : D_SNARE;
            const int bb = a.backbeat == 1 ? D_CLAP : a.backbeat == 2 ? D_SNARE : a.backbeat == 3 ? D_RIM : a.backbeat == 4 ? -1 : own;
            if (I > .2f && bb >= 0) {
                if (a.style == 4) { if (st == 8) hit(b, st, bb, true); }
                else if (st == 4 || st == 12) {
                    hit(b, st, bb, true);
                    if (I > .8f && a.backbeat == 0 && a.style != 6) hit(b, st, bb == D_CLAP ? D_SNARE : D_CLAP, false);
                }
                if (a.style == 3 && (st == 7 || st == 9) && ch(g, X * .6f)) hit(b, st, bb, false);
            }
            // hats
            if (a.hats == 1) { if (!odd) hit(b, st, D_CHAT, beat); }
            else if (a.hats == 2) hit(b, st, D_CHAT, !odd);
            else if (a.hats == 3) { if (off8) hit(b, st, D_OHAT, true); }
            else if (a.hats == 0) {
                const bool open = (a.style <= 1 && I >= .45f && off8) || (a.style == 3 && I > .5f && st == 14) || (a.style == 4 && I > .6f && st == 6) || (a.style == 5 && I > .6f && st == 14);
                const bool sixteenths = I >= .65f || (a.style == 6 && I >= .4f);
                if (open) hit(b, st, D_OHAT, true);
                else if (sixteenths) hit(b, st, D_CHAT, !odd);
                else if (I >= .25f && !odd) hit(b, st, D_CHAT, beat);
                else if (I >= .1f && beat) hit(b, st, D_CHAT, false);
            }
            // extras, more of them the harder the drummer plays, so every bit of Energy is heard, with or without hats
            const float more = std::clamp((I - .2f) / .8f, 0.f, 1.f);         // 0 at 20 % energy .. 1 at full
            if (more > 0.f && odd && ch(g, (.08f + X * .3f) * more)) hit(b, st, D_RIM, false);
            if (more > 0.f && (st == 11 || st == 15) && ch(g, (.05f + X * .25f) * more)) hit(b, st, D_TOM, false);
            if (bb >= 0 && bb != D_RIM && more > 0.f && (st == 7 || st == 10 || st == 15) && ch(g, (.1f + X * .35f) * more)) hit(b, st, D_SNARE, false);   // ghost notes
            if (!kick && more > .3f && (st == 6 || st == 11 || st == 14) && ch(g, (more - .3f) * (.25f + X * .4f))) hit(b, st, D_KICK, false);   // pushed kicks
        }
        if (fill) {
            if (I < .2f) { hit(b, 14, D_SNARE, false); hit(b, 15, D_SNARE, true); }
            else for (int st = stop; st < 16; st++) {
                const bool lastBeat = st >= 12;
                if (!lastBeat && st % 2 == 1 && !ch(g, X)) continue;          // eighths first, then sixteenths
                if (I < .45f && st % 2 == 1 && st < 14 && !ch(g, I)) continue;  // a lighter fill when the drummer plays softly
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
        for (Clip& c : t.clips) if (c.ad.on && c.adKey != autoKey(c)) { autoDrumFill(c); any = true; }
        for (Clip& c : t.cells) if (c.ad.on && c.adKey != autoKey(c)) { autoDrumFill(c); any = true; }
    }
    return any;
}

void varyClip(const Song& s, const Track& t, Clip& c, std::mt19937& g) {
    if (c.ad.on) { c.ad.seed = unsigned(g()); return; }            // a new take from the drummer
    if (t.isDrums()) {
        // add or remove one ghost hit; kept hits stay, and nothing is added on top of one
        for (int tries = 0; tries < 8; tries++) {
            const int step = ri(g, std::max(1, c.loopLen / 24)) * 24, drum = std::vector<int>{D_CHAT, D_CHAT, D_SNARE, D_RIM, D_KICK, D_OHAT}[size_t(ri(g, 6))];
            auto it = std::find_if(c.notes.begin(), c.notes.end(), [&](const Note& n) { return n.start == step && n.deg == drum; });
            if (it != c.notes.end()) { if (it->lock) continue; c.notes.erase(it); return; }
            add(c, step, 12, drum);
            return;
        }
        return;
    }
    (void)s;
    std::vector<size_t> free;
    for (size_t i = 0; i < c.notes.size(); i++) if (!c.notes[i].lock) free.push_back(i);
    if (free.empty()) return;
    const int count = 1 + ri(g, 2);
    for (int k = 0; k < count; k++) {
        Note& n = c.notes[free[size_t(ri(g, int(free.size())))]];
        const float r = rr(g, 0, 1);
        if (r < .65f) n.deg = std::clamp(n.deg + std::vector<int>{-2, -1, 1, 2}[size_t(ri(g, 4))], DEG_MIN, DEG_MAX);
        else if (r < .85f) n.acc = !n.acc;
        else n.slide = !n.slide;
    }
}

std::vector<int> randomProgression(const Song& s, std::mt19937& g) {
    const bool major = s.degToSemis(s.q(2)) == 4;
    static const std::vector<std::vector<int>> minorP = {{0, 0, 5, 6}, {0, 5, 2, 6}, {0, 3, 5, 4}, {0, 6, 5, 6}, {0, 0, 3, 3}, {0, 5, 3, 4}, {0, 0, 0, 0, 5, 5, 6, 6}, {0, 2, 5, 4}, {0, 0, 6, 6, 5, 5, 6, 6}};
    static const std::vector<std::vector<int>> majorP = {{0, 4, 5, 3}, {0, 5, 3, 4}, {0, 3, 0, 4}, {0, 0, 3, 4}, {0, 0, 0, 0, 3, 3, 4, 4}};
    const auto& list = major ? majorP : minorP;
    std::vector<int> out = list[size_t(ri(g, int(list.size())))];
    for (int& d : out) d = s.q(d);
    return out;
}

// ---------------------------------------------------------------- tracks

Track makeTrack(Song& s, int role) {
    Track t;
    t.id = s.newId(); t.role = role; t.name = QString::fromLatin1(ROLE_NAMES[role]);
    static const char* sound[R_COUNT] = {"Acid Squelch", "Berlin Sequence", "String Machine", "Mellow Lead", "Acid Squelch"};
    static const int oct[R_COUNT] = {0, 1, 2, 2, 0};
    t.patch = presetByName(QString::fromLatin1(sound[role]));
    t.kit = drumKits()[0];
    t.octave = oct[role];
    t.chord = role == R_PAD ? 3 : 0;
    t.follow = role != R_LEAD && role != R_DRUMS;
    t.vol = role == R_PAD ? 0.6f : role == R_DRUMS ? 0.8f : 0.72f;
    int same = 0; for (auto& o : s.tracks) if (o.role == role) same++;
    if (same) t.name += " " + QString::number(same + 1);
    return t;
}

int addTrackWithClip(Song& s, int role, std::mt19937& rng) {
    if (int(s.tracks.size()) >= MAX_TRACKS) return -1;
    Track t = makeTrack(s, role);
    Clip c; c.id = s.newId();
    c.start = s.loopStart;
    c.len = s.loopEnd > s.loopStart ? s.loopEnd - s.loopStart : 8 * BAR;
    c.loopLen = std::min(c.len, role == R_LEAD ? 4 * BAR : BAR);
    if (role == R_DRUMS) { c.ad.on = true; c.ad.seed = unsigned(rng()); autoDrumFill(c); }   // drums start with the auto drummer
    else generate(s, t, c, generatorsFor(role)[0].id, 0.6f, rng);
    t.clips.push_back(c);
    s.tracks.push_back(t);
    return t.id;
}

Song demoSong() {
    Song s;
    std::mt19937 g(11);
    s.bpm = 122; s.chords = {0, 0, 5, 6};
    addTrackWithClip(s, R_DRUMS, g);
    addTrackWithClip(s, R_BASS, g);
    addTrackWithClip(s, R_SEQ, g);
    addTrackWithClip(s, R_PAD, g);
    s.tracks.push_back(makeTrack(s, R_LEAD));
    return s;
}

QString pitchName(int pc) {
    pc = ((pc % 12) + 12) % 12;
    if (pc == 11) return QCoreApplication::translate("Model", "B", "note name: the note a semitone below C (German: H)");
    return QString::fromUtf8(NOTE_NAMES[pc]);
}

QString ui(const char* text) { return QCoreApplication::translate("Model", text); }
QString ui(const QString& text) { return text.isEmpty() ? text : QCoreApplication::translate("Model", text.toUtf8().constData()); }

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
                if (i == j || drop[j] || std::abs(b.deg - a.deg) != 1 || std::abs(b.start - a.start) > grid / 2) continue;
                if (b.len >= 2 * a.len) { drop[i] = true; break; }
            }
        }
        for (size_t i = ns.size(); i-- > 0;) if (drop[i]) { ns.erase(ns.begin() + long(i)); r.removed++; }
    }

    // onto the grid: only notes on no musical line at all (a played-in note), so sixteenths survive a coarse grid
    for (Note& n : ns) {
        if (n.lock || !heard(n) || n.start % 12 == 0 || n.start % 16 == 0) continue;
        const int s = int(std::lround(double(n.start) / grid)) * grid % L;
        if (s != n.start) { n.start = s; r.snapped++; }
    }

    // doubled notes: one note, as long as the longest, accented if either was
    std::sort(ns.begin(), ns.end(), [](const Note& a, const Note& b) { return std::tie(a.start, a.deg, a.alt) < std::tie(b.start, b.deg, b.alt); });
    for (size_t i = 1; i < ns.size();) {
        Note& a = ns[i - 1];
        const Note& b = ns[i];
        if (a.start == b.start && a.deg == b.deg && (drums || a.alt == b.alt)) {
            a.len = std::max(a.len, b.len); a.acc |= b.acc; a.slide |= b.slide; a.lock |= b.lock; a.chance = std::max(a.chance, b.chance);
            ns.erase(ns.begin() + long(i));
            r.merged++;
        } else i++;
    }
    // a note that runs into the next one on its row stops where that one starts
    if (!drums)
        for (size_t i = 0; i < ns.size(); i++)
            for (size_t j = i + 1; j < ns.size(); j++)
                if (ns[j].deg == ns[i].deg && ns[j].alt == ns[i].alt && ns[j].start > ns[i].start) {
                    if (!ns[i].slide && !ns[i].lock && ns[i].start + ns[i].len > ns[j].start) ns[i].len = std::max(6, ns[j].start - ns[i].start);
                    break;
                }

    // the repeats: a period of one or two bars that comes round at least three times
    using Key = std::tuple<int, int, int>;   // place in the period, row, semitone off the scale
    int bestP = 0; double bestAgree = 0.0;
    for (int P : {BAR, 2 * BAR}) {
        if (L % P || L / P < 3) continue;
        const int R = L / P;
        // how much of the pattern most of the repeats agree on
        std::map<Key, std::set<int>> reps;
        int all = 0;
        for (const Note& n : ns) if (heard(n)) { reps[{n.start % P, n.deg, n.alt}].insert(n.start / P); all++; }
        int agreed = 0;
        for (const auto& [k, in] : reps) if (in.size() >= 2 && 2 * int(in.size()) > R) agreed += int(in.size());
        const double agree = all ? double(agreed) / all : 0.0;
        if (agree > bestAgree) { bestAgree = agree; bestP = P; }
    }
    if (bestP && bestAgree >= 0.6) {
        const int P = bestP, R = L / P;
        std::map<Key, std::vector<int>> where;   // which repeats have each note
        std::map<Key, size_t> example;           // a note to copy it from
        for (size_t i = 0; i < ns.size(); i++) {
            if (!heard(ns[i])) continue;
            const Key k{ns[i].start % P, ns[i].deg, ns[i].alt};
            where[k].push_back(ns[i].start / P);
            if (!example.count(k)) example[k] = i;
        }
        auto majority = [&](const Key& k) { const auto it = where.find(k); return it != where.end() && int(it->second.size()) >= 2 && 2 * int(it->second.size()) > R; };
        auto busyAt = [&](int rep, int pos) {
            return std::any_of(ns.begin(), ns.end(), [&](const Note& n) { return heard(n) && n.start / P == rep && n.start % P == pos; });
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
                    n.deg = std::get<1>(other); n.alt = std::get<2>(other);
                    where[k].clear(); where[other].push_back(rep);
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
                if (drums && std::any_of(ns.begin(), ns.end(), [&](const Note& n) { return n.start == rep * P + pos && n.deg == std::get<1>(k); })) continue;
                Note n = ns[example[k]];
                n.start = rep * P + pos; n.sel = false; n.lock = false;
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
                for (int o = 0; o < R && alone; o++) if (o != rep && busyAt(o, pos)) alone = false;
                if (alone) { ns.erase(ns.begin() + long(i)); r.removed++; }
            }
        ns.insert(ns.end(), add.begin(), add.end());
    }
    std::sort(ns.begin(), ns.end(), [](const Note& a, const Note& b) { return std::tie(a.start, a.deg) < std::tie(b.start, b.deg); });
    return r;
}

QStringList resolveSamples(Song& s) {
    QStringList missing;
    for (Track& t : s.tracks) {
        if (!t.isDrums()) continue;
        for (int i = 0; i < D_COUNT; i++) {
            if (t.kit.sample[i].isEmpty()) { t.kit.data[i].reset(); continue; }
            auto d = samples::get(t.kit.sample[i]);   // the same loaded file every time, unless the file itself changed
            if (d != t.kit.data[i]) t.kit.data[i] = std::move(d);
            if (!t.kit.data[i]) missing << t.kit.sample[i];
        }
    }
    missing.removeDuplicates();
    return missing;
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
    case 0: return {{intro, A_INTRO, 8}, {build, A_BUILD, 8}, {main1, A_MAIN, 16}, {brk, A_BREAK, 8}, {main2, A_MAIN2, 16}, {outro, A_OUTRO, 8}};
    case 2: return {{intro, A_INTRO, 16}, {build, A_BUILD, 16}, {main1, A_MAIN, 32}, {brk, A_BREAK, 16}, {build, A_BUILD, 8}, {main2, A_MAIN2, 32}, {outro, A_OUTRO, 16}};
    default: return {{intro, A_INTRO, 16}, {build, A_BUILD, 8}, {main1, A_MAIN, 24}, {brk, A_BREAK, 8}, {build, A_BUILD, 8}, {main2, A_MAIN2, 24}, {outro, A_OUTRO, 16}};
    }
}

std::vector<ArrangePart> arrangeSong(Song& s, int length, bool writeLead, std::mt19937& g) {
    const std::vector<ArrangePart> plan = arrangePlan(length);
    s.liveMode = false;
    for (Track& t : s.tracks) {
        // the pattern to build from: the clip at the start of the loop, or else the earliest one
        const Clip* src = nullptr;
        for (const Clip& c : t.clips) if (c.start <= s.loopStart && s.loopStart < c.end()) { src = &c; break; }
        if (!src) for (const Clip& c : t.clips) if (!src || c.start < src->start) src = &c;
        if (!src && !t.cells.empty()) src = t.cell(t.liveCell) ? t.cell(t.liveCell) : &t.cells[0];   // made in the loop grid only
        Clip source = src ? *src : Clip{};
        if (!src) { source.len = source.loopLen = t.role == R_LEAD ? 4 * BAR : BAR; }
        bool has = src && (!src->notes.empty() || src->ad.on);
        if (!has && t.role == R_LEAD && writeLead) {   // a melody for the lead, so the second main part has something new
            source.loopLen = source.len = 4 * BAR; source.notes.clear(); source.ad.on = false;
            generate(s, t, source, generatorsFor(R_LEAD)[0].id, 0.5f, g);
            has = !source.notes.empty();
        }
        if (!has && t.isDrums()) { source.ad = AutoDrum{}; source.ad.on = true; source.ad.seed = unsigned(g()); has = true; }
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
                if (part.type == A_OUTRO) { out = 1; outLen = std::min(len, 4 * BAR); }
                break;
            case R_BASS:
                on = part.type != A_INTRO && part.type != A_BREAK;
                if (part.type == A_BUILD) { in = 2; inLen = len; }                       // the filter opens over the build-up
                if (part.type == A_OUTRO) { out = 3; outLen = len; }
                break;
            case R_SEQ:
                if (part.type == A_INTRO) { in = 3; inLen = len; }                       // fades in with the filter opening
                if (part.type == A_OUTRO) { out = 3; outLen = len; }
                break;
            case R_PAD:
                if (part.type == A_INTRO) { in = 1; inLen = std::min(len, 8 * BAR); }
                if (part.type == A_OUTRO) { out = 1; outLen = len; }
                break;
            case R_LEAD:
                on = part.type == A_MAIN2 || part.type == A_BREAK;
                if (part.type == A_BREAK) { in = 1; inLen = std::min(len, 4 * BAR); }
                break;
            default: break;
            }
            if (on) {
                Clip c = source;
                c.id = s.newId(); c.scene = -1;
                c.start = at; c.len = len; c.offset = 0; c.adKey = 0;
                c.inKind = in; c.inLen = std::min(inLen, len); c.outKind = out; c.outLen = std::min(outLen, len);
                if (c.ad.on) {   // the drummer plays each part its own way
                    AutoDrum& a = c.ad;
                    a = drummer;
                    a.seed = drummer.seed + unsigned(at / BAR);
                    switch (part.type) {
                    case A_INTRO: a.intensity = std::max(0.15f, drummer.intensity * 0.45f); a.complexity *= 0.5f; a.crash = false; a.shape = 0; break;
                    case A_BUILD: a.shape = 1; a.fillEvery = 4; break;
                    case A_MAIN2: a.intensity = std::min(1.f, drummer.intensity + 0.1f); a.complexity = std::min(1.f, drummer.complexity + 0.15f); break;
                    case A_OUTRO: a.shape = 2; break;
                    default: break;
                    }
                } else if (part.type == A_MAIN2 && (t.role == R_SEQ || t.role == R_BASS)) {
                    varyClip(s, t, c, g);   // the second time round, the patterns change a little
                    varyClip(s, t, c, g);
                }
                t.clips.push_back(c);
            }
            at += len;
        }
    }
    // the loop goes around the first main part, switched off so the whole song plays
    int at = 0;
    for (const ArrangePart& part : plan) { if (part.type == A_MAIN) { s.loopStart = at; s.loopEnd = at + part.bars * BAR; break; } at += part.bars * BAR; }
    s.loopOn = false;
    refreshAutoDrums(s);
    return plan;
}

DrumKit chooseKit(const DrumKit& current, const DrumKit& preset) {
    DrumKit k = preset;
    for (int d = 0; d < D_COUNT; d++)
        if (k.sample[d].isEmpty() && !current.sample[d].isEmpty() && !samples::isLibrary(current.sample[d])) {
            k.sample[d] = current.sample[d]; k.data[d] = current.data[d];
            for (int j = 0; j < 3; j++) k.p[d * 3 + j] = current.p[d * 3 + j];   // with the level, tuning and length it had
        }
    return k;
}
