#include "mixer.h"
#include "midi.h"
#include "app.h"
#include "widgets.h"
#include <QFrame>
#include <QMessageBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

using namespace dsp;

static QString pct(float v) {
	return QString::number(int(std::lround(v * 100.f))) + "%";
}
static QString panText(float x) {
	const int c = int(std::lround((x - 0.5f) * 200.f));
	return c == 0  ? ui(QT_TRANSLATE_NOOP("Model", "Centre"))
	       : c < 0 ? ui(QT_TRANSLATE_NOOP("Model", "Left %1")).arg(-c)
	               : ui(QT_TRANSLATE_NOOP("Model", "Right %1")).arg(c);
}

// ---------------------------------------------------------------- Fader

Fader::Fader(QWidget* parent) : QWidget(parent), accent(theme::amber) {
	setMinimumSize(46, 120);
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
	setCursor(Qt::SizeVerCursor);
	setFocusPolicy(Qt::NoFocus);
	setToolTip(tr("Volume. Drag up or down, hold Shift for small steps, double-click to reset. The bar beside it shows "
	              "the level."));
}

void Fader::setValue(float v) {
	v = std::clamp(v, 0.f, 1.f);
	if (v != val) {
		val = v;
		update();
	}
}
void Fader::setPeak(float p) {
	p = std::clamp(p, 0.f, 1.f);
	if (std::fabs(p - peak) > 0.004f) {
		peak = p;
		update();
	}
}

void Fader::paintEvent(QPaintEvent*) {
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);
	const double top = 8, bottom = height() - 8, h = bottom - top;
	// slot with the filled part below the handle
	const QRectF slot(11, top, 6, h);
	p.setPen(Qt::NoPen);
	p.setBrush(theme::slot());
	p.drawRoundedRect(slot, 3, 3);
	const double y = bottom - double(val) * h;
	p.setBrush(accent);
	p.drawRoundedRect(QRectF(11, y, 6, bottom - y), 3, 3);
	// level meter
	const QRectF meter(30, top, 6, h);
	p.setBrush(theme::slot());
	p.drawRoundedRect(meter, 3, 3);
	const double mh = std::sqrt(double(peak)) * h;
	if (mh > 1) {
		p.setBrush(peak > 0.85f ? theme::lcd : theme::ink);
		p.drawRoundedRect(QRectF(30, bottom - mh, 6, mh), 3, 3);
	}
	// handle
	p.setPen(QPen(theme::capEdge, 1.5));
	p.setBrush(theme::cap);
	p.drawRoundedRect(QRectF(2, y - 7, 24, 14), 3, 3);
	p.setPen(QPen(theme::capInk, 1.5));
	p.drawLine(QPointF(6, y), QPointF(22, y));
}

void Fader::mousePressEvent(QMouseEvent* e) {
	if (e->button() != Qt::LeftButton) return;
	dragging = true;
	startY = int(e->globalPosition().y());
	startVal = val;
	emit pressed();
}
void Fader::mouseMoveEvent(QMouseEvent* e) {
	if (!dragging) return;
	const float range = float(std::max(60, height() - 16)) * ((e->modifiers() & Qt::ShiftModifier) ? 4.f : 1.f);
	const float v = std::clamp(startVal + float(startY - int(e->globalPosition().y())) / range, 0.f, 1.f);
	if (v != val) {
		val = v;
		update();
		emit moved(v);
	}
}
void Fader::mouseReleaseEvent(QMouseEvent*) {
	if (dragging) {
		dragging = false;
		emit released();
	}
}
void Fader::mouseDoubleClickEvent(QMouseEvent*) {
	emit pressed();
	val = 0.72f;
	update();
	emit moved(val);
	emit released();
}

// ---------------------------------------------------------------- MixerPanel

MixerPanel::MixerPanel(App* a, QWidget* parent) : QWidget(parent), app(a) {
	row = new QHBoxLayout(this);
	row->setContentsMargins(10, 8, 10, 8);
	row->setSpacing(8);
	row->addStretch(1);
	connect(app, &App::changed, this, &MixerPanel::sync);
	connect(app, &App::selectionChanged, this, &MixerPanel::sync);
	sync();
}

static QPushButton* smallToggle(const QString& text, const QString& tip) {
	auto* b = new QPushButton(text);
	b->setCheckable(true);
	b->setFocusPolicy(Qt::NoFocus);
	b->setToolTip(tip);
	return b;
}

void MixerPanel::rebuild() {
	// the old strips go out of sight at once; they are deleted once the program is back in its event loop
	for (Strip& s : strips) {
		s.box->hide();
		s.box->deleteLater();
	}
	strips.clear();
	if (masterBox) {
		masterBox->hide();
		masterBox->deleteLater();
		masterBox = nullptr;
	}
	if (inputBox) {
		inputBox->hide();
		inputBox->deleteLater();
		inputBox = nullptr;
	}
	inputKnobs.clear();
	int at = 0;
	for (const Track& t : app->song.tracks) {
		const int id = t.id;
		const bool drums = t.isDrums();
		Strip s;
		s.id = id;
		s.box = new QFrame;
		s.box->setObjectName("strip");
		s.box->setFixedWidth(138);
		auto* v = new QVBoxLayout(s.box);
		v->setContentsMargins(8, 8, 8, 8);
		v->setSpacing(6);
		s.name = new QPushButton(ui(t.name));
		s.name->setFocusPolicy(Qt::NoFocus);
		s.name->setObjectName("stripName");
		s.name->setIcon(theme::roleIcon(t.role, theme::ink));
		s.name->setIconSize(QSize(15, 15));
		s.name->setToolTip(tr("Select this track: the Notes and Sound tabs then show it"));
		connect(s.name, &QPushButton::clicked, this, [this, id] { app->selectTrack(id); });
		v->addWidget(s.name);
		auto* mid = new QHBoxLayout;
		mid->setSpacing(4);
		auto* knobs = new QVBoxLayout;
		knobs->setSpacing(0);
		s.pan = new Knob(tr("Pan"));
		s.pan->setDefault(0.5f);
		s.pan->setFormatter(panText);
		s.pan->setToolTip(
		    tr("Where this track sits between the left and right speakers. Double-click for the centre."));
		connect(s.pan, &Knob::pressed, this, [this] { app->begin(tr("Pan")); });
		connect(s.pan, &Knob::moved, this, [this, id](float x) {
			if (Track* k = app->song.track(id)) {
				k->pan = (x - 0.5f) * 2.f;
				app->touch();
			}
		});
		s.echo = new Knob(tr("Echo"));
		s.echo->setDefault(0.2f);
		s.echo->setFormatter(pct);
		s.reverb = new Knob(tr("Reverb"));
		s.reverb->setDefault(0.15f);
		s.reverb->setFormatter(pct);
		s.echo->setToolTip(tr("How much of this track goes to the shared echo"));
		s.reverb->setToolTip(tr("How much of this track goes to the shared reverb"));
		auto send = [this, id, drums](Knob* k, int synthParam,
		                              int drumParam) { // how much of the track goes to the shared echo or reverb
			if (drums) {
				connect(k, &Knob::pressed, this, [this, k] { app->begin(k->name()); });
				connect(k, &Knob::moved, this, [this, id, drumParam](float x) {
					if (Track* t2 = app->song.track(id)) {
						t2->kit.p[drumParam] = x;
						app->touch();
					}
				});
			} else {
				connect(k, &Knob::pressed, this, [this, id, synthParam] { app->paramPress(id, synthParam); });
				connect(k, &Knob::moved, this, [this, id, synthParam](float x) { app->paramMove(id, synthParam, x); });
				connect(k, &Knob::released, this, [this] { app->paramRelease(); });
			}
		};
		send(s.echo, P_DELAY, DP_DELAY);
		send(s.reverb, P_REVERB, DP_REVERB);
		knobs->addWidget(s.pan);
		knobs->addWidget(s.echo);
		knobs->addWidget(s.reverb);
		s.pump = nullptr;
		if (!drums) { // the drums make the pump happen, so they have none of their own
			s.pump = new Knob(tr("Pump"));
			s.pump->setDefault(0.f);
			s.pump->setFormatter(pct);
			s.pump->setProperty("lesson", "pump");
			s.pump->setToolTip(tr("Pump: the track ducks out of the way on every kick drum and swells back up, the "
			                      "breathing sound of so much dance music. Try it on pads and bass lines."));
			connect(s.pump, &Knob::pressed, this, [this] { app->begin(tr("Pump")); });
			connect(s.pump, &Knob::moved, this, [this, id](float x) {
				if (Track* k = app->song.track(id)) {
					k->duck = x;
					app->touch();
				}
			});
			knobs->addWidget(s.pump);
		}
		knobs->addStretch(1);
		mid->addLayout(knobs);
		s.fader = new Fader;
		connect(s.fader, &Fader::pressed, this, [this, id] { app->paramPress(id, AP_VOLUME); });
		connect(s.fader, &Fader::moved, this, [this, id](float x) { app->paramMove(id, AP_VOLUME, x); });
		connect(s.fader, &Fader::released, this, [this] { app->paramRelease(); });
		mid->addWidget(s.fader);
		midiMenuFor(s.fader, app, QStringLiteral("vol:%1").arg(at), tr("the volume of %1").arg(ui(t.name)));
		midiMenuFor(s.pan, app, QStringLiteral("pan:%1").arg(at), tr("the pan of %1").arg(ui(t.name)));
		midiMenuFor(s.echo, app, QStringLiteral("echo:%1").arg(at), tr("the echo of %1").arg(ui(t.name)));
		midiMenuFor(s.reverb, app, QStringLiteral("rev:%1").arg(at), tr("the reverb of %1").arg(ui(t.name)));
		if (s.pump) midiMenuFor(s.pump, app, QStringLiteral("pump:%1").arg(at), tr("the pump of %1").arg(ui(t.name)));
		v->addLayout(mid, 1);
		auto* ms = new QHBoxLayout;
		ms->setSpacing(6);
		ms->setContentsMargins(8, 0, 8, 0); // between the screws in the corners, not over them
		s.mute = smallToggle(tr("Mute"), tr("Silence this track. While the song plays, it switches on the next bar."));
		s.solo = smallToggle(tr("Solo"), tr("Hear only this track"));
		theme::symbolKey(s.mute, QStringLiteral("mute"), tr("Mute"), true);
		theme::symbolKey(s.solo, QStringLiteral("solo"), tr("Solo"), true);
		s.mute->setObjectName("mute");
		s.solo->setProperty("strip", true);
		connect(s.mute, &QPushButton::clicked, this, [this, id] {
			app->onBar([id](Song& sg) {
				if (Track* k = sg.track(id)) k->mute = !k->mute;
			});
		});
		connect(s.solo, &QPushButton::clicked, this, [this, id] {
			app->onBar([id](Song& sg) {
				if (Track* k = sg.track(id)) k->solo = !k->solo;
			});
		});
		ms->addWidget(s.mute);
		ms->addWidget(s.solo);
		v->addLayout(ms);
		row->insertWidget(at++, s.box);
		strips.push_back(s);
	}
	// the input strip: whatever comes in at the sound card, played live through a filter and the effects
	{
		inputBox = new QFrame;
		inputBox->setObjectName("strip");
		inputBox->setFixedWidth(196);
		auto* v = new QVBoxLayout(inputBox);
		v->setContentsMargins(8, 8, 8, 8);
		v->setSpacing(6);
		inputOn = smallToggle(
		    tr("Input"),
		    tr("Play the sound card’s input live through Nachtzug: a guitar, a voice, another synthesiser. "
			   "Use headphones with a microphone, or the speakers feed back into it. It is never part of an export."));
		inputOn->setObjectName("stripName");
		inputOn->setIcon(theme::ledIcon(theme::lcd));
		v->addWidget(inputOn);
		auto* mid = new QHBoxLayout;
		mid->setSpacing(2);
		auto* grid = new QGridLayout;
		grid->setHorizontalSpacing(0);
		grid->setVerticalSpacing(0);
		struct K {
			const char* name;
			float LiveInput::* field;
			float def;
			const char* tip;
		};
		const K ks[] = {
		    {QT_TR_NOOP("Gain"), &LiveInput::gain, 0.5f,
			 QT_TR_NOOP("How loud the input comes in. Turn it up for a quiet guitar or microphone.")},
		    {QT_TR_NOOP("Filter"), &LiveInput::cutoff, 1.f,
			 QT_TR_NOOP("A filter like the synthesiser’s: turn it down to make the input darker")},
		    {QT_TR_NOOP("Squelch"), &LiveInput::res, 0.f,
			 QT_TR_NOOP("The filter rings at its setting, for the acid sound on a guitar or a voice")},
		    {QT_TR_NOOP("Wah"), &LiveInput::wah, 0.f,
			 QT_TR_NOOP("The filter opens as you play louder and closes as it fades, like a wah pedal")},
		    {QT_TR_NOOP("Drive"), &LiveInput::drive, 0.f, QT_TR_NOOP("Distortion: warm at first, then rough")},
		    {QT_TR_NOOP("Pump"), &LiveInput::duck, 0.f,
			 QT_TR_NOOP("The input ducks out of the way on every kick drum")},
		    {QT_TR_NOOP("Echo"), &LiveInput::echo, 0.f, QT_TR_NOOP("How much of the input goes to the shared echo")},
		    {QT_TR_NOOP("Reverb"), &LiveInput::reverb, 0.1f,
			 QT_TR_NOOP("How much of the input goes to the shared reverb")},
		};
		for (int i = 0; i < 8; i++) {
			const K& k = ks[i];
			auto* kn = new Knob(tr(k.name));
			kn->setDefault(k.def);
			kn->setFormatter(pct);
			kn->setToolTip(tr(k.tip));
			const auto field = k.field;
			connect(kn, &Knob::pressed, this, [this] { app->begin(tr("Input")); });
			connect(kn, &Knob::moved, this, [this, field](float x) {
				app->song.input.*field = x;
				app->touch();
			});
			static const char* ids[8] = {"gain", "cutoff", "res", "wah", "drive", "duck", "echo", "reverb"};
			midiMenuFor(kn, app, QStringLiteral("in:") + QLatin1String(ids[i]), tr("%1 of the input").arg(tr(k.name)));
			grid->addWidget(kn, i / 2, i % 2);
			inputKnobs.push_back({kn, field});
		}
		mid->addLayout(grid);
		inputFader = new Fader;
		midiMenuFor(inputFader, app, QStringLiteral("in:vol"), tr("the volume of the input"));
		connect(inputFader, &Fader::pressed, this, [this] { app->begin(tr("Input")); });
		connect(inputFader, &Fader::moved, this, [this](float x) {
			app->song.input.vol = x;
			app->touch();
		});
		mid->addWidget(inputFader);
		v->addLayout(mid, 1);
		inputMute = smallToggle(tr("Mute"), tr("Silence the input without closing it"));
		inputMute->setObjectName("mute");
		theme::symbolKey(inputMute, QStringLiteral("mute"), tr("Mute"), true);
		auto* im = new QHBoxLayout;
		im->setContentsMargins(8, 0, 8, 0); // between the screws in the corners, not over them
		im->addWidget(inputMute);
		v->addLayout(im);
		connect(inputMute, &QPushButton::clicked, this,
		        [this](bool on) { app->edit([on](Song& sg) { sg.input.mute = on; }, tr("Input")); });
		connect(inputOn, &QPushButton::clicked, this, [this](bool on) {
			if (!on) {
				app->stopLiveInput();
				sync();
				return;
			}
			QString err;
			if (!app->startLiveInput(err)) {
				inputOn->setChecked(false);
				QMessageBox::warning(
				    this, tr("Input"),
				    tr("The input could not be opened: %1\n\nChoose it in Settings, on the Sound card page.").arg(err));
				return;
			}
			emit app->status(
			    tr("The input plays live. With a microphone, use headphones: the speakers would feed back into it."));
			sync();
		});
		row->insertSpacing(at++, 10);
		row->insertWidget(at++, inputBox);
	}
	// master strip
	masterBox = new QFrame;
	masterBox->setObjectName("strip");
	masterBox->setFixedWidth(138);
	auto* v = new QVBoxLayout(masterBox);
	v->setContentsMargins(8, 8, 8, 8);
	v->setSpacing(6);
	auto* title = new QLabel(tr("WHOLE SONG"));
	title->setObjectName("title");
	title->setAlignment(Qt::AlignCenter);
	title->setFixedHeight(30);
	v->addWidget(title);
	auto* mid = new QHBoxLayout;
	mid->setSpacing(4);
	auto* knobs = new QVBoxLayout;
	knobs->setSpacing(0);
	echoRepeats = new Knob(tr("Echo repeats"));
	echoRepeats->setDefault(0.45f);
	echoRepeats->setFormatter(pct);
	echoRepeats->setToolTip(tr("How many times the shared echo repeats before it fades away"));
	midiMenuFor(echoRepeats, app, QStringLiteral("dfb"), tr("the echo repeats"));
	connect(echoRepeats, &Knob::pressed, this, [this] { app->begin(tr("Echo repeats")); });
	connect(echoRepeats, &Knob::moved, this, [this](float x) {
		app->song.dfb = x;
		app->touch();
	});
	reverbSize = new Knob(tr("Reverb size"));
	reverbSize->setDefault(0.55f);
	reverbSize->setToolTip(tr("How large the shared reverb’s room sounds"));
	reverbSize->setFormatter([](float x) { return QString::number(double(0.5f + x * x * 11.f), 'f', 1) + " s"; });
	midiMenuFor(reverbSize, app, QStringLiteral("rsize"), tr("the reverb size"));
	connect(reverbSize, &Knob::pressed, this, [this] { app->begin(tr("Reverb size")); });
	connect(reverbSize, &Knob::moved, this, [this](float x) {
		app->song.rsize = x;
		app->touch();
	});
	knobs->addWidget(echoRepeats);
	knobs->addWidget(reverbSize);
	knobs->addStretch(1);
	mid->addLayout(knobs);
	masterFader = new Fader;
	connect(masterFader, &Fader::pressed, this, [this] { app->begin(tr("Volume")); });
	connect(masterFader, &Fader::moved, this, [this](float x) {
		app->song.master = x;
		app->touch();
	});
	mid->addWidget(masterFader);
	v->addLayout(mid, 1);
	v->addSpacing(34);
	row->insertSpacing(at++, 10);
	row->insertWidget(at, masterBox);
}

void MixerPanel::sync() {
	const Song& song = app->song;
	std::vector<int> sig;
	for (const Track& t : song.tracks) {
		sig.push_back(t.id);
		sig.push_back(t.role);
	}
	if (sig != signature || !masterBox) {
		// rebuilding removes the spacer too, so start the row again
		while (QLayoutItem* it = row->takeAt(0)) {
			if (!it->widget()) delete it;
		}
		row->addStretch(1);
		rebuild();
		signature = sig;
	}
	for (Strip& s : strips) {
		const Track* t = song.track(s.id);
		if (!t) continue;
		const QColor col = theme::trackColor(t->role);
		s.name->setText(ui(t->name));
		// the track's colour along the name's lower edge: on the rounded plate of the panel looks as wide as the rest
		// of its border, so it follows the corners; in the flat looks a line under the name
		s.name->setStyleSheet((theme::tactile ? QStringLiteral("border-bottom-color: %1;")
		                                      : QStringLiteral("border-bottom: 3px solid %1;"))
		                          .arg(col.name()));
		const bool sel = t->id == app->selTrack;
		if (s.box->property("sel").toBool() != sel) {
			s.box->setProperty("sel", sel);
			s.box->style()->unpolish(s.box);
			s.box->style()->polish(s.box);
		}
		if (!s.pan->isHeld()) s.pan->setValue(t->pan * 0.5f + 0.5f);
		if (!s.echo->isHeld()) s.echo->setValue(t->isDrums() ? t->kit.p[DP_DELAY] : app->paramValue(*t, P_DELAY));
		if (!s.reverb->isHeld()) s.reverb->setValue(t->isDrums() ? t->kit.p[DP_REVERB] : app->paramValue(*t, P_REVERB));
		if (!s.fader->isHeld()) s.fader->setValue(app->paramValue(*t, AP_VOLUME));
		if (s.pump && !s.pump->isHeld()) s.pump->setValue(t->duck);
		for (Knob* k : {s.pan, s.echo, s.reverb, s.pump})
			if (k) k->setAccent(col);
		s.fader->setAccent(col);
		s.mute->setChecked(t->mute);
		s.solo->setChecked(t->solo);
	}
	if (inputBox) {
		inputOn->setChecked(app->liveInput());
		inputMute->setChecked(song.input.mute);
		for (auto& [k, field] : inputKnobs) {
			if (!k->isHeld()) k->setValue(song.input.*field);
			k->setAccent(theme::ice);
			k->setEnabled(app->liveInput());
		}
		if (!inputFader->isHeld()) inputFader->setValue(song.input.vol);
		inputFader->setAccent(theme::ice);
	}
	if (masterFader && !masterFader->isHeld()) masterFader->setValue(song.master);
	if (echoRepeats && !echoRepeats->isHeld()) echoRepeats->setValue(song.dfb);
	if (reverbSize && !reverbSize->isHeld()) reverbSize->setValue(song.rsize);
	if (masterFader) {
		masterFader->setAccent(theme::amber);
		echoRepeats->setAccent(theme::amber);
		reverbSize->setAccent(theme::amber);
	}
}

void MixerPanel::tick() {
	const Song& song = app->song;
	for (Strip& s : strips) {
		const int i = song.trackIndex(s.id);
		if (i < 0 || i >= MAX_TRACKS) continue;
		s.fader->setPeak(app->engine->trackPeak[i].load(std::memory_order_relaxed));
		const Track& t = song.tracks[size_t(i)];
		if (!s.fader->isHeld() && t.hasMoves(AP_VOLUME)) s.fader->setValue(app->paramValue(t, AP_VOLUME));
	}
	if (inputFader) inputFader->setPeak(app->engine->inputPeak.load(std::memory_order_relaxed));
	if (masterFader)
		masterFader->setPeak(std::max(app->engine->peakL.load(std::memory_order_relaxed),
		                              app->engine->peakR.load(std::memory_order_relaxed)));
}
