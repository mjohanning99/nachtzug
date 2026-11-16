// The self-test: drives the song, the editing commands and the audio engine through every feature without a
// window, the way a session would, and reports what goes wrong. Run it under a memory checker to catch crashes;
// it also fails on silence, NaNs or a song that does not survive saving.
//
// What's here: selfTest(), called from main() for "--selftest". The sections run in order, one after the other,
// each starting from where the last left the program; a new one goes at the end of selfTest(), with its checks
// written as sentences about what should be true.
// Changes in the song file: none (it saves and opens a song in the temporary folder, to check the round trip).
#include "selftest.h"
#include <algorithm>
#include <QRegularExpression>
#include <QImage>
#include "widgets.h"
#include <QCoreApplication>
#include "app.h"
#include "mysounds.h"
#include <QFileInfo>
#include "samples.h"
#include "autosave.h"
#include "midifile.h"
#include "midi.h"
#include "chopdialog.h"
#include <QFile>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QElapsedTimer>
#include <QDir>
#include <QThread>
#include <cmath>
#include <cstdio>

namespace {

struct Test {
	App app{false};
	std::vector<float> buf = std::vector<float>(2 * 512);
	int problems = 0;
	float peak = 0.f;
	void pump(int blocks) {
		for (int i = 0; i < blocks; i++) {
			app.engine->process(buf.data(), 512);
			for (float x : buf) {
				if (!std::isfinite(x)) {
					problems++;
					std::fprintf(stderr, "non-finite sample\n");
					return;
				}
				peak = std::max(peak, std::fabs(x));
			}
		}
		QCoreApplication::processEvents();
	}
	// like pump, but at about real speed, so changes queued for the next bar line get their turn
	void realtime(int blocks) {
		for (int i = 0; i < blocks; i += 2) {
			pump(2);
			QThread::msleep(17);
		}
	}
	void check(bool ok, const char* what) {
		if (!ok) {
			problems++;
			std::fprintf(stderr, "FAILED: %s\n", what);
		}
	}

	void session();
	void swingMapsBack();
	void undoStepsHaveNames();
	void exportFiles();
	void ownSamples();
	void sampleLibrary();
	void arrangeForMe();
	void simpleSoundPage();
	void noteSettings();
	void autosaveCopy();
	void chordsMode();
	void drummer();
	void noStuckNotes();
	void stemsAndMidiFile();
	void listeningFirst();
	void choppingLoops();
	void midiLoopBack();
	void liveInput();
	void rightWrongNotes();
	void testerFindings();
	void pumpDips();
	void stylePictures();
};

// a session: tracks, ideas, sounds, playing and recording, knob lines, clips, the loop grid, the song's
// settings, removing tracks, saving and opening again
void Test::session() {
	app.play();
	pump(300);
	check(app.isPlaying(), "song plays");
	check(peak > 0.05f, "demo song makes sound");

	for (int role = 0; role < R_COUNT; role++) {
		app.addTrack(role);
		pump(40);
	}
	check(int(app.song.tracks.size()) == 10, "five tracks added");

	// every idea generator on every track, then vary
	for (size_t ti = 0; ti < app.song.tracks.size(); ti++) {
		const int id = app.song.tracks[ti].id;
		for (const GenDef& g : generatorsFor(app.song.tracks[ti].role)) {
			app.edit([&](Song& s) {
				Track* t = s.track(id);
				if (t && !t->clips.empty()) generate(s, *t, t->clips[0], g.id, 0.7f, app.rng);
			});
			pump(12);
		}
		app.edit([&](Song& s) {
			Track* t = s.track(id);
			if (t && !t->clips.empty()) varyClip(s, *t, t->clips[0], app.rng);
		});
	}
	// every preset, and rolled sounds, on a synth track while it plays
	const int synthId = app.song.tracks[1].id;
	for (const Patch& p : presets()) {
		app.edit([&](Song& s) { s.track(synthId)->patch = p; });
		pump(25);
	}
	for (int i = 0; i < 30; i++) {
		app.edit([&](Song& s) {
			Track* t = s.track(synthId);
			t->patch = rollPatch(i % 4, app.rng);
		});
		pump(10);
	}

	// play and record notes by hand, in time and with an arpeggio and chords
	app.recording = true;
	app.selectTrack(app.song.tracks[4].id);
	for (int k = 0; k < 12; k++) {
		app.keyDown(k, k % 3, k % 10);
		pump(6);
		app.keyUp(k);
		pump(3);
	}
	app.edit([&](Song& s) {
		Track* t = s.track(app.selTrack);
		t->arp = 3;
		t->chord = 4;
		t->arpOct = 2;
	});
	for (int k = 0; k < 8; k++) {
		app.keyDown(20 + k, 1, k);
		pump(8);
	}
	app.releaseAllKeys();
	pump(20);
	check(!app.song.track(app.selTrack)->clips.empty(), "recording made a clip");

	{ // your own sounds: saved, read back, renamed, moved to another group, deleted (in the test run's own folder)
		mysounds::Sound s;
		s.name = QStringLiteral("Test Bass / one");
		s.group = mysounds::Bass;
		s.words = {QStringLiteral("dark")};
		s.patch = presets()[0];
		s.patch.p[dsp::P_CUTOFF] = 0.123f;
		bool ok = mysounds::save(s) && QFileInfo::exists(s.file);
		mysounds::Sound back;
		ok = ok && mysounds::read(s.file, back) && back.name == s.name && back.words == s.words &&
		     std::abs(back.patch.p[dsp::P_CUTOFF] - 0.123f) < 1e-6f;
		bool listed = false;
		for (const mysounds::Sound& x : mysounds::all())
			listed = listed || x.file == s.file;
		const QString before = s.file;
		ok = ok && listed && mysounds::rename(s, QStringLiteral("Test Bass two")) && !QFileInfo::exists(before) &&
		     QFileInfo::exists(s.file);
		s.group = mysounds::Pad;
		ok = ok && mysounds::save(s) && s.file.contains(QStringLiteral("/pad/"));
		ok = ok && mysounds::remove(s) && !QFileInfo::exists(s.file);
		check(ok, "a sound of your own is saved, read back, renamed, moved and deleted");
	}

	// knob lines: record one, draw one that repeats, pause and resume
	app.paramPress(synthId, dsp::P_CUTOFF);
	for (int i = 0; i < 40; i++) {
		app.paramMove(synthId, dsp::P_CUTOFF, 0.2f + 0.015f * float(i));
		pump(4);
		QThread::msleep(2);
	}
	app.paramRelease();
	check(app.song.track(synthId)->hasMoves(dsp::P_CUTOFF), "knob move was recorded");
	app.edit([&](Song& s) {
		Track* t = s.track(synthId);
		AutoLane l;
		l.param = dsp::P_RES;
		l.loop = BAR;
		l.pts = {{0, 0.1f}, {96, 0.9f}, {300, 0.3f}};
		t->lanes.push_back(l);
	});
	pump(120);
	app.recording = false;
	app.paramPress(synthId, dsp::P_CUTOFF);
	app.paramMove(synthId, dsp::P_CUTOFF, 0.5f);
	app.paramRelease();
	check(app.song.track(synthId)->bypass != 0, "hand move pauses the line");
	app.resumeMoves(synthId);
	pump(20);

	// clips: transitions, cut, duplicate, copy and paste, delete, loop, undo and redo
	const int clipId = app.song.tracks[1].clips[0].id;
	app.edit([&](Song& s) {
		Clip* c = s.clip(clipId);
		c->inKind = 3;
		c->outKind = 3;
		c->outLen = 2 * BAR;
	});
	app.selectClip(clipId);
	app.splitClip(clipId, 3 * BAR + 48);
	pump(20);
	app.focus = App::FocusArrange;
	app.duplicateSelection();
	app.copy();
	app.seek(12 * BAR);
	app.paste();
	app.loopSelection();
	pump(60);
	app.selectAll();
	app.deleteSelection();
	pump(40);
	check(app.song.endTick() == 0, "delete removed every clip");
	for (int i = 0; i < 12; i++) {
		app.undo();
		pump(6);
	}
	for (int i = 0; i < 6; i++) {
		app.redo();
		pump(6);
	}
	for (int i = 0; i < 6; i++)
		app.undo();
	check(app.song.endTick() > 0, "undo brought the clips back");

	// loop grid: cells, launching on the bar, whole columns, recording launches into the song, stopping
	app.setLiveMode(true);
	check(!app.song.tracks[1].cells.empty(), "the loop grid starts with each track's first pattern");
	for (const Track& t : std::vector<Track>(app.song.tracks)) {
		app.newCell(t.id, 1);
		app.newCell(t.id, 2);
	}
	app.launchScene(0);
	realtime(220);
	check(app.song.tracks[1].liveCell == 0, "a column starts on the next bar");
	const int clipsBefore = int(app.song.tracks[1].clips.size());
	app.recording = true;
	for (int sc : {1, 2, 0, 1}) {
		app.launchScene(sc);
		realtime(260);
	}
	app.launch(app.song.tracks[1].id, -1);
	realtime(220);
	check(int(app.song.tracks[1].clips.size()) > clipsBefore, "launched cells are recorded into the song");
	app.recording = false;
	app.deleteCell(app.song.tracks[1].cells[0].id);
	app.stopAllCells();
	realtime(220);
	app.setLiveMode(false);
	pump(40);
	check(app.song.tracks[1].liveCell == -1, "leaving the loop grid stops its cells");

	// song settings and chords, all scales
	for (int sc = 0; sc < NUM_SCALES; sc++) {
		app.edit([&](Song& s) {
			s.scale = sc;
			s.root = (sc * 5) % 12;
			s.swing = 0.1f * float(sc);
			s.setChord(sc * 3, sc % 5);
		});
		pump(30);
	}

	// removing tracks while they sound must leave silence behind them
	while (app.song.tracks.size() > 1) {
		app.removeTrack(app.song.tracks.back().id);
		pump(15);
	}
	app.removeTrack(app.song.tracks[0].id);
	pump(700); // long enough for echo and reverb to die away
	peak = 0.f;
	pump(50);
	check(peak < 0.002f, "no sound is left after the last track is deleted");

	// save and open again
	for (int i = 0; i < 40 && app.canUndo(); i++)
		app.undo();
	const QString path = QDir::temp().filePath(QStringLiteral("nachtzug-selftest.nachtzug"));
	const QJsonObject before = app.song.toJson();
	check(app.save(path) && app.load(path), "save and open");
	check(app.song.toJson() == before, "the song is the same after saving and opening");
	QFile::remove(path);
	app.stop();
	pump(10);
}

// swing: the playhead and musical time map back and forth exactly
void Test::swingMapsBack() {
	double worst = 0;
	for (float sw : {0.f, 0.3f, 0.7f, 1.f})
		for (double m = 0; m < 400; m += 1.37)
			worst = std::max(worst, std::fabs(unswing(swingPos(m, sw), sw) - m));
	check(worst < 1e-9, "swing positions map back exactly");
}

// undo steps carry names, and a big change announces itself
void Test::undoStepsHaveNames() {
	app.setSong(demoSong(), QString());
	QString heard;
	auto conn = QObject::connect(&app, &App::announced, [&](const QString& w) { heard = w; });
	const int tid = app.song.tracks[1].id;
	app.removeTrack(tid);
	check(heard == QStringLiteral("Delete track") && app.undoName() == heard,
	      "deleting a track is a named step that offers Undo");
	app.undo();
	check(app.song.track(tid) && app.redoName() == QStringLiteral("Delete track"),
	      "undo brings the track back and redo knows its name");
	QObject::disconnect(conn);
}

// export: whole song, the loop four times, 24-bit, fading out
void Test::exportFiles() {
	app.setSong(demoSong(), QString());
	const QString wav = QDir::temp().filePath(QStringLiteral("nachtzug-selftest.wav"));
	auto header = [&](int& bits, int& rate, qint64& frames) {
		QFile f(wav);
		if (!f.open(QIODevice::ReadOnly)) return false;
		const QByteArray h = f.read(44);
		auto u16 = [&](int at) { return int(uchar(h[at]) | uchar(h[at + 1]) << 8); };
		auto u32 = [&](int at) {
			return qint64(uchar(h[at]) | uchar(h[at + 1]) << 8 | uchar(h[at + 2]) << 16) | qint64(uchar(h[at + 3]))
			                                                                                   << 24;
		};
		bits = u16(34);
		rate = int(u32(24));
		frames = u32(40) / (2 * bits / 8);
		return h.startsWith("RIFF") && u32(40) == f.size() - 44;
	};
	int bits = 0, rate = 0;
	qint64 frames = 0;
	RenderOptions o;
	RenderResult r = renderSong(app.song, wav, o, {});
	check(r.ok && header(bits, rate, frames) && bits == 16 && rate == 48000, "export the whole song as 16-bit WAV");
	const double songSecs = renderLength(app.song, o);
	check(frames / 48000.0 >= songSecs && frames / 48000.0 < songSecs + o.maxTail + 0.1,
	      "the export ends once the echoes have died away");
	o.loopOnly = true;
	o.repeats = 4;
	o.bits = 24;
	o.sampleRate = 44100.f;
	o.fadeOut = true;
	r = renderSong(app.song, wav, o, {});
	const double loopSecs = (app.song.loopEnd - app.song.loopStart) * 4 * 60.0 / (app.song.bpm * PPQ);
	check(r.ok && header(bits, rate, frames) && bits == 24 && rate == 44100 &&
	          std::fabs(frames / 44100.0 - loopSecs) < 0.05,
	      "export the loop four times as 24-bit, faded out");
	{ // the fade really ends in silence
		QFile f(wav);
		if (!f.open(QIODevice::ReadOnly)) problems++;
		f.seek(f.size() - 3 * 2 * 200);
		const QByteArray endBytes = f.readAll();
		int loudest = 0;
		for (int i = 0; i + 2 < endBytes.size(); i += 3)
			loudest = std::max(loudest, std::abs(int(qint32(uchar(endBytes[i]) << 8 | uchar(endBytes[i + 1]) << 16 |
			                                                uchar(endBytes[i + 2]) << 24) >>
			                                         8)));
		check(loudest < 8388607 / 200, "a faded export ends quietly");
	}
	int cancelAt = 0;
	r = renderSong(app.song, wav, RenderOptions{}, [&](float) { return ++cancelAt < 3; });
	check(!r.ok && !QFile::exists(wav), "cancelling an export leaves no file behind");
	QFile::remove(wav);
}

// your own samples: write one, import it, play it on the kick
void Test::ownSamples() {
	std::vector<float> click(4800);
	for (size_t i = 0; i < click.size(); i++)
		click[i] = std::sin(float(i) * 0.3f) * std::exp(-float(i) / 900.f) * 0.9f;
	QString err;
	const QString name = samples::saveRecording(QStringLiteral("selftest blip"), click, 48000, err);
	check(!name.isEmpty() && samples::list().contains(name), "a recording is saved in the sample library");
	const QString copy = samples::import(QDir(samples::folder()).filePath(name), err);
	check(!copy.isEmpty() && copy != name, "importing a file with a name already in the library makes a new name");
	const auto data = samples::get(name);
	check(data && data->frames() == click.size() && std::fabs(data->l[100] - click[100]) < 1e-5f,
	      "the sample reads back as it was written");
	dsp::SampleData junk;
	check(!samples::readFile(QDir::temp().filePath(QStringLiteral("does-not-exist.wav")), junk, err),
	      "a missing file is refused with a reason");
	// the kick plays the sample: the output follows it
	app.setSong(demoSong(), QString());
	Track* drums = nullptr;
	for (Track& t : app.song.tracks)
		if (t.isDrums()) drums = &t;
	check(drums != nullptr, "the demo has drums");
	if (drums) {
		const int did = drums->id;
		app.edit([&](Song& sg) {
			Track* t = sg.track(did);
			t->kit.sample[dsp::D_KICK] = name;
			t->kit.p[dsp::D_KICK * 3 + 2] = 1.f;
		});
		check(app.song.track(did)->kit.data[dsp::D_KICK] != nullptr, "the drum finds its sample");
		for (Track& t : app.song.tracks)
			if (!t.isDrums()) app.song.track(t.id)->mute = true;
		app.touch();
		app.audition(did, dsp::D_KICK, true);
		peak = 0.f;
		pump(4);
		check(peak > 0.05f, "a drum with a sample makes sound");
		// the song keeps the name; opening it again finds the sample
		const QString sp = QDir::temp().filePath(QStringLiteral("nachtzug-selftest-sample.nachtzug"));
		app.save(sp);
		app.load(sp);
		Track* again = nullptr;
		for (Track& t : app.song.tracks)
			if (t.isDrums()) again = &t;
		check(again && again->kit.sample[dsp::D_KICK] == name && again->kit.data[dsp::D_KICK],
		      "a song remembers its samples");
		// a sample that disappears falls back to the built-in drum
		samples::remove(name);
		app.load(sp);
		again = nullptr;
		for (Track& t : app.song.tracks)
			if (t.isDrums()) again = &t;
		check(again && !again->kit.data[dsp::D_KICK], "a missing sample falls back to the built-in drum");
		app.audition(again->id, dsp::D_KICK, true);
		pump(20);
		QFile::remove(sp);
	}
	samples::remove(copy);
	samples::remove(name);
	app.previewSample(QString());
}

// the Nachtzug library: every sound is there and reads, and the sampled kits play
void Test::sampleLibrary() {
	const auto& lib = samples::library();
	check(lib.size() == 65, "the library has its 65 sounds");
	int bad = 0;
	for (const samples::LibrarySound& ls : lib) {
		const auto d = samples::get(ls.id);
		if (!d || d->frames() < 100 || d->rate < 8000 || ls.name.isEmpty() || ls.about.isEmpty()) {
			bad++;
			std::fprintf(stderr, "library sound %s does not read\n", qPrintable(ls.id));
			continue;
		}
		float pk = 0.f;
		for (size_t i = 0; i < d->frames(); i++)
			pk = std::max(pk, std::fabs(d->l[i]));
		if (pk < 0.25f || pk > 1.f) {
			bad++;
			std::fprintf(stderr, "library sound %s has a peak of %.2f\n", qPrintable(ls.id), double(pk));
		}
	}
	check(bad == 0, "every library sound reads and is at a sensible level");
	check(QDir(QStringLiteral(":/samples")).entryList(QDir::Dirs | QDir::NoDotAndDotDot).size() ==
	          samples::libraryCategories().size(),
	      "every folder of the library has a category");
	check(samples::isLibrary(QStringLiteral("nz:Kicks/Punch")) &&
	          samples::displayName(QStringLiteral("nz:Kicks/Punch")) == QStringLiteral("Punch") &&
	          samples::displayName(QStringLiteral("loop.wav")) == QStringLiteral("loop"),
	      "library and own samples are named right");
	app.setSong(demoSong(), QString());
	int did = -1;
	for (const Track& t : app.song.tracks)
		if (t.isDrums()) did = t.id;
	int sampledKits = 0;
	for (const DrumKit& k : drumKits()) {
		if (k.sample[0].isEmpty()) continue;
		sampledKits++;
		app.edit([&](Song& sg) { sg.track(did)->kit = chooseKit(sg.track(did)->kit, k); });
		bool all = true;
		for (int d = 0; d < dsp::D_COUNT; d++)
			all &= app.song.track(did)->kit.data[d] != nullptr;
		check(all, "a sampled kit finds all its sounds");
		for (int d = 0; d < dsp::D_COUNT; d++) {
			app.audition(did, d, true);
			peak = 0.f;
			pump(3);
			if (peak < 0.02f) {
				problems++;
				std::fprintf(stderr, "FAILED: %s, drum %d is silent\n", qPrintable(k.name), d);
			}
			pump(20);
		}
	}
	check(sampledKits == 4, "there are four sampled kits");
	// choosing a kit: your own sample stays, the library sounds of the old kit go
	QString err;
	const QString mine =
	    samples::saveRecording(QStringLiteral("selftest kit"), std::vector<float>(2000, 0.5f), 48000, err);
	app.edit([&](Song& sg) { sg.track(did)->kit.sample[dsp::D_TOM] = mine; });
	app.edit([&](Song& sg) { sg.track(did)->kit = chooseKit(sg.track(did)->kit, drumKits()[0]); });
	const DrumKit& now = app.song.track(did)->kit;
	check(now.sample[dsp::D_TOM] == mine && now.sample[dsp::D_KICK].isEmpty(),
	      "a new kit keeps your own sample and drops the old kit's");
	const QString sp = QDir::temp().filePath(QStringLiteral("nachtzug-selftest-library.nachtzug"));
	app.edit([&](Song& sg) { sg.track(did)->kit.sample[dsp::D_CYM] = QStringLiteral("nz:Train/Door chime"); });
	app.save(sp);
	app.load(sp);
	int did2 = -1;
	for (const Track& t : app.song.tracks)
		if (t.isDrums()) did2 = t.id;
	check(app.song.track(did2)->kit.sample[dsp::D_CYM] == QStringLiteral("nz:Train/Door chime") &&
	          app.song.track(did2)->kit.data[dsp::D_CYM],
	      "a song remembers a library sound");
	QFile::remove(sp);
	samples::remove(mine);
}

// arrange for me: every length, all parts there, plays cleanly
void Test::arrangeForMe() {
	for (int len = 0; len < 3; len++) {
		app.setSong(demoSong(), QString());
		app.edit([&](Song& sg) { arrangeSong(sg, len, true, app.rng); }, QStringLiteral("Arrange for me"), true);
		int bars = 0;
		for (const ArrangePart& pt : arrangePlan(len))
			bars += pt.bars;
		check(app.song.endTick() == bars * BAR, "the arranged song is as long as its plan");
		bool everyTrack = true;
		for (const Track& t : app.song.tracks)
			everyTrack &= !t.clips.empty();
		check(everyTrack, "every track plays somewhere in the arrangement (the empty lead gets a melody)");
		bool leadOnlyLate = true;
		for (const Track& t : app.song.tracks)
			if (t.role == R_LEAD)
				for (const Clip& c : t.clips)
					leadOnlyLate &= c.start >= 32 * BAR;
		check(leadOnlyLate, "the lead waits for the break and the second main part");
		app.refreshSelection();
		check(app.curClip() != nullptr, "after arranging the editor shows a clip");
		app.seek(0);
		app.play();
		peak = 0.f;
		pump(400);
		check(peak > 0.05f, "the arranged song plays");
		app.stop();
		pump(10);
		app.undo();
		check(app.song.endTick() == 8 * BAR, "undo brings back the song from before arranging");
	}
}

// the simple sound page writes the same patch values as the full one
void Test::simpleSoundPage() {
	app.setSong(demoSong(), QString());
	check(!app.song.tracks.empty(), "demo for the sound pages");
}

// notes that play only sometimes, quick repeats, kept notes
void Test::noteSettings() {
	// the notes a song plays in its first `bars` bars, worked out without sound
	auto notesOf = [](const Song& sg, int bars) {
		std::vector<NoteEvent> log;
		Engine e(48000.f);
		e.eventsOnly = true;
		e.log = &log;
		e.setSong(std::make_shared<const Song>(sg));
		EngineEvent play;
		play.type = EngineEvent::Play;
		e.push(play);
		std::vector<float> b(2 * 480);
		const long frames = long(bars * 4 * 60.0 / sg.bpm * 48000.0);
		for (long f = 0; f < frames; f += 480)
			e.process(b.data(), 480);
		std::erase_if(log, [&](const NoteEvent& ev) {
			return ev.frame >= double(frames) - dsp::CHUNK;
		}); // notes start at the beginning of their 16-sample chunk
		return log;
	};
	Song sg;
	sg.bpm = 120.f;
	sg.loopStart = 0;
	sg.loopEnd = BAR;
	sg.loopOn = true;
	Track dr = makeTrack(sg, R_DRUMS);
	Clip c;
	c.id = sg.newId();
	c.start = 0;
	c.len = BAR;
	c.loopLen = BAR;
	for (int i = 0; i < 16; i++) {
		Note n;
		n.start = i * 24;
		n.len = 12;
		n.deg = dsp::D_CHAT;
		n.chance = 50;
		c.notes.push_back(n);
	}
	Note kick;
	kick.start = 0;
	kick.len = 12;
	kick.deg = dsp::D_KICK;
	c.notes.push_back(kick);
	Note roll;
	roll.start = 192;
	roll.len = 48;
	roll.deg = dsp::D_SNARE;
	roll.rep = 4;
	roll.chance = 50;
	c.notes.push_back(roll);
	Note clap;
	clap.start = 288;
	clap.len = 24;
	clap.deg = dsp::D_CLAP;
	clap.rep = 3;
	c.notes.push_back(clap);
	dr.clips.push_back(c);
	sg.tracks.push_back(dr);
	const int bars = 64;
	const auto log = notesOf(sg, bars);
	int hats = 0, kicks = 0, claps = 0;
	std::vector<int> snaresPerBar(bars + 1, 0);
	const double framesPerBar = 4 * 60.0 / sg.bpm * 48000.0;
	for (const NoteEvent& ev : log) {
		if (!ev.on) continue;
		if (ev.note == dsp::D_CHAT)
			hats++;
		else if (ev.note == dsp::D_KICK)
			kicks++;
		else if (ev.note == dsp::D_CLAP)
			claps++;
		else if (ev.note == dsp::D_SNARE)
			snaresPerBar[size_t(std::min(double(bars), ev.frame / framesPerBar))]++;
	}
	if (qEnvironmentVariableIsSet("NZ_DEBUG")) std::fprintf(stderr, "kicks %d claps %d hats %d\n", kicks, claps, hats);
	check(kicks == bars && claps == 3 * bars, "plain notes and repeated notes play every time");
	check(hats > 16 * bars * 0.4 && hats < 16 * bars * 0.6, "a note with a 50% chance plays about half the time");
	bool together = true;
	int rolls = 0;
	for (int b = 0; b < bars; b++) {
		together &= snaresPerBar[size_t(b)] == 0 || snaresPerBar[size_t(b)] == 4;
		rolls += snaresPerBar[size_t(b)] == 4;
	}
	check(together && rolls > 10 && rolls < bars - 10,
	      "the repeats of a note with a chance play all together or not at all");
	const auto again = notesOf(sg, bars);
	check(again.size() == log.size(), "the same song plays the same chances when written to a file twice");
	// repeats of a synth note are evenly spaced and end before the next one starts
	Song s2;
	s2.bpm = 120.f;
	s2.loopEnd = BAR;
	Track bass = makeTrack(s2, R_BASS);
	Clip bc;
	bc.id = s2.newId();
	bc.loopLen = BAR;
	bc.len = BAR;
	Note bn;
	bn.start = 0;
	bn.len = 96;
	bn.deg = 0;
	bn.rep = 4;
	bc.notes.push_back(bn);
	bass.clips.push_back(bc);
	s2.tracks.push_back(bass);
	std::vector<double> ons, offs;
	for (const NoteEvent& ev : notesOf(s2, 1))
		(ev.on ? ons : offs).push_back(ev.frame);
	const double step = 24 * 60.0 / (s2.bpm * PPQ) * 48000.0;
	if (qEnvironmentVariableIsSet("NZ_DEBUG")) {
		std::fprintf(stderr, "ons %d offs %d:", int(ons.size()), int(offs.size()));
		for (double o : ons)
			std::fprintf(stderr, " %.0f", o);
		std::fprintf(stderr, " / step %.0f, off0 %.0f\n", step, offs.empty() ? -1.0 : offs[0]);
	}
	check(ons.size() == 4 && offs.size() >= 4 && std::fabs(ons[3] - ons[0] - 3 * step) < 40 && offs[0] < ons[1],
	      "a note played four times: four even, separate hits");
	// kept notes
	Clip kc = bc;
	kc.notes.clear();
	for (int i = 0; i < 8; i++) {
		Note n;
		n.start = i * 48;
		n.len = 24;
		n.deg = i;
		n.lock = i % 2 == 0;
		kc.notes.push_back(n);
	}
	const std::vector<Note> was = kc.notes;
	for (int i = 0; i < 300; i++)
		varyClip(s2, s2.tracks[0], kc, app.rng);
	bool keptSame = true;
	for (const Note& w : was)
		if (w.lock)
			keptSame &= std::any_of(kc.notes.begin(), kc.notes.end(), [&](const Note& n) {
				return n.lock && n.start == w.start && n.deg == w.deg && n.len == w.len && n.acc == w.acc &&
				       n.slide == w.slide;
			});
	check(keptSame, "Vary leaves kept notes alone");
	for (const GenDef& g : generatorsFor(R_BASS)) {
		Clip gc = kc;
		generate(s2, s2.tracks[0], gc, g.id, 0.8f, app.rng);
		int found = 0;
		bool overlap = false;
		for (const Note& n : gc.notes) {
			if (n.lock) {
				found++;
				continue;
			}
			for (const Note& k : gc.notes)
				if (k.lock && n.start < k.start + k.len && k.start < n.start + n.len) overlap = true;
		}
		check(found == 4 && !overlap, "a new idea keeps the kept notes and writes around them");
	}
	// the song file keeps all three
	Song s3 = s2;
	s3.tracks[0].clips[0].notes = {bn};
	s3.tracks[0].clips[0].notes[0].chance = 25;
	s3.tracks[0].clips[0].notes[0].lock = true;
	Song back;
	Song::fromJson(s3.toJson(), back);
	const Note& rn = back.tracks[0].clips[0].notes[0];
	check(rn.chance == 25 && rn.rep == 4 && rn.lock, "chance, repeats and keep are saved with the song");
}

// autosave: a copy of unsaved work, found again after a crash, gone after a normal end
void Test::autosaveCopy() {
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
		QFile::copy(QString(as.copyPath()).replace(QStringLiteral(".nachtzug"), QStringLiteral(".json")),
		            QDir(dir).filePath(QStringLiteral("session-crashed.json")));
		as.finish();
		check(!QFile::exists(as.copyPath()), "a normal end deletes the copy");
	}
	const auto left = Autosave::leftovers(dir);
	check(left.size() == 1 && left[0].original == QStringLiteral("/somewhere/My song.nachtzug"),
	      "a copy left by a crash is found, with where the song came from");
	app.setSong(demoSong(), QString());
	if (!left.isEmpty()) {
		Autosave as2(&app, dir, 1000000);
		check(as2.restore(left[0]) && std::fabs(app.song.bpm - 133.f) < 0.01f && app.dirty &&
		          app.filePath == QStringLiteral("/somewhere/My song.nachtzug"),
		      "bringing it back restores the song as unsaved changes");
		check(Autosave::leftovers(dir).isEmpty(), "a restored copy is not offered again");
		as2.finish();
	}
	app.dirty = false;
	QDir(dir).removeRecursively();
}

// Chords mode: the keys change the chord of the tracks that follow it, and Record writes it down
void Test::chordsMode() {
	app.setSong(demoSong(), QString());
	int bassId = -1;
	for (const Track& t : app.song.tracks)
		if (t.role == R_BASS) bassId = t.id;
	app.selectTrack(bassId);
	app.setChordKeys(true);
	app.keyDown(1 * 16 + 3, 1, 3); // the fourth key of the middle row: the chord on the fourth step
	check(app.liveChord() == 3 && app.engine->liveChord.load() == 3, "a key in Chords mode sets the chord");
	app.keyUp(1 * 16 + 3);
	check(app.liveChord() == 3, "the chord stays when the key is let go");
	// what the bass plays moves with it
	Song sg = app.song;
	for (Track& t : sg.tracks)
		if (t.id != bassId) t.mute = true;
	std::vector<NoteEvent> plain, moved;
	for (int pass = 0; pass < 2; pass++) {
		Engine e(48000.f);
		e.eventsOnly = true;
		e.log = pass ? &moved : &plain;
		if (pass) e.liveChord.store(3);
		e.setSong(std::make_shared<const Song>(sg));
		EngineEvent pl;
		pl.type = EngineEvent::Play;
		e.push(pl);
		std::vector<float> b(2 * 480);
		for (int i = 0; i < 100; i++)
			e.process(b.data(), 480);
	}
	int firstPlain = -1, firstMoved = -1;
	for (const NoteEvent& ev : plain)
		if (ev.on) {
			firstPlain = ev.note;
			break;
		}
	for (const NoteEvent& ev : moved)
		if (ev.on) {
			firstMoved = ev.note;
			break;
		}
	check(firstPlain > 0 && firstMoved - firstPlain == sg.degToSemis(3) - sg.degToSemis(sg.chordAt(0)),
	      "tracks that follow the chords play the chord from the keys");
	// recording: the chord goes into the bars the playhead passes
	app.recording = true;
	app.seek(0);
	app.play();
	realtime(30);
	app.keyDown(1 * 16 + 5, 1, 5);
	app.keyUp(1 * 16 + 5);
	realtime(400);
	const int bar = int(app.musicalPos()) / BAR;
	check(app.song.chordAt(std::max(0, bar - 1)) == 5 && app.undoName() == QStringLiteral("Record chords"),
	      "Record writes the chord into the chord row");
	app.stop();
	app.recording = false;
	pump(4);
	app.releaseChord();
	check(app.engine->liveChord.load() == Engine::NO_CHORD, "Esc goes back to the song's chords");
	app.setChordKeys(false);
	app.keyDown(1 * 16 + 2, 1, 2);
	app.keyUp(1 * 16 + 2);
	check(app.liveChord() == Engine::NO_CHORD, "with Chords mode off the keys play notes again");
	app.dirty = false;
}

// the drummer: every step of Energy adds to the beat (with or without hats), and Fills come as often as chosen
void Test::drummer() {
	bool rises = true;
	for (int style = 0; style < 7; style++)
		for (int hats : {0, 4}) {
			size_t last = 0;
			for (int e = 0; e <= 10; e += 2) {
				Clip c;
				c.len = 8 * BAR;
				c.ad.on = true;
				c.ad.style = style;
				c.ad.hats = hats;
				c.ad.intensity = e / 10.f;
				c.ad.complexity = 0.4f;
				c.ad.fillEvery = 4;
				autoDrumFill(c);
				if (e > 0 && c.notes.size() <= last) {
					rises = false;
					std::fprintf(stderr, "style %d hats %d: energy %d%% adds nothing\n", style, hats, e * 10);
				}
				last = c.notes.size();
			}
		}
	check(rises, "each step of the drummer's Energy adds to the beat");
	auto fillBars = [](int every) {
		Clip c;
		c.len = 8 * BAR;
		c.ad.on = true;
		c.ad.style = 1;
		c.ad.intensity = 0.6f;
		c.ad.complexity = 0.4f;
		c.ad.fillEvery = every;
		autoDrumFill(c);
		int bars = 0;
		for (int b = 0; b < 8; b++) {
			int sn = 0;
			for (const Note& n : c.notes)
				if (n.start / BAR == b && n.start % BAR >= 9 * 24 && (n.deg == dsp::D_SNARE || n.deg == dsp::D_TOM))
					sn++;
			bars += sn >= 4;
		}
		return bars;
	};
	check(fillBars(2) == 4 && fillBars(4) == 2 && fillBars(8) == 1 && fillBars(0) == 0,
	      "fills come every 2, 4 or 8 bars, as chosen");
}

// no note gets stuck: sounds, filter, mono/poly, arpeggio and chords changed while notes sound, then Stop
void Test::noStuckNotes() {
	app.setSong(demoSong(), QString());
	QRandomGenerator rnd(1234);
	int stuck = 0;
	for (int round = 0; round < 40 && !stuck; round++) {
		app.seek(0);
		app.play();
		for (int step = 0; step < 30; step++) {
			const int ti = 1 + int(rnd.bounded(int(app.song.tracks.size()) - 1));
			const int what = int(rnd.bounded(9));
			app.edit([&](Song& s) {
				Track& t = s.tracks[size_t(ti)];
				float* p = t.patch.p;
				switch (what) {
				case 0:
					p[dsp::P_MONO] = p[dsp::P_MONO] > 0.5f ? 0.f : 1.f;
					break;
				case 1:
					t.arp = t.arp ? 0 : 1 + int(rnd.bounded(3));
					break;
				case 2:
					t.chord = int(rnd.bounded(4));
					break;
				case 3:
					p[dsp::P_SLOPE] = float(rnd.bounded(4));
					break;
				case 4:
					p[dsp::P_CUTOFF] = float(rnd.generateDouble());
					p[dsp::P_RES] = float(rnd.generateDouble());
					break;
				case 5:
					p[dsp::P_GLIDE] = float(rnd.generateDouble() * 0.6);
					break;
				case 6:
					t.patch = presets()[size_t(rnd.bounded(int(presets().size())))];
					break;
				default:
					p[dsp::P_ENVAMT] = float(rnd.generateDouble());
					p[dsp::P_ACCENT] = float(rnd.generateDouble());
					break;
				}
				p[dsp::P_AR] = std::min(p[dsp::P_AR], 0.55f); // short releases, so the check below need not wait long
			});
			app.selectTrack(app.song.tracks[size_t(ti)].id);
			const int code = int(rnd.bounded(10));
			if (rnd.bounded(2)) app.keyDown(code, 1, code);
			pump(1 + int(rnd.bounded(12)));
			if (rnd.bounded(3)) app.releaseAllKeys();
		}
		app.releaseAllKeys();
		app.stop();
		pump(int(4.0 * 48000 / 512)); // four seconds: every release is over
		if (!app.engine->synthsSilent()) {
			stuck++;
			std::fprintf(stderr, "stuck note after round %d\n", round);
		}
	}
	check(stuck == 0, "no note keeps sounding after Stop, whatever was changed while it played");
	app.dirty = false;
}

// stems add up to the song, and the MIDI file holds the notes
void Test::stemsAndMidiFile() {
	app.setSong(demoSong(), QString());
	app.song.tracks[2].mute = true; // a muted track is left out of both
	const QString dir = QDir::temp().filePath(QStringLiteral("nachtzug-selftest-stems"));
	QDir(dir).removeRecursively();
	RenderOptions o;
	o.bits = 24;
	o.loopOnly = true;
	o.repeats = 1;
	o.fadeOut = true;
	QStringList files;
	const RenderResult r = renderStems(app.song, dir, o, {}, &files);
	int withMusic = 0;
	for (const Track& t : app.song.tracks)
		withMusic += !t.mute && !t.clips.empty();
	check(r.ok && files.size() == withMusic && withMusic == int(audibleTracks(app.song).size()),
	      "one stem per heard track, none for an empty one");
	const QString mixFile = QDir(dir).filePath(QStringLiteral("mix.wav"));
	renderSong(app.song, mixFile, o, {});
	auto read24 = [](const QString& f) {
		std::vector<float> out;
		QFile in(f);
		if (!in.open(QIODevice::ReadOnly)) return out;
		const QByteArray d = in.readAll().mid(44);
		for (int i = 0; i + 2 < d.size(); i += 3)
			out.push_back(float(qint32(uchar(d[i]) << 8 | uchar(d[i + 1]) << 16 | uchar(d[i + 2]) << 24) >> 8) /
			              8388607.f);
		return out;
	};
	const std::vector<float> mix = read24(mixFile);
	std::vector<float> sum(mix.size(), 0.f);
	for (const QString& f : files) {
		const auto st = read24(f);
		for (size_t i = 0; i < st.size() && i < sum.size(); i++)
			sum[i] += st[i];
	}
	double diff = 0, level = 0;
	for (size_t i = 0; i < mix.size(); i++) {
		diff += double(sum[i] - mix[i]) * (sum[i] - mix[i]);
		level += double(mix[i]) * mix[i];
	}
	if (qEnvironmentVariableIsSet("NZ_DEBUG")) {
		float pk = 0;
		for (float x : mix)
			pk = std::max(pk, std::fabs(x));
		std::fprintf(stderr, "stems: diff %.4f, mix peak %.3f, %d vs %d samples\n",
		             std::sqrt(diff / std::max(1e-12, level)), double(pk), int(mix.size()), int(sum.size()));
	}
	check(!mix.empty() && level > 0 && std::sqrt(diff / level) < 0.02, "the stems add up to the song");
	check(files.size() > 1 && QFileInfo(files[0]).fileName().startsWith(QStringLiteral("01 ")),
	      "stems are numbered in track order");
	int cancelAt = 0;
	const QString dir2 = dir + QStringLiteral("-cancel");
	check(!renderStems(app.song, dir2, o, [&](float) { return ++cancelAt < 6; }).ok &&
	          QDir(dir2).entryList(QDir::Files).isEmpty(),
	      "cancelling stems leaves no files behind");
	QDir(dir).removeRecursively();
	QDir(dir2).removeRecursively();

	// MIDI: one track for the tempo and one per heard track; the drums on channel 10 with General MIDI notes
	const QString mid = QDir::temp().filePath(QStringLiteral("nachtzug-selftest.mid"));
	RenderOptions mo;
	mo.loopOnly = true;
	mo.repeats = 2;
	const double secs = writeMidiFile(app.song, mid, mo);
	QFile mf(mid);
	const QByteArray m = mf.open(QIODevice::ReadOnly) ? mf.readAll() : QByteArray();
	mf.close();
	auto be = [&](int at, int n) {
		quint32 v = 0;
		for (int i = 0; i < n; i++)
			v = (v << 8) | uchar(m[at + i]);
		return v;
	};
	check(secs > 0 && m.startsWith("MThd") && be(8, 2) == 1 &&
	          int(be(10, 2)) == 1 + int(audibleTracks(app.song).size()) && be(12, 2) == 960,
	      "the MIDI file has a tempo track and one track per heard track");
	// walk the tracks and count the notes
	int at = 14, ons = 0, drumOns = 0, offDrumNotes = 0, unbalanced = 0, overlaps = 0;
	while (at + 8 <= m.size() && m.mid(at, 4) == "MTrk") {
		const int len = int(be(at + 4, 4));
		int p = at + 8;
		const int end = p + len;
		int status = 0;
		QHash<int, int> open;
		while (p < end) {
			while (uchar(m[p]) & 0x80)
				p++;
			p++; // the time
			int b = uchar(m[p]);
			if (b == 0xff) {
				const int type = uchar(m[p + 1]);
				p += 2;
				int l = 0;
				while (uchar(m[p]) & 0x80)
					l = (l << 7) | (uchar(m[p++]) & 0x7f);
				l = (l << 7) | uchar(m[p++]);
				p += l;
				if (type == 0x2f) break;
				continue;
			}
			if (b & 0x80) {
				status = b;
				p++;
			}
			const int kind = status & 0xf0, ch = status & 0x0f;
			if (kind == 0xc0) {
				p += 1;
				continue;
			}
			const int note = uchar(m[p]), vel = uchar(m[p + 1]);
			p += 2;
			if (kind == 0x90 && vel > 0) {
				ons++;
				if (++open[note] > 1) overlaps++;
				if (ch == 9) {
					drumOns++;
					if (note != 36 && note != 38 && note != 39 && note != 42 && note != 46 && note != 45 &&
					    note != 37 && note != 49)
						offDrumNotes++;
				}
			} else if (kind == 0x80 || (kind == 0x90 && vel == 0))
				open[note]--;
		}
		for (int v : open)
			unbalanced += v != 0;
		at = end;
	}
	check(ons > 50 && drumOns > 10 && offDrumNotes == 0,
	      "the MIDI file holds the notes, the drums on General MIDI drum notes");
	check(unbalanced == 0, "every note in the MIDI file ends");
	check(overlaps == 0, "no note in the MIDI file starts again while it still sounds");
	QFile::remove(mid);
	app.dirty = false;
}

// listening to a preset or kit before choosing it
void Test::listeningFirst() {
	app.setSong(demoSong(), QString());
	for (Track& t : app.song.tracks)
		t.mute = true;
	app.touch();
	pump(200);
	int silentOnes = 0;
	for (const Track& t : std::vector<Track>(app.song.tracks)) {
		app.selectTrack(t.id);
		if (t.isDrums())
			for (const DrumKit& k : drumKits()) {
				app.previewSound(nullptr, &k);
				peak = 0.f;
				pump(40);
				if (peak < 0.05f) {
					silentOnes++;
					std::fprintf(stderr, "kit %s is silent when previewed\n", qPrintable(k.name));
				}
			}
		else
			for (size_t i = 0; i < presets().size(); i += 3) {
				app.previewSound(&presets()[i], nullptr);
				peak = 0.f;
				pump(60);
				if (peak < 0.05f) {
					silentOnes++;
					std::fprintf(stderr, "preset %s is silent when previewed\n", qPrintable(presets()[i].name));
				}
			}
	}
	check(silentOnes == 0, "every preset and kit can be heard before choosing it");
	app.previewSound(nullptr, nullptr);
	pump(500);
	peak = 0.f;
	pump(40);
	check(peak < 0.01f, "closing the list stops the preview");
	app.dirty = false;
}

// chopping a loop: the hits are found where they are, and the pieces play from the drums
void Test::choppingLoops() {
	app.setSong(demoSong(), QString());
	// a bar at 120 BPM made of library hits at known places, quieter ones among them
	const char* hitsIn[8] = {"nz:Kicks/Punch", "nz:Hats/Closed",      "nz:Snares/Tight", "nz:Hats/Closed",
	                         "nz:Kicks/Punch", "nz:Percussion/Clave", "nz:Snares/Tight", "nz:Hats/Open"};
	const float gains[8] = {1.f, 0.4f, 0.9f, 0.35f, 1.f, 0.5f, 0.9f, 0.45f};
	dsp::SampleData loop;
	loop.rate = 48000.f;
	loop.l.assign(96000, 0.f);
	loop.r.assign(96000, 0.f);
	std::vector<size_t> truth;
	for (int i = 0; i < 8; i++) {
		const auto d = samples::get(QString::fromLatin1(hitsIn[i]));
		const size_t at = size_t(i) * 12000 + (i % 2 ? 900 : 0); // a little swing on the off-beats
		truth.push_back(at);
		for (size_t k = 0; d && k < d->frames() && at + k < loop.frames(); k++) {
			const double pos = double(k) * d->rate / 48000.0;
			const size_t j = size_t(pos);
			if (j >= d->frames()) break;
			loop.l[at + k] += d->l[j] * gains[i];
			loop.r[at + k] += d->r[j] * gains[i];
		}
	}
	const auto cuts = samples::findHits(loop, 0.55f, 8);
	int found = 0;
	for (size_t t : truth)
		for (size_t c : cuts)
			if (std::abs(double(c) - double(t)) < 48000 * 0.012) {
				found++;
				break;
			}
	if (qEnvironmentVariableIsSet("NZ_DEBUG")) {
		std::fprintf(stderr, "chop: %d cuts, %d of 8 hits found:", int(cuts.size()), found);
		for (size_t c : cuts)
			std::fprintf(stderr, " %d", int(c));
		std::fprintf(stderr, "\n");
	}
	check(found == 8 && cuts.size() == 8, "chopping finds each hit of a loop, the quiet ones too");
	check(samples::equalCuts(96000, 4) == std::vector<size_t>({0, 24000, 48000, 72000}), "equal pieces are equal");
	check(samples::guessBars(2.0, 120) == 1 && samples::guessBars(8.1, 120) == 4 && samples::guessBars(3.9, 122) == 2,
	      "the length of a loop in bars is guessed from the tempo");
	QString err;
	const int tid = app.chopOntoDrums(loop, cuts, QStringLiteral("selftest beat"), 1, true, err);
	const Track* dt = app.song.track(tid);
	bool onPads = dt != nullptr;
	for (int d = 0; dt && d < 8; d++)
		onPads &= dt->kit.data[d] != nullptr && dt->kit.sample[d].startsWith(QStringLiteral("selftest beat"));
	check(onPads, "the pieces are on the drum pads");
	const Clip* pc = dt ? app.curClip() : nullptr;
	bool inOrder = pc && pc->notes.size() == 8 && pc->loopLen == BAR;
	for (int i = 0; inOrder && i < 8; i++)
		inOrder &=
		    pc->notes[size_t(i)].deg == i &&
		    std::abs(pc->notes[size_t(i)].start - int(std::lround(truth[size_t(i)] / 96000.0 * BAR / 24.0)) * 24) <= 24;
	check(inOrder, "the pattern plays the pieces where they were in the loop");
	app.seek(0);
	app.play();
	peak = 0.f;
	pump(200);
	check(peak > 0.05f, "the chopped loop plays");
	app.stop();
	pump(4);
	app.undo();
	check(app.song.track(tid) == nullptr || app.song.track(tid)->kit.sample[0] != QStringLiteral("selftest beat 1.wav"),
	      "chopping is one step to undo");
	for (const QString& n : samples::list())
		if (n.startsWith(QStringLiteral("selftest beat"))) samples::remove(n);
	app.dirty = false;
}

// MIDI, round the system's loop-back port: notes, a learned knob, and the clock both ways
void Test::midiLoopBack() {
	app.setSong(demoSong(), QString());
	Midi m(&app, false);
	app.midi = &m;
	// messages go in the way the device thread hands them over; what Nachtzug sends comes back the same way
	m.setInput(QString());
	m.sendHook = [&m](const uint8_t* b, int n) { m.incoming(b, n); };
	auto wait = [&](int ms) {
		QElapsedTimer t;
		t.start();
		while (t.elapsed() < ms) {
			QCoreApplication::processEvents();
			QThread::msleep(2);
		}
	};
	auto send = [&](std::initializer_list<int> bytes) {
		uint8_t b[3];
		int n = 0;
		for (int x : bytes)
			b[n++] = uint8_t(x);
		m.incoming(b, n);
	};
	int bassId = -1;
	for (const Track& t : app.song.tracks)
		if (t.role == R_BASS) bassId = t.id;
	app.selectTrack(bassId);
	send({0x90, 60, 110});
	wait(40);
	check(app.keyHeld(1060), "a MIDI key plays the selected track");
	send({0x80, 60, 0});
	wait(40);
	check(!app.keyHeld(1060), "letting go of a MIDI key ends the note");
	check(m.lastHeard().contains(QLatin1String("velocity 110")), "the settings can show what came in");
	m.learn(Midi::targetForParam(dsp::P_CUTOFF), QStringLiteral("Cutoff"));
	send({0xb0, 74, 10});
	wait(40);
	check(m.controllerFor(Midi::targetForParam(dsp::P_CUTOFF)) == 74,
	      "turning a controller knob while learning ties it to the knob");
	send({0xb0, 74, 127});
	wait(40);
	check(app.song.track(bassId)->patch.p[dsp::P_CUTOFF] > 0.99f,
	      "the learned controller knob turns the knob on screen");
	send({0xb0, 74, 0});
	wait(600);
	check(app.song.track(bassId)->patch.p[dsp::P_CUTOFF] < 0.01f && app.undoName() == paramLabel(dsp::P_CUTOFF),
	      "a turn of the controller knob is one undo step");
	// a MIDI keyboard in Chords mode: D above the key note (A) is the chord on the fourth step
	app.setChordKeys(true);
	send({0x90, 48 + app.song.root + app.song.degToSemis(3), 90});
	wait(40);
	check(app.liveChord() == 3, "in Chords mode a MIDI key changes the chord");
	send({0x80, 48 + app.song.root + app.song.degToSemis(3), 0});
	wait(20);
	app.setChordKeys(false);
	// a drum track takes the General MIDI drum notes
	int drumId = -1;
	for (const Track& t : app.song.tracks)
		if (t.isDrums()) drumId = t.id;
	app.selectTrack(drumId);
	send({0x99, 38, 100});
	wait(40);
	check(app.keyHeld(1038), "a drum pad on a MIDI keyboard plays the drum track");
	send({0x89, 38, 0});
	wait(20);
	app.selectTrack(bassId);
	// follow a clock at 133 BPM, then its start and stop
	m.setFollowClock(true);
	QElapsedTimer ct;
	ct.start();
	const double gap = 60.0 / 133.0 / 24.0;
	for (int i = 0; i < 24 * 6; i++) {
		while (ct.nsecsElapsed() < qint64(i * gap * 1e9))
			QThread::usleep(200);
		send({0xf8});
		if (i % 8 == 0) QCoreApplication::processEvents();
	}
	wait(100);
	check(std::fabs(app.song.bpm - 133.f) < 1.0f, "the song follows the tempo of an incoming clock");
	send({0xfa});
	wait(100);
	pump(2); // the self-test has no sound card: the engine runs when pumped
	check(app.isPlaying(), "a start message starts the song");
	send({0xfc});
	wait(100);
	pump(2);
	check(!app.isPlaying(), "a stop message stops it");
	m.setFollowClock(false);
	// the clock Nachtzug sends comes back at the song's tempo
	app.edit([](Song& sg) { sg.bpm = 101.f; });
	pump(2);
	m.setSendClock(true);
	wait(2500); // a little longer, and 2 BPM of room: on a busy machine (GitHub's Macs) the clock's timing wobbles
	const float heard = m.clockBpm();
	if (std::fabs(heard - 101.f) >= 2.0f) std::fprintf(stderr, "  clock out: heard %.1f BPM\n", double(heard));
	check(std::fabs(heard - 101.f) < 2.0f, "the clock Nachtzug sends has the song's tempo");
	m.setSendClock(false);
	m.sendHook = nullptr;
	m.forgetAll();
	app.midi = nullptr;
	app.dirty = false;
}

// the live input: heard through its filter, never in a file, and kept close to what is played
void Test::liveInput() {
	auto through = [](const LiveInput& li, bool rendering) {
		Song sg;
		sg.input = li;
		Engine e(48000.f);
		if (rendering) e.onlyTrack = 9999;
		e.setSong(std::make_shared<const Song>(sg));
		e.inputOn.store(true);
		std::vector<float> tone(480), out(2 * 480);
		double peak = 0, ph = 0;
		for (int b = 0; b < 200; b++) {
			for (float& x : tone) {
				x = float(0.5 * std::sin(ph));
				ph += 2 * M_PI * 3000.0 / 48000.0;
			}
			e.pushInput(tone.data(), 480);
			e.process(out.data(), 480);
			if (b > 100)
				for (float x : out)
					peak = std::max(peak, double(std::fabs(x)));
		}
		return peak;
	};
	LiveInput open;
	LiveInput dark;
	dark.cutoff = 0.25f;
	LiveInput muted;
	muted.mute = true;
	const double o = through(open, false), d = through(dark, false), m = through(muted, false), r = through(open, true);
	if (qEnvironmentVariableIsSet("NZ_DEBUG"))
		std::fprintf(stderr, "input: open %.3f, dark %.3f, muted %.4f, in a file %.4f\n", o, d, m, r);
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
	for (int b = 0; b < 20; b++) {
		e.pushInput(silence.data(), 256);
		e.process(out.data(), 256);
		float pk = 0;
		for (float x : out)
			pk = std::max(pk, std::fabs(x));
		if (pk < 0.001f) quietBlocks++;
	}
	check(quietBlocks > 5, "the input catches up instead of lagging behind");
	app.setSong(demoSong(), QString());
	app.song.input.wah = 0.4f;
	app.song.input.mute = true;
	const QString sp = QDir::temp().filePath(QStringLiteral("nachtzug-selftest-input.nachtzug"));
	app.save(sp);
	app.load(sp);
	check(std::fabs(app.song.input.wah - 0.4f) < 1e-4f && app.song.input.mute,
	      "the input's settings are saved with the song");
	QFile::remove(sp);
	app.dirty = false;
}

// right wrong notes and Clean up
void Test::rightWrongNotes() {
	Song sg;
	sg.scale = 0;
	bool allBend = true;
	for (int d = 0; d < sg.sc().n; d++) {
		const int a = sg.approachAlt(d), semis = sg.degToSemis(d) + a;
		if (a == 0 || sg.degToSemis(sg.degreeNear(semis)) == semis) allBend = false;
	}
	check(allBend, "every note of a seven-note scale has a right wrong note outside the scale");
	// a bent note and a moved chord survive saving and are heard a semitone off
	Track t = makeTrack(sg, R_BASS);
	t.follow = false;
	t.arp = 0;
	t.chord = 0;
	Clip c;
	c.id = sg.newId();
	c.len = c.loopLen = BAR;
	Note n;
	n.start = 0;
	n.len = 24;
	n.deg = 2;
	n.alt = -1;
	c.notes = {n};
	t.clips = {c};
	sg.tracks = {t};
	sg.setChord(1, 3, -1);
	Song back;
	Song::fromJson(sg.toJson(), back);
	check(!back.tracks.empty() && !back.tracks[0].clips.empty() && back.tracks[0].clips[0].notes[0].alt == -1,
	      "a right wrong note survives saving");
	check(back.chordAltAt(1) == -1 && back.chordAltAt(0) == 0 && back.chordAt(1) == 3,
	      "a chord moved a semitone survives saving");
	check(back.chordName(3, -1) != back.chordName(3), "a moved chord has its own name");
	std::vector<NoteEvent> log;
	Engine e(48000.f);
	e.eventsOnly = true;
	e.log = &log;
	back.loopEnd = BAR;
	e.setSong(std::make_shared<const Song>(back));
	EngineEvent pl;
	pl.type = EngineEvent::Play;
	e.push(pl);
	std::vector<float> b(2 * 480);
	for (int i = 0; i < 20; i++)
		e.process(b.data(), 480);
	const int want = back.midiOf(back.tracks[0], 2) - 1;
	check(std::any_of(log.begin(), log.end(), [&](const NoteEvent& ev) { return ev.on && ev.note == want; }),
	      "a right wrong note sounds a semitone off its row");

	// Clean up: a one-bar figure played four times, with a late note, a doubled note, a missed note, a wrong key
	// and a tiny note pressed by accident
	Clip k;
	k.len = k.loopLen = 4 * BAR;
	const int fig[4] = {0, 2, 4, 2};
	for (int bar = 0; bar < 4; bar++)
		for (int i = 0; i < 4; i++) {
			Note m;
			m.start = bar * BAR + i * BEAT;
			m.len = 40;
			m.deg = fig[i];
			k.notes.push_back(m);
		}
	std::vector<Note> neat = k.notes;
	k.notes[1].start += 5;               // late
	k.notes.push_back(k.notes[4]);       // doubled
	k.notes.erase(k.notes.begin() + 10); // missed (bar 3, beat 3)
	k.notes[12].deg = 3;                 // wrong key (bar 4, beat 2) ...
	k.notes[7].deg = 5;                  // ... and another (bar 2, beat 4)
	{
		Note blip;
		blip.start = 2 * BEAT + 2;
		blip.len = 6;
		blip.deg = 5;
		k.notes.push_back(blip);
	} // pressed by accident
	const CleanReport r = cleanUpClip(k, false, 24);
	auto same = [](std::vector<Note> a, std::vector<Note> b2) {
		auto key = [](const Note& x) { return std::tuple(x.start, x.deg, x.alt); };
		std::sort(a.begin(), a.end(), [&](const Note& x, const Note& y) { return key(x) < key(y); });
		std::sort(b2.begin(), b2.end(), [&](const Note& x, const Note& y) { return key(x) < key(y); });
		if (a.size() != b2.size()) return false;
		for (size_t i = 0; i < a.size(); i++)
			if (key(a[i]) != key(b2[i])) return false;
		return true;
	};
	check(same(k.notes, neat), "Clean up restores a repeated figure played with mistakes");
	check(r.snapped == 1 && r.merged == 1 && r.filled == 1 && r.fixed == 2 && r.removed == 1,
	      "Clean up says what it changed");
	check(cleanUpClip(k, false, 24).total() == 0, "Clean up leaves a tidy pattern alone");
	// notes already on the chosen grid stay where they are: each of the demo song's patterns, cleaned up on the
	// coarsest grid all its notes are on (the demo song differs between systems: the same seed makes other choices
	// with another C++ library, triplets among them on a Mac)
	Song gs = demoSong();
	bool tidy = true;
	for (Track& tr : gs.tracks)
		for (Clip& cl : tr.clips)
			if (!cl.ad.on && !cl.notes.empty()) {
				int grid = 6;
				for (int g : {48, 32, 24, 16, 12, 8})
					if (std::all_of(cl.notes.begin(), cl.notes.end(),
					                [g](const Note& n) { return n.start % g == 0; })) {
						grid = g;
						break;
					}
				Clip copy = cl;
				if (cleanUpClip(copy, tr.isDrums(), grid).snapped) tidy = false;
			}
	check(tidy, "Clean up does not move notes that are already on the grid chosen (the demo song's patterns)");
	{ // notes drawn on eighths and sixteenths, cleaned up on a 1/8-triplet grid: every start moves onto it
		Clip drawn;
		drawn.len = drawn.loopLen = 4 * BAR;
		for (int st : {36, 240, 432, 600, 840, 1290})
			drawn.notes.push_back(Note{st, 60, 2});
		const CleanReport moved = cleanUpClip(drawn, false, 32);
		bool onGrid = true;
		for (const Note& n : drawn.notes)
			onGrid = onGrid && n.start % 32 == 0;
		check(onGrid && moved.snapped >= 5, "Clean up puts every note on the grid chosen, a triplet grid too");
	}
}

// the tester's findings stay fixed
void Test::testerFindings() {
	// chords recorded far up or down survive saving
	Song sg;
	sg.chords = {16, -20, 30};
	Song back;
	Song::fromJson(sg.toJson(), back);
	check(back.chords == std::vector<int>({16, -20, 30}), "chords recorded high or low survive saving");
	// repeats stay inside the pattern, and a note beyond a shortened pattern stays silent even with repeats
	auto hitsOf = [](const Clip& c, bool drums) {
		Song s2;
		s2.bpm = 120.f;
		s2.loopEnd = BAR;
		Track t = makeTrack(s2, drums ? R_DRUMS : R_BASS);
		t.clips = {c};
		t.clips[0].id = s2.newId();
		s2.tracks.push_back(t);
		std::vector<NoteEvent> log;
		Engine e(48000.f);
		e.eventsOnly = true;
		e.log = &log;
		e.setSong(std::make_shared<const Song>(s2));
		EngineEvent pl;
		pl.type = EngineEvent::Play;
		e.push(pl);
		std::vector<float> b(2 * 480);
		for (int i = 0; i < 199; i++)
			e.process(b.data(), 480); // just under two bars
		std::vector<double> ons;
		for (const NoteEvent& ev : log)
			if (ev.on) ons.push_back(ev.frame);
		return ons;
	};
	Clip c;
	c.len = 2 * BAR;
	c.loopLen = BAR;
	Note n;
	n.start = 360;
	n.len = 48;
	n.deg = dsp::D_SNARE;
	n.rep = 4;
	c.notes = {n};
	const double barFrames = 4 * 60.0 / 120.0 * 48000.0;
	bool inside = true;
	for (double f : hitsOf(c, true))
		inside &= std::fmod(f, barFrames) >= 359.0 / BAR * barFrames - 32;
	check(inside, "the repeats of a note at the end of a pattern stay in it");
	Note hidden;
	hidden.start = 400;
	hidden.len = 48;
	hidden.deg = dsp::D_SNARE;
	hidden.rep = 4;
	c.notes = {hidden};
	check(hitsOf(c, true).empty(), "a note beyond a shortened pattern stays silent, repeats or not");
	// kept drum hits stay when the drummer takes the clip over, and are saved with it
	Clip dc;
	dc.len = dc.loopLen = BAR;
	Note k;
	k.start = 72;
	k.len = 12;
	k.deg = dsp::D_RIM;
	k.lock = true;
	dc.notes = {k};
	dc.ad.on = true;
	dc.ad.seed = 5;
	autoDrumFill(dc);
	bool keptThere = false;
	for (const Note& x : dc.notes)
		keptThere |= x.lock && x.start == 72 && x.deg == dsp::D_RIM;
	Song s3;
	Track dt = makeTrack(s3, R_DRUMS);
	dc.id = s3.newId();
	dt.clips = {dc};
	s3.tracks.push_back(dt);
	Song s4;
	Song::fromJson(s3.toJson(), s4);
	bool keptSaved = false;
	for (const Note& x : s4.tracks[0].clips[0].notes)
		keptSaved |= x.lock && x.start == 72;
	check(keptThere && keptSaved, "kept hits stay when the drummer writes the clip, and are saved with it");
	// chopping onto a song without drums is one undo step
	app.setSong(Song(), QString());
	app.edit([](Song& sg2) { sg2.tracks.push_back(makeTrack(sg2, R_BASS)); });
	dsp::SampleData loop;
	loop.rate = 48000.f;
	loop.l.assign(48000, 0.f);
	loop.r.assign(48000, 0.f);
	for (int h = 0; h < 4; h++)
		for (int i = 0; i < 2000; i++)
			loop.l[size_t(h * 12000 + i)] = loop.r[size_t(h * 12000 + i)] = std::sin(i * 0.2f) * std::exp(-i / 400.f);
	QString err;
	const int before = int(app.song.tracks.size());
	app.chopOntoDrums(loop, samples::findHits(loop, 0.5f, 8), QStringLiteral("selftest undo"), 1, true, err);
	app.undo();
	check(int(app.song.tracks.size()) == before, "chopping onto a song without drums is one undo step");
	for (const QString& nm : samples::list())
		if (nm.startsWith(QStringLiteral("selftest undo"))) samples::remove(nm);
	app.dirty = false;
}

// Pump: a sustained pad dips after each kick, and only when asked to
void Test::pumpDips() {
	auto dips = [](float amount) {
		Song sg;
		sg.bpm = 120.f;
		sg.loopEnd = BAR;
		Track dr = makeTrack(sg, R_DRUMS);
		Clip c;
		c.id = sg.newId();
		c.len = BAR;
		c.loopLen = BAR;
		for (int b = 0; b < 4; b++) {
			Note n;
			n.start = b * 96;
			n.len = 12;
			n.deg = dsp::D_KICK;
			c.notes.push_back(n);
		}
		dr.clips.push_back(c);
		Track pad = makeTrack(sg, R_PAD);
		pad.duck = amount;
		pad.patch.p[dsp::P_AA] = 0.f;
		pad.patch.p[dsp::P_AS] = 1.f;
		pad.patch.p[dsp::P_DELAY] = 0.f;
		pad.patch.p[dsp::P_REVERB] = 0.f;
		Clip pc;
		pc.id = sg.newId();
		pc.len = BAR;
		pc.loopLen = BAR;
		Note n;
		n.start = 0;
		n.len = BAR;
		n.deg = 0;
		pc.notes.push_back(n);
		pad.clips.push_back(pc);
		const int padId = pad.id;
		sg.tracks.push_back(dr);
		sg.tracks.push_back(pad);
		Engine e(48000.f);
		e.onlyTrack = padId; // the kick drives the pump without being heard
		e.setSong(std::make_shared<const Song>(sg));
		EngineEvent play;
		play.type = EngineEvent::Play;
		e.push(play);
		std::vector<float> out(2 * 48000);
		e.process(out.data(), 48000); // two beats... one second at 120 BPM
		auto rms = [&](double from, double to) {
			double sum = 0;
			int n = 0;
			for (int i = int(from * 48000); i < int(to * 48000); i++) {
				sum += double(out[size_t(i) * 2]) * out[size_t(i) * 2];
				n++;
			}
			return std::sqrt(sum / std::max(1, n));
		};
		return rms(0.505, 0.52) /
		       std::max(1e-9, rms(0.70, 0.98)); // just after the second kick, against the swell before the third
	};
	const double with = dips(1.f), without = dips(0.f);
	check(with < 0.4 && without > 0.8, "Pump: the pad dips after the kick, and does not without it");
}

} // namespace

// the pictures in the style sheet (drop-down arrows, the fader cap, the dividers' grips) load with Qt's own formats:
// the downloads carry no SVG plugin, and an SVG there would simply not show
void Test::stylePictures() {
	const QString sheet = theme::styleSheet();
	check(!sheet.contains(QLatin1String(".svg")), "the style sheet names no SVG picture");
	static const QRegularExpression url(QStringLiteral("url\\(([^)]+)\\)"));
	int pictures = 0, broken = 0;
	for (auto m = url.globalMatch(sheet); m.hasNext();) {
		const QString path = m.next().captured(1);
		pictures++;
		if (QImage(path).isNull()) {
			broken++;
			std::fprintf(stderr, "  the style sheet's picture %s does not load\n", qPrintable(path));
		}
	}
	check(pictures > 0 && broken == 0, "every picture in the style sheet loads");
}

int selfTest() {
	Test t;
	t.session();
	t.swingMapsBack();
	t.undoStepsHaveNames();
	t.exportFiles();
	t.ownSamples();
	t.sampleLibrary();
	t.arrangeForMe();
	t.simpleSoundPage();
	t.noteSettings();
	t.autosaveCopy();
	t.chordsMode();
	t.drummer();
	t.noStuckNotes();
	t.stemsAndMidiFile();
	t.listeningFirst();
	t.choppingLoops();
	t.midiLoopBack();
	t.liveInput();
	t.rightWrongNotes();
	t.testerFindings();
	t.pumpDips();
	t.stylePictures();
	std::printf(t.problems ? "self-test: %d problem(s)\n" : "self-test passed\n", t.problems);
	return t.problems ? 1 : 0;
}
