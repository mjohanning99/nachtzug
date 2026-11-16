#include "midi.h"
#include "app.h"
#include <QMenu>
#include <QSettings>
#include <QWidget>
#include <chrono>
#include <cmath>

static double secondsNow() {
	return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

Midi::Midi(App* a, bool remember) : QObject(a), app(a), remember_(remember) {
	// the clock thread reads the engine: it stops while the engine is replaced, and starts again on the new one
	connect(app, &App::engineReplacing, this, [this] { stopClockThread(); });
	connect(app, &App::engineReplaced, this, [this] {
		if (sendClock_) startClockThread();
	});
	idle_.setSingleShot(true);
	idle_.setInterval(400); // a controller knob left alone this long has been let go
	connect(&idle_, &QTimer::timeout, this, &Midi::release);
	if (remember_) {
		QSettings st;
		const QStringList pairs = st.value(QStringLiteral("midi/knobs")).toStringList();
		for (const QString& p : pairs) {
			const int at = p.indexOf('=');
			if (at > 0) map_.insert(p.left(at).toInt(), p.mid(at + 1));
		}
		followClock_ = st.value(QStringLiteral("midi/followClock"), false).toBool();
#if defined(Q_OS_WIN)
		// Windows lets one program at a time have a MIDI input: listening to every device would lock the others out
		setInput(st.value(QStringLiteral("midi/input")).toString());
#else
		setInput(st.value(QStringLiteral("midi/input"), QStringLiteral("*"))
		             .toString()); // every device, unless told otherwise
#endif
		setOutput(st.value(QStringLiteral("midi/output")).toString());
		setSendClock(st.value(QStringLiteral("midi/sendClock"), false).toBool());
	}
}

Midi::~Midi() {
	stopClockThread();
	io_.listen(QString(), {});
}

void Midi::save() {
	if (!remember_) return;
	QSettings st;
	QStringList pairs;
	for (auto it = map_.begin(); it != map_.end(); ++it)
		pairs << QStringLiteral("%1=%2").arg(it.key()).arg(it.value());
	st.setValue(QStringLiteral("midi/knobs"), pairs);
	st.setValue(QStringLiteral("midi/input"), input_);
	st.setValue(QStringLiteral("midi/output"), output_);
	st.setValue(QStringLiteral("midi/sendClock"), sendClock_);
	st.setValue(QStringLiteral("midi/followClock"), followClock_);
}

void Midi::setInput(const QString& name) {
	input_ = name;
	io_.listen(name, [this](const uint8_t* b, int n) { incoming(b, n); });
	save();
}

// a message from a device, on the MIDI thread: the clock comes 24 times a beat, so its tempo is worked out here;
// everything else goes over to the program's own thread
void Midi::incoming(const uint8_t* b, int n) {
	if (b[0] == 0xf8) {
		const double now = secondsNow();
		if (lastClock_ > 0) {
			const double gap = now - lastClock_;
			if (gap > 0.004 && gap < 0.2) {
				clockAvg_ = clockCount_ < 4 ? gap : clockAvg_ * 0.92 + gap * 0.08;
				if (++clockCount_ >= 24) {
					const float bpm = float(std::lround(600.0 / (clockAvg_ * 24.0)) / 10.0);
					if (std::fabs(bpm - heardBpm_.load()) >= 0.25f) {
						heardBpm_.store(bpm);
						if (followClock_)
							QMetaObject::invokeMethod(
							    this,
							    [this, bpm] {
								    if (std::fabs(app->song.bpm - bpm) >= 0.25f) {
									    app->song.bpm = std::clamp(bpm, 40.f, 240.f);
									    app->touch();
								    }
							    },
							    Qt::QueuedConnection);
					}
				}
			} else
				clockCount_ = 0;
		}
		lastClock_ = now;
		lastClockSeen_.store(now);
		return;
	}
	if (b[0] == 0xfe) return; // "still here" from some keyboards
	const QByteArray m(reinterpret_cast<const char*>(b), n);
	QMetaObject::invokeMethod(this, [this, m] { received(m); }, Qt::QueuedConnection);
}

void Midi::setOutput(const QString& name) {
	output_ = name;
	io_.sendTo(name);
	save();
}
void Midi::setSendClock(bool on) {
	sendClock_ = on;
	if (on)
		startClockThread();
	else
		stopClockThread();
	save();
}
void Midi::setFollowClock(bool on) {
	followClock_ = on;
	save();
}

QString Midi::targetForParam(int param) {
	return QStringLiteral("p:") + QString::fromLatin1(PARAMS[std::clamp(param, 0, dsp::P_COUNT - 1)].id);
}

void Midi::learn(const QString& target, const QString& label) {
	learnTarget_ = target;
	learnLabel_ = label;
	emit app->status(
	    tr("Turn a knob or move a fader on your MIDI controller to control %1. (Esc or right-click again to cancel.)")
	        .arg(label));
}
void Midi::cancelLearn() {
	if (!learnTarget_.isEmpty()) {
		learnTarget_.clear();
		emit app->status(tr("Nothing was learned."));
	}
}
void Midi::forget(const QString& target) {
	for (auto it = map_.begin(); it != map_.end();) {
		if (it.value() == target)
			it = map_.erase(it);
		else
			++it;
	}
	save();
	emit mappingsChanged();
}
void Midi::forgetAll() {
	map_.clear();
	save();
	emit mappingsChanged();
}
int Midi::controllerFor(const QString& target) const {
	for (auto it = map_.begin(); it != map_.end(); ++it)
		if (it.value() == target) return it.key();
	return -1;
}

static QString noteText(int n) {
	return pitchName(n % 12) + QString::number(n / 12 - 1);
}

void Midi::received(const QByteArray& m) {
	const int status = uchar(m[0]), kind = status & 0xf0;
	const int d1 = m.size() > 1 ? uchar(m[1]) : 0, d2 = m.size() > 2 ? uchar(m[2]) : 0;
	if (kind == 0x90 || kind == 0x80) {
		const bool on = kind == 0x90 && d2 > 0;
		app->midiNote(d1, d2, on);
		if (on) {
			last_ = tr("Note %1, velocity %2").arg(noteText(d1)).arg(d2);
			emit heard(last_);
		}
	} else if (kind == 0xb0) {
		if (d1 == 120 || d1 == 123) {
			app->releaseAllKeys();
			return;
		} // all notes off
		control(d1, d2);
		last_ = tr("Controller %1, value %2").arg(d1).arg(d2);
		emit heard(last_);
	} else if (followClock_) {
		// the other device's transport
		bool known = true;
		switch (status) {
		case 0xfa:
			app->seek(0);
			app->play();
			last_ = tr("Start");
			break;
		case 0xfb:
			app->play();
			last_ = tr("Continue");
			break;
		case 0xfc:
			app->stop();
			last_ = tr("Stop");
			break;
		case 0xf2:
			app->seek(double(d1 | (d2 << 7)) * (PPQ / 4.0));
			last_ = tr("Song position");
			break; // in sixteenths
		default:
			known = false;
		}
		if (known) emit heard(last_);
	}
}

void Midi::control(int cc, int value) {
	if (!learnTarget_.isEmpty()) {
		forget(learnTarget_);
		map_.insert(cc, learnTarget_);
		save();
		emit app->status(tr("Learned: controller %1 now turns %2.").arg(cc).arg(learnLabel_));
		learnTarget_.clear();
		emit mappingsChanged();
		return;
	}
	const auto it = map_.find(cc);
	if (it == map_.end()) return;
	apply(it.value(), float(value) / 127.f);
}

// One turn of a controller knob is one undo step, and with Record on it is written into the song like a turn
// of the knob on screen.
void Midi::apply(const QString& target, float v) {
	if (held_ != target) {
		release();
		held_ = target;
		if (target.startsWith(QLatin1String("p:"))) {
			for (int p = 0; p < dsp::P_COUNT; p++)
				if (target.mid(2) == QLatin1String(PARAMS[p].id)) {
					const Track* t = app->curTrack();
					if (t && !t->isDrums()) app->paramPress(t->id, p);
				}
		} else if (target.startsWith(QLatin1String("vol:"))) {
			const int i = target.mid(4).toInt();
			if (i >= 0 && i < int(app->song.tracks.size())) app->paramPress(app->song.tracks[size_t(i)].id, AP_VOLUME);
		} else if ((target.startsWith(QLatin1String("echo:")) || target.startsWith(QLatin1String("rev:"))) &&
		           trackAt(target) && !trackAt(target)->isDrums()) {
			app->paramPress(trackAt(target)->id,
			                target.startsWith(QLatin1String("echo:")) ? dsp::P_DELAY : dsp::P_REVERB);
		} else
			app->begin(target == QLatin1String("master")  ? tr("Volume")
			           : target == QLatin1String("swing") ? tr("Swing")
			                                              : tr("Controller knob"));
		if (target.startsWith(QLatin1String("p:")) && app->curTrack() && app->curTrack()->isDrums())
			emit app->status(
			    tr("This controller knob turns a synth knob of the selected track, but a drum track is selected."));
	}
	idle_.start();
	if (target.startsWith(QLatin1String("p:"))) {
		const Track* t = app->curTrack();
		if (!t || t->isDrums()) return;
		for (int p = 0; p < dsp::P_COUNT; p++)
			if (target.mid(2) == QLatin1String(PARAMS[p].id)) app->paramMove(t->id, p, v);
	} else if (target.startsWith(QLatin1String("vol:"))) {
		const int i = target.mid(4).toInt();
		if (i >= 0 && i < int(app->song.tracks.size())) app->paramMove(app->song.tracks[size_t(i)].id, AP_VOLUME, v);
	} else if (target.startsWith(QLatin1String("pump:"))) {
		const int i = target.mid(5).toInt();
		if (i >= 0 && i < int(app->song.tracks.size()) && !app->song.tracks[size_t(i)].isDrums()) {
			app->song.tracks[size_t(i)].duck = v;
			app->touch();
		}
	} else if (target.startsWith(QLatin1String("pan:"))) {
		if (Track* t = trackAt(target)) {
			t->pan = (v - 0.5f) * 2.f;
			app->touch();
		}
	} else if (target.startsWith(QLatin1String("echo:")) || target.startsWith(QLatin1String("rev:"))) {
		const bool echo = target.startsWith(QLatin1String("echo:"));
		if (Track* t = trackAt(target)) {
			if (t->isDrums()) {
				t->kit.p[echo ? dsp::DP_DELAY : dsp::DP_REVERB] = v;
				app->touch();
			} else
				app->paramMove(t->id, echo ? dsp::P_DELAY : dsp::P_REVERB, v);
		}
	} else if (target.startsWith(QLatin1String("in:"))) {
		LiveInput& li = app->song.input;
		const QString f = target.mid(3);
		float* field = f == QLatin1String("gain")     ? &li.gain
		               : f == QLatin1String("cutoff") ? &li.cutoff
		               : f == QLatin1String("res")    ? &li.res
		               : f == QLatin1String("wah")    ? &li.wah
		               : f == QLatin1String("drive")  ? &li.drive
		               : f == QLatin1String("duck")   ? &li.duck
		               : f == QLatin1String("echo")   ? &li.echo
		               : f == QLatin1String("reverb") ? &li.reverb
		               : f == QLatin1String("vol")    ? &li.vol
		                                              : nullptr;
		if (field) {
			*field = v;
			app->touch();
		}
	} else if (target == QLatin1String("dfb")) {
		app->song.dfb = v;
		app->touch();
	} else if (target == QLatin1String("rsize")) {
		app->song.rsize = v;
		app->touch();
	} else if (target == QLatin1String("master")) {
		app->song.master = v;
		app->touch();
	} else if (target == QLatin1String("swing")) {
		app->song.swing = v;
		app->touch();
	}
}

Track* Midi::trackAt(const QString& target) {
	const int i = target.section(':', 1).toInt();
	return i >= 0 && i < int(app->song.tracks.size()) ? &app->song.tracks[size_t(i)] : nullptr;
}

float Midi::clockBpm() const { // nothing heard for a second: no clock
	return secondsNow() - lastClockSeen_.load() < 1.0 ? heardBpm_.load() : 0.f;
}

void Midi::release() {
	if (held_.startsWith(QLatin1String("p:")) || held_.startsWith(QLatin1String("vol:")) ||
	    held_.startsWith(QLatin1String("echo:")) || held_.startsWith(QLatin1String("rev:")))
		app->paramRelease();
	held_.clear();
}

// ---------------------------------------------------------------- clock out

// 24 clock messages a beat, from the song's tempo, sent all the time (so the other device knows the tempo before
// it starts), with Start when the song starts from the beginning, Continue from elsewhere, and Stop.
void Midi::startClockThread() {
	if (clockRun_.load()) return;
	clockRun_ = true;
	Engine* e = app->engine;
	clockThread_ = std::thread([this, e] {
		double phase = 0.0, last = secondsNow();
		bool wasPlaying = e->playing.load();
		while (clockRun_.load()) {
			const double now = secondsNow();
			const bool playing = e->playing.load(std::memory_order_relaxed);
			if (playing != wasPlaying) {
				if (playing) {
					const double at = e->pos.load(std::memory_order_relaxed);
					if (at < 1.0) {
						const uint8_t s = 0xfa;
						out(&s, 1);
					} else {
						const int sixteenths = std::clamp(int(at / (PPQ / 4.0)), 0, 16383);
						const uint8_t spp[3] = {0xf2, uint8_t(sixteenths & 0x7f), uint8_t(sixteenths >> 7)};
						out(spp, 3);
						const uint8_t c = 0xfb;
						out(&c, 1);
					}
					phase = 0.0; // the first clock goes out with the start
				} else {
					const uint8_t s = 0xfc;
					out(&s, 1);
				}
				wasPlaying = playing;
			}
			phase += (now - last) * double(e->bpmNow.load(std::memory_order_relaxed)) * 24.0 / 60.0;
			last = now;
			for (int guard = 0; phase >= 0.0 && guard < 8; guard++) {
				const uint8_t f8 = 0xf8;
				out(&f8, 1);
				phase -= 1.0;
			}
			std::this_thread::sleep_for(std::chrono::microseconds(500));
		}
	});
}

void Midi::out(const uint8_t* b, int n) {
	if (sendHook)
		sendHook(b, n);
	else
		io_.send(b, n);
}

void Midi::stopClockThread() {
	if (!clockRun_.exchange(false)) return;
	if (clockThread_.joinable()) clockThread_.join();
}

// ---------------------------------------------------------------- menus

void addMidiActions(QMenu* menu, App* app, const QString& target, const QString& label) {
	Midi* m = app ? app->midi : nullptr;
	if (!m) return;
	menu->addSeparator();
	const int cc = m->controllerFor(target);
	if (m->learning() == target)
		menu->addAction(Midi::tr("Stop waiting for a MIDI knob"), m, [m] { m->cancelLearn(); });
	else
		menu->addAction(cc >= 0 ? Midi::tr("Learn MIDI control again (now controller %1)").arg(cc)
		                        : Midi::tr("Learn MIDI control"),
		                m, [m, target, label] { m->learn(target, label); });
	if (cc >= 0) menu->addAction(Midi::tr("Forget MIDI control"), m, [m, target] { m->forget(target); });
}

void midiMenuFor(QWidget* w, App* app, const QString& target, const QString& label) {
	w->setProperty("midiTarget", target);
	w->setContextMenuPolicy(Qt::CustomContextMenu);
	QObject::connect(w, &QWidget::customContextMenuRequested, w, [w, app, target, label](const QPoint& pt) {
		if (!app->midi) return;
		QMenu menu;
		addMidiActions(&menu, app, target, label);
		menu.exec(w->mapToGlobal(pt));
	});
}
