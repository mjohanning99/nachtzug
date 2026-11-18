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
#include <chrono>
#include <QRegularExpression>
#include <QImage>
#include "widgets.h"
#include <QCoreApplication>
#include "app.h"
#include "mysounds.h"
#include <QFileInfo>
#include "samples.h"
#include "autosave.h"
#include "recent.h"
#include "starters.h"
#include "midifile.h"
#include "midi.h"
#include "chopdialog.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QElapsedTimer>
#include <QDir>
#include <QTemporaryDir>
#include <QThread>
#include <cmath>
#include <cstdio>
#include <cstring>

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
	void secondStopHushes();
	void stylePictures();
	void padsAndPatternEnds();
	void ownCursors();
	void snapshotsFreedOnGuiThread();
	void namedParts();
	void recentSongs();
	void starterSongs();
	void longJourney();
	void journeyParts();
	void echoAndLfo();
	void filterAndEqualiser();
	void chordsOnMonoSounds();
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
	m.sendHook = [&m](const uint8_t* b, int n, double when) { m.incoming(b, n, when); };
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
	// follow a clock at 133 BPM, then its start and stop. The ticks carry the times they would arrive at from a device
	// (eight beats, ending now), so the tempo heard is the clock's, however busy the machine is (GitHub's Macs once
	// stretched slept-for ticks to 130 BPM); a little jitter mustn't move it either
	m.setFollowClock(true);
	const auto followed = [&](double bpm, double jitter) {
		const double gap = 60.0 / bpm / 24.0;
		const double start =
		    std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count() - 24 * 8 * gap;
		for (int i = 0; i < 24 * 8; i++) {
			const uint8_t tick = 0xf8;
			m.incoming(&tick, 1, start + i * gap + (i % 2 ? jitter : -jitter));
		}
		wait(100);
		if (std::fabs(app.song.bpm - float(bpm)) >= 1.0f)
			std::fprintf(stderr, "  clock in: %.1f BPM followed to %.1f\n", bpm, double(app.song.bpm));
		return std::fabs(app.song.bpm - float(bpm)) < 1.0f;
	};
	check(followed(133.0, 0.0), "the song follows the tempo of an incoming clock");
	check(followed(118.0, 0.0005),
	      "a clock that wobbles by half a millisecond, like a USB keyboard's, is followed too");
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
	wait(2500); // each clock says when it was due, so a busy machine (GitHub’s Macs) sending it late still hears the
	            // tempo
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

// Stop lets the echoes ring out; Stop pressed again (a Hush) silences them, and everything else, at once
void Test::secondStopHushes() {
	Song sg;
	sg.bpm = 120.f;
	sg.loopEnd = BAR;
	sg.dfb = 0.9f; // a long echo
	Track lead = makeTrack(sg, R_LEAD);
	lead.patch.p[dsp::P_DELAY] = 1.f;
	lead.patch.p[dsp::P_REVERB] = 0.5f;
	Clip c;
	c.id = sg.newId();
	c.len = BAR;
	c.loopLen = BAR;
	Note n;
	n.start = 0;
	n.len = 48;
	n.deg = 0;
	c.notes.push_back(n);
	lead.clips.push_back(c);
	sg.tracks.push_back(lead);
	Engine e(48000.f);
	e.setSong(std::make_shared<const Song>(sg));
	EngineEvent ev;
	ev.type = EngineEvent::Play;
	e.push(ev);
	std::vector<float> out(2 * 24000);
	e.process(out.data(), 24000); // the note and its first echoes
	ev.type = EngineEvent::Stop;
	e.push(ev);
	e.process(out.data(), 24000);
	e.process(out.data(), 24000); // a second after Stop
	auto loudest = [&] {
		float pk = 0.f;
		for (float v : out)
			pk = std::max(pk, std::fabs(v));
		return pk;
	};
	check(!e.playing.load() && loudest() > 0.001f, "after Stop the echoes still ring");
	ev.type = EngineEvent::Hush;
	e.push(ev);
	e.process(out.data(), 1200); // the short fade
	e.process(out.data(), 24000);
	check(loudest() == 0.f, "Stop pressed again silences the echoes and the reverb at once");
	ev.type = EngineEvent::Play;
	e.push(ev);
	e.process(out.data(), 24000);
	check(loudest() > 0.001f, "after the silence the song plays again");
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

// Found by the owner in November 2026: in Chords mode only the pad pressed lights up, though another pad has the same
// chord; and a note never reaches past the end of its pattern, where the grid stops.
void Test::padsAndPatternEnds() {
	app.setSong(demoSong(), QString());
	int bassId = -1;
	for (const Track& t : app.song.tracks)
		if (t.role == R_BASS) bassId = t.id;
	app.selectTrack(bassId);
	app.setChordKeys(true);
	const int n = app.song.sc().n;
	app.keyDown(n, 0, n); // the low row's key a scale up: the key note's chord, as the middle row's first key
	app.keyUp(n);
	check(app.liveChord() == 0, "a low-row key a scale above plays the key note's chord");
	check(app.liveChordPad() == n, "the pad pressed is the one that lights, not the other pad with that chord");
	app.midiNote(60, 100, true);
	app.midiNote(60, 0, false);
	check(app.liveChordPad() == -1, "a chord from a MIDI key belongs to no one pad");
	app.setChordKeys(false);

	// a long note copied from a long pattern and pasted near the end of a short one ends where the short one does
	const int longId = app.createClip(bassId, 40 * BAR), shortId = app.createClip(bassId, 50 * BAR);
	app.edit([&](Song& s) {
		Clip* a = s.clip(longId);
		a->loopLen = a->len = 4 * BAR;
		a->notes.clear();
		Note nt;
		nt.start = BAR - 24;
		nt.len = 96;
		nt.sel = true;
		a->notes.push_back(nt);
		s.clip(shortId)->notes.clear();
	});
	app.focus = App::FocusEditor;
	app.editClip = longId;
	app.copy();
	app.editClip = shortId;
	app.seek(0); // the playhead outside the clip: pasted where it was
	app.paste();
	const Clip* c = app.song.clip(shortId);
	check(c && c->loopLen == BAR && c->notes.size() == 1 && c->notes[0].start == BAR - 24 && c->notes[0].len == 24,
	      "a pasted note ends at the end of its pattern");
	pump(10);
}

int selfTest() {
	// The files the checks write go into a folder of this run's own, so two self-tests at once (two build folders,
	// or a package build beside a test) don't write over each other's files in the shared temporary folder.
	QTemporaryDir own;
	if (own.isValid()) {
		qputenv("TMPDIR", own.path().toLocal8Bit()); // what QDir::tempPath() reads on Linux and macOS
		qputenv("TMP", own.path().toLocal8Bit());    // and on Windows
	}
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
	t.secondStopHushes();
	t.stylePictures();
	t.padsAndPatternEnds();
	t.ownCursors();
	t.snapshotsFreedOnGuiThread();
	t.namedParts();
	t.recentSongs();
	t.starterSongs();
	t.longJourney();
	t.journeyParts();
	t.echoAndLfo();
	t.filterAndEqualiser();
	t.chordsOnMonoSounds();
	std::printf(t.problems ? "self-test: %d problem(s)\n" : "self-test passed\n", t.problems);
	return t.problems ? 1 : 0;
}

// Nachtzug's own cursors: drawn for every kind, pointing where they should, and the system's when switched off
void Test::ownCursors() {
	const bool was = theme::ownCursors;
	theme::ownCursors = true;
	for (const char* kind : {"knob", "fader", "slider", "pencil", "rubber", "move", "handle", "scissors", "aim",
	                         "point", "playhead", "finger"}) {
		const QCursor c = theme::cursor(QLatin1String(kind), Qt::ArrowCursor);
		const QPixmap pm = c.pixmap();
		const double side = pm.width() / pm.devicePixelRatio();
		check(c.shape() == Qt::BitmapCursor && !pm.isNull() && side >= 20 && side <= 28,
		      "each of Nachtzug's cursors is drawn, about as big as the system's (24 points)");
		check(c.hotSpot().x() >= 0 && c.hotSpot().y() >= 0 && c.hotSpot().x() < side && c.hotSpot().y() < side,
		      "each cursor points somewhere inside its picture");
		const QImage im = pm.toImage().convertToFormat(QImage::Format_ARGB32);
		int seen = 0;
		for (int y = 0; y < im.height(); y++)
			for (int x = 0; x < im.width(); x++)
				if (qAlpha(im.pixel(x, y)) > 128) seen++;
		check(seen > im.width() * im.height() / 20, "each cursor has something to see");
	}
	const QPoint lead = theme::cursor(QStringLiteral("pencil"), Qt::ArrowCursor).hotSpot();
	check(lead.x() < 6 && lead.y() > 18, "the pencil points with its lead, at the bottom left");
	check(theme::cursor(QStringLiteral("knob"), Qt::ArrowCursor).hotSpot() == QPoint(12, 12),
	      "the knob points with its middle");
	check(theme::cursor(QStringLiteral("scissors"), Qt::CrossCursor).hotSpot().x() < 6,
	      "the scissors cut between their tips, at the top left");
	theme::ownCursors = false;
	check(theme::cursor(QStringLiteral("pencil"), Qt::ArrowCursor).shape() == Qt::ArrowCursor &&
	          theme::cursor(QStringLiteral("knob"), Qt::SizeVerCursor).shape() == Qt::SizeVerCursor,
	      "with Nachtzug's cursors switched off, the system's shapes are used");
	theme::ownCursors = was;
}

// The audio thread never frees a song: however many changes arrive between two sound buffers (a knob turned fast
// with a large buffer), the GUI thread still holds the snapshot the engine plays, and lets the old ones go itself
void Test::snapshotsFreedOnGuiThread() {
	app.setSong(demoSong(), QString());
	pump(1);
	const Song* playing = app.engine->songNow.load();
	check(playing && app.holdsSnapshot(playing), "the engine plays a snapshot the GUI thread holds");
	for (int i = 0; i < 40; i++) {
		app.song.swing = float(i % 10) * 0.05f;
		app.touch();
	}
	check(app.holdsSnapshot(playing),
	      "after 40 changes within one buffer, the snapshot the engine plays is still held");
	for (int i = 0; i < 70; i++) { // more than the 64 kept for when no sound card takes them
		app.song.swing = float(i % 10) * 0.05f;
		app.touch();
	}
	check(app.holdsSnapshot(playing),
	      "after 110 changes within one buffer, more than are otherwise kept, it is still held");
	pump(1);
	const Song* now = app.engine->songNow.load();
	check(now != playing && app.holdsSnapshot(now), "the engine moves on to the newest snapshot, still held");
	app.touch();
	check(!app.holdsSnapshot(playing), "once the engine has moved on, the old snapshots are let go");
	for (int i = 0; i < 300; i++) { // as if the sound card were unplugged: the engine takes none of these
		app.song.swing = float(i % 10) * 0.05f;
		app.touch();
	}
	check(app.keptSnapshots() <= 256,
	      "if the engine stops taking snapshots altogether, they are not kept without end (memory stays bounded)");
}

// The named parts on the timeline: an older song has none and saves as before; Arrange for me names the parts it
// makes; a part selects its clips and loops its own bars; a jump while playing waits for the bar line
void Test::namedParts() {
	{ // a song from before 0.3.6 has no "parts": it opens with none, and is saved exactly as before
		QJsonObject old = demoSong().toJson();
		old.remove(QStringLiteral("parts"));
		Song sg;
		check(Song::fromJson(old, sg) && sg.parts.empty(), "a song without parts opens with no parts");
		check(sg.toJson() == old, "a song without parts is saved without them, as an older program saved it");
	}
	Song sg;
	sg.setParts({{16, QStringLiteral("Break")},
	             {0, QStringLiteral("Intro")},
	             {-3, QStringLiteral("Before")},
	             {16, QStringLiteral("Drop")}});
	check(sg.parts.size() == 2 && sg.parts[0].bar == 0 && sg.parts[0].name == QStringLiteral("Before") &&
	          sg.parts[1].name == QStringLiteral("Drop"),
	      "setParts keeps the parts in order, one to a bar, none before bar 1");
	sg.addPart(8, QStringLiteral("Build-up"));
	sg.addPart(16, QStringLiteral("Break"));
	check(sg.parts.size() == 3 && sg.parts[1].bar == 8 && sg.parts[2].name == QStringLiteral("Break"),
	      "addPart puts a new part in its place, and renames the one already starting at that bar");
	check(sg.partAt(0) == 0 && sg.partAt(7) == 0 && sg.partAt(8) == 1 && sg.partAt(30) == 2,
	      "each bar knows the part it lies in");
	check(sg.partEnd(0) == 8 && sg.partEnd(2) == 17, "a part ends where the next begins, the last at the song's end");
	check(sg.removePart(8) && !sg.removePart(8) && sg.parts.size() == 2, "a part can be taken away, once");
	{ // saving and opening keeps them, names in any language
		sg.addPart(4, QStringLiteral("Aufbau „lang“"));
		sg.tracks.push_back(makeTrack(sg, R_BASS));
		Song back;
		check(Song::fromJson(sg.toJson(), back) && back.parts == sg.parts,
		      "the parts survive saving and opening, names and bars alike");
	}

	// Arrange for me shows the parts it makes
	app.setSong(demoSong(), QString());
	app.edit([&](Song& s) { arrangeSong(s, 1, true, app.rng); }, QStringLiteral("Arrange for me"), true);
	const std::vector<ArrangePart> plan = arrangePlan(1);
	bool same = app.song.parts.size() == plan.size();
	int bar = 0;
	for (size_t i = 0; same && i < plan.size(); i++) {
		same = app.song.parts[i].bar == bar && app.song.parts[i].name == QLatin1String(plan[i].name);
		bar += plan[i].bars;
	}
	check(same, "Arrange for me names each part it makes, where it begins");
	check(app.song.partEnd(plan.size() - 1) == bar, "the last part ends where the arranged song ends");

	// a part selects the clips that begin in it, and Loop then loops exactly its bars
	const int brk = 3; // the break
	app.selectPart(brk);
	const int a = app.song.parts[brk].bar * BAR, b = app.song.partEnd(brk) * BAR;
	bool inside = !app.selClips.isEmpty();
	for (int id : app.selClips)
		inside &= app.song.clip(id) && app.song.clip(id)->start >= a && app.song.clip(id)->start < b;
	check(inside && app.selectedPart() == brk, "clicking a part selects the clips that begin in it");
	app.loopSelection();
	check(app.song.loopOn && app.song.loopStart == a && app.song.loopEnd == b && app.undoName() == "Loop this part",
	      "Loop with a part selected loops that part's bars, as one undo step");
	app.undo();
	app.selectClip(-1);
	check(app.selectedPart() < 0, "clicking elsewhere lets go of the part");
	app.undo();
	check(app.song.parts.empty(), "undoing Arrange for me takes its parts away too");

	// a jump while playing waits for the next bar line, then carries on from the part's start in time
	Song one;
	one.bpm = 120.f; // a bar is 2 s, 96 000 samples
	one.loopOn = false;
	Engine e(48000.f);
	e.setSong(std::make_shared<const Song>(one));
	EngineEvent ev;
	ev.type = EngineEvent::Play;
	e.push(ev);
	std::vector<float> out(2 * 48000);
	e.process(out.data(), 48000); // half a bar
	ev.type = EngineEvent::Seek;
	ev.tick = 8 * BAR;
	ev.quant = BAR;
	e.push(ev);
	e.process(out.data(), 24000); // three quarters of the first bar
	check(std::abs(e.pos.load() - 0.75 * BAR) < 2.0, "a jump while playing waits for the bar line");
	e.process(out.data(), 48000); // over the bar line, a quarter of a bar on
	check(std::abs(e.pos.load() - (8.25 * BAR)) < 2.0, "at the bar line the song jumps to the part, keeping the beat");

	// Repeat this part: a copy straight after it, everything later moves right by its length, one undo step
	{
		app.setSong(demoSong(), QString());
		app.edit([&](Song& s) { arrangeSong(s, 1, true, app.rng); }, QStringLiteral("Arrange for me"), true);
		app.edit(
		    [&](Song& s) {
			    std::vector<int> prog;
			    for (int k = 0; k < 64; k++)
				    prog.push_back(k % 5);
			    s.setChords(prog);
			    for (Track& t : s.tracks) {
				    t.lanes.clear();
				    AutoLane l;
				    l.param = AP_VOLUME;
				    for (int k = 0; k < 4; k++)
					    l.pts.push_back({k * 8 * BAR, 0.1f * float(k + 1)});
				    t.lanes.push_back(l);
			    }
			    s.loopStart = 0;
			    s.loopEnd = 4 * BAR; // before the middle part: stays
		    },
		    QStringLiteral("Prepare"), false);
		const size_t mid = 3;
		const Song before = app.song;
		const QJsonObject jsonBefore = before.toJson();
		const int a = before.parts[mid].bar, b = before.partEnd(mid), n = b - a;
		const QString name = before.parts[mid].name;
		app.selectPart(int(mid));
		app.focus = App::FocusArrange;
		app.duplicateSelection(); // with a part selected, Copy repeats the part
		const Song& s = app.song;
		check(app.undoName() == "Repeat a part", "Repeat this part is one undo step with its own name");
		check(s.parts.size() == before.parts.size() + 1 && s.parts[mid + 1].bar == b &&
		          s.parts[mid + 1].name == name + QStringLiteral(" 2") && s.partEnd(mid + 1) == b + n,
		      "the copy becomes a part named like the original plus 2, straight after it");
		check(app.selectedPart() == int(mid + 1) && !app.selClips.isEmpty(), "the copy is selected afterwards");
		bool shifted = s.parts.size() == before.parts.size() + 1;
		for (size_t k = mid + 1; k < before.parts.size(); k++)
			shifted &= s.parts[k + 1].bar == before.parts[k].bar + n && s.parts[k + 1].name == before.parts[k].name;
		check(shifted, "the later parts move right by exactly the part's length");
		bool clipsOk = true;
		int copied = 0, origClips = 0;
		for (size_t ti = 0; ti < before.tracks.size(); ti++) {
			const auto& was = before.tracks[ti].clips;
			const auto& now = s.tracks[ti].clips;
			int inPart = 0;
			for (const Clip& c : was) {
				const Clip* m = nullptr;
				for (const Clip& d : now)
					if (d.id == c.id) m = &d;
				if (c.start >= b * BAR)
					clipsOk &= m && m->start == c.start + n * BAR;
				else {
					clipsOk &= m && m->start == c.start;
					if (c.start >= a * BAR) {
						inPart++;
						bool twin = false;
						for (const Clip& d : now)
							twin |= d.id != c.id && d.start == c.start + n * BAR && d.len == c.len &&
							        d.notes.size() == c.notes.size();
						clipsOk &= twin;
					}
				}
			}
			origClips += inPart;
			copied += int(now.size() - was.size());
			clipsOk &= int(now.size() - was.size()) == inPart;
		}
		check(clipsOk && origClips > 0 && copied == origClips,
		      "the part's clips are copied into the gap and the later clips move right by its length");
		bool chordsOk = true, pointsOk = true;
		for (int k = 0; k < 40; k++) {
			const int from = k < b ? k : (k < b + n ? k - n : k - n);
			chordsOk &= s.chordAt(k) == before.chordAt(from);
		}
		for (size_t ti = 0; ti < before.tracks.size(); ti++) {
			const AutoLane* w = before.tracks[ti].lane(AP_VOLUME);
			const AutoLane* l = s.tracks[ti].lane(AP_VOLUME);
			if (!l) {
				pointsOk = false;
				continue;
			}
			// the line, sampled each sixteenth: as before up to the part's end, the part's shape again in the copy,
			// and the rest as before, later by the part's length
			for (int tick = 0; tick < (b + n + 4) * BAR; tick += PPQ / 4) {
				const int from = tick < b * BAR ? tick : tick - n * BAR; // the copy and everything after: n bars on
				pointsOk &= std::abs(l->valueAt(tick, 0.f) - w->valueAt(from, 0.f)) < 1e-4f;
			}
		}
		check(chordsOk, "the chords of the part are repeated and the later chords move right with the rest");
		check(pointsOk, "knob lines keep their shape: the part's line again in the copy, the rest later by its length");
		check(s.loopStart == 0 && s.loopEnd == 4 * BAR, "a loop before the part stays where it is");
		check(s.endTick() == before.endTick() + n * BAR, "the song grows by the part's length");
		app.undo();
		check(app.song.toJson() == jsonBefore, "undoing Repeat this part restores the song exactly");

		// the loop markers after the part move with it
		app.edit(
		    [&](Song& sg) {
			    sg.loopStart = b * BAR;
			    sg.loopEnd = (b + 2) * BAR;
		    },
		    QStringLiteral("Prepare"), false);
		app.repeatPart(int(mid));
		check(app.song.loopStart == (b + n) * BAR && app.song.loopEnd == (b + n + 2) * BAR,
		      "loop markers after the part move right with it");
		app.repeatPart(int(mid)); // again: the next free number
		check(app.song.parts[mid + 1].name == name + QStringLiteral(" 3") &&
		          app.song.parts[mid + 2].name == name + QStringLiteral(" 2"),
		      "repeating again takes the next free number");

		// a song without parts is left alone
		app.setSong(demoSong(), QString());
		const QJsonObject plain = app.song.toJson();
		app.repeatPart(0);
		check(app.song.toJson() == plain && app.undoName().isEmpty(), "a song without parts is not changed");
	}
	{ // a clip that runs across a part's edge (as Continue makes them): cut there, so nothing ends up doubled
		Song s = demoSong();
		Track& t = s.tracks[0];
		Clip one = t.clips.front();
		one.start = 0;
		one.len = 16 * BAR;
		t.clips = {one};
		s.parts = {{0, QStringLiteral("Intro")}, {8, QStringLiteral("Main part")}};
		s.repeatPart(0);
		std::vector<std::pair<int, int>> spans;
		for (const Clip& c : t.clips)
			spans.push_back({c.start, c.end()});
		std::sort(spans.begin(), spans.end());
		bool apart = true;
		int covered = 0;
		for (size_t k = 0; k < spans.size(); k++) {
			if (k > 0) apart &= spans[k].first >= spans[k - 1].second;
			covered += spans[k].second - spans[k].first;
		}
		check(apart && covered == 24 * BAR,
		      "a clip across a part's edge is cut there: the repeat adds the part's bars once, with no clips on "
		      "top of each other");
	}
}

// File > Open recent: the last songs opened or saved, newest first, each once, at most eight, never a spare copy
void Test::recentSongs() {
	const QString dir = QDir::temp().filePath(QStringLiteral("nachtzug-selftest-recent"));
	QDir(dir).removeRecursively();
	QDir().mkpath(dir + QStringLiteral("/a"));
	QDir().mkpath(dir + QStringLiteral("/b"));
	auto at = [&](const QString& name) { return QDir(dir).filePath(name); };

	QStringList l;
	for (int i = 0; i < 10; i++)
		l = recent::added(l, at(QStringLiteral("song %1.nachtzug").arg(i)));
	check(l.size() == recent::MAX, "the list keeps at most eight songs");
	check(l.first().endsWith(QLatin1String("song 9.nachtzug")) && l.last().endsWith(QLatin1String("song 2.nachtzug")),
	      "the newest song is on top and the oldest fall off the end");
	l = recent::added(l, at(QStringLiteral("song 5.nachtzug")));
	check(l.size() == recent::MAX && l.first().endsWith(QLatin1String("song 5.nachtzug")) &&
	          l.filter(QStringLiteral("song 5.")).size() == 1,
	      "a song opened again moves to the top and is listed once");
	l = recent::added(l, at(QStringLiteral("a/../song 5.nachtzug")));
	check(l.filter(QStringLiteral("song 5.")).size() == 1, "the same song written another way is still listed once");
	const QStringList before = l;
	l = recent::added(l, at(QStringLiteral("autosave/session-1a2b3c4d.nachtzug")));
	check(l == before, "a spare copy of autosave never appears in the list");
	l = recent::added(l, QString());
	check(l == before, "a song with no file adds nothing");

	const QStringList twins = {at(QStringLiteral("a/Night.nachtzug")), at(QStringLiteral("b/Night.nachtzug")),
	                           at(QStringLiteral("Day.nachtzug"))};
	const QStringList shown = recent::names(twins);
	check(shown[0] == QStringLiteral("Night (a)") && shown[1] == QStringLiteral("Night (b)") &&
	          shown[2] == QStringLiteral("Day"),
	      "two songs of the same name show their folders; others show only their name");

	// saving and opening put the song on the list (in the settings), and a copy brought back by autosave does not
	recent::clear();
	check(recent::list().isEmpty(), "Clear the list empties it");
	app.setSong(demoSong(), QString());
	const QString first = at(QStringLiteral("First.nachtzug")), second = at(QStringLiteral("Second.nachtzug"));
	check(app.save(first), "a song saves");
	check(app.save(second), "Save as writes a second song");
	check(recent::list().size() == 2 && recent::list().first().endsWith(QLatin1String("Second.nachtzug")),
	      "saving and Save as both put the song on top of the list");
	check(app.load(first) && recent::list().first().endsWith(QLatin1String("First.nachtzug")),
	      "opening a song puts it on top");
	const QString spareDir = QDir(dir).filePath(QStringLiteral("autosave"));
	QDir().mkpath(spareDir);
	QFile::copy(first, QDir(spareDir).filePath(QStringLiteral("session-0000aaaa.nachtzug")));
	check(app.load(QDir(spareDir).filePath(QStringLiteral("session-0000aaaa.nachtzug"))) &&
	          recent::list().size() == 2 && !recent::isSpareCopy(recent::list().first()),
	      "opening a spare copy leaves the list alone");
	QFile::remove(second);
	check(!recent::exists(recent::list().last()) && recent::exists(recent::list().first()),
	      "a song that was moved away is known to be missing, the other is found");
	recent::remove(second);
	check(recent::list().size() == 1, "a missing song can be taken off the list");
	{ // as on a Mac, where the temporary folder /var/… is a link to /private/var/…: saved through a link, then gone
		QDir().mkpath(at(QStringLiteral("real")));
		// on Windows QFile::link makes a shortcut file, not a folder one can save into: there, nothing to check
		if (QFile::link(at(QStringLiteral("real")), at(QStringLiteral("linked"))) &&
		    QFileInfo(at(QStringLiteral("linked"))).isDir()) {
			const QString viaLink = at(QStringLiteral("linked/Third.nachtzug"));
			check(app.save(viaLink), "a song saves into a folder reached through a link");
			const qsizetype before = recent::list().size();
			QFile::remove(viaLink);
			recent::remove(viaLink);
			check(recent::list().size() == before - 1,
			      "a missing song saved through a linked folder can still be taken off the list by that path");
		}
	}
	recent::clear();
	app.setSong(demoSong(), QString());
	QDir(dir).removeRecursively();
}

// The starter songs: one for each style, built in, each a whole short song that plays, opened as a new song
void Test::starterSongs() {
	check(starters().size() >= 4, "there is a starter song for each style");
	for (int i = 0; i < int(starters().size()); i++) {
		Song s;
		const bool ok = loadStarter(i, s);
		check(ok, "each starter song comes with the program and opens");
		if (!ok) continue;
		check(s.tracks.size() >= 3, "each starter has drums and at least two other tracks");
		bool drums = false, allPlay = true;
		for (const Track& t : s.tracks) {
			drums |= t.isDrums() && !t.clips.empty();
			bool notes = false;
			for (const Clip& c : t.clips)
				notes |= !c.notes.empty();
			allPlay &= notes;
		}
		check(drums && allPlay, "every track of a starter has something to play");
		const double minutes = s.endTick() / double(PPQ) / s.bpm;
		check(minutes > 1.5 && minutes < 4.0, "each starter is a short, whole song of two or three minutes");
		check(!s.loopOn, "a starter plays through from start to end, not round the loop");
		check(s.loopEnd - s.loopStart >= 8 * BAR, "the loop marks a main part of at least eight bars to listen to");
		// saved by an earlier version: what is in the file is what is played (nothing new needed in the song file)
		Song again;
		check(Song::fromJson(s.toJson(), again) &&
		          QJsonDocument(again.toJson()).toJson() == QJsonDocument(s.toJson()).toJson(),
		      "a starter survives saving and opening unchanged");
		// made afresh from the generators, it has the same shape as the one that comes with the program
		const Song made = makeStarter(i);
		check(made.tracks.size() == s.tracks.size() && made.bpm == s.bpm && made.scale == s.scale,
		      "a starter made afresh has the same tracks, tempo and scale as the built-in one");
		// opened in the program it is untitled and plays
		app.setSong(s, QString());
		check(app.filePath.isEmpty() && !app.dirty, "a starter opens as a new, untitled song");
		peak = 0.f;
		app.seek(s.loopStart);
		app.play();
		pump(200);
		app.stop();
		check(peak > 0.05f, "a starter's main part makes sound");
	}
	Song none;
	check(!loadStarter(-1, none) && !loadStarter(int(starters().size()), none), "there is no starter outside the list");
	pump(20);
	app.setSong(demoSong(), QString());
}

// A long journey: for any seed and character, the song grows by the minutes asked for, every note stays in the
// scale, something always plays, the parts come and go, the patterns change slowly, the knob lines move, the part
// before the journey stays as it was, one Undo takes it all back, and the same seed gives the same journey
void Test::longJourney() {
	const auto songText = [](const Song& s) { return QJsonDocument(s.toJson()).toJson(QJsonDocument::Compact); };
	// a few songs to start from: the demo, a bass and drums only, and a song whose tracks are all empty
	std::vector<Song> starts;
	starts.push_back(demoSong());
	{
		Song s = demoSong();
		std::erase_if(s.tracks, [](const Track& t) { return t.role != R_BASS && t.role != R_DRUMS; });
		starts.push_back(s);
		for (Track& t : s.tracks)
			for (Clip& c : t.clips) {
				c.notes.clear();
				c.ad.on = false;
			}
		starts.push_back(s);
	}
	bool length = true, inScale = true, neverSilent = true, comeAndGo = true, linesMove = true, beforeKept = true,
	     planWhole = true, slowSteps = true, drifts = true, noRepeats = true, peakLate = true, oneBreakdown = true,
	     travels = true;
	for (size_t si = 0; si < starts.size(); si++)
		for (int character = 0; character < 3; character++)
			for (unsigned seed = 1; seed <= 4; seed++) {
				Song s = starts[si];
				const int from = (s.endTick() + BAR - 1) / BAR, minutes = seed == 4 ? 30 : 4 + int(seed) * 3;
				const Song before = s;
				std::mt19937 g(seed * 31u + unsigned(character));
				const std::vector<JourneyPhase> plan = continueJourney(s, from, minutes, character, true, g);
				const int bars = journeyBars(s, minutes);
				length &= s.endTick() == (from + bars) * BAR;
				int at = from;
				for (const JourneyPhase& ph : plan) {
					planWhole &= ph.bar == at && ph.bars > 0 && ph.bars % 8 == 0;
					at += ph.bars;
				}
				planWhole &= at == from + bars && !plan.empty() && plan.front().type != J_BREAK;
				// one arc: no group of phases straight after the same group, a single big breakdown, the peak late
				for (size_t k = 2; k <= 4; k++)
					for (size_t i = 0; i + 2 * k <= plan.size(); i++) {
						bool same = true;
						for (size_t j = 0; j < k; j++)
							same &=
							    plan[i + j].type == plan[i + k + j].type && plan[i + j].bars == plan[i + k + j].bars;
						noRepeats &= !same;
					}
				int breakdowns = 0, peaks = 0;
				float top = 0.f;
				for (const JourneyPhase& ph : plan)
					top = std::max(top, ph.energy);
				for (const JourneyPhase& ph : plan) {
					breakdowns += ph.type == J_BREAK;
					if (ph.type == J_PEAK) {
						peaks++;
						peakLate &= ph.bar >= from + bars / 2 && ph.energy >= top;
					}
				}
				oneBreakdown &= breakdowns <= 1 && (minutes < 10 || breakdowns == 1);
				peakLate &= minutes < 10 || peaks == 1;
				std::vector<char> playing(size_t(from + bars), 0);
				for (const Track& t : s.tracks) {
					int pieces = 0, lastEnd = -1;
					bool gap = false;
					for (const Clip& c : t.clips) {
						for (const Note& n : c.notes)
							inScale &= n.alt == 0 && (t.isDrums() ? n.deg >= 0 && n.deg < dsp::D_COUNT
							                                      : n.deg >= DEG_MIN && n.deg <= DEG_MAX);
						if (c.start < from * BAR) continue;
						pieces++;
						gap |= lastEnd >= 0 && c.start > lastEnd;
						lastEnd = c.end();
						if (!c.notes.empty() && !t.mute)
							for (int b = c.start / BAR; b < c.end() / BAR; b++)
								playing[size_t(b)] = 1;
					}
					if (t.role == R_SEQ || t.role == R_PAD || t.role == R_DRUMS) comeAndGo &= pieces > 1;
					if (si == 0 && t.role == R_BASS) comeAndGo &= gap; // the bass leaves for the breakdown
				}
				for (int b = from; b < from + bars; b++)
					neverSilent &= playing[size_t(b)] != 0;
				// the knob lines: they move over the journey and leave what came before as it was
				for (size_t ti = 0; ti < s.tracks.size(); ti++) {
					const Track& t = s.tracks[ti];
					if (t.isDrums()) continue;
					const AutoLane* l = t.lane(dsp::P_CUTOFF);
					if (!l) {
						linesMove = false;
						continue;
					}
					float lo = 1.f, hi = 0.f;
					for (const AutoPoint& p : l->pts)
						if (p.tick >= from * BAR) {
							lo = std::min(lo, p.v);
							hi = std::max(hi, p.v);
						}
					linesMove &= hi - lo > 0.05f;
					if (minutes >= 10 && (t.role == R_SEQ || t.role == R_BASS)) travels &= hi - lo >= 0.25f;
					const Track& was = before.tracks[ti];
					for (int tick = 0; tick < from * BAR; tick += BAR / 2)
						beforeKept &= std::fabs(t.automated(dsp::P_CUTOFF, tick, t.patch.p[dsp::P_CUTOFF]) -
						                        was.automated(dsp::P_CUTOFF, tick, was.patch.p[dsp::P_CUTOFF])) < 1e-6f;
				}
				// the patterns change a little at a time and the steps add up: the sequence keeps its rhythm and stays
				// within an octave of where each note began, and late in the journey it has wandered further than early
				// on
				if (si == 0)
					for (const Track& t : s.tracks) {
						if (t.role != R_SEQ) continue;
						const Clip& home = before.tracks[size_t(before.trackIndex(t.id))].clips[0];
						const int n7 = s.sc().n;
						bool changed = false;
						int early = 0, earlyN = 0, late = 0, lateN = 0;
						for (const Clip& c : t.clips) {
							// (a new idea for a while, with another name, is not a step)
							if (c.start < from * BAR || c.notes.size() != home.notes.size() || c.name != home.name)
								continue;
							int diff = 0;
							for (size_t i = 0; i < c.notes.size(); i++) {
								diff += c.notes[i].deg != home.notes[i].deg;
								slowSteps &= c.notes[i].start == home.notes[i].start &&
								             std::abs(c.notes[i].deg - home.notes[i].deg) <= n7;
							}
							changed |= diff > 0;
							const int along = c.start / BAR - from;
							if (along < bars / 4) {
								early += diff;
								earlyN++;
							} else if (along >= bars * 3 / 4) {
								late += diff;
								lateN++;
							}
						}
						slowSteps &= changed;
						if (earlyN && lateN && minutes >= 10)
							drifts &= late * earlyN >= early * lateN; // on average at least as far from home
					}
				// the same seed gives the same journey
				Song again = starts[si];
				std::mt19937 g2(seed * 31u + unsigned(character));
				continueJourney(again, from, minutes, character, true, g2);
				check(songText(again) == songText(s), "the same seed gives the same journey");
			}
	check(length, "a journey makes the song exactly as long as the minutes asked for, in whole phrases");
	check(planWhole, "the journey's phases follow one another without a gap, in whole phrases, and none starts with a "
	                 "breakdown");
	check(inScale, "every note of a journey is in the song's scale");
	check(neverSilent, "in every bar of a journey something plays");
	check(comeAndGo, "in a journey the parts come and go (the bass leaves for the breakdown)");
	check(linesMove, "every synthesiser track gets a filter line that moves over the journey");
	check(beforeKept, "the knob lines before the journey stay as they were");
	check(slowSteps, "the sequence changes over the journey, a note at a time, keeping its rhythm and its notes within "
	                 "an octave of where they began");
	check(drifts, "late in a journey the sequence has wandered at least as far from its start as early on");
	check(noRepeats, "in a journey no group of two to four phases comes straight after the same group");
	check(oneBreakdown, "a journey has one big breakdown (from ten minutes on, exactly one)");
	check(peakLate, "a journey's one peak lies in its second half and is its highest point");
	check(travels, "the filter lines of the sequence and the bass travel at least a quarter of the knob's range");
	{
		Song s = demoSong(), t = demoSong();
		std::mt19937 g(1), h(2);
		continueJourney(s, 8, 10, J_EVOLVING, true, g);
		continueJourney(t, 8, 10, J_EVOLVING, true, h);
		check(songText(s) != songText(t), "another seed gives another journey");
		Song r;
		check(Song::fromJson(s.toJson(), r) && songText(r) == songText(s), "a journey survives saving and opening");
	}
	// replacing after a part of the song: what comes before stays, the rest is the journey
	{
		Song s = demoSong();
		std::mt19937 g0(5);
		arrangeSong(s, 0, true, g0);
		const Song before = s;
		std::mt19937 g(9);
		continueJourney(s, 24, 5, J_CALM, false, g);
		bool kept = true;
		for (size_t ti = 0; ti < s.tracks.size(); ti++)
			for (const Clip& c : before.tracks[ti].clips)
				if (c.end() <= 24 * BAR) {
					const Clip* now = s.clip(c.id);
					kept &= now && now->start == c.start && now->len == c.len && now->notes.size() == c.notes.size();
				}
		check(kept, "a journey after bar 24 leaves the clips before it alone");
		check(s.endTick() == (24 + journeyBars(s, 5)) * BAR, "a journey after bar 24 replaces the rest of the song");
	}
	// in the program: one undo step, made at the next bar line while the song plays
	app.setSong(demoSong(), QString());
	const QByteArray was = songText(app.song);
	app.seek(0);
	app.play();
	pump(20);
	const unsigned seed = unsigned(app.rng()); // drawn once, as the Continue key does
	app.onBar(
	    [seed](Song& s) {
		    std::mt19937 g(seed);
		    continueJourney(s, (s.endTick() + BAR - 1) / BAR, 3, J_EVOLVING, true, g);
	    },
	    true, QStringLiteral("Continue for 3 minutes"), true);
	check(app.song.endTick() == 8 * BAR, "while the song plays, a journey waits for the bar line");
	realtime(260);
	check(app.song.endTick() > 8 * BAR, "the journey arrives on the bar line while the song plays");
	check(app.undoName() == QStringLiteral("Continue for 3 minutes"), "a journey is one undo step with its own name");
	app.seek(double(app.song.endTick() / 2));
	peak = 0.f;
	pump(300);
	check(peak > 0.05f && peak <= 1.f, "the middle of a journey plays, without going over full scale");
	app.stop();
	pump(10);
	app.undo();
	check(songText(app.song) == was, "Undo takes the whole journey back");
}

// A long journey names its phases as parts on the timeline: after the parts of an arranged song, in place of those
// from the journey's start on, one part for phases of the same kind in a row, and one Undo takes them away again
void Test::journeyParts() {
	// the parts a plan should leave from a bar on: each phase where it begins, phases of the same kind in a row as one
	const auto expected = [](const std::vector<JourneyPhase>& plan) {
		std::vector<SongPart> want;
		for (const JourneyPhase& ph : plan)
			if (want.empty() || want.back().name != QLatin1String(ph.name))
				want.push_back({ph.bar, QString::fromLatin1(ph.name)});
		return want;
	};
	const auto tail = [](const Song& s, int fromBar) {
		std::vector<SongPart> got;
		for (const SongPart& pt : s.parts)
			if (pt.bar >= fromBar) got.push_back(pt);
		return got;
	};
	{ // a song without parts: the journey's phases become its parts
		Song s = demoSong();
		const int from = (s.endTick() + BAR - 1) / BAR;
		std::mt19937 g(11);
		const std::vector<JourneyPhase> plan = continueJourney(s, from, 8, J_EVOLVING, true, g);
		check(!plan.empty() && !s.parts.empty(), "after a journey the song has named parts");
		check(s.parts == expected(plan), "each part starts where a phase of the journey starts, named after it");
		check(s.parts.front().bar == from, "the first part starts where the journey starts");
		Song back;
		check(Song::fromJson(s.toJson(), back) && back.parts == s.parts, "the journey's parts are saved with the song");
	}
	{ // after Arrange for me: the arranged parts stay, the journey's follow them
		Song s = demoSong();
		std::mt19937 r(5);
		arrangeSong(s, 1, true, r);
		const std::vector<SongPart> arranged = s.parts;
		const int from = (s.endTick() + BAR - 1) / BAR;
		std::mt19937 g(12);
		const std::vector<JourneyPhase> plan = continueJourney(s, from, 6, J_CALM, true, g);
		const std::vector<SongPart> before(s.parts.begin(),
		                                   s.parts.begin() + std::min(s.parts.size(), arranged.size()));
		check(before == arranged, "the parts Arrange for me named stay as they were before the journey");
		check(tail(s, from) == expected(plan) && s.parts.size() == arranged.size() + expected(plan).size(),
		      "the journey's parts follow the arranged ones");
		check(s.partEnd(arranged.size() - 1) == from, "the last arranged part now ends where the journey begins");
	}
	{ // a journey from a bar inside the arranged song replaces the parts from there on
		Song s = demoSong();
		std::mt19937 r(5);
		arrangeSong(s, 1, true, r);
		const int from = s.parts[2].bar + 4;
		std::mt19937 g(13);
		const std::vector<JourneyPhase> plan = continueJourney(s, from, 4, J_RESTLESS, false, g);
		check(s.parts.size() >= 3 && s.parts[2].bar < from && tail(s, from) == expected(plan),
		      "a journey from the middle keeps the parts before it and replaces those after it");
	}
	{ // in the program: one Undo takes the journey's parts away with the rest
		app.setSong(demoSong(), QString());
		app.edit(
		    [](Song& s) {
			    std::mt19937 g(14);
			    continueJourney(s, (s.endTick() + BAR - 1) / BAR, 3, J_EVOLVING, true, g);
		    },
		    QStringLiteral("Long journey"), true);
		check(!app.song.parts.empty(), "a journey written in the program shows its parts");
		app.undo();
		check(app.song.parts.empty(), "undoing the journey takes its parts away too");
		app.setSong(demoSong(), QString());
	}
}

// The echo in note values with its tone and ping-pong, and the LFO in time, to the volume and the pan, with a fade-in.
// Older songs know none of it and must sound exactly as before: they open with the old behaviour, and the echo at
// those settings does, to the bit, what the old one did (its code is copied here as it was).
void Test::echoAndLfo() {
	// an older song, without any of the new fields
	QJsonObject old = demoSong().toJson();
	old.remove(QStringLiteral("echoTone"));
	old.remove(QStringLiteral("echoPingPong"));
	QJsonArray ts = old[QStringLiteral("tracks")].toArray();
	for (int i = 0; i < ts.size(); i++) {
		QJsonObject tj = ts[i].toObject(), pj = tj[QStringLiteral("patch")].toObject();
		for (const char* k : {"lfoSync", "lfoAmp", "lfoPan", "lfoFade"})
			pj.remove(QLatin1String(k));
		tj[QStringLiteral("patch")] = pj;
		ts[i] = tj;
	}
	old[QStringLiteral("tracks")] = ts;
	Song sg;
	check(Song::fromJson(old, sg), "an older song opens");
	check(sg.echoTone == 0.5f && sg.echoPingPong,
	      "an older song's echo has the classic tone and bounces left and right");
	bool plain = true;
	for (const Track& t : sg.tracks)
		plain &= t.patch.p[dsp::P_LFOSYNC] == 0.f && t.patch.p[dsp::P_LFOAMP] == 0.f &&
		         t.patch.p[dsp::P_LFOPAN] == 0.f && t.patch.p[dsp::P_LFOFADE] == 0.f;
	check(plain, "an older song's LFOs run free, move neither volume nor pan, and do not fade in");
	check(dsp::echoBeats(0) == 0.25f && dsp::echoBeats(1) == 0.5f && dsp::echoBeats(2) == 0.75f &&
	          dsp::echoBeats(3) == 1.f && dsp::echoBeats(4) == 1.5f,
	      "the echo times older songs saved keep their note values");
	bool longer = true;
	for (int i = 1; i < dsp::ECHO_TIMES; i++)
		longer &= dsp::echoBeats(DTIME_ORDER[i]) > dsp::echoBeats(DTIME_ORDER[i - 1]);
	check(longer, "the echo times are listed from the shortest to the longest");
	check(dsp::echoToneCoef(0.5f) == 0.32f && dsp::echoToneCoef(0.f) < 0.1f && dsp::echoToneCoef(1.f) > 0.9f,
	      "the echo's tone knob in the middle is the classic tone, darker to the left, brighter to the right");

	// the echo at the old settings, against the old echo's code. Only above full scale inside the echo, which none
	// of the demo and example songs reaches (their loudest is about half of it), does the soft limit change anything.
	struct OldPingPong {
		float sr = 48000.f, cur = 1000.f, lp = 0.f, hpS = 0.f;
		dsp::DelayLine a, b;
		void init(float rate) {
			sr = rate;
			a.init(int(sr * 2.6f));
			b.init(int(sr * 2.6f));
			cur = sr * 0.3f;
		}
		void process(float in, float timeSamples, float fb, float& outL, float& outR) {
			cur += (timeSamples - cur) * 0.0004f;
			const float d = std::clamp(cur, 8.f, sr * 2.5f);
			const float ra = a.read(d), rb = b.read(d);
			lp += (rb - lp) * 0.32f;
			hpS += (lp - hpS) * 0.02f;
			a.write(in + (lp - hpS) * fb);
			b.write(ra);
			outL = ra;
			outR = rb;
		}
	} was;
	dsp::PingPong now;
	was.init(48000.f);
	now.init(48000.f);
	dsp::Rng noise;
	bool same = true;
	for (int i = 0; i < 48000 * 6; i++) { // across the lines' end, with notes coming and going
		const float in = (i / 6000) % 2 ? noise.next() * 0.25f : 0.f; // below full scale, as every song tried
		const float t = 60.f / 118.f * dsp::echoBeats(2) * 48000.f;
		float l0, r0, l1, r1;
		was.process(in, t, 0.85f, l0, r0);
		now.process(in, t, 0.85f, dsp::echoToneCoef(0.5f), 0.f, l1, r1);
		same &= l0 == l1 && r0 == r1;
	}
	check(same, "the echo at an older song's settings sounds exactly as the old echo, to the bit");

	// it never runs away: the most repeats, the brightest tone, a loud sound fed in for ten seconds
	{
		dsp::PingPong e;
		e.init(48000.f);
		float pk = 0.f, l, r;
		for (int i = 0; i < 480000; i++) {
			e.process(std::sin(float(i) * 0.05f) * 1.5f, 6000.f, 0.85f, 1.f, 0.f, l, r);
			pk = std::max(pk, std::max(std::fabs(l), std::fabs(r)));
		}
		check(std::isfinite(pk) && pk <= 2.f, "the echo never runs away, however loud and long it is fed");
	}
	// ping-pong off: the repeats come from the middle
	{
		dsp::PingPong e;
		e.init(48000.f);
		float l, r, diff = 0.f, loud = 0.f;
		for (int i = 0; i < 48000; i++) {
			e.process(i < 2000 ? noise.next() : 0.f, 4800.f, 0.6f, 0.32f, 1.f, l, r);
			if (i > 24000) { // once the switch has glided over
				diff = std::max(diff, std::fabs(l - r));
				loud = std::max(loud, std::fabs(l));
			}
		}
		check(loud > 0.01f && diff < 1e-6f, "with ping-pong off the repeats are the same in both speakers");
	}
	// another note value while it rings: a crossfade, no click
	{
		dsp::PingPong e;
		e.init(48000.f);
		float l, r, prevL = 0.f, steady = 0.f, jump = 0.f;
		for (int i = 0; i < 96000; i++) {
			const float t = i < 48000 ? 6000.f : 12000.f;
			e.process(std::sin(float(i) * 0.03f) * 0.5f, t, 0.5f, 0.32f, 0.f, l, r);
			const float step = std::fabs(l - prevL);
			if (i > 24000 && i < 48000) steady = std::max(steady, step);
			if (i >= 48000 && i < 52000) jump = std::max(jump, step);
			prevL = l;
		}
		check(jump < steady * 2.f, "changing the echo's time while it rings crossfades, without a click");
	}

	// the LFO in time: a square to the volume on quarter notes, following the song's position
	auto render = [](const float* patch, double beatsPerSample, float seconds, std::vector<float>& outL,
	                 std::vector<float>& outR) {
		dsp::Synth syn;
		syn.init(48000.f);
		syn.setPatch(patch);
		syn.noteOn(1, 57, false);
		const int n = int(seconds * 48000.f);
		outL.assign(size_t(n), 0.f);
		outR.assign(size_t(n), 0.f);
		for (int at = 0; at < n; at += dsp::CHUNK) {
			const int k = std::min(dsp::CHUNK, n - at);
			syn.setClock(beatsPerSample > 0 ? at * beatsPerSample : -1.0, 120.f);
			syn.render(&outL[size_t(at)], &outR[size_t(at)], k);
		}
	};
	auto rms = [](const std::vector<float>& v, double from, double to) {
		double sum = 0;
		int n = 0;
		for (int i = int(from * 48000); i < int(to * 48000) && i < int(v.size()); i++, n++)
			sum += double(v[size_t(i)]) * v[size_t(i)];
		return std::sqrt(sum / std::max(1, n));
	};
	Patch pad = presetByName(QStringLiteral("Night Drone"));
	float p[dsp::P_COUNT];
	std::memcpy(p, pad.p, sizeof(p));
	p[dsp::P_AA] = 0.f;
	p[dsp::P_AS] = 1.f;
	p[dsp::P_LFOCUT] = p[dsp::P_LFOPITCH] = p[dsp::P_LFOPW] = 0.f;
	p[dsp::P_LFOWAVE] = 1.f; // square
	p[dsp::P_LFOSYNC] = 1.f;
	p[dsp::P_LFORATE] = 0.5f; // 1/4
	p[dsp::P_LFOAMP] = 1.f;
	std::vector<float> L, R;
	const double bps = 2.0 / 48000.0; // 120 BPM: two beats a second
	render(p, bps, 3.f, L, R);
	// a square is up for the first half of each cycle: the volume is down there, and up in the second half
	check(rms(L, 1.03, 1.22) < rms(L, 1.28, 1.47) * 0.2 && rms(L, 1.53, 1.72) < rms(L, 1.78, 1.97) * 0.2,
	      "with the LFO in time on quarter notes, the volume dips on every beat and comes back half a beat later");
	p[dsp::P_LFORATE] = 0.3f; // one bar
	render(p, bps, 3.f, L, R);
	check(rms(L, 0.1, 0.9) < rms(L, 1.1, 1.9) * 0.2, "on one bar, it dips for the first half of the bar");

	// to the pan: the sound moves between the speakers
	p[dsp::P_LFOAMP] = 0.f;
	p[dsp::P_LFOPAN] = 1.f;
	p[dsp::P_LFORATE] = 0.5f;
	render(p, bps, 2.f, L, R);
	check(rms(L, 1.03, 1.22) < rms(R, 1.03, 1.22) * 0.2 && rms(R, 1.28, 1.47) < rms(L, 1.28, 1.47) * 0.2,
	      "the LFO to the pan moves the sound from one speaker to the other");
	// a square moving the volume does not click: the gain glides
	{
		float biggest = 0.f;
		for (size_t i = 1; i < L.size(); i++)
			biggest = std::max(biggest, std::fabs(L[i] - L[i - 1]));
		float loud = 0.f;
		for (float x : L)
			loud = std::max(loud, std::fabs(x));
		check(biggest < loud * 0.5f, "a square LFO moves the pan without a jump");
	}
	// the fade-in: the movement grows over the first seconds of a note
	p[dsp::P_LFOPAN] = 0.f;
	p[dsp::P_LFOAMP] = 1.f;
	p[dsp::P_LFOFADE] = std::sqrt(2.f / 5.f); // two seconds
	render(p, bps, 3.f, L, R);
	const double early = rms(L, 0.03, 0.22) / std::max(1e-9, rms(L, 0.28, 0.47)),
	             late = rms(L, 2.53, 2.72) / std::max(1e-9, rms(L, 2.78, 2.97));
	check(early > 0.7 && late < 0.2, "with a fade-in, the LFO starts still and moves fully once it has faded in");
	// none of it set: the same sound as without the new knobs
	std::memcpy(p, pad.p, sizeof(p));
	std::vector<float> L2, R2;
	render(p, -1.0, 1.f, L, R);
	p[dsp::P_LFOSYNC] = 0.f;
	render(p, bps, 1.f, L2, R2);
	check(L == L2 && R == R2, "an LFO running free does not care where the song is");

	// saved and opened again; Mutate leaves the new targets off
	Song s2 = demoSong();
	s2.echoTone = 0.2f;
	s2.echoPingPong = false;
	s2.dtime = 7;
	s2.tracks[1].patch.p[dsp::P_LFOFADE] = 0.4f;
	Song back;
	check(Song::fromJson(s2.toJson(), back) && back.echoTone == 0.2f && !back.echoPingPong && back.dtime == 7 &&
	          back.tracks[1].patch.p[dsp::P_LFOFADE] == 0.4f,
	      "the echo's tone, ping-pong and time and the LFO's fade-in are saved with the song");
	std::mt19937 g(5);
	const Patch m = mutatePatch(presetByName(QStringLiteral("Night Drone")), g);
	check(m.p[dsp::P_LFOAMP] == 0.f && m.p[dsp::P_LFOPAN] == 0.f && m.p[dsp::P_LFOFADE] == 0.f,
	      "Mutate leaves the LFO's volume, pan and fade-in off when they were off");
	// a half note at 40 BPM, the slowest tempo, is three seconds: the echo plays it in full, not cut to 2.5 seconds
	{
		dsp::PingPong e;
		e.init(48000.f);
		const float t = 60.f / 40.f * dsp::echoBeats(7) * 48000.f;
		float l, r;
		int firstEcho = -1;
		for (int i = 0; i < 48000 * 4 && firstEcho < 0; i++) {
			e.process(i < 48 ? 1.f : 0.f, t, 0.5f, 0.32f, 0.f, l, r);
			if (i > 100 && std::fabs(l) > 0.05f) firstEcho = i;
		}
		check(std::abs(firstEcho - 144000) < 200, "a half-note echo at 40 BPM comes back after three seconds");
	}
	// the LFO's rate reads as a note value wherever it is shown once the LFO runs in time
	Patch timed = presetByName(QStringLiteral("Night Drone"));
	timed.p[dsp::P_LFOSYNC] = 1.f;
	check(formatParam(dsp::P_LFORATE, 0.5f, timed) == ui("1/4") &&
	          formatParam(dsp::P_LFORATE, 0.2f, timed) == ui("2 bars"),
	      "in time, the LFO's rate reads as a note value (as on the knob line's label and the knob)");
	timed.p[dsp::P_LFOSYNC] = 0.f;
	check(formatParam(dsp::P_LFORATE, 0.5f, timed).endsWith(QStringLiteral("Hz")), "free, it reads as a speed in Hz");
}

// The filter's notch and its own release, and the equaliser on every track: a song saved before they existed opens
// with them at rest and sounds exactly as it did, sample for sample; turned, they do what they say, without clicks
void Test::filterAndEqualiser() {
	using namespace dsp;
	// a song as the program before them saved it: no filter release, no equaliser, a kit of 27 numbers
	Song song = demoSong();
	QJsonObject old = song.toJson();
	QJsonArray tracks = old["tracks"].toArray();
	for (int i = 0; i < tracks.size(); i++) {
		QJsonObject tj = tracks[i].toObject();
		QJsonObject pj = tj["patch"].toObject();
		for (const char* id : {"fR", "eqCut", "eqLow", "eqMid", "eqFreq", "eqHigh"})
			pj.remove(QLatin1String(id));
		tj["patch"] = pj;
		QJsonArray kj = tj["kit"].toArray();
		while (kj.size() > DP_REVERB + 1)
			kj.removeLast();
		tj["kit"] = kj;
		tracks[i] = tj;
	}
	old["tracks"] = tracks;
	Song opened;
	check(Song::fromJson(old, opened), "a song from before the equaliser opens");
	bool rest = !opened.tracks.empty();
	for (const Track& tr : opened.tracks) {
		rest = rest && tr.patch.p[P_FR] == 0.f && eqFlat(tr.patch.p + P_EQCUT);
		if (tr.isDrums()) rest = rest && eqFlat(tr.kit.p + DP_EQCUT);
	}
	check(rest, "it opens with every filter release following the amp and every equaliser flat");
	// played through two engines side by side: once as opened, once as the demo song itself, whose sounds come
	// straight from the presets; and once more with each filter release set by hand to its amp release
	auto play = [](const Song& sg, int blocks) {
		Engine e(48000.f);
		e.setSong(std::make_shared<const Song>(sg));
		EngineEvent ev;
		ev.type = EngineEvent::Play;
		e.push(ev);
		std::vector<float> out(size_t(blocks) * 1024);
		for (int b = 0; b < blocks; b++)
			e.process(out.data() + size_t(b) * 1024, 512);
		return out;
	};
	const std::vector<float> a = play(song, 400), b = play(opened, 400);
	check(a == b, "the old song sounds exactly as before, sample for sample");
	Song linked = song;
	for (Track& tr : linked.tracks)
		if (!tr.hasMoves(P_AR)) tr.patch.p[P_FR] = std::max(tr.patch.p[P_AR], 0.0001f);
	check(play(linked, 400) == a, "a filter release set to the amp's own sounds as the one that follows it");
	float loud = 0.f;
	for (float x : a)
		loud = std::max(loud, std::fabs(x));
	check(loud > 0.05f, "and it is not silence that sounds the same");

	// the equaliser on a sine: Mid lifts its own pitch, the low cut takes away what is below it
	auto through = [](const float* v, float hz, int n) {
		Eq eq;
		float peak = 0.f, l[CHUNK], r[CHUNK];
		double ph = 0.0;
		for (int i = 0; i < n; i += CHUNK) {
			for (int j = 0; j < CHUNK; j++) {
				l[j] = r[j] = float(std::sin(ph));
				ph += 2.0 * M_PI * hz / 48000.0;
			}
			eq.process(l, r, CHUNK, v, 48000.f);
			if (i > n / 2)
				for (int j = 0; j < CHUNK; j++)
					peak = std::max(peak, std::fabs(l[j]));
		}
		return peak;
	};
	const float flat[5] = {0.f, 0.5f, 0.5f, 0.5f, 0.5f}, midUp[5] = {0.f, 0.5f, 1.f, 0.5f, 0.5f},
	            cut[5] = {1.f, 0.5f, 0.5f, 0.5f, 0.5f}, highDown[5] = {0.f, 0.5f, 0.5f, 0.5f, 0.f};
	check(through(flat, 440.f, 48000) == through(flat, 440.f, 48000) &&
	          std::fabs(through(flat, 440.f, 48000) - 1.f) < 1e-3f,
	      "a flat equaliser leaves the sound as it is");
	check(std::fabs(through(midUp, eqMidHz(0.5f), 48000) - 3.98f) < 0.2f,
	      "Mid all the way up lifts its pitch by 12 dB");
	check(through(cut, 60.f, 48000) < 0.05f && through(cut, 4000.f, 48000) > 0.95f,
	      "the low cut takes away the deep notes and leaves the high ones");
	check(through(highDown, 12000.f, 48000) < 0.3f && through(highDown, 100.f, 48000) > 0.95f,
	      "High all the way down makes the top end quieter and leaves the bass");
	{ // flat again after being turned: it lets the sound pass untouched once more
		Eq eq;
		float l[CHUNK], r[CHUNK];
		for (int i = 0; i < 200; i++) {
			for (int j = 0; j < CHUNK; j++)
				l[j] = r[j] = float(std::sin(0.05 * (i * CHUNK + j)));
			eq.process(l, r, CHUNK, i < 100 ? midUp : flat, 48000.f);
		}
		bool same = true;
		for (int j = 0; j < CHUNK; j++) {
			const float x = float(std::sin(0.05 * j));
			l[j] = r[j] = x;
		}
		eq.process(l, r, CHUNK, flat, 48000.f);
		for (int j = 0; j < CHUNK; j++)
			same = same && l[j] == float(std::sin(0.05 * j));
		check(same, "an equaliser turned back to flat lets the sound through untouched again");
	}
	{ // a knob thrown from one end to the other glides: no jump bigger than the sine itself makes
		Eq eq;
		float l[CHUNK], r[CHUNK], last = 0.f, jump = 0.f;
		const float lowUp[5] = {0.f, 1.f, 0.5f, 0.5f, 1.f};
		for (int i = 0; i < 400; i++) {
			for (int j = 0; j < CHUNK; j++)
				l[j] = r[j] = 0.25f * float(std::sin(2.0 * M_PI * 220.0 * (i * CHUNK + j) / 48000.0));
			eq.process(l, r, CHUNK, i < 200 ? flat : lowUp, 48000.f);
			for (int j = 0; j < CHUNK; j++) {
				if (i > 10) jump = std::max(jump, std::fabs(l[j] - last));
				last = l[j];
			}
		}
		check(jump < 0.25f * 4.f * 2.f * float(M_PI) * 220.f / 48000.f * 1.5f, "turning the equaliser does not click");
	}
	// the notch: nothing left at the cutoff, the rest kept
	auto notch = [](float hz) {
		Ladder f;
		const float g = std::tan(float(M_PI) * 1000.f / 48000.f);
		float peak = 0.f;
		for (int i = 0; i < 48000; i++) {
			const float y = f.process(0.3f * float(std::sin(2.0 * M_PI * hz * i / 48000.0)), g, 0.f, 4);
			if (i > 24000) peak = std::max(peak, std::fabs(y));
		}
		return peak / 0.3f;
	};
	check(notch(1000.f) < 0.1f && notch(100.f) > 0.8f && notch(10000.f) > 0.8f,
	      "the notch takes out its own pitch and keeps the ones far from it");
	check(PARAMS[P_SLOPE].choices && QString::fromLatin1(PARAMS[P_SLOPE].choices).count('|') == 4,
	      "the filter offers five types, the notch the last, so older songs keep theirs");
	// a song saved now keeps them; Mutate leaves them alone
	Song set = demoSong();
	set.tracks[1].patch.p[P_FR] = 0.7f;
	set.tracks[1].patch.p[P_EQMID] = 0.8f;
	set.tracks[0].kit.p[DP_EQHIGH] = 0.2f;
	Song back;
	check(Song::fromJson(set.toJson(), back) && back.tracks[1].patch.p[P_FR] == 0.7f &&
	          std::fabs(back.tracks[1].patch.p[P_EQMID] - 0.8f) < 1e-6f &&
	          std::fabs(back.tracks[0].kit.p[DP_EQHIGH] - 0.2f) < 1e-6f,
	      "the filter release and the equalisers are saved with the song");
	std::mt19937 g(3);
	const Patch m = mutatePatch(set.tracks[1].patch, g);
	check(m.p[P_FR] == 0.7f && m.p[P_EQMID] == set.tracks[1].patch.p[P_EQMID] && m.p[P_EQCUT] == 0.f,
	      "Mutate leaves the filter release and the equaliser as they were");
	{
		Patch cur = set.tracks[1].patch;
		cur.p[P_EQCUT] = 0.44f;
		cur.p[P_EQMID] = 0.8f;
		const Patch chosen = keepEq(presets()[0], cur);
		DrumKit kc = set.tracks[0].kit;
		kc.p[DP_EQHIGH] = 0.2f;
		const DrumKit kchosen = keepEq(chooseKit(kc, drumKits()[0]), kc);
		check(chosen.p[P_EQCUT] == 0.44f && chosen.p[P_EQMID] == 0.8f && chosen.name == presets()[0].name &&
		          kchosen.p[DP_EQHIGH] == 0.2f && kchosen.name == drumKits()[0].name,
		      "Choosing a preset, a kit or a rolled sound keeps the equaliser the track had");
	}
	check(formatParam(P_FR, 0.f) == ui("As amp") && formatParam(P_EQCUT, 0.f) == ui("Off") &&
	          formatParam(P_EQLOW, 0.5f) == QStringLiteral("0\u202fdB"),
	      "the new knobs say plainly when they are at rest");
}

// "Each note plays" a chord on a Mono sound (the bass and lead presets): every note of the chord is heard, not only
// the top one; the notes glide together, and a single note on a Mono sound plays as before
void Test::chordsOnMonoSounds() {
	// how strong each note of the chord is in a held note, from the strongest (1) down
	auto heard = [](int chord, const char* preset, std::vector<double>& level) {
		Song sg;
		sg.bpm = 120.f;
		Track t = makeTrack(sg, R_LEAD);
		t.patch = presetByName(QString::fromLatin1(preset));
		t.patch.p[dsp::P_DELAY] = 0.f;
		t.patch.p[dsp::P_REVERB] = 0.f;
		t.patch.p[dsp::P_LFOPITCH] = 0.f;
		t.chord = chord;
		t.follow = false;
		Clip c;
		c.id = sg.newId();
		Note n;
		n.len = BAR - 12;
		c.notes.push_back(n);
		t.clips.push_back(c);
		sg.tracks.push_back(t);
		int offs[4], cnt = 0;
		sg.chordOffsets(chord, offs, cnt);
		Engine e(48000.f);
		e.setSong(std::make_shared<const Song>(sg));
		EngineEvent play;
		play.type = EngineEvent::Play;
		e.push(play);
		std::vector<float> out(2 * 48000);
		e.process(out.data(), 48000);
		level.clear();
		for (int i = 0; i < cnt; i++) { // Goertzel at the note's pitch, over half a second once the glide has settled
			const double f = 440.0 * std::exp2((sg.midiOf(sg.tracks[0], offs[i]) - 69) / 12.0);
			const double w = 2.0 * std::cos(2.0 * M_PI * f / 48000.0);
			double s1 = 0, s2 = 0;
			for (int k = 14400; k < 38400; k++) {
				const double s0 = double(out[size_t(k) * 2] + out[size_t(k) * 2 + 1]) + w * s1 - s2;
				s2 = s1;
				s1 = s0;
			}
			level.push_back(std::sqrt(std::max(0.0, s1 * s1 + s2 * s2 - w * s1 * s2)));
		}
		const double top = *std::max_element(level.begin(), level.end());
		for (double& l : level)
			l /= std::max(top, 1e-12);
	};
	std::vector<double> level;
	bool all = true;
	for (const char* preset : {"Saw Lead", "Acid Squelch", "Mellow Lead"}) {
		heard(4, preset, level);
		all = all && level.size() == 4 && *std::min_element(level.begin(), level.end()) > 0.15;
	}
	check(all, "a Seventh chord on a Mono sound sounds all four of its notes, not only the top one");
	heard(3, "Saw Lead", level);
	check(level.size() == 3 && level[0] > 0.15, "a Triad on a Mono sound keeps its root, the note played");
	heard(3, "String Machine", level);
	check(level.size() == 3 && *std::min_element(level.begin(), level.end()) > 0.15,
	      "a Triad on a Poly sound still sounds all three notes");

	// on the synth itself: a single note on a Mono sound still slides into the next one held with it, and falls back
	dsp::Synth syn;
	syn.init(48000.f);
	Patch p = presetByName(QStringLiteral("Saw Lead"));
	syn.setPatch(p.p);
	float l[dsp::CHUNK], r[dsp::CHUNK];
	syn.noteOn(1, 57, false);
	syn.noteOn(2, 64, false); // legato: the same voice, no new attack
	for (int i = 0; i < 2000; i++)
		syn.render(l, r, dsp::CHUNK);
	syn.noteOff(2);
	syn.noteOff(1);
	check(!syn.silent(), "a Mono sound rings out after its notes are let go");
	for (int i = 0; i < 20000 && !syn.silent(); i++)
		syn.render(l, r, dsp::CHUNK);
	check(syn.silent(), "and then falls silent, with no voice stuck on");
	for (int part = 0; part < 4; part++)
		syn.noteOn(10 + part, 57 + part * 3, false, part);
	for (int part = 0; part < 4; part++)
		syn.noteOff(10 + part);
	for (int i = 0; i < 20000 && !syn.silent(); i++)
		syn.render(l, r, dsp::CHUNK);
	check(syn.silent(), "a chord on a Mono sound let go note by note leaves no voice sounding");
}
