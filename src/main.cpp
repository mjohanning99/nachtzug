// Nachtzug: a scale-locked synth workstation for acid and Berlin School music.
//
//   nachtzug [song.nachtzug]                 open the program
//   nachtzug --render out.wav [song]         write a song to a WAV file without opening a window
//   nachtzug --screenshot out.png [song]     save a picture of the window and quit (for testing);
//                                            add --tab 0|1|2, --track N, --theme NAME, --loops, --lines or --guide to choose what it shows
//   nachtzug --selftest                      drive the program through its features without a window and report problems
#include "app.h"
#include "mainwindow.h"
#include "editor.h"
#include "widgets.h"
#include "soundpanel.h"
#include "welcome.h"
#include "samples.h"
#include "timeline.h"
#include "autosave.h"
#include "midifile.h"
#include "lessons.h"
#include "midi.h"
#include "chopdialog.h"
#include <QApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>
#include <QFileOpenEvent>
#include <QRegularExpression>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>
#include <QStyleFactory>
#include <QTabWidget>
#include <QAbstractButton>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QGroupBox>
#include <QMouseEvent>
#include <QRandomGenerator>
#include <QTabBar>
#include <QTimer>
#include <QMenu>
#include <QElapsedTimer>
#include <QPushButton>
#include <QFrame>
#include <QToolButton>
#include <QKeyEvent>
#include <QDialog>
#include <QLineEdit>
#include <QRadioButton>
#include <QDir>
#include <QThread>
#include <QProcess>
#include <QScreen>
#include <cmath>
#include <functional>
#include <cstdio>
#if defined(_WIN32)
#include <windows.h>
#endif

#if defined(Q_OS_MACOS)
// On a Mac, Qt puts what Ctrl does elsewhere on the Command key, so the texts should say ⌘ too.
// Sits in front of the real translation and only touches texts that mention Ctrl (Strg in German).
class MacKeyNames : public QTranslator {
public:
    explicit MacKeyNames(const QTranslator* inner) : inner_(inner) {}
    bool isEmpty() const override { return false; }
    QString translate(const char* ctx, const char* src, const char* dis, int n) const override {
        const QString t = inner_ && !inner_->isEmpty() ? inner_->translate(ctx, src, dis, n) : QString();
        QString s = t.isEmpty() ? QString::fromUtf8(src) : t;
        if (!s.contains(QLatin1String("Ctrl")) && !s.contains(QLatin1String("Strg"))) return t;
        static const QRegularExpression word(QStringLiteral("(Ctrl|Strg)\\+(?=\\w\\w)")), plus(QStringLiteral("(Ctrl|Strg)\\+")), bare(QStringLiteral("\\b(Ctrl|Strg)\\b"));
        s.replace(word, QStringLiteral("⌘-")).replace(plus, QStringLiteral("⌘")).replace(bare, QStringLiteral("⌘"));
        return s;
    }
private:
    const QTranslator* inner_;
};
#endif

// macOS hands over a song double-clicked in the Finder as an event, not on the command line
class FileOpener : public QObject {
public:
    FileOpener(App* app, MainWindow* w) : app_(app), w_(w) {}
    bool eventFilter(QObject* o, QEvent* e) override {
        if (e->type() != QEvent::FileOpen) return QObject::eventFilter(o, e);
        const QString path = static_cast<QFileOpenEvent*>(e)->file();
        if (!path.isEmpty() && path != app_->filePath && w_->maybeSave() && !app_->load(path))
            std::fprintf(stderr, "Could not read the song %s\n", qPrintable(path));
        return true;
    }
private:
    App* app_;
    MainWindow* w_;
};

// Exercises the song model, the editing commands and the audio engine together, the way a session would.
// Run it under a memory checker to catch crashes; it also fails on silence, NaNs or a song that does not survive saving.
static int selfTest() {
    App app(false);
    std::vector<float> buf(2 * 512);
    int problems = 0;
    float peak = 0.f;
    auto pump = [&](int blocks) {
        for (int i = 0; i < blocks; i++) {
            app.engine->process(buf.data(), 512);
            for (float x : buf) { if (!std::isfinite(x)) { problems++; std::fprintf(stderr, "non-finite sample\n"); return; } peak = std::max(peak, std::fabs(x)); }
        }
        QCoreApplication::processEvents();
    };
    // like pump, but at about real speed, so changes queued for the next bar line get their turn
    auto realtime = [&](int blocks) { for (int i = 0; i < blocks; i += 2) { pump(2); QThread::msleep(17); } };
    auto check = [&](bool ok, const char* what) { if (!ok) { problems++; std::fprintf(stderr, "FAILED: %s\n", what); } };

    app.play(); pump(300);
    check(app.isPlaying(), "song plays");
    check(peak > 0.05f, "demo song makes sound");

    for (int role = 0; role < R_COUNT; role++) { app.addTrack(role); pump(40); }
    check(int(app.song.tracks.size()) == 10, "five tracks added");

    // every idea generator on every track, then vary
    for (size_t ti = 0; ti < app.song.tracks.size(); ti++) {
        const int id = app.song.tracks[ti].id;
        for (const GenDef& g : generatorsFor(app.song.tracks[ti].role)) {
            app.edit([&](Song& s) { Track* t = s.track(id); if (t && !t->clips.empty()) generate(s, *t, t->clips[0], g.id, 0.7f, app.rng); });
            pump(12);
        }
        app.edit([&](Song& s) { Track* t = s.track(id); if (t && !t->clips.empty()) varyClip(s, *t, t->clips[0], app.rng); });
    }
    // every preset, and rolled sounds, on a synth track while it plays
    const int synthId = app.song.tracks[1].id;
    for (const Patch& p : presets()) { app.edit([&](Song& s) { s.track(synthId)->patch = p; }); pump(25); }
    for (int i = 0; i < 30; i++) { app.edit([&](Song& s) { Track* t = s.track(synthId); t->patch = rollPatch(i % 4, app.rng); }); pump(10); }

    // play and record notes by hand, in time and with an arpeggio and chords
    app.recording = true;
    app.selectTrack(app.song.tracks[4].id);
    for (int k = 0; k < 12; k++) { app.keyDown(k, k % 3, k % 10); pump(6); app.keyUp(k); pump(3); }
    app.edit([&](Song& s) { Track* t = s.track(app.selTrack); t->arp = 3; t->chord = 4; t->arpOct = 2; });
    for (int k = 0; k < 8; k++) { app.keyDown(20 + k, 1, k); pump(8); }
    app.releaseAllKeys(); pump(20);
    check(!app.song.track(app.selTrack)->clips.empty(), "recording made a clip");

    // knob lines: record one, draw one that repeats, pause and resume
    app.paramPress(synthId, dsp::P_CUTOFF);
    for (int i = 0; i < 40; i++) { app.paramMove(synthId, dsp::P_CUTOFF, 0.2f + 0.015f * float(i)); pump(4); QThread::msleep(2); }
    app.paramRelease();
    check(app.song.track(synthId)->hasMoves(dsp::P_CUTOFF), "knob move was recorded");
    app.edit([&](Song& s) { Track* t = s.track(synthId); AutoLane l; l.param = dsp::P_RES; l.loop = BAR; l.pts = {{0, 0.1f}, {96, 0.9f}, {300, 0.3f}}; t->lanes.push_back(l); });
    pump(120);
    app.recording = false;
    app.paramPress(synthId, dsp::P_CUTOFF); app.paramMove(synthId, dsp::P_CUTOFF, 0.5f); app.paramRelease();
    check(app.song.track(synthId)->bypass != 0, "hand move pauses the line");
    app.resumeMoves(synthId); pump(20);

    // clips: transitions, cut, duplicate, copy and paste, delete, loop, undo and redo
    const int clipId = app.song.tracks[1].clips[0].id;
    app.edit([&](Song& s) { Clip* c = s.clip(clipId); c->inKind = 3; c->outKind = 3; c->outLen = 2 * BAR; });
    app.selectClip(clipId);
    app.splitClip(clipId, 3 * BAR + 48); pump(20);
    app.focus = App::FocusArrange;
    app.duplicateSelection(); app.copy(); app.seek(12 * BAR); app.paste(); app.loopSelection(); pump(60);
    app.selectAll(); app.deleteSelection(); pump(40);
    check(app.song.endTick() == 0, "delete removed every clip");
    for (int i = 0; i < 12; i++) { app.undo(); pump(6); }
    for (int i = 0; i < 6; i++) { app.redo(); pump(6); }
    for (int i = 0; i < 6; i++) app.undo();
    check(app.song.endTick() > 0, "undo brought the clips back");

    // loop grid: cells, launching on the bar, whole columns, recording launches into the song, stopping
    app.setLiveMode(true);
    check(!app.song.tracks[1].cells.empty(), "the loop grid starts with each track's first pattern");
    for (const Track& t : std::vector<Track>(app.song.tracks)) { app.newCell(t.id, 1); app.newCell(t.id, 2); }
    app.launchScene(0); realtime(220);
    check(app.song.tracks[1].liveCell == 0, "a column starts on the next bar");
    const int clipsBefore = int(app.song.tracks[1].clips.size());
    app.recording = true;
    for (int sc : {1, 2, 0, 1}) { app.launchScene(sc); realtime(260); }
    app.launch(app.song.tracks[1].id, -1);
    realtime(220);
    check(int(app.song.tracks[1].clips.size()) > clipsBefore, "launched cells are recorded into the song");
    app.recording = false;
    app.deleteCell(app.song.tracks[1].cells[0].id);
    app.stopAllCells(); realtime(220);
    app.setLiveMode(false); pump(40);
    check(app.song.tracks[1].liveCell == -1, "leaving the loop grid stops its cells");

    // song settings and chords, all scales
    for (int sc = 0; sc < NUM_SCALES; sc++) { app.edit([&](Song& s) { s.scale = sc; s.root = (sc * 5) % 12; s.swing = 0.1f * float(sc); s.setChord(sc * 3, sc % 5); }); pump(30); }

    // removing tracks while they sound must leave silence behind them
    while (app.song.tracks.size() > 1) { app.removeTrack(app.song.tracks.back().id); pump(15); }
    app.removeTrack(app.song.tracks[0].id);
    pump(700);   // long enough for echo and reverb to die away
    peak = 0.f; pump(50);
    check(peak < 0.002f, "no sound is left after the last track is deleted");

    // save and open again
    for (int i = 0; i < 40 && app.canUndo(); i++) app.undo();
    const QString path = QDir::temp().filePath(QStringLiteral("nachtzug-selftest.nachtzug"));
    const QJsonObject before = app.song.toJson();
    check(app.save(path) && app.load(path), "save and open");
    check(app.song.toJson() == before, "the song is the same after saving and opening");
    QFile::remove(path);
    app.stop(); pump(10);

    // ---- swing: the playhead and musical time map back and forth exactly
    {
        double worst = 0;
        for (float sw : {0.f, 0.3f, 0.7f, 1.f}) for (double m = 0; m < 400; m += 1.37) worst = std::max(worst, std::fabs(unswing(swingPos(m, sw), sw) - m));
        check(worst < 1e-9, "swing positions map back exactly");
    }

    // ---- undo steps carry names, and a big change announces itself
    {
        app.setSong(demoSong(), QString());
        QString heard;
        auto conn = QObject::connect(&app, &App::announced, [&](const QString& w) { heard = w; });
        const int tid = app.song.tracks[1].id;
        app.removeTrack(tid);
        check(heard == QStringLiteral("Delete track") && app.undoName() == heard, "deleting a track is a named step that offers Undo");
        app.undo();
        check(app.song.track(tid) && app.redoName() == QStringLiteral("Delete track"), "undo brings the track back and redo knows its name");
        QObject::disconnect(conn);
    }

    // ---- export: whole song, the loop four times, 24-bit, fading out
    {
        app.setSong(demoSong(), QString());
        const QString wav = QDir::temp().filePath(QStringLiteral("nachtzug-selftest.wav"));
        auto header = [&](int& bits, int& rate, qint64& frames) {
            QFile f(wav); if (!f.open(QIODevice::ReadOnly)) return false;
            const QByteArray h = f.read(44);
            auto u16 = [&](int at) { return int(uchar(h[at]) | uchar(h[at + 1]) << 8); };
            auto u32 = [&](int at) { return qint64(uchar(h[at]) | uchar(h[at + 1]) << 8 | uchar(h[at + 2]) << 16) | qint64(uchar(h[at + 3])) << 24; };
            bits = u16(34); rate = int(u32(24)); frames = u32(40) / (2 * bits / 8);
            return h.startsWith("RIFF") && u32(40) == f.size() - 44;
        };
        int bits = 0, rate = 0; qint64 frames = 0;
        RenderOptions o;
        RenderResult r = renderSong(app.song, wav, o, {});
        check(r.ok && header(bits, rate, frames) && bits == 16 && rate == 48000, "export the whole song as 16-bit WAV");
        const double songSecs = renderLength(app.song, o);
        check(frames / 48000.0 >= songSecs && frames / 48000.0 < songSecs + o.maxTail + 0.1, "the export ends once the echoes have died away");
        o.loopOnly = true; o.repeats = 4; o.bits = 24; o.sampleRate = 44100.f; o.fadeOut = true;
        r = renderSong(app.song, wav, o, {});
        const double loopSecs = (app.song.loopEnd - app.song.loopStart) * 4 * 60.0 / (app.song.bpm * PPQ);
        check(r.ok && header(bits, rate, frames) && bits == 24 && rate == 44100 && std::fabs(frames / 44100.0 - loopSecs) < 0.05, "export the loop four times as 24-bit, faded out");
        {   // the fade really ends in silence
            QFile f(wav); if (!f.open(QIODevice::ReadOnly)) problems++; f.seek(f.size() - 3 * 2 * 200);
            const QByteArray endBytes = f.readAll(); int loudest = 0;
            for (int i = 0; i + 2 < endBytes.size(); i += 3) loudest = std::max(loudest, std::abs(int(qint32(uchar(endBytes[i]) << 8 | uchar(endBytes[i + 1]) << 16 | uchar(endBytes[i + 2]) << 24) >> 8)));
            check(loudest < 8388607 / 200, "a faded export ends quietly");
        }
        int cancelAt = 0;
        r = renderSong(app.song, wav, RenderOptions{}, [&](float) { return ++cancelAt < 3; });
        check(!r.ok && !QFile::exists(wav), "cancelling an export leaves no file behind");
        QFile::remove(wav);
    }

    // ---- your own samples: write one, import it, play it on the kick
    {
        std::vector<float> click(4800);
        for (size_t i = 0; i < click.size(); i++) click[i] = std::sin(float(i) * 0.3f) * std::exp(-float(i) / 900.f) * 0.9f;
        QString err;
        const QString name = samples::saveRecording(QStringLiteral("selftest blip"), click, 48000, err);
        check(!name.isEmpty() && samples::list().contains(name), "a recording is saved in the sample library");
        const QString copy = samples::import(QDir(samples::folder()).filePath(name), err);
        check(!copy.isEmpty() && copy != name, "importing a file with a name already in the library makes a new name");
        const auto data = samples::get(name);
        check(data && data->frames() == click.size() && std::fabs(data->l[100] - click[100]) < 1e-5f, "the sample reads back as it was written");
        dsp::SampleData junk;
        check(!samples::readFile(QDir::temp().filePath(QStringLiteral("does-not-exist.wav")), junk, err), "a missing file is refused with a reason");
        // the kick plays the sample: the output follows it
        app.setSong(demoSong(), QString());
        Track* drums = nullptr; for (Track& t : app.song.tracks) if (t.isDrums()) drums = &t;
        check(drums != nullptr, "the demo has drums");
        if (drums) {
            const int did = drums->id;
            app.edit([&](Song& sg) { Track* t = sg.track(did); t->kit.sample[dsp::D_KICK] = name; t->kit.p[dsp::D_KICK * 3 + 2] = 1.f; });
            check(app.song.track(did)->kit.data[dsp::D_KICK] != nullptr, "the drum finds its sample");
            for (Track& t : app.song.tracks) if (!t.isDrums()) app.song.track(t.id)->mute = true;
            app.touch();
            app.audition(did, dsp::D_KICK, true);
            peak = 0.f; pump(4);
            check(peak > 0.05f, "a drum with a sample makes sound");
            // the song keeps the name; opening it again finds the sample
            const QString sp = QDir::temp().filePath(QStringLiteral("nachtzug-selftest-sample.nachtzug"));
            app.save(sp); app.load(sp);
            Track* again = nullptr; for (Track& t : app.song.tracks) if (t.isDrums()) again = &t;
            check(again && again->kit.sample[dsp::D_KICK] == name && again->kit.data[dsp::D_KICK], "a song remembers its samples");
            // a sample that disappears falls back to the built-in drum
            samples::remove(name);
            app.load(sp);
            again = nullptr; for (Track& t : app.song.tracks) if (t.isDrums()) again = &t;
            check(again && !again->kit.data[dsp::D_KICK], "a missing sample falls back to the built-in drum");
            app.audition(again->id, dsp::D_KICK, true); pump(20);
            QFile::remove(sp);
        }
        samples::remove(copy); samples::remove(name);
        app.previewSample(QString());
    }

    // ---- the Nachtzug library: every sound is there and reads, and the sampled kits play
    {
        const auto& lib = samples::library();
        check(lib.size() == 65, "the library has its 65 sounds");
        int bad = 0;
        for (const samples::LibrarySound& ls : lib) {
            const auto d = samples::get(ls.id);
            if (!d || d->frames() < 100 || d->rate < 8000 || ls.name.isEmpty() || ls.about.isEmpty()) { bad++; std::fprintf(stderr, "library sound %s does not read\n", qPrintable(ls.id)); continue; }
            float pk = 0.f; for (size_t i = 0; i < d->frames(); i++) pk = std::max(pk, std::fabs(d->l[i]));
            if (pk < 0.25f || pk > 1.f) { bad++; std::fprintf(stderr, "library sound %s has a peak of %.2f\n", qPrintable(ls.id), double(pk)); }
        }
        check(bad == 0, "every library sound reads and is at a sensible level");
        check(QDir(QStringLiteral(":/samples")).entryList(QDir::Dirs | QDir::NoDotAndDotDot).size() == samples::libraryCategories().size(), "every folder of the library has a category");
        check(samples::isLibrary(QStringLiteral("nz:Kicks/Punch")) && samples::displayName(QStringLiteral("nz:Kicks/Punch")) == QStringLiteral("Punch") && samples::displayName(QStringLiteral("loop.wav")) == QStringLiteral("loop"), "library and own samples are named right");
        app.setSong(demoSong(), QString());
        int did = -1; for (const Track& t : app.song.tracks) if (t.isDrums()) did = t.id;
        int sampledKits = 0;
        for (const DrumKit& k : drumKits()) {
            if (k.sample[0].isEmpty()) continue;
            sampledKits++;
            app.edit([&](Song& sg) { sg.track(did)->kit = chooseKit(sg.track(did)->kit, k); });
            bool all = true; for (int d = 0; d < dsp::D_COUNT; d++) all &= app.song.track(did)->kit.data[d] != nullptr;
            check(all, "a sampled kit finds all its sounds");
            for (int d = 0; d < dsp::D_COUNT; d++) { app.audition(did, d, true); peak = 0.f; pump(3); if (peak < 0.02f) { problems++; std::fprintf(stderr, "FAILED: %s, drum %d is silent\n", qPrintable(k.name), d); } pump(20); }
        }
        check(sampledKits == 4, "there are four sampled kits");
        // choosing a kit: your own sample stays, the library sounds of the old kit go
        QString err;
        const QString mine = samples::saveRecording(QStringLiteral("selftest kit"), std::vector<float>(2000, 0.5f), 48000, err);
        app.edit([&](Song& sg) { sg.track(did)->kit.sample[dsp::D_TOM] = mine; });
        app.edit([&](Song& sg) { sg.track(did)->kit = chooseKit(sg.track(did)->kit, drumKits()[0]); });
        const DrumKit& now = app.song.track(did)->kit;
        check(now.sample[dsp::D_TOM] == mine && now.sample[dsp::D_KICK].isEmpty(), "a new kit keeps your own sample and drops the old kit's");
        const QString sp = QDir::temp().filePath(QStringLiteral("nachtzug-selftest-library.nachtzug"));
        app.edit([&](Song& sg) { sg.track(did)->kit.sample[dsp::D_CYM] = QStringLiteral("nz:Train/Door chime"); });
        app.save(sp); app.load(sp);
        int did2 = -1; for (const Track& t : app.song.tracks) if (t.isDrums()) did2 = t.id;
        check(app.song.track(did2)->kit.sample[dsp::D_CYM] == QStringLiteral("nz:Train/Door chime") && app.song.track(did2)->kit.data[dsp::D_CYM], "a song remembers a library sound");
        QFile::remove(sp); samples::remove(mine);
    }

    // ---- arrange for me: every length, all parts there, plays cleanly
    for (int len = 0; len < 3; len++) {
        app.setSong(demoSong(), QString());
        app.edit([&](Song& sg) { arrangeSong(sg, len, true, app.rng); }, QStringLiteral("Arrange for me"), true);
        int bars = 0; for (const ArrangePart& pt : arrangePlan(len)) bars += pt.bars;
        check(app.song.endTick() == bars * BAR, "the arranged song is as long as its plan");
        bool everyTrack = true; for (const Track& t : app.song.tracks) everyTrack &= !t.clips.empty();
        check(everyTrack, "every track plays somewhere in the arrangement (the empty lead gets a melody)");
        bool leadOnlyLate = true;
        for (const Track& t : app.song.tracks) if (t.role == R_LEAD) for (const Clip& c : t.clips) leadOnlyLate &= c.start >= 32 * BAR;
        check(leadOnlyLate, "the lead waits for the break and the second main part");
        app.refreshSelection();
        check(app.curClip() != nullptr, "after arranging the editor shows a clip");
        app.seek(0); app.play(); peak = 0.f; pump(400);
        check(peak > 0.05f, "the arranged song plays");
        app.stop(); pump(10);
        app.undo();
        check(app.song.endTick() == 8 * BAR, "undo brings back the song from before arranging");
    }
    // the simple sound page writes the same patch values as the full one
    {
        app.setSong(demoSong(), QString());
        check(!app.song.tracks.empty(), "demo for the sound pages");
    }

    // ---- notes that play only sometimes, quick repeats, kept notes
    {
        // the notes a song plays in its first `bars` bars, worked out without sound
        auto notesOf = [](const Song& sg, int bars) {
            std::vector<NoteEvent> log;
            Engine e(48000.f);
            e.eventsOnly = true; e.log = &log;
            e.setSong(std::make_shared<const Song>(sg));
            EngineEvent play; play.type = EngineEvent::Play; e.push(play);
            std::vector<float> b(2 * 480);
            const long frames = long(bars * 4 * 60.0 / sg.bpm * 48000.0);
            for (long f = 0; f < frames; f += 480) e.process(b.data(), 480);
            std::erase_if(log, [&](const NoteEvent& ev) { return ev.frame >= double(frames) - dsp::CHUNK; });   // notes start at the beginning of their 16-sample chunk
            return log;
        };
        Song sg; sg.bpm = 120.f; sg.loopStart = 0; sg.loopEnd = BAR; sg.loopOn = true;
        Track dr = makeTrack(sg, R_DRUMS);
        Clip c; c.id = sg.newId(); c.start = 0; c.len = BAR; c.loopLen = BAR;
        for (int i = 0; i < 16; i++) { Note n; n.start = i * 24; n.len = 12; n.deg = dsp::D_CHAT; n.chance = 50; c.notes.push_back(n); }
        Note kick; kick.start = 0; kick.len = 12; kick.deg = dsp::D_KICK; c.notes.push_back(kick);
        Note roll; roll.start = 192; roll.len = 48; roll.deg = dsp::D_SNARE; roll.rep = 4; roll.chance = 50; c.notes.push_back(roll);
        Note clap; clap.start = 288; clap.len = 24; clap.deg = dsp::D_CLAP; clap.rep = 3; c.notes.push_back(clap);
        dr.clips.push_back(c);
        sg.tracks.push_back(dr);
        const int bars = 64;
        const auto log = notesOf(sg, bars);
        int hats = 0, kicks = 0, claps = 0; std::vector<int> snaresPerBar(bars + 1, 0);
        const double framesPerBar = 4 * 60.0 / sg.bpm * 48000.0;
        for (const NoteEvent& ev : log) {
            if (!ev.on) continue;
            if (ev.note == dsp::D_CHAT) hats++;
            else if (ev.note == dsp::D_KICK) kicks++;
            else if (ev.note == dsp::D_CLAP) claps++;
            else if (ev.note == dsp::D_SNARE) snaresPerBar[size_t(std::min(double(bars), ev.frame / framesPerBar))]++;
        }
        if (qEnvironmentVariableIsSet("NZ_DEBUG")) std::fprintf(stderr, "kicks %d claps %d hats %d\n", kicks, claps, hats);
        check(kicks == bars && claps == 3 * bars, "plain notes and repeated notes play every time");
        check(hats > 16 * bars * 0.4 && hats < 16 * bars * 0.6, "a note with a 50% chance plays about half the time");
        bool together = true; int rolls = 0;
        for (int b = 0; b < bars; b++) { together &= snaresPerBar[size_t(b)] == 0 || snaresPerBar[size_t(b)] == 4; rolls += snaresPerBar[size_t(b)] == 4; }
        check(together && rolls > 10 && rolls < bars - 10, "the repeats of a note with a chance play all together or not at all");
        const auto again = notesOf(sg, bars);
        check(again.size() == log.size(), "the same song plays the same chances when written to a file twice");
        // repeats of a synth note are evenly spaced and end before the next one starts
        Song s2; s2.bpm = 120.f; s2.loopEnd = BAR;
        Track bass = makeTrack(s2, R_BASS);
        Clip bc; bc.id = s2.newId(); bc.loopLen = BAR; bc.len = BAR;
        Note bn; bn.start = 0; bn.len = 96; bn.deg = 0; bn.rep = 4; bc.notes.push_back(bn);
        bass.clips.push_back(bc); s2.tracks.push_back(bass);
        std::vector<double> ons, offs;
        for (const NoteEvent& ev : notesOf(s2, 1)) (ev.on ? ons : offs).push_back(ev.frame);
        const double step = 24 * 60.0 / (s2.bpm * PPQ) * 48000.0;
        if (qEnvironmentVariableIsSet("NZ_DEBUG")) { std::fprintf(stderr, "ons %d offs %d:", int(ons.size()), int(offs.size())); for (double o : ons) std::fprintf(stderr, " %.0f", o); std::fprintf(stderr, " / step %.0f, off0 %.0f\n", step, offs.empty() ? -1.0 : offs[0]); }
        check(ons.size() == 4 && offs.size() >= 4 && std::fabs(ons[3] - ons[0] - 3 * step) < 40 && offs[0] < ons[1], "a note played four times: four even, separate hits");
        // kept notes
        Clip kc = bc; kc.notes.clear();
        for (int i = 0; i < 8; i++) { Note n; n.start = i * 48; n.len = 24; n.deg = i; n.lock = i % 2 == 0; kc.notes.push_back(n); }
        const std::vector<Note> was = kc.notes;
        for (int i = 0; i < 300; i++) varyClip(s2, s2.tracks[0], kc, app.rng);
        bool keptSame = true;
        for (const Note& w : was) if (w.lock) keptSame &= std::any_of(kc.notes.begin(), kc.notes.end(), [&](const Note& n) { return n.lock && n.start == w.start && n.deg == w.deg && n.len == w.len && n.acc == w.acc && n.slide == w.slide; });
        check(keptSame, "Vary leaves kept notes alone");
        for (const GenDef& g : generatorsFor(R_BASS)) {
            Clip gc = kc;
            generate(s2, s2.tracks[0], gc, g.id, 0.8f, app.rng);
            int found = 0; bool overlap = false;
            for (const Note& n : gc.notes) {
                if (n.lock) { found++; continue; }
                for (const Note& k : gc.notes) if (k.lock && n.start < k.start + k.len && k.start < n.start + n.len) overlap = true;
            }
            check(found == 4 && !overlap, "a new idea keeps the kept notes and writes around them");
        }
        // the song file keeps all three
        Song s3 = s2; s3.tracks[0].clips[0].notes = {bn};
        s3.tracks[0].clips[0].notes[0].chance = 25; s3.tracks[0].clips[0].notes[0].lock = true;
        Song back; Song::fromJson(s3.toJson(), back);
        const Note& rn = back.tracks[0].clips[0].notes[0];
        check(rn.chance == 25 && rn.rep == 4 && rn.lock, "chance, repeats and keep are saved with the song");
    }

    // ---- autosave: a copy of unsaved work, found again after a crash, gone after a normal end
    {
        const QString dir = QDir::temp().filePath(QStringLiteral("nachtzug-selftest-autosave"));
        QDir(dir).removeRecursively();
        app.setSong(demoSong(), QString());
        app.filePath = QStringLiteral("/somewhere/My song.nachtzug");
        {
            Autosave as(&app, dir, 1000000);
            as.writeNow();
            check(!QFile::exists(as.copyPath()), "no copy while nothing has changed");
            app.edit([](Song& sg) { sg.bpm = 133.f; }, QStringLiteral("Tempo"));
            as.writeNow();
            check(QFile::exists(as.copyPath()), "a change is written to the spare copy");
            check(Autosave::leftovers(dir).isEmpty(), "the copy of a running program is not offered for recovery");
            // pretend the program crashed: copy the files somewhere the lock does not cover
            QFile::copy(as.copyPath(), QDir(dir).filePath(QStringLiteral("session-crashed.nachtzug")));
            QFile::copy(QString(as.copyPath()).replace(QStringLiteral(".nachtzug"), QStringLiteral(".json")), QDir(dir).filePath(QStringLiteral("session-crashed.json")));
            as.finish();
            check(!QFile::exists(as.copyPath()), "a normal end deletes the copy");
        }
        const auto left = Autosave::leftovers(dir);
        check(left.size() == 1 && left[0].original == QStringLiteral("/somewhere/My song.nachtzug"), "a copy left by a crash is found, with where the song came from");
        app.setSong(demoSong(), QString());
        if (!left.isEmpty()) {
            Autosave as2(&app, dir, 1000000);
            check(as2.restore(left[0]) && std::fabs(app.song.bpm - 133.f) < 0.01f && app.dirty && app.filePath == QStringLiteral("/somewhere/My song.nachtzug"), "bringing it back restores the song as unsaved changes");
            check(Autosave::leftovers(dir).isEmpty(), "a restored copy is not offered again");
            as2.finish();
        }
        app.dirty = false;
        QDir(dir).removeRecursively();
    }

    // ---- Chords mode: the keys change the chord of the tracks that follow it, and Record writes it down
    {
        app.setSong(demoSong(), QString());
        int bassId = -1; for (const Track& t : app.song.tracks) if (t.role == R_BASS) bassId = t.id;
        app.selectTrack(bassId);
        app.setChordKeys(true);
        app.keyDown(1 * 16 + 3, 1, 3);   // the fourth key of the middle row: the chord on the fourth step
        check(app.liveChord() == 3 && app.engine->liveChord.load() == 3, "a key in Chords mode sets the chord");
        app.keyUp(1 * 16 + 3);
        check(app.liveChord() == 3, "the chord stays when the key is let go");
        // what the bass plays moves with it
        Song sg = app.song;
        for (Track& t : sg.tracks) if (t.id != bassId) t.mute = true;
        std::vector<NoteEvent> plain, moved;
        for (int pass = 0; pass < 2; pass++) {
            Engine e(48000.f); e.eventsOnly = true; e.log = pass ? &moved : &plain;
            if (pass) e.liveChord.store(3);
            e.setSong(std::make_shared<const Song>(sg));
            EngineEvent pl; pl.type = EngineEvent::Play; e.push(pl);
            std::vector<float> b(2 * 480); for (int i = 0; i < 100; i++) e.process(b.data(), 480);
        }
        int firstPlain = -1, firstMoved = -1;
        for (const NoteEvent& ev : plain) if (ev.on) { firstPlain = ev.note; break; }
        for (const NoteEvent& ev : moved) if (ev.on) { firstMoved = ev.note; break; }
        check(firstPlain > 0 && firstMoved - firstPlain == sg.degToSemis(3) - sg.degToSemis(sg.chordAt(0)), "tracks that follow the chords play the chord from the keys");
        // recording: the chord goes into the bars the playhead passes
        app.recording = true; app.seek(0); app.play();
        realtime(30);
        app.keyDown(1 * 16 + 5, 1, 5); app.keyUp(1 * 16 + 5);
        realtime(400);
        const int bar = int(app.musicalPos()) / BAR;
        check(app.song.chordAt(std::max(0, bar - 1)) == 5 && app.undoName() == QStringLiteral("Record chords"), "Record writes the chord into the chord row");
        app.stop(); app.recording = false; pump(4);
        app.releaseChord();
        check(app.engine->liveChord.load() == Engine::NO_CHORD, "Esc goes back to the song's chords");
        app.setChordKeys(false);
        app.keyDown(1 * 16 + 2, 1, 2); app.keyUp(1 * 16 + 2);
        check(app.liveChord() == Engine::NO_CHORD, "with Chords mode off the keys play notes again");
        app.dirty = false;
    }

    // ---- the drummer: every step of Energy adds to the beat (with or without hats), and Fills come as often as chosen
    {
        bool rises = true;
        for (int style = 0; style < 7; style++) for (int hats : {0, 4}) {
            size_t last = 0;
            for (int e = 0; e <= 10; e += 2) {
                Clip c; c.len = 8 * BAR; c.ad.on = true; c.ad.style = style; c.ad.hats = hats; c.ad.intensity = e / 10.f; c.ad.complexity = 0.4f; c.ad.fillEvery = 4;
                autoDrumFill(c);
                if (e > 0 && c.notes.size() <= last) { rises = false; std::fprintf(stderr, "style %d hats %d: energy %d%% adds nothing\n", style, hats, e * 10); }
                last = c.notes.size();
            }
        }
        check(rises, "each step of the drummer's Energy adds to the beat");
        auto fillBars = [](int every) {
            Clip c; c.len = 8 * BAR; c.ad.on = true; c.ad.style = 1; c.ad.intensity = 0.6f; c.ad.complexity = 0.4f; c.ad.fillEvery = every;
            autoDrumFill(c);
            int bars = 0;
            for (int b = 0; b < 8; b++) { int sn = 0; for (const Note& n : c.notes) if (n.start / BAR == b && n.start % BAR >= 9 * 24 && (n.deg == dsp::D_SNARE || n.deg == dsp::D_TOM)) sn++; bars += sn >= 4; }
            return bars;
        };
        check(fillBars(2) == 4 && fillBars(4) == 2 && fillBars(8) == 1 && fillBars(0) == 0, "fills come every 2, 4 or 8 bars, as chosen");
    }

    // ---- no note gets stuck: sounds, filter, mono/poly, arpeggio and chords changed while notes sound, then Stop
    {
        app.setSong(demoSong(), QString());
        QRandomGenerator rnd(1234);
        int stuck = 0;
        for (int round = 0; round < 40 && !stuck; round++) {
            app.seek(0); app.play();
            for (int step = 0; step < 30; step++) {
                const int ti = 1 + int(rnd.bounded(int(app.song.tracks.size()) - 1));
                const int what = int(rnd.bounded(9));
                app.edit([&](Song& s) {
                    Track& t = s.tracks[size_t(ti)];
                    float* p = t.patch.p;
                    switch (what) {
                    case 0: p[dsp::P_MONO] = p[dsp::P_MONO] > 0.5f ? 0.f : 1.f; break;
                    case 1: t.arp = t.arp ? 0 : 1 + int(rnd.bounded(3)); break;
                    case 2: t.chord = int(rnd.bounded(4)); break;
                    case 3: p[dsp::P_SLOPE] = float(rnd.bounded(4)); break;
                    case 4: p[dsp::P_CUTOFF] = float(rnd.generateDouble()); p[dsp::P_RES] = float(rnd.generateDouble()); break;
                    case 5: p[dsp::P_GLIDE] = float(rnd.generateDouble() * 0.6); break;
                    case 6: t.patch = presets()[size_t(rnd.bounded(int(presets().size())))]; break;
                    default: p[dsp::P_ENVAMT] = float(rnd.generateDouble()); p[dsp::P_ACCENT] = float(rnd.generateDouble()); break;
                    }
                    p[dsp::P_AR] = std::min(p[dsp::P_AR], 0.55f);   // short releases, so the check below need not wait long
                });
                app.selectTrack(app.song.tracks[size_t(ti)].id);
                const int code = int(rnd.bounded(10));
                if (rnd.bounded(2)) app.keyDown(code, 1, code);
                pump(1 + int(rnd.bounded(12)));
                if (rnd.bounded(3)) app.releaseAllKeys();
            }
            app.releaseAllKeys(); app.stop();
            pump(int(4.0 * 48000 / 512));   // four seconds: every release is over
            if (!app.engine->synthsSilent()) { stuck++; std::fprintf(stderr, "stuck note after round %d\n", round); }
        }
        check(stuck == 0, "no note keeps sounding after Stop, whatever was changed while it played");
        app.dirty = false;
    }

    // ---- stems add up to the song, and the MIDI file holds the notes
    {
        app.setSong(demoSong(), QString());
        app.song.tracks[2].mute = true;   // a muted track is left out of both
        const QString dir = QDir::temp().filePath(QStringLiteral("nachtzug-selftest-stems"));
        QDir(dir).removeRecursively();
        RenderOptions o; o.bits = 24; o.loopOnly = true; o.repeats = 1; o.fadeOut = true;
        QStringList files;
        const RenderResult r = renderStems(app.song, dir, o, {}, &files);
        int withMusic = 0; for (const Track& t : app.song.tracks) withMusic += !t.mute && !t.clips.empty();
        check(r.ok && files.size() == withMusic && withMusic == int(audibleTracks(app.song).size()), "one stem per heard track, none for an empty one");
        const QString mixFile = QDir(dir).filePath(QStringLiteral("mix.wav"));
        renderSong(app.song, mixFile, o, {});
        auto read24 = [](const QString& f) {
            std::vector<float> out; QFile in(f); if (!in.open(QIODevice::ReadOnly)) return out;
            const QByteArray d = in.readAll().mid(44);
            for (int i = 0; i + 2 < d.size(); i += 3) out.push_back(float(qint32(uchar(d[i]) << 8 | uchar(d[i + 1]) << 16 | uchar(d[i + 2]) << 24) >> 8) / 8388607.f);
            return out;
        };
        const std::vector<float> mix = read24(mixFile);
        std::vector<float> sum(mix.size(), 0.f);
        for (const QString& f : files) { const auto st = read24(f); for (size_t i = 0; i < st.size() && i < sum.size(); i++) sum[i] += st[i]; }
        double diff = 0, level = 0;
        for (size_t i = 0; i < mix.size(); i++) { diff += double(sum[i] - mix[i]) * (sum[i] - mix[i]); level += double(mix[i]) * mix[i]; }
        if (qEnvironmentVariableIsSet("NZ_DEBUG")) { float pk = 0; for (float x : mix) pk = std::max(pk, std::fabs(x)); std::fprintf(stderr, "stems: diff %.4f, mix peak %.3f, %d vs %d samples\n", std::sqrt(diff / std::max(1e-12, level)), double(pk), int(mix.size()), int(sum.size())); }
        check(!mix.empty() && level > 0 && std::sqrt(diff / level) < 0.02, "the stems add up to the song");
        check(files.size() > 1 && QFileInfo(files[0]).fileName().startsWith(QStringLiteral("01 ")), "stems are numbered in track order");
        int cancelAt = 0;
        const QString dir2 = dir + QStringLiteral("-cancel");
        check(!renderStems(app.song, dir2, o, [&](float) { return ++cancelAt < 6; }).ok && QDir(dir2).entryList(QDir::Files).isEmpty(), "cancelling stems leaves no files behind");
        QDir(dir).removeRecursively(); QDir(dir2).removeRecursively();

        // MIDI: one track for the tempo and one per heard track; the drums on channel 10 with General MIDI notes
        const QString mid = QDir::temp().filePath(QStringLiteral("nachtzug-selftest.mid"));
        RenderOptions mo; mo.loopOnly = true; mo.repeats = 2;
        const double secs = writeMidiFile(app.song, mid, mo);
        QFile mf(mid); const QByteArray m = mf.open(QIODevice::ReadOnly) ? mf.readAll() : QByteArray(); mf.close();
        auto be = [&](int at, int n) { quint32 v = 0; for (int i = 0; i < n; i++) v = (v << 8) | uchar(m[at + i]); return v; };
        check(secs > 0 && m.startsWith("MThd") && be(8, 2) == 1 && int(be(10, 2)) == 1 + int(audibleTracks(app.song).size()) && be(12, 2) == 960, "the MIDI file has a tempo track and one track per heard track");
        // walk the tracks and count the notes
        int at = 14, ons = 0, drumOns = 0, offDrumNotes = 0, unbalanced = 0, overlaps = 0;
        while (at + 8 <= m.size() && m.mid(at, 4) == "MTrk") {
            const int len = int(be(at + 4, 4)); int p = at + 8; const int end = p + len;
            int status = 0; QHash<int, int> open;
            while (p < end) {
                while (uchar(m[p]) & 0x80) p++;
                p++;   // the time
                int b = uchar(m[p]);
                if (b == 0xff) { const int type = uchar(m[p + 1]); p += 2; int l = 0; while (uchar(m[p]) & 0x80) l = (l << 7) | (uchar(m[p++]) & 0x7f); l = (l << 7) | uchar(m[p++]); p += l; if (type == 0x2f) break; continue; }
                if (b & 0x80) { status = b; p++; }
                const int kind = status & 0xf0, ch = status & 0x0f;
                if (kind == 0xc0) { p += 1; continue; }
                const int note = uchar(m[p]), vel = uchar(m[p + 1]); p += 2;
                if (kind == 0x90 && vel > 0) { ons++; if (++open[note] > 1) overlaps++; if (ch == 9) { drumOns++; if (note != 36 && note != 38 && note != 39 && note != 42 && note != 46 && note != 45 && note != 37 && note != 49) offDrumNotes++; } }
                else if (kind == 0x80 || (kind == 0x90 && vel == 0)) open[note]--;
            }
            for (int v : open) unbalanced += v != 0;
            at = end;
        }
        check(ons > 50 && drumOns > 10 && offDrumNotes == 0, "the MIDI file holds the notes, the drums on General MIDI drum notes");
        check(unbalanced == 0, "every note in the MIDI file ends");
        check(overlaps == 0, "no note in the MIDI file starts again while it still sounds");
        QFile::remove(mid);
        app.dirty = false;
    }

    // ---- listening to a preset or kit before choosing it
    {
        app.setSong(demoSong(), QString());
        for (Track& t : app.song.tracks) t.mute = true;
        app.touch(); pump(200);
        int silentOnes = 0;
        for (const Track& t : std::vector<Track>(app.song.tracks)) {
            app.selectTrack(t.id);
            if (t.isDrums()) for (const DrumKit& k : drumKits()) { app.previewSound(nullptr, &k); peak = 0.f; pump(40); if (peak < 0.05f) { silentOnes++; std::fprintf(stderr, "kit %s is silent when previewed\n", qPrintable(k.name)); } }
            else for (size_t i = 0; i < presets().size(); i += 3) { app.previewSound(&presets()[i], nullptr); peak = 0.f; pump(60); if (peak < 0.05f) { silentOnes++; std::fprintf(stderr, "preset %s is silent when previewed\n", qPrintable(presets()[i].name)); } }
        }
        check(silentOnes == 0, "every preset and kit can be heard before choosing it");
        app.previewSound(nullptr, nullptr);
        pump(500); peak = 0.f; pump(40);
        check(peak < 0.01f, "closing the list stops the preview");
        app.dirty = false;
    }

    // ---- chopping a loop: the hits are found where they are, and the pieces play from the drums
    {
        app.setSong(demoSong(), QString());
        // a bar at 120 BPM made of library hits at known places, quieter ones among them
        const char* hitsIn[8] = {"nz:Kicks/Punch", "nz:Hats/Closed", "nz:Snares/Tight", "nz:Hats/Closed", "nz:Kicks/Punch", "nz:Percussion/Clave", "nz:Snares/Tight", "nz:Hats/Open"};
        const float gains[8] = {1.f, 0.4f, 0.9f, 0.35f, 1.f, 0.5f, 0.9f, 0.45f};
        dsp::SampleData loop; loop.rate = 48000.f;
        loop.l.assign(96000, 0.f); loop.r.assign(96000, 0.f);
        std::vector<size_t> truth;
        for (int i = 0; i < 8; i++) {
            const auto d = samples::get(QString::fromLatin1(hitsIn[i]));
            const size_t at = size_t(i) * 12000 + (i % 2 ? 900 : 0);   // a little swing on the off-beats
            truth.push_back(at);
            for (size_t k = 0; d && k < d->frames() && at + k < loop.frames(); k++) {
                const double pos = double(k) * d->rate / 48000.0; const size_t j = size_t(pos); if (j >= d->frames()) break;
                loop.l[at + k] += d->l[j] * gains[i]; loop.r[at + k] += d->r[j] * gains[i];
            }
        }
        const auto cuts = samples::findHits(loop, 0.55f, 8);
        int found = 0;
        for (size_t t : truth) for (size_t c : cuts) if (std::abs(double(c) - double(t)) < 48000 * 0.012) { found++; break; }
        if (qEnvironmentVariableIsSet("NZ_DEBUG")) { std::fprintf(stderr, "chop: %d cuts, %d of 8 hits found:", int(cuts.size()), found); for (size_t c : cuts) std::fprintf(stderr, " %d", int(c)); std::fprintf(stderr, "\n"); }
        check(found == 8 && cuts.size() == 8, "chopping finds each hit of a loop, the quiet ones too");
        check(samples::equalCuts(96000, 4) == std::vector<size_t>({0, 24000, 48000, 72000}), "equal pieces are equal");
        check(samples::guessBars(2.0, 120) == 1 && samples::guessBars(8.1, 120) == 4 && samples::guessBars(3.9, 122) == 2, "the length of a loop in bars is guessed from the tempo");
        QString err;
        const int tid = app.chopOntoDrums(loop, cuts, QStringLiteral("selftest beat"), 1, true, err);
        const Track* dt = app.song.track(tid);
        bool onPads = dt != nullptr;
        for (int d = 0; dt && d < 8; d++) onPads &= dt->kit.data[d] != nullptr && dt->kit.sample[d].startsWith(QStringLiteral("selftest beat"));
        check(onPads, "the pieces are on the drum pads");
        const Clip* pc = dt ? app.curClip() : nullptr;
        bool inOrder = pc && pc->notes.size() == 8 && pc->loopLen == BAR;
        for (int i = 0; inOrder && i < 8; i++) inOrder &= pc->notes[size_t(i)].deg == i && std::abs(pc->notes[size_t(i)].start - int(std::lround(truth[size_t(i)] / 96000.0 * BAR / 24.0)) * 24) <= 24;
        check(inOrder, "the pattern plays the pieces where they were in the loop");
        app.seek(0); app.play(); peak = 0.f; pump(200);
        check(peak > 0.05f, "the chopped loop plays");
        app.stop(); pump(4);
        app.undo();
        check(app.song.track(tid) == nullptr || app.song.track(tid)->kit.sample[0] != QStringLiteral("selftest beat 1.wav"), "chopping is one step to undo");
        for (const QString& n : samples::list()) if (n.startsWith(QStringLiteral("selftest beat"))) samples::remove(n);
        app.dirty = false;
    }

    // ---- MIDI, round the system's loop-back port: notes, a learned knob, and the clock both ways
    {
        app.setSong(demoSong(), QString());
        Midi m(&app, false);
        app.midi = &m;
        // messages go in the way the device thread hands them over; what Nachtzug sends comes back the same way
        m.setInput(QString());
        m.sendHook = [&m](const uint8_t* b, int n) { m.incoming(b, n); };
        auto wait = [&](int ms) { QElapsedTimer t; t.start(); while (t.elapsed() < ms) { QCoreApplication::processEvents(); QThread::msleep(2); } };
        auto send = [&](std::initializer_list<int> bytes) { uint8_t b[3]; int n = 0; for (int x : bytes) b[n++] = uint8_t(x); m.incoming(b, n); };
        int bassId = -1; for (const Track& t : app.song.tracks) if (t.role == R_BASS) bassId = t.id;
        app.selectTrack(bassId);
        send({0x90, 60, 110}); wait(40);
        check(app.keyHeld(1060), "a MIDI key plays the selected track");
        send({0x80, 60, 0}); wait(40);
        check(!app.keyHeld(1060), "letting go of a MIDI key ends the note");
        check(m.lastHeard().contains(QLatin1String("velocity 110")), "the settings can show what came in");
        m.learn(Midi::targetForParam(dsp::P_CUTOFF), QStringLiteral("Cutoff"));
        send({0xb0, 74, 10}); wait(40);
        check(m.controllerFor(Midi::targetForParam(dsp::P_CUTOFF)) == 74, "turning a controller knob while learning ties it to the knob");
        send({0xb0, 74, 127}); wait(40);
        check(app.song.track(bassId)->patch.p[dsp::P_CUTOFF] > 0.99f, "the learned controller knob turns the knob on screen");
        send({0xb0, 74, 0}); wait(600);
        check(app.song.track(bassId)->patch.p[dsp::P_CUTOFF] < 0.01f && app.undoName() == paramLabel(dsp::P_CUTOFF), "a turn of the controller knob is one undo step");
        // a MIDI keyboard in Chords mode: D above the key note (A) is the chord on the fourth step
        app.setChordKeys(true);
        send({0x90, 48 + app.song.root + app.song.degToSemis(3), 90}); wait(40);
        check(app.liveChord() == 3, "in Chords mode a MIDI key changes the chord");
        send({0x80, 48 + app.song.root + app.song.degToSemis(3), 0}); wait(20);
        app.setChordKeys(false);
        // a drum track takes the General MIDI drum notes
        int drumId = -1; for (const Track& t : app.song.tracks) if (t.isDrums()) drumId = t.id;
        app.selectTrack(drumId);
        send({0x99, 38, 100}); wait(40);
        check(app.keyHeld(1038), "a drum pad on a MIDI keyboard plays the drum track");
        send({0x89, 38, 0}); wait(20);
        app.selectTrack(bassId);
        // follow a clock at 133 BPM, then its start and stop
        m.setFollowClock(true);
        QElapsedTimer ct; ct.start();
        const double gap = 60.0 / 133.0 / 24.0;
        for (int i = 0; i < 24 * 6; i++) { while (ct.nsecsElapsed() < qint64(i * gap * 1e9)) QThread::usleep(200); send({0xf8}); if (i % 8 == 0) QCoreApplication::processEvents(); }
        wait(100);
        check(std::fabs(app.song.bpm - 133.f) < 1.0f, "the song follows the tempo of an incoming clock");
        send({0xfa}); wait(100); pump(2);   // the self-test has no sound card: the engine runs when pumped
        check(app.isPlaying(), "a start message starts the song");
        send({0xfc}); wait(100); pump(2);
        check(!app.isPlaying(), "a stop message stops it");
        m.setFollowClock(false);
        // the clock Nachtzug sends comes back at the song's tempo
        app.edit([](Song& sg) { sg.bpm = 101.f; });
        pump(2);
        m.setSendClock(true);
        wait(1500);
        if (qEnvironmentVariableIsSet("NZ_DEBUG")) std::fprintf(stderr, "clock out: heard %.1f BPM\n", double(m.clockBpm()));
        check(std::fabs(m.clockBpm() - 101.f) < 1.0f, "the clock Nachtzug sends has the song's tempo");
        m.setSendClock(false);
        m.sendHook = nullptr;
        m.forgetAll();
        app.midi = nullptr;
        app.dirty = false;
    }

    // ---- the live input: heard through its filter, never in a file, and kept close to what is played
    {
        auto through = [](const LiveInput& li, bool rendering) {
            Song sg; sg.input = li;
            Engine e(48000.f);
            if (rendering) e.onlyTrack = 9999;
            e.setSong(std::make_shared<const Song>(sg));
            e.inputOn.store(true);
            std::vector<float> tone(480), out(2 * 480);
            double peak = 0, ph = 0;
            for (int b = 0; b < 200; b++) {
                for (float& x : tone) { x = float(0.5 * std::sin(ph)); ph += 2 * M_PI * 3000.0 / 48000.0; }
                e.pushInput(tone.data(), 480);
                e.process(out.data(), 480);
                if (b > 100) for (float x : out) peak = std::max(peak, double(std::fabs(x)));
            }
            return peak;
        };
        LiveInput open; LiveInput dark; dark.cutoff = 0.25f; LiveInput muted; muted.mute = true;
        const double o = through(open, false), d = through(dark, false), m = through(muted, false), r = through(open, true);
        if (qEnvironmentVariableIsSet("NZ_DEBUG")) std::fprintf(stderr, "input: open %.3f, dark %.3f, muted %.4f, in a file %.4f\n", o, d, m, r);
        check(o > 0.1 && d < o * 0.3, "the input is heard, and its filter darkens it");
        check(m < 0.002 && r < 0.002, "a muted input, and an input while a file is written, are silent");
        // a long backlog is dropped, so the input never lags far behind
        Engine e(48000.f);
        e.setSong(std::make_shared<const Song>(Song()));
        e.inputOn.store(true);
        std::vector<float> lots(20000, 0.1f), out(2 * 256);
        e.pushInput(lots.data(), int(lots.size()));
        e.process(out.data(), 256);
        e.process(out.data(), 256);
        std::vector<float> silence(256, 0.f);
        int quietBlocks = 0;
        for (int b = 0; b < 20; b++) { e.pushInput(silence.data(), 256); e.process(out.data(), 256); float pk = 0; for (float x : out) pk = std::max(pk, std::fabs(x)); if (pk < 0.001f) quietBlocks++; }
        check(quietBlocks > 5, "the input catches up instead of lagging behind");
        app.setSong(demoSong(), QString());
        app.song.input.wah = 0.4f; app.song.input.mute = true;
        const QString sp = QDir::temp().filePath(QStringLiteral("nachtzug-selftest-input.nachtzug"));
        app.save(sp); app.load(sp);
        check(std::fabs(app.song.input.wah - 0.4f) < 1e-4f && app.song.input.mute, "the input's settings are saved with the song");
        QFile::remove(sp); app.dirty = false;
    }

    // ---- right wrong notes and Clean up
    {
        Song sg; sg.scale = 0;
        bool allBend = true;
        for (int d = 0; d < sg.sc().n; d++) {
            const int a = sg.approachAlt(d), semis = sg.degToSemis(d) + a;
            if (a == 0 || sg.degToSemis(sg.degreeNear(semis)) == semis) allBend = false;
        }
        check(allBend, "every note of a seven-note scale has a right wrong note outside the scale");
        // a bent note and a moved chord survive saving and are heard a semitone off
        Track t = makeTrack(sg, R_BASS); t.follow = false; t.arp = 0; t.chord = 0;
        Clip c; c.id = sg.newId(); c.len = c.loopLen = BAR;
        Note n; n.start = 0; n.len = 24; n.deg = 2; n.alt = -1; c.notes = {n};
        t.clips = {c}; sg.tracks = {t};
        sg.setChord(1, 3, -1);
        Song back; Song::fromJson(sg.toJson(), back);
        check(!back.tracks.empty() && !back.tracks[0].clips.empty() && back.tracks[0].clips[0].notes[0].alt == -1, "a right wrong note survives saving");
        check(back.chordAltAt(1) == -1 && back.chordAltAt(0) == 0 && back.chordAt(1) == 3, "a chord moved a semitone survives saving");
        check(back.chordName(3, -1) != back.chordName(3), "a moved chord has its own name");
        std::vector<NoteEvent> log; Engine e(48000.f); e.eventsOnly = true; e.log = &log;
        back.loopEnd = BAR; e.setSong(std::make_shared<const Song>(back));
        EngineEvent pl; pl.type = EngineEvent::Play; e.push(pl);
        std::vector<float> b(2 * 480); for (int i = 0; i < 20; i++) e.process(b.data(), 480);
        const int want = back.midiOf(back.tracks[0], 2) - 1;
        check(std::any_of(log.begin(), log.end(), [&](const NoteEvent& ev) { return ev.on && ev.note == want; }), "a right wrong note sounds a semitone off its row");

        // Clean up: a one-bar figure played four times, with a late note, a doubled note, a missed note, a wrong key
        // and a tiny note pressed by accident
        Clip k; k.len = k.loopLen = 4 * BAR;
        const int fig[4] = {0, 2, 4, 2};
        for (int bar = 0; bar < 4; bar++)
            for (int i = 0; i < 4; i++) { Note m; m.start = bar * BAR + i * BEAT; m.len = 40; m.deg = fig[i]; k.notes.push_back(m); }
        std::vector<Note> neat = k.notes;
        k.notes[1].start += 5;                                        // late
        k.notes.push_back(k.notes[4]);                                // doubled
        k.notes.erase(k.notes.begin() + 10);                          // missed (bar 3, beat 3)
        k.notes[12].deg = 3;                                          // wrong key (bar 4, beat 2) ...
        k.notes[7].deg = 5;                                           // ... and another (bar 2, beat 4)
        { Note blip; blip.start = 2 * BEAT + 2; blip.len = 6; blip.deg = 5; k.notes.push_back(blip); }   // pressed by accident
        const CleanReport r = cleanUpClip(k, false, 24);
        auto same = [](std::vector<Note> a, std::vector<Note> b2) {
            auto key = [](const Note& x) { return std::tuple(x.start, x.deg, x.alt); };
            std::sort(a.begin(), a.end(), [&](const Note& x, const Note& y) { return key(x) < key(y); });
            std::sort(b2.begin(), b2.end(), [&](const Note& x, const Note& y) { return key(x) < key(y); });
            if (a.size() != b2.size()) return false;
            for (size_t i = 0; i < a.size(); i++) if (key(a[i]) != key(b2[i])) return false;
            return true;
        };
        check(same(k.notes, neat), "Clean up restores a repeated figure played with mistakes");
        check(r.snapped == 1 && r.merged == 1 && r.filled == 1 && r.fixed == 2 && r.removed == 1, "Clean up says what it changed");
        check(cleanUpClip(k, false, 24).total() == 0, "Clean up leaves a tidy pattern alone");
        // a generated pattern is already tidy, even on a coarse grid
        Song gs = demoSong();
        bool tidy = true;
        for (Track& tr : gs.tracks) for (Clip& cl : tr.clips) if (!cl.ad.on) { Clip copy = cl; if (cleanUpClip(copy, tr.isDrums(), 48).snapped) tidy = false; }
        check(tidy, "Clean up does not move the notes of the demo song");
    }

    // ---- the tester's findings stay fixed
    {
        // chords recorded far up or down survive saving
        Song sg; sg.chords = {16, -20, 30};
        Song back; Song::fromJson(sg.toJson(), back);
        check(back.chords == std::vector<int>({16, -20, 30}), "chords recorded high or low survive saving");
        // repeats stay inside the pattern, and a note beyond a shortened pattern stays silent even with repeats
        auto hitsOf = [](const Clip& c, bool drums) {
            Song s2; s2.bpm = 120.f; s2.loopEnd = BAR;
            Track t = makeTrack(s2, drums ? R_DRUMS : R_BASS);
            t.clips = {c}; t.clips[0].id = s2.newId();
            s2.tracks.push_back(t);
            std::vector<NoteEvent> log; Engine e(48000.f); e.eventsOnly = true; e.log = &log;
            e.setSong(std::make_shared<const Song>(s2));
            EngineEvent pl; pl.type = EngineEvent::Play; e.push(pl);
            std::vector<float> b(2 * 480); for (int i = 0; i < 199; i++) e.process(b.data(), 480);   // just under two bars
            std::vector<double> ons; for (const NoteEvent& ev : log) if (ev.on) ons.push_back(ev.frame);
            return ons;
        };
        Clip c; c.len = 2 * BAR; c.loopLen = BAR;
        Note n; n.start = 360; n.len = 48; n.deg = dsp::D_SNARE; n.rep = 4; c.notes = {n};
        const double barFrames = 4 * 60.0 / 120.0 * 48000.0;
        bool inside = true; for (double f : hitsOf(c, true)) inside &= std::fmod(f, barFrames) >= 359.0 / BAR * barFrames - 32;
        check(inside, "the repeats of a note at the end of a pattern stay in it");
        Note hidden; hidden.start = 400; hidden.len = 48; hidden.deg = dsp::D_SNARE; hidden.rep = 4; c.notes = {hidden};
        check(hitsOf(c, true).empty(), "a note beyond a shortened pattern stays silent, repeats or not");
        // kept drum hits stay when the drummer takes the clip over, and are saved with it
        Clip dc; dc.len = dc.loopLen = BAR;
        Note k; k.start = 72; k.len = 12; k.deg = dsp::D_RIM; k.lock = true; dc.notes = {k};
        dc.ad.on = true; dc.ad.seed = 5;
        autoDrumFill(dc);
        bool keptThere = false; for (const Note& x : dc.notes) keptThere |= x.lock && x.start == 72 && x.deg == dsp::D_RIM;
        Song s3; Track dt = makeTrack(s3, R_DRUMS); dc.id = s3.newId(); dt.clips = {dc}; s3.tracks.push_back(dt);
        Song s4; Song::fromJson(s3.toJson(), s4);
        bool keptSaved = false; for (const Note& x : s4.tracks[0].clips[0].notes) keptSaved |= x.lock && x.start == 72;
        check(keptThere && keptSaved, "kept hits stay when the drummer writes the clip, and are saved with it");
        // chopping onto a song without drums is one undo step
        app.setSong(Song(), QString());
        app.edit([](Song& sg2) { sg2.tracks.push_back(makeTrack(sg2, R_BASS)); });
        dsp::SampleData loop; loop.rate = 48000.f; loop.l.assign(48000, 0.f); loop.r.assign(48000, 0.f);
        for (int h = 0; h < 4; h++) for (int i = 0; i < 2000; i++) loop.l[size_t(h * 12000 + i)] = loop.r[size_t(h * 12000 + i)] = std::sin(i * 0.2f) * std::exp(-i / 400.f);
        QString err;
        const int before = int(app.song.tracks.size());
        app.chopOntoDrums(loop, samples::findHits(loop, 0.5f, 8), QStringLiteral("selftest undo"), 1, true, err);
        app.undo();
        check(int(app.song.tracks.size()) == before, "chopping onto a song without drums is one undo step");
        for (const QString& nm : samples::list()) if (nm.startsWith(QStringLiteral("selftest undo"))) samples::remove(nm);
        app.dirty = false;
    }

    // ---- Pump: a sustained pad dips after each kick, and only when asked to
    {
        auto dips = [](float amount) {
            Song sg; sg.bpm = 120.f; sg.loopEnd = BAR;
            Track dr = makeTrack(sg, R_DRUMS);
            Clip c; c.id = sg.newId(); c.len = BAR; c.loopLen = BAR;
            for (int b = 0; b < 4; b++) { Note n; n.start = b * 96; n.len = 12; n.deg = dsp::D_KICK; c.notes.push_back(n); }
            dr.clips.push_back(c);
            Track pad = makeTrack(sg, R_PAD);
            pad.duck = amount; pad.patch.p[dsp::P_AA] = 0.f; pad.patch.p[dsp::P_AS] = 1.f; pad.patch.p[dsp::P_DELAY] = 0.f; pad.patch.p[dsp::P_REVERB] = 0.f;
            Clip pc; pc.id = sg.newId(); pc.len = BAR; pc.loopLen = BAR;
            Note n; n.start = 0; n.len = BAR; n.deg = 0; pc.notes.push_back(n);
            pad.clips.push_back(pc);
            const int padId = pad.id;
            sg.tracks.push_back(dr); sg.tracks.push_back(pad);
            Engine e(48000.f);
            e.onlyTrack = padId;   // the kick drives the pump without being heard
            e.setSong(std::make_shared<const Song>(sg));
            EngineEvent play; play.type = EngineEvent::Play; e.push(play);
            std::vector<float> out(2 * 48000);
            e.process(out.data(), 48000);   // two beats... one second at 120 BPM
            auto rms = [&](double from, double to) { double sum = 0; int n = 0; for (int i = int(from * 48000); i < int(to * 48000); i++) { sum += double(out[size_t(i) * 2]) * out[size_t(i) * 2]; n++; } return std::sqrt(sum / std::max(1, n)); };
            return rms(0.505, 0.52) / std::max(1e-9, rms(0.70, 0.98));   // just after the second kick, against the swell before the third
        };
        const double with = dips(1.f), without = dips(0.f);
        check(with < 0.4 && without > 0.8, "Pump: the pad dips after the kick, and does not without it");
    }

    std::printf(problems ? "self-test: %d problem(s)\n" : "self-test passed\n", problems);
    return problems ? 1 : 0;
}

int main(int argc, char** argv) {
    QString renderPath, shotPath, songPath, themeName, langName, dialogName, geomPath, kitName;
    int tab = 0, trackIndex = -1, arrangeLen = -1;
    QSize winSize;   // for pictures: the window at this size (as small as it allows)
    QString lessonArg;   // for pictures: "acid:3" shows step 3 of that lesson
    bool guide = false, selftest = false, loopsView = false, linesView = false, tipCheck = false, simpleView = false, guiTest = false, chordKeys = false;
    for (int i = 1; i < argc; i++) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        if (a == "--render" && i + 1 < argc) renderPath = QString::fromLocal8Bit(argv[++i]);
        else if (a == "--screenshot" && i + 1 < argc) shotPath = QString::fromLocal8Bit(argv[++i]);
        else if (a == "--theme" && i + 1 < argc) themeName = QString::fromLocal8Bit(argv[++i]);
        else if (a == "--tab" && i + 1 < argc) tab = QString::fromLocal8Bit(argv[++i]).toInt();
        else if (a == "--track" && i + 1 < argc) trackIndex = QString::fromLocal8Bit(argv[++i]).toInt();
        else if (a == "--lang" && i + 1 < argc) langName = QString::fromLocal8Bit(argv[++i]);
        else if (a == "--guide") guide = true;
        else if (a == "--simple") simpleView = true;
        else if (a == "--chord-keys") chordKeys = true;
        else if (a == "--lesson" && i + 1 < argc) lessonArg = QString::fromLocal8Bit(argv[++i]);
        else if (a == "--size" && i + 1 < argc) { const QStringList wh = QString::fromLocal8Bit(argv[++i]).split('x'); if (wh.size() == 2) winSize = QSize(wh[0].toInt(), wh[1].toInt()); }
        else if (a == "--geometry" && i + 1 < argc) geomPath = QString::fromLocal8Bit(argv[++i]);
        else if (a == "--kit" && i + 1 < argc) kitName = QString::fromLocal8Bit(argv[++i]);
        else if (a == "--arrange" && i + 1 < argc) arrangeLen = QString::fromLocal8Bit(argv[++i]).toInt();
        else if (a == "--dialog" && i + 1 < argc) dialogName = QString::fromLocal8Bit(argv[++i]);
        else if (a == "--loops") loopsView = true;
        else if (a == "--lines") linesView = true;
        else if (a == "--check-tooltips") tipCheck = true;
        else if (a == "--selftest") selftest = true;
        else if (a == "--guitest") guiTest = true;
        else if (!a.startsWith("--")) songPath = a;
    }
    if ((!renderPath.isEmpty() || !shotPath.isEmpty() || selftest || tipCheck || guiTest) && !qEnvironmentVariable("QT_QPA_PLATFORM").startsWith(QLatin1String("offscreen")))
        qputenv("QT_QPA_PLATFORM", "offscreen");   // (an offscreen setting given from outside, such as a screen size, is kept)
    // a finished Mac program has no window-less platform packed in: its build script tests it on the real one
    if (qEnvironmentVariableIsSet("NZ_TEST_PLATFORM") && (selftest || !renderPath.isEmpty())) qputenv("QT_QPA_PLATFORM", qgetenv("NZ_TEST_PLATFORM"));

    // test runs and pictures keep their own settings and sample folder, so they neither depend on nor change yours
    const bool testRun = !shotPath.isEmpty() || selftest || tipCheck || guiTest;
    // the size of everything (Settings > General). It has to be known before the program starts drawing.
    // Automatic uses a size worked out from the screen at an earlier start (see fitToScreen below).
    double uiScale = 1.0;
    bool autoScale = true;
    const QByteArray outsideScale = qgetenv("QT_SCALE_FACTOR");   // one set from outside is kept, and multiplied
    if (!testRun && renderPath.isEmpty()) {
        QSettings pre(QStringLiteral("Nachtzug"), QStringLiteral("Nachtzug"));   // the same place QSettings() uses once the program has its name
        const double chosen = pre.value(QStringLiteral("uiScale"), 0.0).toDouble();
        autoScale = chosen <= 0.0;
        uiScale = std::clamp(autoScale ? pre.value(QStringLiteral("autoScale"), 1.0).toDouble() : chosen, 0.5, 3.0);
        if (std::fabs(uiScale - 1.0) > 0.001) {
            const double base = qEnvironmentVariableIsSet("QT_SCALE_FACTOR") ? qEnvironmentVariable("QT_SCALE_FACTOR").toDouble() : 1.0;
            qputenv("QT_SCALE_FACTOR", QByteArray::number(std::max(0.25, base) * uiScale, 'f', 3));
        }
    }

#if defined(Q_OS_WIN)
    // a window program on Windows has no console: the tests and --render print into the one they were started from
    if ((selftest || guiTest || tipCheck || !renderPath.isEmpty()) && AttachConsole(ATTACH_PARENT_PROCESS)) {
        std::freopen("CONOUT$", "w", stdout); std::freopen("CONOUT$", "w", stderr);
    }
#endif
    QApplication qa(argc, argv);
    qa.setProperty("uiScale", uiScale);   // for the settings window: the size in use now
    qa.setProperty("outsideScale", QString::fromLocal8Bit(outsideScale));   // for starting again: the scale set from outside, if any
    QApplication::setApplicationName(testRun ? QStringLiteral("Nachtzug-test") : QStringLiteral("Nachtzug"));
    QApplication::setOrganizationName(QStringLiteral("Nachtzug"));
    if (testRun) { QSettings().clear(); QSettings().setValue(QStringLiteral("welcomed"), true); }
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QApplication::setWindowIcon(theme::appIcon());
    theme::loadFonts();
    // the language: chosen in View > Language, or the system's; English is built in
    if (langName.isEmpty() && (selftest || guiTest || tipCheck)) langName = QStringLiteral("en");   // the tests look for English words
    if (langName.isEmpty()) langName = QSettings().value(QStringLiteral("language")).toString();
    if (langName.isEmpty()) langName = QLocale::system().name().left(2);
    static QTranslator own, qtbase;
    // numbers follow the language of the program (5,3 ms in German), whatever the system is set to
    QLocale::setDefault(own.load(QStringLiteral(":/i18n/nachtzug_%1.qm").arg(langName)) ? QLocale(langName) : QLocale(QLocale::English, QLocale::UnitedKingdom));
    if (!own.isEmpty()) {
        QApplication::installTranslator(&own);
        const QString qtName = QStringLiteral("qtbase_%1").arg(langName);
        if (qtbase.load(qtName, QLibraryInfo::path(QLibraryInfo::TranslationsPath))
            || qtbase.load(qtName, QCoreApplication::applicationDirPath() + QStringLiteral("/../Resources/translations")))   // inside Nachtzug.app
            QApplication::installTranslator(&qtbase);
    }
#if defined(Q_OS_MACOS)
    static MacKeyNames macKeys(&own);   // asks own first when it holds a language
    QApplication::installTranslator(&macKeys);
#endif
    if (themeName.isEmpty()) themeName = QSettings().value(QStringLiteral("look"), QStringLiteral("Bakelite")).toString();   // the instrument look by default
    int themeIndex = 0;
    for (int i = 0; i < theme::count(); i++) if (theme::name(i).compare(themeName, Qt::CaseInsensitive) == 0) themeIndex = i;
    theme::apply(themeIndex);
    theme::installWells();
    QApplication::setPalette(theme::palette());
    qa.setStyleSheet(theme::styleSheet());

    // the very first start: ask for the language and how to begin
    if (!testRun && renderPath.isEmpty() && !QSettings().value(QStringLiteral("welcomed"), false).toBool()) {
        WelcomeDialog welcome([&](const QString& code) {
            QApplication::removeTranslator(&own); QApplication::removeTranslator(&qtbase);
            const QString l = code.isEmpty() ? QLocale::system().name().left(2) : code;
            if (own.load(QStringLiteral(":/i18n/nachtzug_%1.qm").arg(l))) {
                QLocale::setDefault(QLocale(l));
                QApplication::installTranslator(&own);
                if (qtbase.load(QStringLiteral("qtbase_%1").arg(l), QLibraryInfo::path(QLibraryInfo::TranslationsPath))
                    || qtbase.load(QStringLiteral("qtbase_%1").arg(l), QCoreApplication::applicationDirPath() + QStringLiteral("/../Resources/translations")))
                    QApplication::installTranslator(&qtbase);
            } else {
                QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedKingdom));
                // English: an empty translator still sends the change, so the window goes back to English
                QEvent ev(QEvent::LanguageChange);
                for (QWidget* w : QApplication::topLevelWidgets()) QApplication::sendEvent(w, &ev);
            }
        });
        if (welcome.exec() != QDialog::Accepted) QSettings().setValue(QStringLiteral("welcomed"), true);   // closed: do not ask again
    }

    if (selftest) return selfTest();
    if (!renderPath.isEmpty()) {
        Song s = demoSong();
        if (!songPath.isEmpty()) {
            QFile f(songPath);
            if (!f.open(QIODevice::ReadOnly) || !Song::fromJson(QJsonDocument::fromJson(f.readAll()).object(), s)) {
                std::fprintf(stderr, "Could not read the song %s\n", qPrintable(songPath));
                return 1;
            }
        }
        if (arrangeLen >= 0) { std::mt19937 g(7); arrangeSong(s, arrangeLen, true, g); }
        for (const DrumKit& k : drumKits()) if (k.name == kitName) for (Track& t : s.tracks) if (t.isDrums()) t.kit = chooseKit(t.kit, k);
        resolveSamples(s);
        const bool ok = renderToWav(s, renderPath, 48000.f, {});
        if (!ok) std::fprintf(stderr, "Could not write %s\n", qPrintable(renderPath));
        return ok ? 0 : 1;
    }

    if (dialogName == QLatin1String("welcome") && !shotPath.isEmpty()) {   // picture of the first-start window
        WelcomeDialog wd({});
        wd.show();
        QTimer::singleShot(600, &wd, [&] { wd.grab().save(shotPath); QApplication::quit(); });
        return qa.exec();
    }
    App app(shotPath.isEmpty());
    {   // keyboards, controllers and clock (test runs do not keep their choices)
        app.midi = new Midi(&app, !testRun);
        QObject::connect(app.midi, &Midi::mappingsChanged, &app, [] { for (QWidget* x : QApplication::allWidgets()) if (x->property("midiTarget").isValid()) x->update(); });
    }
    Knob::midiLookup = [&app](const QString& target) { return app.midi ? app.midi->controllerFor(target) : -1; };
    if (!songPath.isEmpty() && !app.load(songPath)) std::fprintf(stderr, "Could not read the song %s\n", qPrintable(songPath));
    if (trackIndex >= 0 && trackIndex < int(app.song.tracks.size())) app.selectTrack(app.song.tracks[size_t(trackIndex)].id);
    if (linesView && app.song.tracks.size() > 3) { // for test pictures: a drawn cutoff line on the bass, and a fade on the pad
        Track& bass = app.song.tracks[1];
        bass.laneOpen = true; bass.laneParam = dsp::P_CUTOFF;
        AutoLane l; l.param = dsp::P_CUTOFF; l.loop = 2 * BAR;
        l.pts = {{0, 0.22f}, {BAR, 0.62f}, {BAR + 192, 0.40f}, {2 * BAR - 24, 0.30f}};
        bass.lanes.push_back(l);
        if (!app.song.tracks[3].clips.empty()) { Clip& c = app.song.tracks[3].clips[0]; c.inKind = 3; c.inLen = 2 * BAR; c.outKind = 1; c.outLen = BAR; }
        app.touch();
    }
    if (loopsView) { // for test pictures: the loop grid with a few cells, one column playing
        app.setLiveMode(true);    // fills column 1 from the clips; the window switches to the grid after it is shown, as a person would
        app.setLiveMode(false);
        for (size_t i = 0; i < app.song.tracks.size(); i++) { const int id = app.song.tracks[i].id; app.newCell(id, 1); if (i % 2) app.newCell(id, 2); }
        for (Track& t : app.song.tracks) t.liveCell = t.cell(0) ? 0 : -1;
        app.touch();
    }
    if (arrangeLen >= 0) { arrangeSong(app.song, arrangeLen, true, app.rng); app.editClip = -1; app.touch(); app.refreshSelection(); }   // for test pictures
    for (const DrumKit& k : drumKits()) if (k.name == kitName) { for (Track& t : app.song.tracks) if (t.isDrums()) t.kit = chooseKit(t.kit, k); app.touch(); }
    MainWindow w(&app);
    FileOpener opener(&app, &w);
    qa.installEventFilter(&opener);
    if (auto* tabs = w.findChild<QTabWidget*>()) tabs->setCurrentIndex(tab);
    if (simpleView) if (auto* sp = w.findChild<SoundPanel*>()) sp->setSimple(true);
    if (chordKeys) { app.setChordKeys(true); app.playChord(app.song.q(5)); }   // for pictures: Chords mode, with the sixth playing
    if (winSize.isValid()) w.resize(winSize);   // as near as the window allows
    if (!testRun) {
        // does the window fit the screen? With the size on Automatic, a size that fits is worked out and the program
        // starts again at it (once, before anything is shown). Otherwise it opens as large as the screen allows.
        const QRect avail = w.screen() ? w.screen()->availableGeometry() : QRect(0, 0, 1560, 980);
        const QSize need = w.minimumSizeHint() + QSize(0, 34);   // and the window's title bar
        const double fit = std::min(double(avail.width()) / need.width(), double(avail.height()) / need.height()) * uiScale;
        if (autoScale && !qEnvironmentVariableIsSet("NZ_RELAUNCHED")) {
            // on a laptop screen everything a little smaller, so the window is not cramped: the size at which a
            // window of 1500 × 920 would fit, if that is smaller than the size the window merely needs
            const double comfort = std::min(double(avail.width()) / 1500.0, double(avail.height()) / 920.0) * uiScale;
            double best = 0.8;   // the smallest size offered; below it the window is maximised and scrolls
            for (double c : {0.8, 0.85, 0.9, 1.0}) if (c <= std::min(fit, std::max(0.85, comfort)) + 0.001) best = c;
            if (std::fabs(best - uiScale) > 0.001) {
                QSettings().setValue(QStringLiteral("autoScale"), best);
                QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
                env.insert(QStringLiteral("NZ_RELAUNCHED"), QStringLiteral("1"));
                if (outsideScale.isEmpty()) env.remove(QStringLiteral("QT_SCALE_FACTOR")); else env.insert(QStringLiteral("QT_SCALE_FACTOR"), QString::fromLocal8Bit(outsideScale));
                QProcess again;
                again.setProgram(QCoreApplication::applicationFilePath());
                again.setArguments(QCoreApplication::arguments().mid(1));
                again.setProcessEnvironment(env);
                if (again.startDetached()) return 0;
            }
        }
        const QSize want(std::min(1560, avail.width()), std::min(980, avail.height() - 34));
        w.resize(want.expandedTo(w.minimumSizeHint()));
        if (w.width() > avail.width() || w.height() + 34 > avail.height()) w.setWindowState(Qt::WindowMaximized);
    }
    w.show();
    if (!testRun) {   // a spare copy of unsaved work, and the one a crash left behind
        auto* autosave = new Autosave(&app);
        QObject::connect(&qa, &QApplication::aboutToQuit, autosave, &Autosave::finish);
        QTimer::singleShot(300, &w, [autosave, &w] { autosave->offerRecovery(&w); });
    }
    if (arrangeLen >= 0) QTimer::singleShot(400, &w, [&] { if (auto* tl = w.findChild<Timeline*>()) tl->showWholeSong(); });
    if (!lessonArg.isEmpty()) QTimer::singleShot(200, &w, [&] {
        const QStringList parts = lessonArg.split(':');
        w.startLesson(parts[0]);
        for (int k = 1; k < (parts.size() > 1 ? parts[1].toInt() : 1) && w.lessons(); k++) w.lessons()->next();
    });
    if (guiTest) QTimer::singleShot(150000, &w, [] {   // a test that hangs is a failed test
        std::fprintf(stderr, "FAILED: the window test did not finish in time\n");
        for (QWidget* p = QApplication::activePopupWidget(); p; p = QApplication::activePopupWidget()) p->close();
        std::_Exit(2);
    });
    if (guiTest) QTimer::singleShot(300, &w, [&] {   // drive the window's own controls, the way a person would
        int problems = 0;
        auto check = [&](bool ok, const char* what) { if (!ok) { problems++; std::fprintf(stderr, "FAILED: %s\n", what); } };
        auto* sp = w.findChild<SoundPanel*>();
        auto* tabs = w.findChild<QTabWidget*>();
        tabs->setCurrentIndex(1);
        Track* bass = nullptr; for (Track& t : app.song.tracks) if (t.role == R_BASS) bass = &t;
        app.selectTrack(bass->id);
        if (qEnvironmentVariableIsSet("NZ_DEBUG")) std::fprintf(stderr, "step: simple switch\n");
        // View > Simple sound page
        QAction* simpleAct = w.findChild<QAction*>(QStringLiteral("simpleSound"));
        check(simpleAct != nullptr && simpleAct->isCheckable() && !simpleAct->isChecked(), "the View menu has Simple sound page, not ticked on the full page");
        if (simpleAct) simpleAct->trigger();
        QApplication::processEvents();
        check(sp->isSimple() && QSettings().value(QStringLiteral("simpleSound")).toBool(), "Simple shows the simple page and is remembered");
        if (qEnvironmentVariableIsSet("NZ_DEBUG")) std::fprintf(stderr, "step: big knobs\n");
        // every big knob moves its settings, and one undo step takes each move back
        QList<Knob*> big;
        for (Knob* k : sp->findChildren<Knob*>()) if (k->isVisible() && k->width() > 70) big << k;
        check(big.size() == 8, "the simple synth page has eight big knobs");
        for (Knob* k : big) {
            const Song before = app.song;
            const float v0 = k->value();
            emit k->pressed(); emit k->moved(std::clamp(v0 + 0.3f, 0.f, 1.f)); emit k->released();
            const Track* t = app.song.track(bass->id);
            bool changed = false; for (int i = 0; i < dsp::P_COUNT; i++) changed |= t->patch.p[i] != before.track(bass->id)->patch.p[i];
            if (v0 < 0.69f) check(changed, "a big knob changes the sound");
            check(app.undoName() == k->name(), "the knob's move is an undo step named after it");
            app.undo();
            bool same = true; for (int i = 0; i < dsp::P_COUNT; i++) same &= app.song.track(bass->id)->patch.p[i] == before.track(bass->id)->patch.p[i];
            check(same, "undo puts every setting back");
        }
        if (qEnvironmentVariableIsSet("NZ_DEBUG")) std::fprintf(stderr, "step: drum page\n");
        // drums simple page: eight drums, each with its sample button
        Track* dr = nullptr; for (Track& t : app.song.tracks) if (t.isDrums()) dr = &t;
        app.selectTrack(dr->id); QApplication::processEvents();
        int sampleButtons = 0; for (QPushButton* b : sp->findChildren<QPushButton*>(QStringLiteral("sample"))) sampleButtons += b->isVisible();
        check(sampleButtons == dsp::D_COUNT, "each drum on the simple drum page can take a sample");
        if (qEnvironmentVariableIsSet("NZ_DEBUG")) std::fprintf(stderr, "step: sample menu\n");
        // the sample menu under a drum: the library in submenus, and resting on a sound plays it
        {
            QPushButton* sb = nullptr;
            for (QPushButton* b : sp->findChildren<QPushButton*>(QStringLiteral("sample"))) if (b->isVisible()) { sb = b; break; }
            bool sawLibrary = false, previewed = false;
            int tries = 0;
            std::function<void()> inspect = [&] {
                QMenu* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
                if (!menu) for (QWidget* top : QApplication::topLevelWidgets()) if (auto* m = qobject_cast<QMenu*>(top); m && m->isVisible() && !m->actions().isEmpty() && m->actions().first()->isCheckable()) menu = m;
                if (!menu) {   // not open yet: look again in a moment, but never wait for ever
                    if (++tries < 40) QTimer::singleShot(50, &w, inspect);
                    else for (QWidget* top : QApplication::topLevelWidgets()) if (auto* m = qobject_cast<QMenu*>(top); m && m->isVisible()) m->close();
                    return;
                }
                QAction* train = nullptr;
                for (QAction* a : menu->actions())   // the submenu that holds the rail joint, in any language
                    if (a->menu()) for (QAction* x : a->menu()->actions()) if (x->data().toString() == QLatin1String("nz:Train/Rail joint")) train = a;
                sawLibrary = train != nullptr;
                if (qEnvironmentVariableIsSet("NZ_GUISHOT")) menu->grab().save(qEnvironmentVariable("NZ_GUISHOT") + QStringLiteral(".menu.png"));
                if (train) {
                    menu->setActiveAction(train);
                    QTimer::singleShot(250, &w, [&, menu, train] {
                        QMenu* sub = train->menu();
                        if (sub && !sub->actions().isEmpty()) {
                            app.engine->previewReq.store(nullptr);
                            emit sub->hovered(sub->actions().first());   // as when the mouse rests on it
                            previewed = app.engine->previewReq.load() != nullptr;
                            if (qEnvironmentVariableIsSet("NZ_GUISHOT")) sub->grab().save(qEnvironmentVariable("NZ_GUISHOT") + QStringLiteral(".sub.png"));
                        }
                        menu->close();
                    });
                } else menu->close();
            };
            QTimer::singleShot(150, &w, inspect);
            if (sb) sb->click();
            check(sb && sawLibrary, "the sample menu offers the Nachtzug library");
            check(previewed, "resting on a library sound plays it");
        }
        if (qEnvironmentVariableIsSet("NZ_DEBUG")) std::fprintf(stderr, "step: back to full\n");
        // back to Full
        if (simpleAct) simpleAct->trigger();
        QApplication::processEvents();
        check(!sp->isSimple(), "Full shows every knob again");
        sp->setSimple(true); QApplication::processEvents();   // the settings' choice ticks the menu item too
        check(simpleAct && simpleAct->isChecked(), "the menu item follows the settings");
        sp->setSimple(false); QApplication::processEvents();
        if (qEnvironmentVariableIsSet("NZ_DEBUG")) std::fprintf(stderr, "step: panels\n");
        // the panels: a section folds shut when its name is clicked; the inspector goes away and comes back with its
        // sections; the tabs go into a window of their own and come back
        {
            for (const Track& tk : app.song.tracks) if (!tk.isDrums()) { app.selectTrack(tk.id); break; }
            sp->setSimple(false);
            w.findChild<QTabWidget*>()->setCurrentIndex(1);
            QApplication::processEvents();
            QGroupBox* osc = nullptr;
            for (QGroupBox* g : sp->findChildren<QGroupBox*>()) if (g->isVisible() && g->title().contains(QStringLiteral("OSCILLATORS"))) osc = g;
            check(osc != nullptr, "the Sound tab has its Oscillators section");
            auto clickAt = [](QWidget* wd, QPoint at) {
                QMouseEvent press(QEvent::MouseButtonPress, QPointF(at), QPointF(wd->mapToGlobal(at)), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(wd, &press);
                QMouseEvent rel(QEvent::MouseButtonRelease, QPointF(at), QPointF(wd->mapToGlobal(at)), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(wd, &rel);
            };
            if (osc) {
                const int tall = osc->height();
                clickAt(osc, QPoint(30, 6));
                QApplication::processEvents();
                check(osc->property("folded").toBool() && osc->height() < tall / 2, "clicking a section's name folds it shut");
                clickAt(osc, QPoint(30, 6));
                QApplication::processEvents();
                check(!osc->property("folded").toBool() && osc->height() >= tall - 2, "and clicking it again opens it");
            }
            auto* insp = w.findChild<QWidget*>(QStringLiteral("inspector"));
            QAction* inspAct = w.findChild<QAction*>(QStringLiteral("inspectorAct"));
            QGroupBox* how = nullptr;
            for (QGroupBox* g : w.findChildren<QGroupBox*>()) if (g->title().contains(QStringLiteral("HOW NOTES"))) how = g;
            check(insp && insp->isVisible() && how && insp->isAncestorOf(how), "the inspector holds How notes are played");
            if (inspAct && how) {
                inspAct->trigger(); QApplication::processEvents();
                check(!insp->isVisible() && sp->isAncestorOf(how), "without the inspector, How notes are played is back on the Sound tab");
                auto* tab = w.findChild<QToolButton*>(QStringLiteral("inspectorTab"));
                check(tab && tab->isVisible(), "a tab on the left edge offers the inspector back");
                if (tab) tab->click(); QApplication::processEvents();
                check(insp->isVisible() && insp->isAncestorOf(how) && !tab->isVisible(), "the tab brings the inspector back, with How notes are played in it");
            }
            QAction* own = w.findChild<QAction*>(QStringLiteral("ownWindowAct"));
            auto* tabsW = w.findChild<QTabWidget*>();
            if (own) {
                own->trigger(); QApplication::processEvents();
                QTabWidget* t2 = nullptr; for (QWidget* top : QApplication::topLevelWidgets()) if (top != &w && top->isVisible()) if (auto* t = top->findChild<QTabWidget*>()) t2 = t;
                check(t2 != nullptr && !w.isAncestorOf(t2), "the tabs go into a window of their own");
                own->trigger(); QApplication::processEvents();
                check(w.findChild<QTabWidget*>() != nullptr && w.findChild<QTabWidget*>()->isVisible(), "and come back when it closes");
            }
            (void)tabsW;
        }
        {   // the on-screen keys open and close with their button, whatever room the editor has
            w.findChild<QTabWidget*>()->setCurrentIndex(0);
            QApplication::processEvents();
            auto* toggle = w.findChild<QPushButton*>(QStringLiteral("padsToggle"));
            auto* keys = w.findChild<KeyPads*>();
            check(toggle && keys && toggle->isVisible(), "the keys row has a button for the on-screen keys");
            if (toggle && keys) {
                const bool before = keys->isVisible();
                toggle->click(); QApplication::processEvents();
                check(keys->isVisible() != before, "the button shows or hides the on-screen keys");
                toggle->click(); QApplication::processEvents();
                check(keys->isVisible() == before, "and a second press undoes it");
            }
        }
        if (qEnvironmentVariableIsSet("NZ_DEBUG")) std::fprintf(stderr, "step: undo key\n");
        // the undo key in the top bar, and the note that offers Undo after a big change
        app.selectTrack(bass->id);
        const int clips = int(app.song.track(bass->id)->clips.size());
        app.removeTrack(bass->id); QApplication::processEvents();
        auto* toast = w.findChild<Toast*>();
        check(toast && toast->isVisible(), "deleting a track shows a note offering Undo");
        if (qEnvironmentVariableIsSet("NZ_GUISHOT")) w.grab().save(qEnvironmentVariable("NZ_GUISHOT"));   // a picture with the note showing
        QToolButton* undoKey = nullptr;
        for (QToolButton* b : w.findChildren<QToolButton*>()) if (b->toolTip().startsWith(QStringLiteral("Undo: Delete track"))) undoKey = b;
        check(undoKey && undoKey->isEnabled(), "the undo key says what it will undo");
        if (undoKey) undoKey->click();
        QApplication::processEvents();
        check(app.song.track(bass->id) && int(app.song.track(bass->id)->clips.size()) == clips, "the undo key brings the track back");
        // the export key is there and says what it does
        bool exportKey = false; for (QToolButton* b : w.findChildren<QToolButton*>(QStringLiteral("exportKey"))) exportKey = b->isVisible() && b->toolTip().contains(QStringLiteral("WAV"));
        check(exportKey, "the export key is in the top bar");
        // ---- round 3: the new controls, used the way a person would
        auto dbg = [](const char* step) { if (qEnvironmentVariableIsSet("NZ_DEBUG")) std::fprintf(stderr, "step: %s\n", step); };
        auto settle = [](int ms) { QElapsedTimer t; t.start(); while (t.elapsed() < ms) { QApplication::processEvents(); QThread::msleep(5); } };
        // runs f on the next window that opens over the main one, then makes sure it is gone
        auto onModal = [&](std::function<void(QWidget*)> f) {
            auto tries = std::make_shared<int>(0);
            auto poll = std::make_shared<std::function<void()>>();
            *poll = [&w, f, tries, poll] {
                QWidget* d = QApplication::activeModalWidget();
                if (!d) { if (++*tries < 100) QTimer::singleShot(30, &w, *poll); return; }
                f(d);
                if (d->isVisible()) d->close();
            };
            QTimer::singleShot(30, &w, *poll);
        };
        auto byLessonKey = [&](const char* key) -> QWidget* { for (QWidget* x : w.findChildren<QWidget*>()) if (x->isVisible() && x->property("lesson").toString() == QLatin1String(key)) return x; return nullptr; };
        tabs->setCurrentIndex(0);
        app.selectTrack(bass->id);
        if (Track* bt = app.song.track(bass->id); bt && !bt->clips.empty()) app.openClip(bt->clips[0].id);
        settle(50);
        dbg("chance menu");
        {
            Clip* c = app.curClip();
            for (Note& n : c->notes) n.sel = n.start < BAR / 2;
            auto* chanceKey = qobject_cast<QToolButton*>(byLessonKey("chance"));
            check(chanceKey && chanceKey->menu() && chanceKey->menu()->actions().size() == 4, "the Chance key offers four choices");
            if (chanceKey && chanceKey->menu()) chanceKey->menu()->actions()[2]->trigger();
            bool half = true, any = false; for (const Note& n : app.curClip()->notes) if (n.start < BAR / 2) { any = true; half &= n.chance == 50; }
            check(any && half, "Half the time sets the chance of the selected notes");
            dbg("keep and vary");
            QPushButton* keepKey = nullptr; for (QPushButton* b : w.findChildren<QPushButton*>()) if (b->isVisible() && b->text() == QStringLiteral("Keep")) keepKey = b;
            if (keepKey) keepKey->click();
            const std::vector<Note> was = app.curClip()->notes;
            QPushButton* varyKey = qobject_cast<QPushButton*>(byLessonKey("vary"));
            for (int i = 0; i < 25 && varyKey; i++) { varyKey->click(); settle(5); }
            bool kept = true;
            for (const Note& n0 : was) if (n0.sel) kept &= std::any_of(app.curClip()->notes.begin(), app.curClip()->notes.end(), [&](const Note& n) { return n.start == n0.start && n.deg == n0.deg && n.lock; });
            check(keepKey && kept, "Keep protects notes from Vary");
            app.undo();
        }
        dbg("chords switch");
        {
            QWidget* ck = byLessonKey("chordKeys");
            if (auto* b = qobject_cast<QPushButton*>(ck)) b->click();
            check(app.chordKeys(), "the Chords switch under the notes turns Chords mode on");
            app.keyDown(1 * 16 + 4, 1, 4); app.keyUp(1 * 16 + 4);
            check(app.liveChord() == 4, "a letter key in Chords mode sets the chord");
            QKeyEvent esc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QApplication::sendEvent(&w, &esc);
            check(app.liveChord() == Engine::NO_CHORD, "Esc goes back to the song's chords");
            if (auto* b = qobject_cast<QPushButton*>(byLessonKey("noteKeys"))) b->click();
            check(!app.chordKeys(), "the Notes switch turns Chords mode off");
        }
        dbg("preset preview");
        {
            tabs->setCurrentIndex(1); settle(30);
            QComboBox* presetBox = nullptr;
            for (QComboBox* c : sp->findChildren<QComboBox*>()) if (c->isVisible() && c->toolTip().contains(QStringLiteral("rest the mouse on a sound"))) presetBox = c;
            check(presetBox != nullptr, "the preset list can be heard");
            if (presetBox) {
                presetBox->showPopup(); settle(50);
                app.engine->phraseReq.store(nullptr);
                emit presetBox->highlighted(3);
                settle(300);
                check(app.engine->phraseReq.load() != nullptr || app.engine->stopPhrase.load() == false, "resting on a preset plays a phrase with it");
                presetBox->hidePopup(); settle(30);
            }
            tabs->setCurrentIndex(0);
        }
        dbg("lesson");
        {
            app.stop(); settle(100);
            w.startLesson(QStringLiteral("acid"));
            check(w.lessons() && w.lessons()->running() && w.lessons()->step() == 0, "a lesson starts at its first step");
            if (app.audioRunning()) {
                app.play();
                settle(1600);
                check(w.lessons()->step() == 1, "a lesson step moves on by itself once it is done");
                app.stop();
            }
            QPushButton* end = nullptr; for (QPushButton* b : w.findChildren<QPushButton*>()) if (b->isVisible() && b->text() == QStringLiteral("End lesson")) end = b;
            if (end) end->click();
            check(end && !w.lessons()->running(), "End lesson ends it");
        }
        dbg("export kinds");
        {
            bool stemsPath = false, midiOff = false;
            onModal([&](QWidget* d) {
                QList<QRadioButton*> radios = d->findChildren<QRadioButton*>();
                for (QRadioButton* r : radios) if (r->text().contains(QStringLiteral("each track"))) r->click();
                for (QLineEdit* e : d->findChildren<QLineEdit*>()) stemsPath = e->text().endsWith(QStringLiteral(" (tracks)"));
                for (QRadioButton* r : radios) if (r->text().contains(QStringLiteral("MIDI"))) r->click();
                for (QComboBox* c : d->findChildren<QComboBox*>()) midiOff = !c->isEnabled();
                for (QRadioButton* r : radios) if (r->text().contains(QStringLiteral("One sound file"))) r->click();
                static_cast<QDialog*>(d)->reject();
            });
            w.showDialog(QStringLiteral("export"));
            check(stemsPath, "choosing stems makes the export a folder of track files");
            check(midiOff, "a MIDI export has no sound quality to choose");
        }
        dbg("chop");
        {
            int pieces = 0;
            onModal([&](QWidget* d) {
                if (auto* v = d->findChild<ChopView*>()) pieces = int(v->cuts().size());
                for (QLineEdit* e : d->findChildren<QLineEdit*>()) e->setText(QStringLiteral("guitest beat"));
                for (QPushButton* b : d->findChildren<QPushButton*>()) if (b->text() == QStringLiteral("Put on the drums")) { b->click(); break; }
            });
            w.showDialog(QStringLiteral("chop"));
            const Track* dt = nullptr; for (const Track& t : app.song.tracks) if (t.isDrums()) dt = &t;
            check(pieces >= 4 && dt && dt->kit.sample[0].startsWith(QStringLiteral("guitest beat")), "a beat chopped in the dialog lands on the drums");
            app.undo();
            for (const QString& n : samples::list()) if (n.startsWith(QStringLiteral("guitest beat"))) samples::remove(n);
        }
        dbg("settings size and midi pages");
        {
            int sizes = 0, pages = 0;
            onModal([&](QWidget* d) {
                for (QComboBox* c : d->findChildren<QComboBox*>()) if (c->itemText(0).startsWith(QStringLiteral("Automatic"))) sizes = c->count();
                if (auto* t = d->findChild<QTabWidget*>()) pages = t->count();
            });
            w.showSettings(0);
            check(sizes == 9 && pages == 4, "the settings have a size choice and a MIDI page");
        }
        dbg("mixer");
        {
            tabs->setCurrentIndex(2); settle(50);
            int pumps = 0; for (Knob* k : w.findChildren<Knob*>()) pumps += k->isVisible() && k->name() == QStringLiteral("Pump");
            int synths = 0; for (const Track& t : app.song.tracks) synths += !t.isDrums();
            if (qEnvironmentVariableIsSet("NZ_DEBUG")) std::fprintf(stderr, "pumps %d, synth tracks %d\n", pumps, synths);
            check(pumps == synths + 1, "every synth track and the input have a Pump knob");
            QPushButton* inKey = nullptr; for (QPushButton* b : w.findChildren<QPushButton*>(QStringLiteral("stripName"))) if (b->isVisible() && b->text() == QStringLiteral("Input")) inKey = b;
            check(inKey != nullptr, "the mixer has an Input strip");
            if (inKey) {
                onModal([](QWidget* d) { d->close(); });   // no input here: the warning is closed
                inKey->click(); settle(100);
                check(inKey->isChecked() == app.liveInput(), "the Input key shows whether the input is open");
                if (app.liveInput()) { inKey->click(); settle(50); }
            }
            tabs->setCurrentIndex(0);
        }

        // the sound card, if there is one: switching rate, buffer and sound system, and a moment of input
        if (app.audioRunning()) {
            const App::AudioSetup was = app.audioSetup();
            std::printf("sound: %s at %d Hz, %d samples; systems: %s; outputs: %d, inputs: %d\n", qPrintable(app.audioDriver()), app.audioRate(), app.audioBuffer(),
                        qPrintable(App::audioDrivers().join(QStringLiteral(", "))), int(app.audioDevices(false).size()), int(app.audioDevices(true).size()));
            app.seek(5 * BAR);
            App::AudioSetup s = was; s.rate = 44100; s.buffer = 512;
            check(app.openAudio(s), "the sound card opens again at 44.1 kHz with a larger buffer");
            check(std::fabs(app.pos() - 5 * BAR) < 1.0, "the playhead stays where it was when the sound card is switched");
            std::printf("  now %d Hz, %d samples\n", app.audioRate(), app.audioBuffer());
            for (const QString& d : App::audioDrivers()) {
                if (d == QLatin1String("jack")) continue;   // starting a JACK server is not something a test should do
                App::AudioSetup t = was; t.driver = d;
                const bool ok = app.openAudio(t);
                std::printf("  sound system %-10s %s%s\n", qPrintable(d), ok ? "works" : "does not open: ", ok ? "" : qPrintable(app.audioError));
            }
            check(app.openAudio(was) && app.audioRunning(), "back to the sound card it started with");
            QString err;
            if (app.startCapture(QString(), err)) {
                QElapsedTimer t; t.start();
                while (t.elapsed() < 300) QApplication::processEvents();
                int rate = 0;
                const std::vector<float> got = app.stopCapture(rate);
                std::printf("  input: %d samples at %d Hz in 0.3 s (thrown away)\n", int(got.size()), rate);
                check(!got.empty(), "the input delivers sound");
            } else std::printf("  no input: %s\n", qPrintable(err));
        } else std::printf("no sound card here: %s\n", qPrintable(app.audioError));
        std::printf(problems ? "gui test: %d problem(s)\n" : "gui test passed\n", problems);
        app.dirty = false;
        QCoreApplication::exit(problems ? 1 : 0);
    });
    if (tipCheck) { // every control a person can use should explain itself when hovered: list the ones that do not
        QTimer::singleShot(300, &w, [&] {
            QStringList missing;
            auto sweep = [&](const QString& where) {
                QApplication::processEvents();
                for (QWidget* x : w.findChildren<QWidget*>()) {
                    if (!x->isVisible()) continue;
                    if (QLatin1String(x->metaObject()->className()) == QLatin1String("QScrollBar")) continue;   // scroll bars explain themselves
                    const bool control = qobject_cast<QAbstractButton*>(x) || qobject_cast<QComboBox*>(x) || qobject_cast<QAbstractSlider*>(x)
                                         || qobject_cast<QAbstractSpinBox*>(x) || qobject_cast<Knob*>(x);
                    if (!control || qobject_cast<QTabBar*>(x->parentWidget())) continue;
                    if (!x->toolTip().isEmpty()) continue;
                    QString name = x->metaObject()->className();
                    if (auto* b = qobject_cast<QAbstractButton*>(x)) name += " \"" + b->text() + "\"";
                    if (auto* c = qobject_cast<QComboBox*>(x)) name += " [" + c->currentText() + "]";
                    QString path; for (QWidget* p = x->parentWidget(); p && p != &w; p = p->parentWidget()) if (auto* g = qobject_cast<QGroupBox*>(p)) { path = g->title(); break; }
                    missing << where + ": " + name + (path.isEmpty() ? QString() : "  in " + path);
                }
            };
            auto* tabs = w.findChild<QTabWidget*>();
            for (int tr = 0; tr < 2; tr++) {
                app.selectTrack(app.song.tracks[size_t(tr == 0 ? 1 : 0)].id);
                for (int t = 0; t < 3; t++) { tabs->setCurrentIndex(t); sweep(QStringLiteral("%1 / tab %2").arg(tr == 0 ? "synth" : "drums").arg(t)); }
            }
            if (auto* sp = w.findChild<SoundPanel*>()) {   // the simple sound pages too
                sp->setSimple(true);
                tabs->setCurrentIndex(1);
                for (int tr = 0; tr < 2; tr++) { app.selectTrack(app.song.tracks[size_t(tr == 0 ? 1 : 0)].id); sweep(QStringLiteral("simple %1").arg(tr == 0 ? "synth" : "drums")); }
                sp->setSimple(false);
            }
            app.setLiveMode(true); sweep("loops");
            missing.removeDuplicates();
            for (const QString& m : missing) std::fprintf(stderr, "%s\n", qPrintable(m));
            std::fprintf(stderr, "%d controls without a tooltip\n", int(missing.size()));
            app.dirty = false;
            QApplication::quit();
        });
    }
    if (loopsView) QTimer::singleShot(150, &w, [&] {
        app.setLiveMode(true);
        for (Track& t : app.song.tracks) t.liveCell = t.cell(0) ? 0 : -1;
        app.touch();
        if (qEnvironmentVariableIsSet("NZ_DEBUG")) std::fprintf(stderr, "after switching to loops: %dx%d\n", w.width(), w.height());
    });
    if (!shotPath.isEmpty()) {
        if (!dialogName.isEmpty()) { // picture of one of the dialogs instead
            QTimer::singleShot(1200, &w, [&] { if (QWidget* d = QApplication::activeModalWidget()) { d->grab().save(shotPath); d->close(); } app.dirty = false; QApplication::quit(); });
            QTimer::singleShot(300, &w, [&] { w.showDialog(dialogName); });
        } else if (guide) { // picture of the how-to window instead
            QTimer::singleShot(900, &w, [&] { if (QWidget* d = QApplication::activeModalWidget()) { d->grab().save(shotPath); d->close(); } app.dirty = false; QApplication::quit(); });
            QTimer::singleShot(200, &w, [&] { w.showHelp(); });
        } else QTimer::singleShot(800, &w, [&] {
            if (!geomPath.isEmpty()) {   // where each part of the window is, for cutting pictures for the manual
                QJsonObject o;
                auto put = [&](const QString& name, QWidget* x) {
                    if (!x || !x->isVisible()) return;
                    const QPoint at = x->mapTo(&w, QPoint(0, 0));
                    o[name] = QJsonArray{at.x(), at.y(), x->width(), x->height()};
                };
                put(QStringLiteral("window"), &w);
                put(QStringLiteral("topbar"), w.findChild<QWidget*>(QStringLiteral("topbar")));
                QList<QFrame*> units = w.findChildren<QFrame*>(QStringLiteral("unit"));
                std::sort(units.begin(), units.end(), [&](QFrame* a, QFrame* b) { return a->mapTo(&w, QPoint()).x() < b->mapTo(&w, QPoint()).x(); });
                const char* unitNames[] = {"transport", "song", "output"};
                for (int i = 0; i < units.size() && i < 3; i++) put(QString::fromLatin1(unitNames[i]), units[i]->parentWidget());
                put(QStringLiteral("tabs"), w.findChild<QTabWidget*>());
                put(QStringLiteral("drummer"), w.findChild<QWidget*>(QStringLiteral("drummerBox")));
                put(QStringLiteral("timeline"), w.findChild<Timeline*>());
                for (QWidget* x : w.findChildren<QWidget*>()) {
                    const QString cls = QString::fromLatin1(x->metaObject()->className());
                    if (cls == QLatin1String("KeyPads")) put(QStringLiteral("pads"), x);
                    else if (cls == QLatin1String("LoopGrid")) put(QStringLiteral("loops"), x);
                    else if (cls == QLatin1String("MixerPanel")) put(QStringLiteral("mixer"), x);
                    else if (auto* g = qobject_cast<QGroupBox*>(x)) { QString t = g->title(); if (t.startsWith(QStringLiteral("▾ ")) || t.startsWith(QStringLiteral("▸ "))) t = t.mid(2); put(QStringLiteral("group:") + t, g); }
                }
                QFile f(geomPath);
                if (f.open(QIODevice::WriteOnly)) f.write(QJsonDocument(o).toJson());
            }
            if (qEnvironmentVariableIsSet("NZ_DEBUG")) {
                std::fprintf(stderr, "window %dx%d, smallest %dx%d\n", w.width(), w.height(), w.minimumSizeHint().width(), w.minimumSizeHint().height());
                std::function<void(QWidget*, int)> dump = [&](QWidget* x, int depth) {
                    if (depth > 7 || !x->isVisible()) return;
                    const QSize m = x->minimumSizeHint().expandedTo(x->minimumSize());
                    if (m.width() > 300 || m.height() > 300 || x->objectName() == QLatin1String("unit")) std::fprintf(stderr, "%*s%s %s min %dx%d\n", depth * 2, "", x->metaObject()->className(), qPrintable(x->objectName()), m.width(), m.height());
                    for (QObject* c : x->children()) if (auto* cw = qobject_cast<QWidget*>(c)) dump(cw, depth + 1);
                };
                dump(&w, 0);
            }
            if (qEnvironmentVariableIsSet("NZ_DEBUG")) for (QFrame* f : w.findChildren<QFrame*>(QStringLiteral("unit"))) std::fprintf(stderr, "unit %d wide, min %d, hint %d\n", f->width(), f->minimumSizeHint().width(), f->sizeHint().width());
            if (qEnvironmentVariableIsSet("NZ_DEBUG")) for (QToolButton* b : w.findChildren<QToolButton*>()) std::fprintf(stderr, "%s %s %dx%d hint %dx%d\n", qPrintable(b->objectName()), qPrintable(b->text()), b->width(), b->height(), b->sizeHint().width(), b->sizeHint().height());
            w.grab().save(shotPath); app.dirty = false; QApplication::quit(); });   // not dirty: no save question on the way out
    }
    return qa.exec();
}
