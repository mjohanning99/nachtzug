// The program's own controls: the knob, the transport's display, the level meters, the sliding switch, the note
// with an Undo key, the layout that flows controls into rows, the logo at the top left and a label that shortens.
//
// What's here: Knob, Lcd, VuMeter, Switch, Toast, FlowLayout, Emblem, ElideLabel; declared in widgets.h.
// Changes in the song file: none.
#include "widgets.h"
#include <QLabel>
#include "model.h"
#include "app.h"
#include <QDateTime>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <functional>

// ---------------------------------------------------------------- Knob

Knob::Knob(const QString& l, QWidget* parent) : QWidget(parent), label(l) {
	setFixedSize(sizeHint());
	setCursor(Qt::SizeVerCursor);
	setFocusPolicy(Qt::NoFocus);
	setToolTip(tr("Drag up or down. Hold Shift for small steps. Double-click to reset."));
}

void Knob::setValue(float v) {
	v = std::clamp(v, 0.f, 1.f);
	if (v == val) return;
	val = v;
	update();
}

std::function<int(const QString&)> Knob::midiLookup;

// a knob a MIDI controller turns has a small ring at its top left
void Knob::drawMidiMark(QPainter& p, const QRectF& dial) const {
	const QString target = property("midiTarget").toString();
	if (target.isEmpty() || !midiLookup || midiLookup(target) < 0) return;
	p.save();
	p.setRenderHint(QPainter::Antialiasing);
	p.setPen(QPen(theme::ice, 1.6));
	p.setBrush(Qt::NoBrush);
	p.drawEllipse(QPointF(dial.left() - 1, dial.top() + 4), 3, 3);
	p.restore();
}

void Knob::paintEvent(QPaintEvent*) {
	QPainter p(this);
	if (!isEnabled()) p.setOpacity(0.45); // a knob that does nothing just now looks faded
	p.setRenderHint(QPainter::Antialiasing);
	p.scale(scale, scale); // everything below is drawn at the normal size
	const int W = int(width() / scale);
	const QRectF dial((W - 44) / 2.0, 2, 44, 44);
	if (theme::tactile) { // a ribbed black knob on a printed scale, with a coloured ring showing the value
		const QPointF c = dial.center();
		const double a0 = 225.0, span = 270.0;
		p.setPen(QPen(theme::dim, 1));
		for (int i = 0; i <= 10; i++) { // printed scale
			const double a = (a0 - span * i / 10) * M_PI / 180.0, r1 = 20.5, r2 = i % 5 == 0 ? 23.0 : 22.0;
			p.setPen(QPen(theme::dim, i % 5 == 0 ? 1.4 : 0.9, Qt::SolidLine, Qt::RoundCap));
			p.drawLine(QPointF(c.x() + r1 * std::cos(a), c.y() - r1 * std::sin(a)),
			           QPointF(c.x() + r2 * std::cos(a), c.y() - r2 * std::sin(a)));
		}
		const QRectF ring(c.x() - 18, c.y() - 18, 36, 36);
		p.setPen(QPen(theme::gridBar, 2.5, Qt::SolidLine, Qt::FlatCap));
		p.drawArc(ring, int(a0 * 16), int(-span * 16));
		p.setPen(QPen(accent, 2.5, Qt::SolidLine, Qt::FlatCap));
		p.drawArc(ring, int(a0 * 16), int(-span * 16 * val));
		// soft shadow under the knob
		p.setPen(Qt::NoPen);
		for (int i = 0; i < 4; i++) {
			p.setBrush(QColor(0, 0, 0, 22 - i * 5));
			p.drawEllipse(c + QPointF(0, 2.0 + i * 0.6), 14.5 + i, 14.5 + i);
		}
		// ribbed skirt
		p.setBrush(QColor(0x1f, 0x1c, 0x1a));
		for (int i = 0; i < 28; i++) {
			const double a = 2 * M_PI * i / 28;
			p.drawEllipse(QPointF(c.x() + 13.4 * std::cos(a), c.y() + 13.4 * std::sin(a)), 1.7, 1.7);
		}
		QLinearGradient skirt(c.x(), c.y() - 14, c.x(), c.y() + 14);
		skirt.setColorAt(0, QColor(0x45, 0x40, 0x3c));
		skirt.setColorAt(1, QColor(0x14, 0x12, 0x11));
		p.setBrush(skirt);
		p.drawEllipse(c, 13.4, 13.4);
		QRadialGradient capG(c + QPointF(-3, -4), 13);
		capG.setColorAt(0, QColor(0x4e, 0x48, 0x44));
		capG.setColorAt(1, QColor(0x1c, 0x19, 0x18));
		p.setPen(QPen(QColor(0, 0, 0, 140), 0.8));
		p.setBrush(capG);
		p.drawEllipse(c, 10, 10);
		const double a = (a0 - span * double(val)) * M_PI / 180.0;
		p.setPen(QPen(theme::capInk, 2.4, Qt::SolidLine, Qt::RoundCap));
		p.drawLine(QPointF(c.x() + 4 * std::cos(a), c.y() - 4 * std::sin(a)),
		           QPointF(c.x() + 9.5 * std::cos(a), c.y() - 9.5 * std::sin(a)));
		if (marked) {
			p.setPen(Qt::NoPen);
			p.setBrush(QColor(0xc4, 0x36, 0x1c)); // the one red (mute, record, the VU’s red zone)
			p.drawEllipse(QPointF(dial.right() + 1, dial.top() + 3), 3, 3);
		}
		drawMidiMark(p, dial);
		if (caption_) {
			showCaption();
			return;
		}
		QFont f = font();
		f.setPixelSize(11);
		f.setLetterSpacing(QFont::AbsoluteSpacing, 0.3);
		p.setFont(f);
		const bool showValue = (dragging || hover) && fmt;
		p.setPen(showValue ? theme::ink : theme::dim);
		p.drawText(QRect(0, 48, W, 30), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
		           showValue ? fmt(val) : label);
		return;
	}
	QPen track(theme::slot(), 4, Qt::SolidLine, Qt::RoundCap);
	p.setPen(track);
	p.drawArc(dial.adjusted(2, 2, -2, -2), 225 * 16, -270 * 16);
	p.setPen(QPen(accent, 4, Qt::SolidLine, Qt::RoundCap));
	p.drawArc(dial.adjusted(2, 2, -2, -2), 225 * 16, int(-270.f * 16.f * val));
	const QPointF c = dial.center();
	p.setPen(QPen(theme::capEdge, 2));
	p.setBrush(theme::cap);
	p.drawEllipse(c, 15, 15);
	const double a = (225.0 - 270.0 * double(val)) * M_PI / 180.0;
	p.setPen(QPen(theme::capInk, 2, Qt::SolidLine, Qt::RoundCap));
	p.drawLine(QPointF(c.x() + 5 * std::cos(a), c.y() - 5 * std::sin(a)),
	           QPointF(c.x() + 13 * std::cos(a), c.y() - 13 * std::sin(a)));
	if (marked) {
		p.setPen(Qt::NoPen);
		p.setBrush(theme::lcd);
		p.drawEllipse(QPointF(dial.right() + 1, dial.top() + 4), 3, 3);
	}
	drawMidiMark(p, dial);
	if (caption_) {
		showCaption();
		return;
	}
	QFont f = font();
	f.setPixelSize(11);
	p.setFont(f);
	const bool showValue = (dragging || hover) && fmt;
	p.setPen(showValue ? theme::ink : theme::dim);
	p.drawText(QRect(0, 48, W, 30), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, showValue ? fmt(val) : label);
}

void Knob::setCaption(QLabel* caption) {
	caption_ = caption;
	setFixedSize(sizeHint());
	showCaption();
}

void Knob::showCaption() {
	if (!caption_) return;
	const QString t = (dragging || hover) && fmt ? fmt(val) : label.toUpper();
	if (caption_->text() != t) caption_->setText(t);
}

void Knob::mousePressEvent(QMouseEvent* e) {
	if (e->button() != Qt::LeftButton) {
		QWidget::mousePressEvent(e);
		return;
	}
	dragging = true;
	startY = int(e->globalPosition().y());
	startVal = val;
	emit pressed();
	update();
}
void Knob::mouseMoveEvent(QMouseEvent* e) {
	if (!dragging) return;
	const float range = (e->modifiers() & Qt::ShiftModifier) ? 700.f : 170.f;
	const float v = std::clamp(startVal + float(startY - int(e->globalPosition().y())) / range, 0.f, 1.f);
	if (v != val) {
		val = v;
		update();
		emit moved(v);
	}
}
void Knob::mouseReleaseEvent(QMouseEvent*) {
	if (!dragging) return;
	dragging = false;
	emit released();
	update();
}
void Knob::mouseDoubleClickEvent(QMouseEvent*) {
	emit pressed();
	val = def;
	emit moved(def);
	emit released();
	update();
}
void Knob::enterEvent(QEnterEvent*) {
	hover = true;
	update();
}
void Knob::leaveEvent(QEvent*) {
	hover = false;
	update();
}

// ---------------------------------------------------------------- Lcd

Lcd::Lcd(App* a, QWidget* parent) : QWidget(parent), app(a) {
	setMinimumSize(210, 58);
	setToolTip(tr("The display: where the song is (bar and beat), which track the letter keys play, and short "
	              "messages. The wave on the left is what you hear."));
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void Lcd::setMessage(const QString& m) {
	message = m;
	messageUntil = QDateTime::currentMSecsSinceEpoch() + 3500;
	update();
}

void Lcd::paintEvent(QPaintEvent*) {
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);
	// the glass sunk into the panel behind a thin frame, as the tempo box and the level meters
	const QRectF glass = QRectF(rect()).adjusted(1, 1, -1, -1);
	if (theme::tactile) {
		p.setPen(Qt::NoPen);
		p.setBrush(theme::rule.darker(125));
		p.drawRoundedRect(QRectF(rect()), 4.5, 4.5);
		theme::drawIvory(p, glass, 4);
		QLinearGradient shade(0, glass.top(), 0, glass.top() + 8);
		shade.setColorAt(0, QColor(0, 0, 0, 60));
		shade.setColorAt(1, Qt::transparent);
		p.setBrush(shade);
		p.drawRoundedRect(glass, 4, 4);
	} else {
		p.setPen(theme::lcdBg.lightness() > 128 ? QPen(theme::rule, 1) : QPen(Qt::NoPen));
		p.setBrush(theme::lcdBg);
		p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
	}

	// oscilloscope
	const bool wide = width() >= 200; // the wave picture only when there is room for it beside the text
	const QRect sr(9, 5, wide ? std::clamp(width() - 160, 40, 84) : 0, height() - 10); // the text keeps its room
	const Engine* e = app->engine;
	const int w = e->scopeW.load(std::memory_order_relaxed), N = Engine::SCOPE_N;
	int startIdx = (w + N - 600) % N;
	for (int i = 0; i < 200; i++) { // start on a rising zero crossing so the picture stands still
		const int a = (startIdx + i) % N, b = (a + 1) % N;
		if (e->scope[a].load(std::memory_order_relaxed) < 0.f && e->scope[b].load(std::memory_order_relaxed) >= 0.f) {
			startIdx = b;
			break;
		}
	}
	QPainterPath path;
	for (int x = 0; x < sr.width(); x++) {
		const float v = std::clamp(e->scope[(startIdx + x * 3) % N].load(std::memory_order_relaxed), -1.f, 1.f);
		const QPointF pt(sr.left() + x, sr.center().y() - double(v) * sr.height() * 0.48);
		if (x == 0)
			path.moveTo(pt);
		else
			path.lineTo(pt);
	}
	p.setPen(QPen(theme::lcd, 1.5));
	p.setBrush(Qt::NoBrush);
	if (wide) p.drawPath(path);

	// the type follows the height: 17 and 12 pixels in a tall display, 14 and 10 in the top bar's
	QFont f = theme::lcdFont(std::clamp(height() * 35 / 100 - (width() < 260 ? 3 : 0), 12, 17), true);
	QFont f2 = theme::lcdFont(std::clamp(height() / 4, 9, 12), false);
	p.setFont(f);
	const double m = app->musicalPos();
	const int bar = int(m) / BAR + 1, beat = int(m) % BAR / BEAT + 1;
	// a sign for what the transport is doing, then the position
	const QString state = app->isPlaying() ? (app->recording ? QStringLiteral("● REC") : QStringLiteral("▶"))
	                                       : (app->recording ? QStringLiteral("○ REC") : QStringLiteral("■"));
	const QString line1 = QStringLiteral("%1  ").arg(state) + tr("BAR %1 · BEAT %2").arg(bar).arg(beat);
	QString line2;
	if (QDateTime::currentMSecsSinceEpoch() < messageUntil)
		line2 = message;
	else if (app->pendingChanges() > 0)
		line2 = tr("CHANGE LANDS ON THE NEXT BAR");
	else if (app->recording && app->isPlaying())
		line2 = tr("RECORDING: PLAY KEYS, TURN KNOBS");
	else if (app->recording)
		line2 = tr("RECORD IS ARMED: PRESS PLAY");
	else if (!everPlayed && !app->isPlaying())
		line2 = tr("PRESS SPACE TO PLAY");
	else if (const Track* t = app->song.track(app->selTrack))
		line2 = tr("KEYS PLAY: %1").arg(ui(t->name).toUpper());
	if (app->isPlaying()) everPlayed = true;
	const int tx = wide ? sr.right() + 13 : 12;
	const int tw = width() - tx - 13; // keep clear of the level meter on the right
	if (height() < 34) {              // one line: the position on the left, messages on the right
		const int w1 = p.fontMetrics().horizontalAdvance(line1) + 24;
		p.drawText(QRect(tx, 0, tw, height()), Qt::AlignLeft | Qt::AlignVCenter, line1);
		p.setFont(f2);
		p.drawText(QRect(tx + w1, 0, tw - w1, height()), Qt::AlignRight | Qt::AlignVCenter,
		           p.fontMetrics().elidedText(line2.toUpper(), Qt::ElideLeft, tw - w1));
	} else {
		const int lh = (height() - 6) / 2;
		// in a narrow display (or a language with longer words) the position gets a smaller type rather than cut short
		while (f.pixelSize() > 10 && QFontMetrics(f).horizontalAdvance(line1) > tw)
			f.setPixelSize(f.pixelSize() - 1);
		p.setFont(f);
		p.drawText(QRect(tx, 3, tw, lh), Qt::AlignLeft | Qt::AlignVCenter,
		           p.fontMetrics().elidedText(line1, Qt::ElideRight, tw));
		p.setFont(f2);
		p.drawText(QRect(tx, 3 + lh, tw, lh), Qt::AlignLeft | Qt::AlignVCenter,
		           p.fontMetrics().elidedText(line2.toUpper(), Qt::ElideRight, tw));
	}
	if (theme::tactile)
		p.drawLine(QPointF(tx - 7, 6), QPointF(tx - 7, height() - 6)); // a printed line between the wave and the text

	// output level
	const float pk = std::max(e->peakL.load(std::memory_order_relaxed), e->peakR.load(std::memory_order_relaxed));
	const int mh = int(std::clamp(pk, 0.f, 1.f) * (height() - 12));
	p.setPen(Qt::NoPen);
	p.setBrush(theme::lcd);
	p.drawRect(width() - 8, height() - 6 - mh, 3, mh);
	if (theme::tactile) { // the light on the glass, over everything shown behind it
		QLinearGradient refl(glass.topLeft(), QPointF(glass.left() + glass.width() * 0.25, glass.bottom()));
		refl.setColorAt(0, QColor(255, 255, 255, 110));
		refl.setColorAt(0.44, QColor(255, 255, 255, 18));
		refl.setColorAt(0.45, Qt::transparent);
		refl.setColorAt(1, Qt::transparent);
		p.setBrush(refl);
		p.drawRoundedRect(glass, 4, 4);
	}
}

// ---------------------------------------------------------------- VuMeter

VuMeter::VuMeter(const QString& l, QWidget* parent) : QWidget(parent), label(l) {
	setFixedSize(sizeHint());
	setToolTip(tr("How loud the song is. When the needle stays in the red, turn tracks down in the mixer."));
}

void VuMeter::feed(float peak) {
	// decibels from -20 to +3 along the scale; the needle rises fast and falls slowly
	const float db = 20.f * std::log10(std::max(peak, 1e-4f));
	const float target = std::clamp((db + 20.f) / 23.f, 0.f, 1.f);
	needle = target > needle ? needle + (target - needle) * 0.55f : needle + (target - needle) * 0.12f;
	update();
}

// A VU meter's face: the arc from -20 to +3 over ninety degrees, wider near 0 as on a real meter, red from 0 up, the
// numbers outside the arc, VU in the middle, the channel at the corner, and the pivot under a black cover. The face
// sits behind a thin frame, as the display and the tempo box.
void VuMeter::paintEvent(QPaintEvent*) {
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);
	const QRectF r = QRectF(rect());
	const QRectF face = r.adjusted(1, 1, -1, -1);
	p.setPen(Qt::NoPen);
	p.setBrush(theme::tactile ? theme::rule.darker(125) : theme::rule);
	p.drawRoundedRect(r, 4.5, 4.5);
	theme::drawIvory(p, face, 4);
	const bool darkFace = theme::lcdBg.lightness() < 128;
	const QColor marks = darkFace ? theme::lcd : QColor(0x3a, 0x33, 0x2e), red(0xc4, 0x36, 0x1c);
	p.save();
	QPainterPath clip;
	clip.addRoundedRect(face, 4, 4);
	p.setClipPath(clip);
	const double R = std::min(face.height() * 0.78, face.width() * 0.4);
	const QPointF pivot(face.center().x(), face.top() + std::min(face.height() + 9, R + 25));
	// dB to the angle from upright, in degrees
	static const double pts[][2] = {{-20, -45}, {-10, -28}, {-7, -19}, {-5, -10}, {-3, 0}, {-2, 6},
	                                {-1, 12},   {0, 20},    {1, 27},   {2, 35},   {3, 45}};
	auto angle = [&](double db) {
		for (int k = 0; k < 10; k++)
			if (db <= pts[k + 1][0])
				return pts[k][1] + (pts[k + 1][1] - pts[k][1]) * (db - pts[k][0]) / (pts[k + 1][0] - pts[k][0]);
		return 45.0;
	};
	auto at = [&](double deg, double rr) {
		const double a = (deg - 90.0) * M_PI / 180.0;
		return QPointF(pivot.x() + rr * std::cos(a), pivot.y() + rr * std::sin(a));
	};
	auto arc = [&](double d0, double d1, double rr) {
		QPainterPath path(at(d0, rr));
		for (int k = 1; k <= 16; k++)
			path.lineTo(at(d0 + (d1 - d0) * k / 16, rr));
		p.drawPath(path);
	};
	p.setBrush(Qt::NoBrush);
	p.setPen(QPen(marks, 0.9));
	arc(-45, 20, R);
	p.setPen(QPen(red, 2.6, Qt::SolidLine, Qt::FlatCap));
	arc(20, 45, R + 1.3);
	QFont f = font();
	f.setPixelSize(7);
	f.setWeight(QFont::Medium);
	p.setFont(f);
	for (const auto& pt : pts) {
		const double db = pt[0], a = pt[1];
		const bool big = db == -20 || db == -10 || db == -5 || db == 0 || db == 3;
		p.setPen(QPen(db > 0 ? red : marks, 1));
		p.drawLine(at(a, R), at(a, R + (big ? 4.5 : 3)));
		if (big) {
			const QPointF c = at(a, R + 9.5);
			p.drawText(QRectF(c.x() - 10, c.y() - 5, 20, 10), Qt::AlignCenter,
			           db > 0 ? QStringLiteral("+%1").arg(int(db)) : QString::number(int(db)));
		}
	}
	f.setPixelSize(8);
	f.setWeight(QFont::DemiBold);
	f.setLetterSpacing(QFont::AbsoluteSpacing, 1);
	p.setFont(f);
	p.setPen(marks);
	p.drawText(QRectF(face.left(), face.bottom() - 25, face.width(), 10), Qt::AlignCenter, QStringLiteral("VU"));
	f.setPixelSize(7); // whole pixels only (7.5 was rounded down to 7 anyway; Clang on the Mac refuses it)
	f.setLetterSpacing(QFont::AbsoluteSpacing, 0);
	p.setFont(f);
	p.drawText(QRectF(face.left(), face.bottom() - 12, face.width() - 5, 10), Qt::AlignRight | Qt::AlignVCenter, label);
	p.setPen(QPen(darkFace ? theme::lcd.lighter(115) : QColor(0x1d, 0x1a, 0x18), 1.1, Qt::SolidLine, Qt::RoundCap));
	p.drawLine(pivot, at(angle(needle * 23.0 - 20.0), R + 3));
	// the cover over the pivot
	p.setPen(Qt::NoPen);
	p.setBrush(darkFace ? theme::cap.lighter(130) : QColor(0x2a, 0x26, 0x24));
	p.drawEllipse(QPointF(face.center().x(), face.bottom() + 1), 15, 11);
	p.restore();
	if (theme::tactile) theme::drawGlass(p, face, 4);
}

// ---------------------------------------------------------------- Switch

Switch::Switch(const QString& l, Label where, QWidget* parent)
    : QAbstractButton(parent), where_(where), slide_(new QVariantAnimation(this)) {
	setText(l);
	setCheckable(true);
	setCursor(Qt::PointingHandCursor);
	setFocusPolicy(Qt::NoFocus);
	if (where_ == Above)
		setFixedSize(sizeHint());
	else
		setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	slide_->setDuration(140);
	slide_->setEasingCurve(QEasingCurve::OutCubic);
	connect(slide_, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
		at_ = v.toDouble();
		update();
	});
	connect(this, &QAbstractButton::toggled, this, [this](bool on) {
		slide_->stop();
		if (!isVisible()) { // nothing to see slide: straight there
			at_ = on ? 1.0 : 0.0;
			update();
			return;
		}
		slide_->setStartValue(at_);
		slide_->setEndValue(on ? 1.0 : 0.0);
		slide_->start();
	});
}

QSize Switch::sizeHint() const {
	if (where_ == Above) return {64, 40};
	return {34 + 8 + fontMetrics().horizontalAdvance(text()) + 2, 22};
}

void Switch::paintEvent(QPaintEvent*) {
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);
	QRectF track;
	if (where_ == Above) {
		QFont f = font();
		f.setPixelSize(12);
		p.setFont(f);
		p.setPen(isEnabled() ? theme::dim : theme::rule);
		p.drawText(QRect(0, 0, width(), 15), Qt::AlignCenter, text());
		track = QRectF((width() - 34) / 2.0, 19, 34, 16);
	} else {
		track = QRectF(0.5, (height() - 16) / 2.0, 34, 16);
		p.setPen(isEnabled() ? theme::ink : theme::rule);
		p.drawText(QRectF(track.right() + 8, 0, width() - track.right() - 8, height()),
		           Qt::AlignVCenter | Qt::AlignLeft, text());
	}
	const double t = at_;
	auto blend = [t](const QColor& a, const QColor& b) {
		return QColor::fromRgbF(float(a.redF() + (b.redF() - a.redF()) * t),
		                        float(a.greenF() + (b.greenF() - a.greenF()) * t),
		                        float(a.blueF() + (b.blueF() - a.blueF()) * t));
	};
	p.setPen(QPen(blend(theme::rule, theme::amber.darker(130)), 1));
	p.setBrush(blend(theme::slot(), theme::amber));
	p.drawRoundedRect(track, 8, 8);
	if (t < 1.0) { // the slot's shadow, fading as it fills
		QLinearGradient sh(0, track.top(), 0, track.top() + 5);
		sh.setColorAt(0, QColor(0, 0, 0, int(40 * (1.0 - t))));
		sh.setColorAt(1, Qt::transparent);
		p.setPen(Qt::NoPen);
		p.setBrush(sh);
		p.drawRoundedRect(track, 8, 8);
	}
	const QPointF knobAt(track.left() + 8 + (track.width() - 16) * t, track.center().y());
	p.setPen(Qt::NoPen);
	p.setBrush(QColor(0, 0, 0, 40));
	p.drawEllipse(knobAt + QPointF(0, 1.2), 7, 7);
	QRadialGradient kg(knobAt + QPointF(-2, -2), 8);
	kg.setColorAt(0, QColor(0xff, 0xfd, 0xf8));
	kg.setColorAt(1, QColor(0xdf, 0xd7, 0xc8));
	p.setPen(QPen(QColor(0, 0, 0, 70), 0.8));
	p.setBrush(kg);
	p.drawEllipse(knobAt, 6.5, 6.5);
}

// ---------------------------------------------------------------- Toast

Toast::Toast(QWidget* parent) : QWidget(parent), hideTimer(new QTimer(this)) {
	hideTimer->setSingleShot(true);
	connect(hideTimer, &QTimer::timeout, this, &QWidget::hide);
	setMouseTracking(true);
	setAttribute(Qt::WA_NoSystemBackground);
	parent->installEventFilter(this); // stays at the bottom centre when the window is resized
	hide();
}

void Toast::showMessage(const QString& t, const QString& a, int ms) {
	text = t;
	action = a;
	hoverAction = false;
	QFont f = font();
	f.setPixelSize(14);
	f.setWeight(QFont::DemiBold);
	const QFontMetrics fm(f);
	const int w = 28 + fm.horizontalAdvance(text) + (action.isEmpty() ? 0 : 22 + fm.horizontalAdvance(action) + 26) + 6;
	resize(std::min(w, parentWidget()->width() - 40), 44);
	place();
	show();
	raise();
	update();
	hideTimer->start(ms);
}

void Toast::place() {
	const QWidget* p = parentWidget();
	move((p->width() - width()) / 2, p->height() - height() - 44);
}

QRect Toast::actionRect() const {
	if (action.isEmpty()) return {};
	QFont f = font();
	f.setPixelSize(14);
	f.setWeight(QFont::DemiBold);
	const int aw = QFontMetrics(f).horizontalAdvance(action) + 26;
	return {width() - aw - 8, 8, aw, height() - 16};
}

// an ivory plate in the displays' colour, its corners the keys' radius, with a real key for the action (Undo)
void Toast::paintEvent(QPaintEvent*) {
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);
	const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
	p.setPen(Qt::NoPen);
	p.setBrush(QColor(0, 0, 0, 50));
	p.drawRoundedRect(r.translated(0, 2), 6, 6); // a soft shadow under it
	QLinearGradient body(0, r.top(), 0, r.bottom());
	body.setColorAt(0, theme::lcdBg.lighter(106));
	body.setColorAt(1, theme::lcdBg.darker(104));
	p.setBrush(body);
	p.setPen(QPen(theme::rule.darker(130), 1.5));
	p.drawRoundedRect(r.adjusted(0, 0, 0, -2), 6, 6);
	QFont f = font();
	f.setPixelSize(14);
	f.setWeight(QFont::DemiBold);
	p.setFont(f);
	p.setPen(theme::lcdBg.lightness() < 128 ? theme::lcd : theme::ink);
	const QRect ar = actionRect();
	p.drawText(QRect(16, 0, (ar.isNull() ? width() : ar.left()) - 22, height() - 2), Qt::AlignVCenter | Qt::AlignLeft,
	           p.fontMetrics().elidedText(text, Qt::ElideRight, (ar.isNull() ? width() : ar.left()) - 26));
	if (!ar.isNull()) { // a key as on the panel: its cap, its edge, lit under the mouse
		const QRectF k = QRectF(ar).adjusted(0.5, 0.5, -0.5, -2.5);
		QLinearGradient kg(0, k.top(), 0, k.bottom());
		kg.setColorAt(0, hoverAction ? theme::wellHi.lighter(108) : theme::wellHi.lighter(103));
		kg.setColorAt(0.86, theme::mix(theme::wellHi, theme::rule, 0.18));
		kg.setColorAt(0.88, theme::rule.darker(125));
		kg.setColorAt(1, theme::rule.darker(125));
		p.setBrush(kg);
		p.setPen(QPen(theme::rule, 1));
		p.drawRoundedRect(k, 4, 4);
		p.setPen(theme::ink);
		p.drawText(ar.adjusted(0, 0, 0, -4), Qt::AlignCenter, action);
	}
}

void Toast::mousePressEvent(QMouseEvent* e) {
	if (actionRect().contains(e->pos())) {
		hide();
		emit actionClicked();
	} else
		hide(); // a click anywhere else on it dismisses it
}
void Toast::mouseMoveEvent(QMouseEvent* e) {
	const bool h = actionRect().contains(e->pos());
	if (h != hoverAction) {
		hoverAction = h;
		setCursor(h ? Qt::PointingHandCursor : Qt::ArrowCursor);
		update();
	}
}
void Toast::enterEvent(QEnterEvent*) {
	hideTimer->stop();
}
void Toast::leaveEvent(QEvent*) {
	hoverAction = false;
	update();
	hideTimer->start(2500);
}
bool Toast::eventFilter(QObject* o, QEvent* e) {
	if (o == parentWidget() && e->type() == QEvent::Resize && isVisible()) place();
	return QWidget::eventFilter(o, e);
}

// ---------------------------------------------------------------- FlowLayout

QSize FlowLayout::sizeHint() const { // everything on one row
	int w = 0, hgt = 0, n = 0;
	for (QLayoutItem* it : items_) {
		if (it->isEmpty()) continue;
		const QSize s = it->sizeHint();
		w += s.width() + (n++ ? h_ : 0);
		hgt = std::max(hgt, s.height());
	}
	const QMargins m = contentsMargins();
	return QSize(w + m.left() + m.right(), hgt + m.top() + m.bottom());
}

QSize FlowLayout::minimumSize() const { // as narrow as the widest single item
	QSize s;
	for (QLayoutItem* it : items_)
		if (!it->isEmpty()) s = s.expandedTo(it->sizeHint());
	const QMargins m = contentsMargins();
	return s + QSize(m.left() + m.right(), m.top() + m.bottom());
}

int FlowLayout::place(const QRect& r, bool apply) const {
	const QMargins m = contentsMargins();
	const QRect area = r.adjusted(m.left(), m.top(), -m.right(), -m.bottom());
	// first the rows: as many items as fit side by side
	std::vector<std::vector<QLayoutItem*>> rows(1);
	int x = 0;
	for (QLayoutItem* it : items_) {
		if (it->isEmpty()) continue;
		const int w = it->sizeHint().width();
		if (!rows.back().empty() && x + w > area.width()) {
			rows.emplace_back();
			x = 0;
		}
		rows.back().push_back(it);
		x += w + h_;
	}
	int y = area.y();
	for (const auto& row : rows) {
		if (row.empty()) continue;
		int used = -h_, lineH = 0, growW = 0;
		for (QLayoutItem* it : row) {
			const QSize s = it->sizeHint();
			used += s.width() + h_;
			lineH = std::max(lineH, justify_ ? std::max(s.height(), it->heightForWidth(s.width())) : s.height());
			if (it->maximumSize().width() > s.width()) growW += s.width();
		}
		const int extra = justify_ && growW > 0 ? std::max(0, area.width() - used) : 0;
		if (apply) {
			int cx = area.x(), given = 0, growSeen = 0;
			for (QLayoutItem* it : row) {
				const QSize s = it->sizeHint();
				int w = s.width();
				if (extra > 0 &&
				    it->maximumSize().width() > s.width()) { // a share of the extra, the last one takes what is left
					growSeen += s.width();
					const int upTo = int(std::lround(double(extra) * growSeen / growW));
					w += upTo - given;
					given = upTo;
				}
				it->setGeometry(QRect(cx, y, w, justify_ ? lineH : s.height()));
				cx += w + h_;
			}
		}
		y += lineH + v_;
	}
	if (y == area.y()) return m.top() + m.bottom(); // nothing to show
	return y - v_ - r.y() + m.bottom();
}

// ---------------------------------------------------------------- Emblem, ElideLabel

// the sign, and the program's name under it in Nachtzug Grotesk with its tz ligature, as on the manual's cover and
// the website; in a narrow place only the sign
void Emblem::paintEvent(QPaintEvent*) {
	if (std::min(width(), height()) < 40) return; // too small to be worth drawing
	QPainter p(this);
	p.setRenderHint(QPainter::SmoothPixmapTransform);
	p.setRenderHint(QPainter::TextAntialiasing);
	QFont f(QStringLiteral("Nachtzug Grotesk"));
	f.setWeight(QFont::DemiBold);
	f.setPixelSize(18);
	// the t and z joined on one rail; liga is on by default, so a Qt before 6.7 (Debian 12, Ubuntu 24.04), which
	// can't name features, still joins them
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
	f.setFeature(QFont::Tag("liga"), 1);
	f.setFeature(QFont::Tag("dlig"), 1);
#endif
	const QString name = QStringLiteral("Nachtzug");
	const bool named = width() >= QFontMetrics(f).horizontalAdvance(name) + 4 && height() >= 80;
	const int side = named ? 56 : std::min(width(), height());
	const int top = named ? 2 : (height() - side) / 2;
	p.drawPixmap(QRect((width() - side) / 2, top, side, side), theme::logo(192));
	if (named) {
		p.setFont(f);
		p.setPen(theme::ink);
		p.drawText(QRect(0, top + side + 2, width(), 24), Qt::AlignHCenter | Qt::AlignVCenter, name);
	}
}

void ElideLabel::paintEvent(QPaintEvent*) {
	if (width() < 60) return;
	QPainter p(this);
	p.setPen(theme::dim);
	p.setFont(font());
	p.drawText(rect(), Qt::AlignVCenter | Qt::AlignRight, fontMetrics().elidedText(text_, Qt::ElideRight, width()));
}
