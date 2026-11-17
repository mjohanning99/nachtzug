// The App: the one place the song is changed. It owns the song, the undo history, the selection, the clipboard,
// recording, the loop grid's launching, the computer keyboard as an instrument, and the sound card.
//
// What's here: begin(), edit(), onBar() and tick() (changes held for the bar line), undo(), redo(), save(), load();
// the transport (play, stop, seek); notes from the keys, MIDI and Chords mode (keyDown(), midiNote(), playChord());
// knob moves written as lines (paramPress/Move/Release()); the clip commands (split, copy, paste, delete, loop);
// the loop grid (launch(), launchScene(), cellToTimeline()); the sound card, the live input and recording a sample
// (openAudio(), startLiveInput(), startCapture()). Every panel calls it; publish() hands the engine a new snapshot.
// Changes in the song file: none here (songfile.cpp); save() and load() write and read the file through it.

#include "app.h"
#include "samples.h"
#include "midi.h"
#include <QFile>
#include <QJsonDocument>
#include <QSettings>
#include <climits>
#include <cmath>
#include <cstring>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#ifndef SDL_HINT_AUDIODRIVER
#define SDL_HINT_AUDIODRIVER "SDL_AUDIODRIVER" // older SDL 2 releases: the same name works as a hint
#endif

App::App(bool openAudio, QObject* parent) : QObject(parent) {
	handDriven_ = !openAudio; // tests and pictures run the engine themselves
	clock_.start();
	engine = new Engine(48000.f);
	song = demoSong();
	fixSelection();
	publish();
	if (openAudio) {
		SDL_SetMainReady();
		QSettings st;
		AudioSetup s;
		s.driver = st.value(QStringLiteral("audio/driver")).toString();
		s.device = st.value(QStringLiteral("audio/device")).toString();
		s.rate = st.value(QStringLiteral("audio/rate"), 48000).toInt();
		s.buffer = st.value(QStringLiteral("audio/buffer"), 256).toInt();
		if (!this->openAudio(s) && (!s.driver.isEmpty() || !s.device.isEmpty())) {
			// the saved sound card is gone (unplugged, another system): fall back to the default one
			const QString first = audioError;
			if (this->openAudio(AudioSetup{}))
				emit status(
				    tr("The chosen sound output could not be opened (%1), so the default one is used.").arg(first));
		}
	}
	connect(&timer_, &QTimer::timeout, this, &App::tick);
	timer_.start(16);
}

App::~App() {
	delete midi;
	midi = nullptr; // its clock thread reads the engine: it goes first
	if (liveDev) SDL_CloseAudioDevice(liveDev);
	if (capDev) SDL_CloseAudioDevice(capDev);
	if (audioDev) SDL_CloseAudioDevice(audioDev);
	if (sdlUp) SDL_Quit();
	delete engine;
}

void App::audioCallback(void* user, unsigned char* stream, int len) {
	static_cast<App*>(user)->engine->process(reinterpret_cast<float*>(stream), len / int(sizeof(float) * 2));
}

// ---------------------------------------------------------------- sound card

QStringList App::audioDrivers() {
	QStringList out;
	for (int i = 0; i < SDL_GetNumAudioDrivers(); i++) {
		const QString d = QString::fromUtf8(SDL_GetAudioDriver(i));
		if (d == QLatin1String("dummy") || d == QLatin1String("disk")) continue; // these make no sound
#if defined(Q_OS_LINUX)
		if (d == QLatin1String("sndio") || d == QLatin1String("dsp"))
			continue; // BSD and old OSS systems: not there on a modern Linux
#endif
		out << d;
	}
	return out;
}

QString App::audioDriver() const {
	const char* d = sdlUp ? SDL_GetCurrentAudioDriver() : nullptr;
	return d ? QString::fromUtf8(d) : QString();
}

QStringList App::audioDevices(bool input) const {
	QStringList out;
	if (!sdlUp) return out;
	const int n = SDL_GetNumAudioDevices(input ? 1 : 0);
	for (int i = 0; i < n; i++)
		if (const char* name = SDL_GetAudioDeviceName(i, input ? 1 : 0)) out << QString::fromUtf8(name);
	return out;
}

bool App::openAudio(const AudioSetup& s) {
	// where the music was, so it can carry on after the switch
	const double where = engine->pos.load();
	const bool wasPlaying = engine->playing.load(), click = engine->metronome.load();
	if (capDev) {
		SDL_CloseAudioDevice(capDev);
		capDev = 0;
	}
	const bool wasLive = liveDev != 0;
	stopLiveInput();
	if (audioDev) {
		SDL_CloseAudioDevice(audioDev);
		audioDev = 0;
	} // waits until the audio thread has left the engine
	audioError.clear();
	// a different sound system (driver) means starting SDL's audio part again
	if (!sdlUp || s.driver != setup_.driver) {
		if (sdlUp) {
			SDL_QuitSubSystem(SDL_INIT_AUDIO);
			sdlUp = false;
		}
		SDL_SetHint(SDL_HINT_AUDIODRIVER, s.driver.toUtf8().constData()); // empty: SDL picks the best one
		if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0)
			sdlUp = true;
		else {
			audioError = QString::fromUtf8(SDL_GetError());
			if (!s.driver.isEmpty()) {
				SDL_SetHint(SDL_HINT_AUDIODRIVER, "");
				sdlUp = SDL_InitSubSystem(SDL_INIT_AUDIO) == 0;
			}
		}
	}
	setup_ = s;
	if (!sdlUp) {
		if (audioError.isEmpty()) audioError = tr("The sound system could not be started.");
		return false;
	}
	SDL_AudioSpec want{}, have{};
	want.freq = s.rate > 0 ? s.rate : 48000;
	want.format = AUDIO_F32SYS;
	want.channels = 2;
	want.samples = Uint16(std::clamp(s.buffer, 32, 4096));
	want.callback = &App::audioCallback;
	want.userdata = this;
	const QByteArray dev = s.device.toUtf8();
	audioDev = SDL_OpenAudioDevice(s.device.isEmpty() ? nullptr : dev.constData(), 0, &want, &have,
	                               SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
	if (!audioDev) {
		audioError = QString::fromUtf8(SDL_GetError());
		if (audioError.isEmpty()) audioError = tr("The sound output could not be opened.");
		return false;
	}
	gotBuffer_ = have.samples;
	if (int(engine->sampleRate()) != have.freq) { // the engine is built for one sample rate: make a new one
		emit engineReplacing();
		delete engine;
		engine = new Engine(float(have.freq));
		engine->metronome.store(click);
		engine->liveChord.store(liveChord_); // a chord held in Chords mode carries on
		engine->liveChordAlt.store(liveChordAlt_);
		publish();
		EngineEvent e;
		e.type = EngineEvent::Seek;
		e.tick = where;
		engine->push(e);
		engine->pos.store(where);
		if (wasPlaying) {
			EngineEvent p;
			p.type = EngineEvent::Play;
			p.tick = where;
			engine->push(p);
		}
		emit engineReplaced();
	}
	audioError.clear();
	engine->previewReq.store(nullptr);
	engine->phraseReq.store(nullptr); // nothing left waiting from before
	SDL_PauseAudioDevice(audioDev, 0);
	if (wasLive) {
		QString err;
		startLiveInput(err);
	} // the input carries on at the new setting
	return true;
}

void App::previewSample(const QString& name) {
	auto d = samples::get(name);
	if (!d || !enginePlays()) {
		engine->stopPreview.store(true);
		return;
	}
	previewKeep_[previewIdx_++ % 4] = d;
	engine->previewReq.store(d.get(), std::memory_order_release);
}

void App::previewData(std::shared_ptr<const dsp::SampleData> d) {
	if (!d || !enginePlays()) {
		engine->stopPreview.store(true);
		return;
	}
	previewKeep_[previewIdx_++ % 4] = d;
	engine->previewReq.store(d.get(), std::memory_order_release);
}

int App::chopOntoDrums(const dsp::SampleData& src, const std::vector<size_t>& cutsIn, const QString& name, int bars,
                       bool pattern, QString& error) {
	std::vector<size_t> cuts = cutsIn;
	std::sort(cuts.begin(), cuts.end());
	cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
	if (cuts.empty() || src.frames() == 0) {
		error = tr("There is nothing to chop.");
		return -1;
	}
	if (int(cuts.size()) > dsp::D_COUNT) cuts.resize(dsp::D_COUNT);
	// the drum track: the selected one, or the first, or a new one (made below, inside the same undo step)
	int tid = -1;
	if (const Track* t = curTrack(); t && t->isDrums()) tid = t->id;
	for (const Track& t : song.tracks)
		if (tid < 0 && t.isDrums()) tid = t.id;
	if (tid < 0 && int(song.tracks.size()) >= MAX_TRACKS) {
		error = tr("There is no room for a drum track: the song already has %1 tracks.").arg(MAX_TRACKS);
		return -1;
	}
	// the slices go into the sample library, so the song finds them again
	QStringList names;
	const QString base = name.trimmed().isEmpty() ? tr("Loop") : name.trimmed();
	for (size_t i = 0; i < cuts.size(); i++) {
		const dsp::SampleData piece = samples::slice(src, cuts[i], i + 1 < cuts.size() ? cuts[i + 1] : src.frames());
		const QString saved =
		    samples::saveSound(QStringLiteral("%1 %2").arg(base).arg(i + 1), piece.l, piece.r, int(piece.rate), error);
		if (saved.isEmpty()) {
			for (const QString& n : names)
				samples::remove(n);
			return -1;
		}
		names << saved;
	}
	begin(tr("Chop a loop"), true);
	if (tid < 0) { // a drum track of its own, without the pattern a new track usually gets
		Track nt = makeTrack(song, R_DRUMS);
		tid = nt.id;
		song.tracks.push_back(nt);
	}
	Track* t = song.track(tid);
	for (int d = 0; d < int(names.size()); d++) {
		t->kit.sample[d] = names[d];
		t->kit.p[d * 3] = 0.85f;
		t->kit.p[d * 3 + 1] = 0.5f;
		t->kit.p[d * 3 + 2] = 1.f; // as loud as it was, at its own pitch, whole
	}
	t->kit.name = base;
	if (pattern) {
		const int len = std::clamp(bars, 1, 8) * BAR;
		Clip c;
		c.id = song.newId();
		c.start = song.loopOn && !song.liveMode ? song.loopStart : 0;
		c.len = c.loopLen = len;
		c.name = base;
		for (size_t i = 0; i < cuts.size(); i++) {
			Note n;
			n.deg = int(i);
			n.len = 12;
			n.start =
			    std::clamp(int(std::lround(double(cuts[i]) / double(src.frames()) * len / 24.0)) * 24, 0, len - 24);
			c.notes.push_back(n);
		}
		// the old drum clips under the new one would play on top of it
		std::erase_if(t->clips, [&](const Clip& k) { return k.start < c.start + c.len && c.start < k.end(); });
		t->clips.push_back(c);
		editClip = c.id;
		selClips = {c.id};
	}
	selTrack = tid;
	touch();
	emit selectionChanged();
	return tid;
}

void App::previewSound(const Patch* patch, const DrumKit* kit) {
	if ((!patch && !kit) || !enginePlays()) {
		engine->stopPhrase.store(true);
		return;
	}
	auto ph = std::make_shared<Phrase>();
	const double step = 60.0 / song.bpm / 4.0; // a sixteenth, in seconds
	auto at = [&](double s16) { return float(s16 * step); };
	if (kit) {
		ph->drums = true;
		std::memcpy(ph->kit, kit->p, sizeof(ph->kit));
		for (int d = 0; d < dsp::D_COUNT; d++)
			if (!kit->sample[d].isEmpty()) ph->samples[d] = samples::get(kit->sample[d]);
		// a bar of a plain beat that lets every drum of the kit be heard once
		const std::pair<int, int> hits[] = {{0, dsp::D_KICK},  {0, dsp::D_CYM},   {2, dsp::D_CHAT},   {4, dsp::D_KICK},
		                                    {4, dsp::D_CLAP},  {6, dsp::D_CHAT},  {7, dsp::D_RIM},    {8, dsp::D_KICK},
		                                    {10, dsp::D_CHAT}, {12, dsp::D_KICK}, {12, dsp::D_SNARE}, {13, dsp::D_TOM},
		                                    {14, dsp::D_OHAT}, {15, dsp::D_TOM}};
		for (auto [s16, d] : hits)
			ph->notes.push_back({at(s16), at(1), d, s16 % 4 == 0});
		ph->length = at(16);
	} else {
		std::memcpy(ph->patch, patch->p, sizeof(ph->patch));
		const Track* t = curTrack();
		Track tmp;
		tmp.role = t && !t->isDrums() ? t->role : R_SEQ;
		tmp.octave = t && !t->isDrums() ? t->octave : 1;
		const int n = song.sc().n;
		auto midi = [&](int deg) { return song.midiOf(tmp, deg); };
		auto note = [&](double s16, double len16, int deg, bool acc = false) {
			ph->notes.push_back({at(s16), at(len16), midi(deg), acc});
		};
		switch (tmp.role) {
		case R_BASS: { // an acid line: accents, and slides where a note runs into the next
			const int line[16] = {0, 0, n, 0,         song.q(4), 0,         song.q(2), n,
			                      0, 0, n, song.q(6), 0,         song.q(4), song.q(2), 0};
			const bool acc[16] = {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0},
			           sld[16] = {0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0};
			for (int i = 0; i < 16; i++)
				note(i, sld[i] ? 1.15 : 0.7, line[i], acc[i]);
			break;
		}
		case R_SEQ: { // a Berlin School run up and down the chord
			const int run[16] = {0, song.q(2), song.q(4), n, n + song.q(2), n, song.q(4), song.q(2),
			                     0, song.q(2), song.q(4), n, n + song.q(4), n, song.q(4), song.q(2)};
			for (int i = 0; i < 16; i++)
				note(i, 0.6, run[i], i % 4 == 0);
			break;
		}
		case R_PAD: // the chord, held
			for (int d : {0, song.q(2), song.q(4), n})
				note(0, 13, d);
			break;
		default: { // a few notes of a tune
			const int tune[5] = {song.q(4), n, n + song.q(1), n + song.q(2), n};
			const double when[5] = {0, 2, 4, 6, 10}, len[5] = {1.8, 1.8, 1.8, 3.6, 5};
			for (int i = 0; i < 5; i++)
				note(when[i], len[i], tune[i], i == 3);
			break;
		}
		}
		std::sort(ph->notes.begin(), ph->notes.end(),
		          [](const PhraseNote& a, const PhraseNote& b) { return a.at < b.at; });
		ph->length = at(16);
	}
	phraseKeep_[phraseIdx_++ % 4] = ph;
	engine->phraseReq.store(ph.get(), std::memory_order_release);
}

// ---------------------------------------------------------------- the input, live

void App::liveCallback(void* user, unsigned char* stream, int len) {
	static_cast<App*>(user)->engine->pushInput(reinterpret_cast<const float*>(stream), len / int(sizeof(float)));
}

bool App::startLiveInput(QString& error) {
	stopLiveInput();
	if (!sdlUp || !audioDev) {
		error = tr("The sound card is not running.");
		return false;
	}
	SDL_AudioSpec want{}, have{};
	want.freq = audioRate();
	want.format = AUDIO_F32SYS;
	want.channels = 1;
	want.samples = Uint16(std::clamp(gotBuffer_, 64, 1024)); // as small as the output's, for a short delay
	want.callback = &App::liveCallback;
	want.userdata = this;
	const QString device = QSettings().value(QStringLiteral("audio/input")).toString();
	const QByteArray dev = device.toUtf8();
	liveDev = SDL_OpenAudioDevice(device.isEmpty() ? nullptr : dev.constData(), 1, &want, &have,
	                              0); // at the output's rate: SDL converts if it must
	if (!liveDev) {
		error = QString::fromUtf8(SDL_GetError());
		if (error.isEmpty()) error = tr("The input could not be opened.");
		return false;
	}
	engine->inputOn.store(true);
	SDL_PauseAudioDevice(liveDev, 0);
	return true;
}

void App::stopLiveInput() {
	engine->inputOn.store(false);
	engine->inputPeak.store(0.f);
	if (liveDev) {
		SDL_CloseAudioDevice(liveDev);
		liveDev = 0;
	}
}

// ---------------------------------------------------------------- recording from an input

void App::captureCallback(void* user, unsigned char* stream, int len) {
	App* a = static_cast<App*>(user);
	const float* in = reinterpret_cast<const float*>(stream);
	const int n = len / int(sizeof(float));
	size_t at = a->capLen_.load(std::memory_order_relaxed);
	float pk = a->capLevel_.load(std::memory_order_relaxed) * 0.9f;
	for (int i = 0; i < n; i++) {
		pk = std::max(pk, std::fabs(in[i]));
		if (at < a->capMax_) a->capBuf_[at++] = in[i];
	}
	a->capLevel_.store(pk, std::memory_order_relaxed);
	a->capLen_.store(at, std::memory_order_release);
}

bool App::startCapture(const QString& device, QString& error) {
	if (capDev) {
		SDL_CloseAudioDevice(capDev);
		capDev = 0;
	}
	liveWasOn = liveWasOn || liveDev != 0;
	if (liveDev) stopLiveInput(); // one use of the input at a time: some systems allow no more
	if (!sdlUp) {
		error = tr("The sound system is not running.");
		return false;
	}
	capMax_ = size_t(96000) * 120; // up to two minutes, at any rate up to 96 kHz
	capBuf_.assign(capMax_, 0.f);
	capLen_.store(0);
	capLevel_.store(0.f);
	SDL_AudioSpec want{}, have{};
	want.freq = audioRate();
	want.format = AUDIO_F32SYS;
	want.channels = 1;
	want.samples = 512;
	want.callback = &App::captureCallback;
	want.userdata = this;
	const QByteArray dev = device.toUtf8();
	capDev = SDL_OpenAudioDevice(device.isEmpty() ? nullptr : dev.constData(), 1, &want, &have,
	                             SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
	if (!capDev) {
		error = QString::fromUtf8(SDL_GetError());
		capBuf_.clear();
		return false;
	}
	capRate_ = have.freq;
	SDL_PauseAudioDevice(capDev, 0);
	return true;
}

double App::captureSeconds() const {
	return capDev ? double(capLen_.load(std::memory_order_acquire)) / double(capRate_) : 0.0;
}

std::vector<float> App::stopCapture(int& rate) {
	if (capDev) {
		SDL_CloseAudioDevice(capDev);
		capDev = 0;
	} // the capture thread is finished after this
	rate = capRate_;
	std::vector<float> out(capBuf_.begin(), capBuf_.begin() + long(std::min(capLen_.load(), capBuf_.size())));
	capBuf_.clear();
	capBuf_.shrink_to_fit();
	capLen_.store(0);
	capLevel_.store(0.f);
	if (liveWasOn) {
		liveWasOn = false;
		QString err;
		startLiveInput(err);
	} // back to playing it live
	return out;
}

// ---------------------------------------------------------------- history

void App::publish() {
	resolveSamples(song);
	auto snap = std::make_shared<const Song>(song);
	keep_[keepIdx++ % 16] = snap; // the GUI thread, not the audio thread, frees old snapshots
	engine->setSong(std::move(snap));
}

void App::begin(const QString& what, bool announce) {
	if (!what.isEmpty()) editCount_[what]++;
	undo_.push_back(song);
	undoWhat_.push_back(what);
	if (undo_.size() > 200) {
		undo_.erase(undo_.begin());
		undoWhat_.erase(undoWhat_.begin());
	}
	redo_.clear();
	redoWhat_.clear();
	dirty = true;
	if (announce && !what.isEmpty()) emit announced(what);
}

void App::touch() {
	refreshAutoDrums(song);
	publish();
	emit changed();
}

void App::onBar(const std::function<void(Song&)>& f, bool undoable, const QString& what, bool announce) {
	if (!isPlaying()) {
		if (undoable) begin(what, announce);
		f(song);
		touch();
		return;
	}
	pending_.push_back({f, undoable, what, announce});
	emit status(tr("Change lands on the next bar"));
	emit changed();
}

// Runs every 16 ms: applies queued changes just before the bar line and keeps writing a held knob.
void App::tick() {
	if (!pending_.empty()) {
		bool now = !isPlaying();
		if (!now) {
			const double m = musicalPos(), toBar = BAR - std::fmod(m, double(BAR));
			now = toBar / (double(song.bpm) * PPQ / 60000.0) < 40.0;
		}
		if (now) {
			const auto list = std::move(pending_);
			pending_.clear();
			bool step = recording && song.liveMode, announce = false; // launches being recorded change the timeline
			QString what;
			for (const auto& p : list) {
				step |= p.undoable;
				if (!p.what.isEmpty()) what = p.what;
				announce |= p.announce;
			}
			if (step) begin(what, announce);
			for (const auto& p : list)
				p.f(song);
			fixSelection();
			touch();
		}
	}
	// clips being written by launched cells grow bar by bar, and close when recording or playing ends
	if (!recOpen_.isEmpty()) {
		const int end = (int(musicalPos()) / BAR + 1) * BAR;
		if (!isPlaying() || !recording || !song.liveMode) {
			for (auto it = recOpen_.begin(); it != recOpen_.end(); ++it)
				if (Clip* c = song.clip(it.value())) c->len = std::max(BAR, end - c->start);
			recOpen_.clear();
			touch();
		} else {
			bool grew = false;
			for (auto it = recOpen_.begin(); it != recOpen_.end(); ++it)
				if (Clip* c = song.clip(it.value()); c && c->end() < end) {
					c->len = end - c->start;
					grew = true;
				}
			if (grew) touch();
		}
	}
	if (writing && isPlaying() && std::abs(int(musicalPos()) - wLast) >= 6) {
		writePoint(wValue);
		publish();
	}
	// recording in Chords mode: the chord pressed goes into each bar the playhead reaches, until the next key, or
	// until the loop comes round to the start again
	if (chordWriting_ && liveChord_ != Engine::NO_CHORD && recording && isPlaying() && !song.liveMode) {
		const int bar = int(musicalPos()) / BAR;
		if (chordLastBar_ == INT_MIN && bar == chordFromBar_)
			chordLastBar_ = bar; // the loop came round to where this chord begins
		if (chordLastBar_ == INT_MIN) {
		} // waiting for it
		else if (bar < chordLastBar_)
			chordWriting_ = false; // the loop came round: this chord is written
		else {
			if (bar >= chordFromBar_ && (song.chordAt(bar) != liveChord_ || song.chordAltAt(bar) != liveChordAlt_)) {
				if (!chordTake_) {
					begin(tr("Record chords"));
					chordTake_ = true;
				}
				song.setChord(bar, liveChord_, liveChordAlt_);
				touch();
			}
			chordLastBar_ = std::max(chordLastBar_, bar);
		}
	} else if (!recording || !isPlaying()) {
		chordWriting_ = false;
		chordTake_ = false;
	}
	// without Loop, stop by itself a bar after the last clip
	if (isPlaying() && !song.liveMode && !song.loopOn && pos() > std::max(song.endTick(), BAR) + BAR) {
		stop();
		seek(0);
	}
}

void App::fixSelection() {
	if (!song.track(selTrack)) selTrack = song.tracks.empty() ? -1 : song.tracks[0].id;
	if (!song.clip(editClip)) editClip = -1;
	QSet<int> keep;
	for (int id : selClips)
		if (song.clip(id)) keep.insert(id);
	selClips = keep;
	if (editClip < 0) {
		if (Track* t = curTrack(); t && !t->clips.empty()) editClip = t->clips[0].id;
	}
}

void App::undo() {
	if (undo_.empty()) return;
	pending_.clear();
	queued_.clear();
	const QString what = undoWhat_.back();
	redo_.push_back(song);
	redoWhat_.push_back(what);
	song = undo_.back();
	undo_.pop_back();
	undoWhat_.pop_back();
	dirty = true;
	fixSelection();
	touch();
	emit selectionChanged();
	emit undone(what, false);
}

void App::redo() {
	if (redo_.empty()) return;
	pending_.clear();
	queued_.clear();
	const QString what = redoWhat_.back();
	undo_.push_back(song);
	undoWhat_.push_back(what);
	song = redo_.back();
	redo_.pop_back();
	redoWhat_.pop_back();
	dirty = true;
	fixSelection();
	touch();
	emit selectionChanged();
	emit undone(what, true);
}

void App::setSong(const Song& s, const QString& path) {
	stop();
	EngineEvent e;
	e.type = EngineEvent::Panic;
	engine->push(e);
	song = s;
	filePath = path;
	dirty = false;
	undo_.clear();
	redo_.clear();
	undoWhat_.clear();
	redoWhat_.clear();
	pending_.clear();
	queued_.clear();
	recOpen_.clear();
	writing = false;
	down_.clear();
	chordDown_.clear();
	releaseChord();
	selTrack = -1;
	editClip = -1;
	selClips.clear();
	fixSelection();
	seek(0);
	touch();
	emit selectionChanged();
}

bool App::save(const QString& path) {
	QFile f(path);
	if (!f.open(QIODevice::WriteOnly)) return false;
	f.write(QJsonDocument(song.toJson()).toJson(QJsonDocument::Compact));
	f.close();
	filePath = path;
	dirty = false;
	emit changed();
	return true;
}

bool App::load(const QString& path) {
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly)) return false;
	Song s;
	if (!Song::fromJson(QJsonDocument::fromJson(f.readAll()).object(), s)) return false;
	const QStringList missing = resolveSamples(s);
	setSong(s, path);
	if (!missing.isEmpty())
		emit status(tr("Some samples this song uses are not in the sample library: %1. Their drums play the built-in "
		               "sound instead.")
		                .arg(missing.join(QStringLiteral(", "))));
	return true;
}

// ---------------------------------------------------------------- selection

void App::selectTrack(int id) {
	if (selTrack == id) return;
	releaseAllKeys();
	selTrack = id;
	if (Track* t = curTrack()) { // show one of this track's patterns in the editor
		Track* owner = nullptr;
		if (!song.clip(editClip, &owner) || owner != t) {
			if (song.liveMode) {
				const Clip* c = t->cell(t->liveCell);
				editClip = c ? c->id : t->cells.empty() ? -1 : t->cells[0].id;
			} else
				editClip = t->clips.empty() ? -1 : t->clips[0].id;
		}
	}
	emit selectionChanged();
}

void App::selectClip(int id, bool add) {
	if (!add) selClips.clear();
	if (id >= 0) {
		if (add && selClips.contains(id))
			selClips.remove(id);
		else
			selClips.insert(id);
		Track* owner = nullptr;
		if (song.clip(id, &owner)) {
			editClip = id;
			if (owner && owner->id != selTrack) {
				releaseAllKeys();
				selTrack = owner->id;
			}
		}
	}
	emit selectionChanged();
}

void App::openClip(int id) {
	selectClip(id);
	focus = FocusEditor;
}

// ---------------------------------------------------------------- transport

void App::play() {
	double p = pos();
	// with Loop on, starting behind the loop would never come back to it: start at the loop instead
	if (song.loopOn && song.loopEnd > song.loopStart && p >= song.loopEnd) p = song.loopStart;
	EngineEvent e;
	e.type = EngineEvent::Play;
	e.tick = p;
	engine->push(e);
}
void App::stop() {
	EngineEvent e;
	e.type = EngineEvent::Stop;
	engine->push(e);
}
void App::togglePlay() {
	if (isPlaying())
		stop();
	else
		play();
}
void App::seek(double tick) {
	EngineEvent e;
	e.type = EngineEvent::Seek;
	e.tick = std::max(0.0, tick);
	engine->push(e);
	engine->pos.store(e.tick); // so the display moves even if no audio device is running
}

// ---------------------------------------------------------------- playing and recording notes

void App::keyDown(int code, int row, int col, bool bent) {
	if (chordKeys_) {
		if (chordDown_.contains(code)) return;
		chordDown_.insert(code);
		playChord(keyDegree(row, col, song.sc().n), bent ? -1 : 0);
		liveChordPad_ = row * 16 + col; // a few pads share a chord: only the one pressed lights up
		return;
	}
	Track* t = curTrack();
	if (!t) return;
	if (t->isDrums()) {
		pressNote(code, col % dsp::D_COUNT, false);
		return;
	}
	const int deg = keyDegree(row, col, song.sc().n);
	pressNote(code, deg, false, bent ? song.approachAlt(deg) : 0);
}

void App::pressNote(int code, int deg, bool acc, int alt) {
	Track* t = curTrack();
	if (!t || down_.contains(code)) return;
	if (t->isDrums()) alt = 0;
	const bool playingNow = isPlaying();
	EngineEvent e;
	e.type = EngineEvent::NoteOn;
	e.track = t->id;
	e.deg = deg;
	e.key = code;
	e.acc = acc;
	e.alt = alt;
	e.quant = inTime && playingNow ? std::min(grid, 48) : 0;
	engine->push(e);
	down_.insert(code, Down{t->id, deg, musicalPos(), clock_.elapsed(), recording && playingNow, acc, alt});
	if (alt)
		emit status(tr("Passing note: %1, leaning into %2").arg(song.noteName(*t, deg, alt), song.noteName(*t, deg)));
}

void App::midiNote(int note, int velocity, bool on) {
	const int code = 1000 + std::clamp(note, 0, 127); // apart from the letter keys and the pads
	if (!on || velocity == 0) {
		keyUp(code);
		return;
	}
	const bool acc = velocity >= 100;
	if (chordKeys_) { // the key's note, as a chord of the scale, counted from the key note around middle C
		if (chordDown_.contains(code)) return;
		chordDown_.insert(code);
		playChord(song.degreeNear(note - (48 + song.root)));
		return;
	}
	Track* t = curTrack();
	if (!t) return;
	if (t->isDrums()) {
		int drum;
		switch (note) {
		case 35:
		case 36:
			drum = dsp::D_KICK;
			break;
		case 38:
		case 40:
			drum = dsp::D_SNARE;
			break;
		case 39:
			drum = dsp::D_CLAP;
			break;
		case 42:
		case 44:
			drum = dsp::D_CHAT;
			break;
		case 46:
			drum = dsp::D_OHAT;
			break;
		case 41:
		case 43:
		case 45:
		case 47:
		case 48:
		case 50:
			drum = dsp::D_TOM;
			break;
		case 37:
			drum = dsp::D_RIM;
			break;
		case 49:
		case 51:
		case 52:
		case 55:
		case 57:
			drum = dsp::D_CYM;
			break;
		default:
			drum = ((note - 36) % dsp::D_COUNT + dsp::D_COUNT) %
			       dsp::D_COUNT; // a keyboard without drum pads: eight keys in a row
		}
		pressNote(code, drum, acc);
		return;
	}
	pressNote(code, std::clamp(song.degreeNear(note - song.midiOf(*t, 0)), DEG_MIN, DEG_MAX), acc);
}

void App::keyUp(int code) {
	if (chordDown_.remove(code)) return; // the chord stays
	auto it = down_.find(code);
	if (it == down_.end()) return;
	const Down d = it.value();
	down_.erase(it);
	EngineEvent e;
	e.type = EngineEvent::NoteOff;
	e.track = d.track;
	e.key = code;
	engine->push(e);
	if (d.rec) recordNote(d, clock_.elapsed() - d.ms);
}

void App::releaseAllKeys() {
	const QList<int> codes = down_.keys();
	for (int c : codes)
		keyUp(c);
	chordDown_.clear();
}

// ---------------------------------------------------------------- Chords mode

void App::setChordKeys(bool on) {
	if (on == chordKeys_) return;
	releaseAllKeys();
	chordKeys_ = on;
	if (!on) releaseChord();
	emit chordKeysChanged(on);
	emit changed();
}

void App::playChord(int deg, int alt) {
	deg = std::clamp(deg, -42, 42); // as far as a song file keeps
	alt = std::clamp(alt, -1, 1);
	liveChord_ = deg;
	liveChordAlt_ = alt;
	liveChordPad_ = -1;
	engine->liveChordAlt.store(alt);
	engine->liveChord.store(deg);
	if (recording && isPlaying() && !song.liveMode) {
		// written from this bar if the key came early in it, otherwise from the next one, until another key or the
		// loop coming round (so a chord pressed once does not paint over the whole loop on the next pass)
		const double m = musicalPos();
		const int bar = int(m) / BAR;
		chordFromBar_ = std::fmod(m, double(BAR)) < BAR / 2 ? bar : bar + 1;
		chordLastBar_ = bar;
		if (song.loopOn && chordFromBar_ * BAR >= song.loopEnd) {
			chordFromBar_ = song.loopStart / BAR;
			chordLastBar_ = INT_MIN;
		} // starts when the loop comes round
		chordWriting_ = true;
	} else
		chordWriting_ = false;
	emit status(alt ? tr("Chord: %1, leaning into %2").arg(song.chordName(deg, alt), song.chordName(deg))
	                : tr("Chord: %1").arg(song.chordName(deg)));
	emit changed();
}

void App::releaseChord() {
	if (liveChord_ == Engine::NO_CHORD) return;
	liveChord_ = Engine::NO_CHORD;
	liveChordAlt_ = 0;
	liveChordPad_ = -1;
	engine->liveChord.store(Engine::NO_CHORD);
	engine->liveChordAlt.store(0);
	chordTake_ = false;
	chordWriting_ = false;
	emit status(tr("Back to the song’s own chords"));
	emit changed();
}

void App::audition(int trackId, int deg, bool acc, int alt) {
	EngineEvent e;
	e.type = EngineEvent::Audition;
	e.track = trackId;
	e.deg = deg;
	e.acc = acc;
	e.alt = alt;
	engine->push(e);
}

void App::recordNote(const Down& d, qint64 durMs) {
	Track* t = song.track(d.track);
	if (!t) return;
	begin(tr("Record notes"));
	const int tick = int(d.tick);
	Clip* c = nullptr;
	for (Clip& k : t->clips)
		if (k.start <= tick && tick < k.end()) {
			c = &k;
			break;
		}
	if (!c) { // make a clip to record into: the loop if we are inside it, otherwise four bars from this bar
		Clip k;
		k.id = song.newId();
		if (song.loopOn && tick >= song.loopStart && tick < song.loopEnd) {
			k.start = song.loopStart;
			k.len = song.loopEnd - song.loopStart;
		} else {
			k.start = tick / BAR * BAR;
			k.len = 4 * BAR;
		}
		k.loopLen = k.len;
		t->clips.push_back(k);
		c = &t->clips.back();
	}
	c->ad.on = false; // playing into a drummer clip turns it into a normal one
	Note n;
	int local = (tick - c->start + c->offset) % c->loopLen;
	if (quantize || inTime) {
		const int g = std::min(grid, 48);
		local = int(std::lround(double(local) / g)) * g % c->loopLen;
	}
	n.start = local;
	n.len = t->isDrums() ? 12 : std::max(12, int(double(durMs) * double(song.bpm) * PPQ / 60000.0));
	n.len = std::max(3, std::min(n.len, c->loopLen - n.start)); // it ends where the pattern does, as drawn
	n.acc = d.acc;
	n.alt = t->isDrums() ? 0 : d.alt;
	n.deg = d.deg - (t->follow && !t->isDrums() ? song.chordAt(tick / BAR) : 0);
	if (!t->isDrums()) n.deg = std::clamp(n.deg, DEG_MIN, DEG_MAX);
	c->notes.push_back(n);
	editClip = c->id;
	touch();
}

// ---------------------------------------------------------------- knobs and recorded knob moves

void App::paramPress(int trackId, int param) {
	Track* t = song.track(trackId);
	if (!t) return;
	begin(paramLabel(param));
	wTrack = trackId;
	wParam = param;
	wValue = baseValue(*t, param);
	writing = recording && isPlaying() && (param == AP_VOLUME || PARAMS[param].kind != K_CHOICE);
	if (writing) {
		if (!t->lane(param)) {
			AutoLane l;
			l.param = param;
			t->lanes.push_back(l);
		}
		AutoLane* l = t->lane(param);
		if (l->pts.empty()) l->pts.push_back({0, wValue}); // before the first move the knob stays where it was
		t->bypass &= ~(quint64(1) << param);
		wLast = int(musicalPos());
	}
}

void App::writePoint(float v) {
	Track* t = song.track(wTrack);
	AutoLane* l = t ? t->lane(wParam) : nullptr;
	if (!l) return;
	const int now = int(musicalPos());
	int tick = now, last = wLast;
	if (l->loop > 0) {
		tick %= l->loop;
		last %= l->loop;
	} // a repeating line is written inside its own length
	auto& p = l->pts;
	// replace whatever was there between the last write and now
	if (tick >= last)
		std::erase_if(p, [&](const AutoPoint& a) { return a.tick > last && a.tick <= tick; });
	else
		std::erase_if(
		    p, [&](const AutoPoint& a) { return a.tick > last || (a.tick <= tick && a.tick > 0); }); // wrapped round
	std::erase_if(p, [&](const AutoPoint& a) { return a.tick == tick; });
	const auto at = std::upper_bound(p.begin(), p.end(), tick, [](int tk, const AutoPoint& a) { return tk < a.tick; });
	p.insert(at, AutoPoint{tick, v});
	wLast = now;
}

void App::paramMove(int trackId, int param, float v) {
	Track* t = song.track(trackId);
	if (!t) return;
	if (param == AP_VOLUME)
		t->vol = v;
	else
		t->patch.p[param] = v;
	wValue = v;
	if (writing && trackId == wTrack && param == wParam)
		writePoint(v);
	else if (t->hasMoves(param) && !((t->bypass >> param) & 1)) {
		t->bypass |= quint64(1) << param;
		emit status(tr("Recorded moves for %1 are paused while you set it by hand. “Resume lines” brings them back.")
		                .arg(paramLabel(param)));
	}
	touch();
}

void App::paramRelease() {
	if (writing) {
		writePoint(wValue);
		writing = false;
		touch();
	}
	wTrack = -1;
	wParam = -1;
}

float App::paramValue(const Track& t, int param) const {
	const float base = baseValue(t, param);
	if (t.id == wTrack && param == wParam) return base;
	return t.automated(param, musicalPos(), base);
}

void App::forgetMoves(int trackId, int param) {
	Track* t = song.track(trackId);
	if (!t) return;
	begin(tr("Delete line"), true);
	if (param < 0) {
		t->lanes.clear();
		t->bypass = 0;
	} else {
		std::erase_if(t->lanes, [&](const AutoLane& l) { return l.param == param; });
		t->bypass &= ~(quint64(1) << param);
	}
	touch();
}

void App::resumeMoves(int trackId) {
	if (Track* t = song.track(trackId)) {
		t->bypass = 0;
		touch();
	}
}

// ---------------------------------------------------------------- tracks and clips

int App::addTrack(int role) {
	if (int(song.tracks.size()) >= MAX_TRACKS) {
		emit status(tr("The song already has %1 tracks, which is the most it can hold.").arg(MAX_TRACKS));
		return -1;
	}
	begin(tr("Add track"));
	const int id = addTrackWithClip(song, role, rng);
	selTrack = id;
	if (Track* t = song.track(id); t && !t->clips.empty()) {
		editClip = t->clips[0].id;
		selClips = {editClip};
	}
	touch();
	emit selectionChanged();
	return id;
}

void App::removeTrack(int id) {
	const int i = song.trackIndex(id);
	if (i < 0) return;
	// drop queued changes (they may refer to this track) and any keys still held on it
	pending_.clear();
	queued_.clear();
	recOpen_.remove(id);
	releaseAllKeys();
	begin(tr("Delete track"), true);
	song.tracks.erase(song.tracks.begin() + i);
	fixSelection();
	touch();
	emit selectionChanged();
}

int App::createClip(int trackId, int start) {
	Track* t = song.track(trackId);
	if (!t) return -1;
	begin(tr("New clip"));
	Clip c;
	c.id = song.newId();
	c.start = start;
	c.len = 4 * BAR;
	c.loopLen = t->role == R_LEAD ? 4 * BAR : BAR;
	if (t->isDrums()) {
		c.ad.on = true;
		c.ad.seed = unsigned(rng());
	} // new drum clips start with the auto drummer
	t->clips.push_back(c);
	const int id = c.id;
	touch();
	selectClip(id);
	return id;
}

void App::splitClip(int clipId, int tick) {
	Track* t = nullptr;
	Clip* c = song.clip(clipId, &t);
	if (!c || !t || tick <= c->start || tick >= c->end()) return;
	begin(tr("Cut clip in two"));
	Clip right = *c;
	right.id = song.newId();
	right.start = tick;
	right.len = c->end() - tick;
	right.offset = (c->offset + (tick - c->start)) % c->loopLen;
	right.inKind = 0; // the transitions stay at the outer ends
	c->outKind = 0;
	c->len = tick - c->start;
	t->clips.push_back(right); // c is invalid from here on
	touch();
}

void App::splitAtPlayhead() {
	const int tick = int(std::lround(musicalPos() / grid)) * grid;
	QList<int> ids;
	for (const Track& t : song.tracks)
		for (const Clip& c : t.clips)
			if (c.start < tick && tick < c.end() && (selClips.isEmpty() ? t.id == selTrack : selClips.contains(c.id)))
				ids << c.id;
	if (ids.isEmpty()) {
		emit status(tr("Put the playhead inside a clip to cut it there."));
		return;
	}
	for (int id : ids)
		splitClip(id, tick);
}

void App::selectAll() {
	if (focus == FocusEditor) {
		if (Clip* c = curClip()) {
			for (Note& n : c->notes)
				n.sel = true;
			emit changed();
		}
	} else {
		selClips.clear();
		for (const Track& t : song.tracks)
			for (const Clip& c : t.clips)
				selClips.insert(c.id);
		emit selectionChanged();
	}
}

void App::copy() {
	if (focus == FocusEditor) {
		Clip* c = curClip();
		if (!c) return;
		std::vector<Note> picked;
		for (const Note& n : c->notes)
			if (n.sel) picked.push_back(n);
		if (!picked.empty()) clipNotes_ = picked;
	} else {
		if (selClips.isEmpty()) return;
		clipClips_.clear();
		for (const Track& t : song.tracks)
			for (const Clip& c : t.clips)
				if (selClips.contains(c.id)) clipClips_.push_back({t.isDrums(), c});
	}
}

void App::deleteSelection() {
	if (focus == FocusEditor) {
		Clip* c = curClip();
		if (!c) return;
		if (c->ad.on) {
			emit status(
			    tr("The auto drummer writes this clip. Switch off “The drummer plays this clip” first to delete "
				   "single hits."));
			return;
		}
		bool any = false;
		for (const Note& n : c->notes)
			any |= n.sel;
		if (!any) {
			emit status(
			    tr("No notes are selected. Click a note first, or right-click a note to delete it straight away."));
			return;
		}
		begin(tr("Delete notes"));
		std::erase_if(c->notes, [](const Note& n) { return n.sel; });
		touch();
	} else {
		if (song.liveMode) {
			deleteCell(editClip);
			return;
		}
		if (selClips.isEmpty()) {
			emit status(tr("No clip is selected. Click a clip in the timeline first."));
			return;
		}
		begin(selClips.size() > 1 ? tr("Delete clips") : tr("Delete clip"), true);
		for (Track& t : song.tracks) {
			// a clip lying exactly under a selected one is invisible and would keep playing: take it out too
			std::vector<std::pair<int, int>> spots;
			for (const Clip& c : t.clips)
				if (selClips.contains(c.id)) spots.push_back({c.start, c.len});
			std::erase_if(t.clips, [&](const Clip& c) {
				if (selClips.contains(c.id)) return true;
				for (auto& sp : spots)
					if (c.start == sp.first && c.len == sp.second) return true;
				return false;
			});
		}
		selClips.clear();
		fixSelection();
		touch();
		emit selectionChanged();
	}
}

void App::cut() {
	copy();
	deleteSelection();
}

void App::loopSelection() {
	int lo = INT_MAX, hi = 0;
	for (const Track& t : song.tracks)
		for (const Clip& c : t.clips)
			if (selClips.contains(c.id)) {
				lo = std::min(lo, c.start);
				hi = std::max(hi, c.end());
			}
	if (lo == INT_MAX) {
		emit status(tr("Click a clip first, then this sets the loop around it."));
		return;
	}
	edit(
	    [&](Song& s) {
		    s.loopStart = lo / BAR * BAR;
		    s.loopEnd = std::max(s.loopStart + BAR, (hi + BAR - 1) / BAR * BAR);
		    s.loopOn = true;
	    },
	    tr("Loop this part"));
	if (pos() < song.loopStart || pos() >= song.loopEnd) seek(song.loopStart);
}

void App::paste() {
	if (focus == FocusEditor) {
		Clip* c = curClip();
		if (!c || c->ad.on || clipNotes_.empty()) return;
		begin(tr("Paste"));
		int first = INT_MAX;
		for (const Note& n : clipNotes_)
			first = std::min(first, n.start);
		int at = first;
		const double m = musicalPos();
		if (m >= c->start && m < c->end())
			at = int(std::lround(std::fmod(m - c->start + c->offset, double(c->loopLen)) / grid)) * grid;
		for (Note& n : c->notes)
			n.sel = false;
		for (Note n : clipNotes_) {
			n.start = (n.start - first + at) % c->loopLen;
			n.len = std::min(n.len, c->loopLen - n.start); // inside the pattern, as the grid shows it
			n.sel = true;
			c->notes.push_back(n);
		}
		touch();
	} else {
		Track* t = curTrack();
		if (!t || clipClips_.empty()) return;
		int first = INT_MAX;
		for (auto& pc : clipClips_)
			first = std::min(first, pc.second.start);
		const int at = int(musicalPos()) / BAR * BAR;
		bool any = false;
		for (auto& pc : clipClips_)
			any |= pc.first == t->isDrums();
		if (!any) {
			emit status(t->isDrums() ? tr("Those clips hold notes, so they can only be pasted onto a synth track.")
			                         : tr("Those are drum clips, so they can only be pasted onto a drum track."));
			return;
		}
		begin(tr("Paste"));
		selClips.clear();
		for (auto& pc : clipClips_) {
			if (pc.first != t->isDrums()) continue;
			Clip c = pc.second;
			c.id = song.newId();
			c.start = c.start - first + at;
			t->clips.push_back(c);
			selClips.insert(c.id);
			editClip = c.id;
		}
		touch();
		emit selectionChanged();
	}
}

void App::duplicateSelection() {
	if (focus == FocusEditor) {
		Clip* c = curClip();
		if (!c || c->ad.on) return;
		int lo = INT_MAX, hi = 0;
		for (const Note& n : c->notes)
			if (n.sel) {
				lo = std::min(lo, n.start);
				hi = std::max(hi, n.start + n.len);
			}
		if (lo == INT_MAX) return;
		begin(tr("Duplicate"));
		const int span = std::max(grid, (hi - lo + grid - 1) / grid * grid);
		std::vector<Note> extra;
		for (Note& n : c->notes)
			if (n.sel) {
				Note d = n;
				d.start += span;
				n.sel = false;
				if (d.start < c->loopLen) extra.push_back(d);
			}
		c->notes.insert(c->notes.end(), extra.begin(), extra.end());
		touch();
	} else {
		if (selClips.isEmpty()) return;
		begin(tr("Duplicate"));
		QSet<int> fresh;
		for (Track& t : song.tracks) {
			std::vector<Clip> extra;
			for (const Clip& c : t.clips)
				if (selClips.contains(c.id)) {
					Clip d = c;
					d.id = song.newId();
					d.start = c.end();
					extra.push_back(d);
					fresh.insert(d.id);
				}
			t.clips.insert(t.clips.end(), extra.begin(), extra.end());
		}
		selClips = fresh;
		touch();
		emit selectionChanged();
	}
}

// ---------------------------------------------------------------- loop grid

void App::setLiveMode(bool on) {
	if (song.liveMode == on) return;
	song.liveMode = on;
	if (on) { // the first time, put each track's first clip into the first column so the grid is not empty
		for (Track& t : song.tracks) {
			if (!t.cells.empty() || t.clips.empty()) continue;
			Clip k = t.clips[0];
			k.id = song.newId();
			k.scene = 0;
			k.start = 0;
			k.offset = 0;
			k.inKind = k.outKind = 0;
			if (k.ad.on) {
				k.len = k.loopLen = 4 * BAR;
				k.adKey = 0;
			} else
				k.len = k.loopLen;
			t.cells.push_back(k);
		}
	} else {
		queued_.clear();
		for (Track& t : song.tracks)
			t.liveCell = -1;
	}
	if (Track* t = curTrack()) { // show a pattern of the new view in the editor
		const Clip* c = on ? (t->cell(t->liveCell) ? t->cell(t->liveCell)
		                      : t->cells.empty()   ? nullptr
		                                           : &t->cells[0])
		                   : (t->clips.empty() ? nullptr : &t->clips[0]);
		editClip = c ? c->id : -1;
	}
	touch();
	emit selectionChanged();
}

int App::newCell(int trackId, int scene) {
	Track* t = song.track(trackId);
	if (!t || t->cell(scene)) return -1;
	begin(tr("New cell"));
	Clip c;
	c.id = song.newId();
	c.scene = scene;
	c.start = 0;
	if (t->isDrums()) {
		c.loopLen = c.len = 4 * BAR;
		c.ad.on = true;
		c.ad.seed = unsigned(rng());
	} else {
		c.loopLen = c.len = t->role == R_LEAD ? 4 * BAR : BAR;
		const auto gens = generatorsFor(t->role);
		generate(song, *t, c, gens[size_t(rng() % gens.size())].id, density, rng);
	}
	t->cells.push_back(c);
	const int id = c.id;
	touch();
	selectClip(id);
	return id;
}

void App::closeRecorded(int trackId, int tick) {
	const auto it = recOpen_.find(trackId);
	if (it == recOpen_.end()) return;
	Track* t = song.track(trackId);
	const int id = it.value();
	recOpen_.erase(it);
	if (!t) return;
	for (size_t i = 0; i < t->clips.size(); i++)
		if (t->clips[i].id == id) {
			if (tick <= t->clips[i].start)
				t->clips.erase(t->clips.begin() + long(i));
			else
				t->clips[i].len = tick - t->clips[i].start;
			return;
		}
}

void App::applyLaunch(Song& s, int trackId, int scene) {
	Track* t = s.track(trackId);
	if (!t || t->liveCell == scene) return;
	t->liveCell = scene;
	if (!recording || !s.liveMode) return;
	// Record is on: write what was launched into the song timeline, from this bar on
	const int tick = int(std::lround(musicalPos() / BAR)) * BAR;
	closeRecorded(trackId, tick);
	if (const Clip* cell = t->cell(scene)) {
		Clip k = *cell;
		k.id = s.newId();
		k.scene = -1;
		k.start = tick;
		k.len = BAR;
		k.adKey = 0;
		t->clips.push_back(k);
		recOpen_.insert(trackId, k.id);
	}
}

void App::launch(int trackId, int scene) {
	if (!isPlaying()) { // nothing is running: switch at once and start
		if (recording && song.liveMode) begin();
		applyLaunch(song, trackId, scene);
		touch();
		play();
		return;
	}
	queued_.insert(trackId, scene);
	onBar(
	    [this, trackId, scene](Song& s) {
		    applyLaunch(s, trackId, scene);
		    queued_.remove(trackId);
	    },
	    false);
}

void App::launchScene(int scene) {
	const bool wasPlaying = isPlaying();
	for (const Track& t : song.tracks) {
		const int target = t.cell(scene) ? scene : -1;
		if (wasPlaying)
			launch(t.id, target);
		else {
			if (Track* k = song.track(t.id)) applyLaunch(song, k->id, target);
		}
	}
	if (!wasPlaying) {
		touch();
		play();
	}
}

void App::stopAllCells() {
	for (const Track& t : song.tracks)
		if (t.liveCell >= 0 || queued_.contains(t.id)) launch(t.id, -1);
}

void App::deleteCell(int cellId) {
	Track* t = nullptr;
	Clip* c = song.clip(cellId, &t);
	if (!c || !t || c->scene < 0) {
		emit status(tr("Click a cell in the loop grid first."));
		return;
	}
	begin(tr("Delete cell"), true);
	if (t->liveCell == c->scene) t->liveCell = -1;
	std::erase_if(t->cells, [&](const Clip& k) { return k.id == cellId; });
	fixSelection();
	touch();
	emit selectionChanged();
}

void App::cellToTimeline(int cellId) {
	Track* t = nullptr;
	Clip* c = song.clip(cellId, &t);
	if (!c || !t || c->scene < 0) return;
	begin(tr("Copy cell into the song"));
	Clip k = *c;
	k.id = song.newId();
	k.scene = -1;
	k.adKey = 0;
	k.start = int(musicalPos()) / BAR * BAR;
	k.len = std::max(4 * BAR, k.loopLen);
	t->clips.push_back(k);
	touch();
	emit status(tr("Copied into the song timeline at bar %1.").arg(k.start / BAR + 1));
}
