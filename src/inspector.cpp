// The inspector: see inspector.h.
//
// What's here: Inspector, made by the main window; sync() after every change, adopt() when the layout changes;
// the Sound section is built by SoundPanel::buildQuick(), How notes and the drummer are EditorPanel's own.
// Changes in the song file: none.

#include "inspector.h"
#include <QFormLayout>
#include "app.h"
#include "editor.h"
#include "soundpanel.h"
#include "widgets.h"
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

using namespace dsp;

static QString pct(float v) {
	return ui(QT_TRANSLATE_NOOP("Model", "%1%")).arg(int(std::lround(v * 100.f)));
}
static QString panText(float x) {
	const int c = int(std::lround((x - 0.5f) * 200.f));
	return c == 0  ? ui(QT_TRANSLATE_NOOP("Model", "Centre"))
	       : c < 0 ? ui(QT_TRANSLATE_NOOP("Model", "Left %1")).arg(-c)
	               : ui(QT_TRANSLATE_NOOP("Model", "Right %1")).arg(c);
}

Inspector::Inspector(App* a, SoundPanel* s, EditorPanel* e, QWidget* parent)
    : QScrollArea(parent), app(a), sound(s), editor(e) {
	setWidgetResizable(true);
	setFrameShape(QFrame::NoFrame);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setProperty("noHBar", true);
	setFocusPolicy(Qt::NoFocus);
	setObjectName("inspector");
	auto* inner = new QWidget;
	inner->setObjectName("inspectorInner");
	col = new QVBoxLayout(inner);
	col->setContentsMargins(6, 6, 4, 8);
	col->setSpacing(4);

	// the track's name, and « to put the inspector away
	auto* top = new QHBoxLayout;
	top->setSpacing(6);
	head = new QLabel;
	head->setObjectName("inspectorHead");
	top->addWidget(head, 1);
	auto* away = new QToolButton;
	away->setText(QStringLiteral("«"));
	away->setObjectName("help");
	away->setToolTip(
	    tr("Put the inspector away. The » tab on the left edge, View → Inspector or Ctrl+I bring it back."));
	away->setFocusPolicy(Qt::NoFocus);
	connect(away, &QToolButton::clicked, this, &Inspector::hideRequested);
	top->addWidget(away);
	col->addLayout(top);

	// ---- the track: level, place, sends, mute and solo
	auto* g = new QGroupBox(tr("Track").toUpper());
	auto* tv = new QVBoxLayout(g);
	theme::wellMargins(tv, 8, 6);
	tv->setSpacing(4);
	auto* vr = new QHBoxLayout;
	vr->setSpacing(8);
	auto* vl = new QLabel(tr("Volume"));
	vl->setObjectName("dim");
	volume = new QSlider(Qt::Horizontal);
	volume->setRange(0, 100);
	volume->setFocusPolicy(Qt::NoFocus);
	volume->setToolTip(tr("How loud this track is in the mix"));
	connect(volume, &QSlider::sliderPressed, this, [this] { app->paramPress(app->selTrack, AP_VOLUME); });
	connect(volume, &QSlider::sliderMoved, this,
	        [this](int v) { app->paramMove(app->selTrack, AP_VOLUME, float(v) / 100.f); });
	connect(volume, &QSlider::sliderReleased, this, [this] { app->paramRelease(); });
	vr->addWidget(vl);
	vr->addWidget(volume, 1);
	tv->addLayout(vr);
	auto* kr = new QHBoxLayout;
	kr->setSpacing(0);
	pan = new Knob(tr("Pan"));
	pan->setDefault(0.5f);
	pan->setFormatter(panText);
	pan->setToolTip(tr("Where this track sits between the left and right speakers. Double-click for the centre."));
	connect(pan, &Knob::pressed, this, [this] { app->begin(tr("Pan")); });
	connect(pan, &Knob::moved, this, [this](float x) {
		if (Track* t = app->curTrack()) {
			t->pan = (x - 0.5f) * 2.f;
			app->touch();
		}
	});
	echo = new Knob(tr("Echo"));
	echo->setDefault(0.2f);
	echo->setFormatter(pct);
	reverb = new Knob(tr("Reverb"));
	reverb->setDefault(0.15f);
	reverb->setFormatter(pct);
	echo->setToolTip(tr("How much of this track goes to the shared echo"));
	reverb->setToolTip(tr("How much of this track goes to the shared reverb"));
	for (auto [k, synthParam, drumParam] :
	     {std::tuple{echo, int(P_DELAY), int(DP_DELAY)}, {reverb, int(P_REVERB), int(DP_REVERB)}}) {
		connect(k, &Knob::pressed, this, [this, k, synthParam] {
			if (Track* t = app->curTrack(); t && !t->isDrums())
				app->paramPress(t->id, synthParam);
			else
				app->begin(k->name());
		});
		connect(k, &Knob::moved, this, [this, synthParam, drumParam](float x) {
			Track* t = app->curTrack();
			if (!t) return;
			if (t->isDrums()) {
				t->kit.p[drumParam] = x;
				app->touch();
			} else
				app->paramMove(t->id, synthParam, x);
		});
		connect(k, &Knob::released, this, [this] { app->paramRelease(); });
	}
	pump = new Knob(tr("Pump"));
	pump->setDefault(0.f);
	pump->setFormatter(pct);
	pump->setToolTip(tr("Pump: the track ducks out of the way on every kick drum and swells back up, the breathing "
	                    "sound of so much dance music. Try it on pads and bass lines."));
	connect(pump, &Knob::pressed, this, [this] { app->begin(tr("Pump")); });
	connect(pump, &Knob::moved, this, [this](float x) {
		if (Track* t = app->curTrack()) {
			t->duck = x;
			app->touch();
		}
	});
	for (Knob* k : {pan, echo, reverb, pump}) {
		k->setScale(0.8);
		kr->addWidget(k);
	}
	tv->addLayout(kr);
	auto* ms = new QHBoxLayout;
	ms->setSpacing(6);
	mute = new QPushButton(tr("Mute"));
	solo = new QPushButton(tr("Solo"));
	mute->setObjectName("mute");
	mute->setToolTip(tr("Silence this track. While the song plays, it switches on the next bar."));
	solo->setToolTip(tr("Hear only this track"));
	theme::symbolKey(mute, QStringLiteral("mute"), tr("Mute"));
	theme::symbolKey(solo, QStringLiteral("solo"), tr("Solo"));
	for (QPushButton* b : {mute, solo}) {
		b->setCheckable(true);
		b->setFocusPolicy(Qt::NoFocus);
		ms->addWidget(b);
	}
	connect(mute, &QPushButton::clicked, this, [this] {
		const int id = app->selTrack;
		app->onBar([id](Song& sg) {
			if (Track* k = sg.track(id)) k->mute = !k->mute;
		});
	});
	connect(solo, &QPushButton::clicked, this, [this] {
		const int id = app->selTrack;
		app->onBar([id](Song& sg) {
			if (Track* k = sg.track(id)) k->solo = !k->solo;
		});
	});
	tv->addLayout(ms);
	theme::makeFoldable(g, QStringLiteral("inspector/track"), [this] {
		const Track* t = app->curTrack();
		return t ? tr("Volume %1").arg(pct(t->vol)) + (t->mute ? tr(" · muted") : QString()) : QString();
	});
	col->addWidget(g);

	col->addWidget(sound->buildQuick());
	col->addStretch(1);
	setWidget(inner);

	// as wide as what it holds needs, in any language
	QTimer::singleShot(0, this, [this] { setMinimumWidth(std::max(270, widget()->minimumSizeHint().width() + 14)); });
	connect(app, &App::changed, this, &Inspector::sync);
	connect(app, &App::selectionChanged, this, &Inspector::sync);
	sync();
}

void Inspector::adopt(bool on) {
	if (on == adopted) return;
	adopted = on;
	// they go in above the stretch at the bottom
	auto* host = new QVBoxLayout;
	host->setContentsMargins(0, 0, 0, 0);
	host->setSpacing(6);
	if (on) {
		col->insertLayout(col->count() - 1, host);
		sound->placeHowGroup(host);
		editor->placeDrummer(host);
		alignForms();
	} else {
		sound->placeHowGroup(nullptr);
		editor->placeDrummer(nullptr);
		delete host;
	}
}

void Inspector::alignForms() {
	QList<QLabel*> labels;
	int widest = 0;
	for (QFormLayout* form : widget()->findChildren<QFormLayout*>())
		for (int row = 0; row < form->rowCount(); row++)
			if (auto* item = form->itemAt(row, QFormLayout::LabelRole))
				if (auto* label = qobject_cast<QLabel*>(item->widget())) {
					label->setMinimumWidth(0);
					widest = std::max(widest, label->sizeHint().width());
					labels << label;
				}
	for (QLabel* label : labels)
		label->setMinimumWidth(widest);
}

void Inspector::sync() {
	const Track* t = app->curTrack();
	setEnabled(t != nullptr);
	head->setText(t ? QStringLiteral("<b>%1</b>").arg(ui(t->name).toHtmlEscaped().toUpper()) : tr("No track selected"));
	if (!t) return;
	if (!volume->isSliderDown()) volume->setValue(int(std::lround(t->vol * 100.f)));
	if (!pan->isHeld()) pan->setValue(t->pan * 0.5f + 0.5f);
	if (!echo->isHeld()) echo->setValue(t->isDrums() ? t->kit.p[DP_DELAY] : t->patch.p[P_DELAY]);
	if (!reverb->isHeld()) reverb->setValue(t->isDrums() ? t->kit.p[DP_REVERB] : t->patch.p[P_REVERB]);
	pump->setVisible(!t->isDrums()); // the drums make the pump happen
	if (!pump->isHeld()) pump->setValue(t->duck);
	mute->setChecked(t->mute);
	solo->setChecked(t->solo);
	const QColor c = theme::trackColor(t->role);
	for (Knob* k : {pan, echo, reverb, pump})
		k->setAccent(c);
}
