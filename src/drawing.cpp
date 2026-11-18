// Drawing the instrument's parts that the style sheet can't: clips and keys with depth, the screwed wells, the
// ivory displays and their glass, sliders, and the icons (tracks, transport, the keys above the timeline, the
// mixer, lamps), the Nachtzug logo and Nachtzug's own cursors.
//
// What's here: logo(), blockFill(), drawBlock(), drawKey(), drawSlider(), drawRoleIcon(), roleIcon(), mixerIcon(),
// drawGlass(), drawSunken(), drawWell(), drawIvory(), ledIcon(), transportIcon(), drawSymbol(), symbolIcon(),
// symbolKey(), dressSymbolKeys(), appIcon(); used by
// every panel that paints itself. cursor(), setOwnCursor() and applyCursorLook() give the cursors (the knob, the
// pencil, the scissors …), at the size of the system's, to the panels where a click does something particular.
// Changes in the song file: none.
#include "widgets.h"
#include "model.h"
#include <QPainter>
#include <QPainterPath>
#include <QImage>
#include <QSettings>
#include <QToolButton>
#include <QApplication>
#include <QCursor>
#include <QHash>
#include <QScreen>
#include <QSlider>
#include <QEvent>
#include <cmath>

namespace theme {

QPixmap logo(int size) {
	// the enamel sign; at 24 px and below its simpler version, without the rule, so the tz still reads
	static const QImage picture(QStringLiteral(":/logo.png")), small(QStringLiteral(":/logo-small.png"));
	const QImage& source = size <= 24 && !small.isNull() ? small : picture;
	if (!source.isNull()) {
		QPixmap pm = QPixmap::fromImage(source.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
		return pm;
	}
	QPixmap pm(size, size); // fallback: the first, drawn version of the mark
	pm.fill(Qt::transparent);
	QPainter p(&pm);
	p.setRenderHint(QPainter::Antialiasing);
	const double s = size / 64.0;
	p.scale(s, s);
	const QColor night(0x2b, 0x26, 0x23), moon(0xed, 0x9d, 0x24), cream(0xec, 0xe8, 0xdf), rust(0xa9, 0x27, 0x0a);
	p.setPen(Qt::NoPen);
	p.setBrush(night);
	p.drawRoundedRect(QRectF(2, 2, 60, 60), 14, 14);
	p.setBrush(moon);
	p.drawEllipse(QPointF(45, 19), 9, 9);
	QPainterPath body;
	body.moveTo(8, 46);
	body.lineTo(8, 34);
	body.quadTo(8, 30, 12, 30);
	body.lineTo(42, 30);
	body.quadTo(54, 31, 57, 43);
	body.lineTo(57, 46);
	body.closeSubpath();
	p.setBrush(cream);
	p.drawPath(body);
	p.setBrush(rust);
	p.drawRoundedRect(QRectF(6, 49, 52, 3.5), 1.75, 1.75);
	return pm;
}

QColor blockFill(const QColor& track, bool hover, bool playing) {
	QColor fill = track;
	if (tactile) {
		if (!playing) fill = fill.lighter(hover ? 108 : 118);
	} else
		fill.setAlpha(playing ? 255 : hover ? 215 : 150);
	return fill;
}

void drawBlock(QPainter& p, const QRectF& r, const QColor& c, double radius, bool raised) {
	p.save();
	p.setRenderHint(QPainter::Antialiasing);
	if (!tactile) {
		p.setPen(Qt::NoPen);
		p.setBrush(c);
		p.drawRoundedRect(r, radius, radius);
		p.restore();
		return;
	}
	if (raised && r.height() > 6) { // soft shadow under the block
		p.setPen(Qt::NoPen);
		for (int i = 0; i < 3; i++) {
			p.setBrush(QColor(0, 0, 0, 26 - i * 8));
			p.drawRoundedRect(r.translated(0, 1.0 + i).adjusted(-i * 0.5, 0, i * 0.5, 0), radius + i, radius + i);
		}
	}
	QLinearGradient g(r.topLeft(), r.bottomLeft());
	g.setColorAt(0, c.lighter(106));
	g.setColorAt(1, c.darker(108));
	p.setPen(QPen(c.darker(140), 1));
	p.setBrush(g);
	p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
	if (r.height() > 8) { // a thin light edge along the top, like a moulded part
		p.setPen(QPen(QColor(255, 255, 255, 70), 1));
		p.drawLine(QPointF(r.left() + radius, r.top() + 1.5), QPointF(r.right() - radius, r.top() + 1.5));
	}
	p.restore();
}

void drawKey(QPainter& p, const QRectF& r, const QString& text, bool on, const QColor& onColour) {
	p.save();
	p.setRenderHint(QPainter::Antialiasing);
	if (!tactile) {
		p.setPen(on ? QPen(Qt::NoPen) : QPen(rule, 1));
		p.setBrush(on ? onColour : panel);
		p.drawRoundedRect(r, 3, 3);
		p.setPen(!on ? ink : onColour == key ? keyInk : accentInk);
		p.drawText(r, Qt::AlignCenter, text);
		p.restore();
		return;
	}
	const double depth = on ? 1.0 : 2.5;
	p.setPen(Qt::NoPen);
	p.setBrush(on ? onColour.darker(150) : rule.darker(120));
	p.drawRoundedRect(r.adjusted(0, 1, 0, 0), 3.5, 3.5); // the side of the key
	const QRectF face = r.adjusted(0, on ? 1.5 : 0, 0, -depth);
	QLinearGradient g(face.topLeft(), face.bottomLeft());
	if (on) {
		g.setColorAt(0, onColour.lighter(118));
		g.setColorAt(1, onColour);
	} else {
		g.setColorAt(0, QColor(0xf5, 0xf0, 0xe7));
		g.setColorAt(1, QColor(0xe2, 0xda, 0xcb));
	}
	p.setPen(QPen(on ? onColour.darker(140) : rule, 0.8));
	p.setBrush(g);
	p.drawRoundedRect(face, 3, 3);
	p.setPen(on ? (onColour == key ? keyInk : accentInk) : ink);
	p.drawText(face, Qt::AlignCenter, text);
	p.restore();
}

void drawKey(QPainter& p, const QRectF& r, const QString& text, bool on, const QColor& onColour,
             const QString& symbol) {
	if (symbol.isEmpty()) {
		drawKey(p, r, text, on, onColour);
		return;
	}
	drawKey(p, r, QString(), on, onColour);
	const QColor inkNow = !on ? ink : onColour == key ? keyInk : accentInk;
	const double depth = tactile ? (on ? 1.0 : 2.5) : 0, top = tactile && on ? 1.5 : 0;
	const QRectF face = r.adjusted(0, top, 0, -depth);
	const double s = std::min(14.0, face.height() - 3);
	if (text.isEmpty()) {
		drawSymbol(p, symbol, QRectF(face.center().x() - s / 2, face.center().y() - s / 2, s, s), inkNow,
		           on ? inkNow : amber);
		return;
	}
	const int tw = p.fontMetrics().horizontalAdvance(text);
	const double x0 = face.center().x() - (s + 3 + tw) / 2;
	drawSymbol(p, symbol, QRectF(x0, face.center().y() - s / 2, s, s), inkNow, on ? inkNow : amber);
	p.save();
	p.setPen(inkNow);
	p.drawText(QRectF(x0 + s + 3, face.top(), tw + 2, face.height()), Qt::AlignVCenter | Qt::AlignLeft, text);
	p.restore();
}

void drawSlider(QPainter& p, const QRectF& r, float value, const QColor& fill) {
	p.save();
	p.setRenderHint(QPainter::Antialiasing);
	const double rad = r.height() / 2;
	if (!tactile) {
		p.setPen(Qt::NoPen);
		p.setBrush(slot());
		p.drawRoundedRect(r, rad, rad);
		p.setBrush(fill);
		p.drawRoundedRect(QRectF(r.left(), r.top(), r.width() * double(value), r.height()), rad, rad);
		p.restore();
		return;
	}
	QLinearGradient g(r.topLeft(), r.bottomLeft());
	g.setColorAt(0, gridBar.darker(118));
	g.setColorAt(0.45, gridBar);
	g.setColorAt(1, gridBar.lighter(110));
	p.setPen(QPen(rule, 1));
	p.setBrush(g);
	p.drawRoundedRect(r, rad, rad);
	const QRectF filled(r.left(), r.top(), std::max(r.height(), r.width() * double(value)), r.height());
	QLinearGradient fg(filled.topLeft(), filled.bottomLeft());
	fg.setColorAt(0, fill);
	fg.setColorAt(1, fill.lighter(115));
	p.setPen(QPen(fill.darker(140), 1));
	p.setBrush(fg);
	p.drawRoundedRect(filled, rad, rad);
	// the cap
	const double x = r.left() + r.width() * double(value);
	const QRectF cap(x - 6, r.center().y() - 8, 12, 16);
	p.setPen(Qt::NoPen);
	p.setBrush(QColor(0, 0, 0, 50));
	p.drawRoundedRect(cap.translated(0, 1.5), 2.5, 2.5);
	QLinearGradient cg(cap.topLeft(), cap.bottomLeft());
	cg.setColorAt(0, QColor(0x57, 0x50, 0x4b));
	cg.setColorAt(0.5, QColor(0x2e, 0x2a, 0x27));
	cg.setColorAt(1, QColor(0x1a, 0x17, 0x16));
	p.setPen(QPen(QColor(0x0d, 0x0b, 0x0a), 1));
	p.setBrush(cg);
	p.drawRoundedRect(cap, 2.5, 2.5);
	p.setPen(QPen(amber, 1.6, Qt::SolidLine, Qt::RoundCap));
	p.drawLine(QPointF(x, cap.top() + 3), QPointF(x, cap.bottom() - 3));
	p.restore();
}

// Little symbols, drawn in a 16 x 16 box: bass a low wave, sequence a row of steps, pad a stack of held notes,
// lead a single rising line of notes, drums a drum seen from the side.
void drawRoleIcon(QPainter& p, const QRectF& r, int role, const QColor& c) {
	p.save();
	p.setRenderHint(QPainter::Antialiasing);
	p.translate(r.topLeft());
	p.scale(r.width() / 16.0, r.height() / 16.0);
	QPen pen(c, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
	p.setPen(pen);
	p.setBrush(Qt::NoBrush);
	switch (role) {
	case R_BASS: {
		QPainterPath w;
		w.moveTo(1, 9);
		w.cubicTo(4, 2, 6, 2, 8, 9);
		w.cubicTo(10, 16, 12, 16, 15, 9);
		p.drawPath(w);
		break;
	}
	case R_SEQ: {
		p.setPen(Qt::NoPen);
		p.setBrush(c);
		const double hs[4] = {6, 10, 4, 8};
		for (int i = 0; i < 4; i++)
			p.drawRoundedRect(QRectF(1 + i * 3.8, 14 - hs[i], 2.8, hs[i]), 1, 1);
		break;
	}
	case R_PAD: {
		for (int i = 0; i < 3; i++)
			p.drawLine(QPointF(2, 4 + i * 4), QPointF(14, 4 + i * 4));
		break;
	}
	case R_LEAD: {
		p.setPen(Qt::NoPen);
		p.setBrush(c);
		p.drawEllipse(QPointF(5, 12), 2.6, 2.1);
		p.setPen(pen);
		p.drawLine(QPointF(7.4, 12), QPointF(7.4, 2.5));
		p.drawLine(QPointF(7.4, 2.5), QPointF(12, 5));
		break;
	}
	default: {
		p.drawEllipse(QRectF(2, 3, 12, 4));
		p.drawLine(QPointF(2, 5), QPointF(2, 12));
		p.drawLine(QPointF(14, 5), QPointF(14, 12));
		p.drawArc(QRectF(2, 9.5, 12, 4.5), 180 * 16, 180 * 16);
		break;
	}
	}
	p.restore();
}

QIcon roleIcon(int role, const QColor& c) {
	QPixmap pm(32, 32);
	pm.setDevicePixelRatio(2.0);
	pm.fill(Qt::transparent);
	QPainter p(&pm);
	drawRoleIcon(p, QRectF(0, 0, 16, 16), role, c);
	return QIcon(pm);
}

QIcon mixerIcon(const QColor& c) {
	QPixmap pm(32, 32);
	pm.setDevicePixelRatio(2.0);
	pm.fill(Qt::transparent);
	QPainter p(&pm);
	p.setRenderHint(QPainter::Antialiasing);
	const double x[3] = {3.5, 8, 12.5}, cap[3] = {9.5, 5, 11.5};
	for (int i = 0; i < 3; i++) {
		QColor slot = c;
		slot.setAlphaF(0.5);
		p.setPen(QPen(slot, 1.4, Qt::SolidLine, Qt::RoundCap));
		p.drawLine(QPointF(x[i], 2), QPointF(x[i], 14));
		p.setPen(Qt::NoPen);
		p.setBrush(c);
		p.drawRoundedRect(QRectF(x[i] - 2.2, cap[i] - 1.5, 4.4, 3), 1, 1);
	}
	return QIcon(pm);
}

void drawGlass(QPainter& p, const QRectF& face, double radius) {
	p.save();
	p.setPen(Qt::NoPen);
	QLinearGradient sh(0, face.top(), 0, face.top() + 8);
	sh.setColorAt(0, QColor(0, 0, 0, 70));
	sh.setColorAt(1, Qt::transparent);
	p.setBrush(sh);
	p.drawRoundedRect(QRectF(face.left(), face.top(), face.width(), 8), radius, radius);
	QLinearGradient sl(face.left(), 0, face.left() + 6, 0);
	sl.setColorAt(0, QColor(0, 0, 0, 45));
	sl.setColorAt(1, Qt::transparent);
	p.setBrush(sl);
	p.drawRect(QRectF(face.left(), face.top(), 6, face.height()));
	QLinearGradient refl(face.topLeft(), face.bottomRight());
	refl.setColorAt(0, QColor(255, 255, 255, 80));
	refl.setColorAt(0.38, QColor(255, 255, 255, 14));
	refl.setColorAt(0.39, Qt::transparent);
	refl.setColorAt(1, Qt::transparent);
	p.setBrush(refl);
	p.drawRoundedRect(face, radius, radius);
	QPolygonF streak;
	streak << QPointF(face.left() + face.width() * 0.52, face.top())
	       << QPointF(face.left() + face.width() * 0.60, face.top())
	       << QPointF(face.left() + face.width() * 0.28, face.bottom())
	       << QPointF(face.left() + face.width() * 0.22, face.bottom());
	p.setBrush(QColor(255, 255, 255, 26));
	p.drawPolygon(streak);
	p.setPen(QPen(QColor(255, 255, 255, 140), 1));
	p.setBrush(Qt::NoBrush);
	p.drawRoundedRect(face.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
	p.restore();
}

void drawSunken(QPainter& p, const QRectF& r) {
	if (!tactile) return;
	p.save();
	QLinearGradient top(0, r.top(), 0, r.top() + 7);
	top.setColorAt(0, QColor(0, 0, 0, 48));
	top.setColorAt(1, Qt::transparent);
	p.fillRect(QRectF(r.left(), r.top(), r.width(), 7), top);
	QLinearGradient left(r.left(), 0, r.left() + 5, 0);
	left.setColorAt(0, QColor(0, 0, 0, 30));
	left.setColorAt(1, Qt::transparent);
	p.fillRect(QRectF(r.left(), r.top(), 5, r.height()), left);
	p.restore();
}

static void drawScrew(QPainter& p, const QPointF& c, double angle) {
	p.setPen(Qt::NoPen);
	p.setBrush(QColor(255, 255, 255, 170));
	p.drawEllipse(c + QPointF(0, 0.8), 3.6, 3.6); // light catching the lower rim of the hole
	QRadialGradient g(c - QPointF(1.2, 1.2), 4.5);
	g.setColorAt(0, mix(panel, Qt::white, 0.25));
	g.setColorAt(1, mix(panel, rule, 1.0).darker(135));
	p.setBrush(g);
	p.setPen(QPen(QColor(0, 0, 0, 90), 0.6));
	p.drawEllipse(c, 3.5, 3.5);
	p.save();
	p.translate(c);
	p.rotate(angle);
	p.setPen(QPen(QColor(60, 45, 30, 150), 1.0, Qt::SolidLine, Qt::RoundCap));
	p.drawLine(QPointF(-2.4, 0), QPointF(2.4, 0));
	p.restore();
}

void drawWell(QPainter& p, const QRectF& r0, bool selected) {
	p.save();
	p.setRenderHint(QPainter::Antialiasing);
	const QRectF r = r0.adjusted(0.5, 1.5, -0.5, -1.5);
	const double rad = 8;
	p.setPen(Qt::NoPen);
	p.setBrush(QColor(255, 255, 255, 200));
	p.drawRoundedRect(r.translated(0, 1), rad, rad); // the light lower lip
	p.setBrush(QColor(110, 90, 65, 70));
	p.drawRoundedRect(r.translated(0, -1), rad, rad); // the shadowed upper lip
	QLinearGradient floor(0, r.top(), 0, r.bottom());
	floor.setColorAt(0, mix(panel, rule, 0.42));
	floor.setColorAt(0.6, mix(panel, rule, 0.24));
	floor.setColorAt(1, mix(panel, rule, 0.15));
	p.setBrush(floor);
	p.setPen(QPen(rule.darker(128), 1));
	p.drawRoundedRect(r, rad, rad);
	QPainterPath inside;
	inside.addRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), rad, rad);
	p.setClipPath(inside);
	QLinearGradient lip(0, r.top(), 0, r.top() + 10); // the shadow the top edge throws onto the floor
	lip.setColorAt(0, QColor(70, 45, 20, 100));
	lip.setColorAt(1, Qt::transparent);
	p.fillRect(QRectF(r.left(), r.top(), r.width(), 10), lip);
	QLinearGradient side(r.left(), 0, r.left() + 5, 0);
	side.setColorAt(0, QColor(70, 45, 20, 40));
	side.setColorAt(1, Qt::transparent);
	p.fillRect(QRectF(r.left(), r.top(), 5, r.height()), side);
	p.setPen(QPen(QColor(255, 255, 255, 140), 1.2));
	p.drawLine(QPointF(r.left() + rad, r.bottom() - 1), QPointF(r.right() - rad, r.bottom() - 1));
	p.setClipping(false);
	// a screw in each corner, each turned its own way, as if put in by hand
	const double in = 9.5;
	const QPointF corner[4] = {{r.left() + in, r.top() + in},
	                           {r.right() - in, r.top() + in},
	                           {r.left() + in, r.bottom() - in},
	                           {r.right() - in, r.bottom() - in}};
	const double turn[4] = {30, -50, 75, 10};
	for (int i = 0; i < 4; i++)
		drawScrew(p, corner[i], turn[i] + (int(r.width()) % 7) * 9);
	if (selected) { // a ring round the whole well, just inside its edge (outside it, it was cut off but for a sliver)
		p.setBrush(Qt::NoBrush);
		p.setPen(QPen(amber, 2));
		p.drawRoundedRect(r.adjusted(1, 1, -1, -1), rad - 1, rad - 1);
	}
	p.restore();
}

void drawIvory(QPainter& p, const QRectF& glass, double radius) {
	p.save();
	p.setRenderHint(QPainter::Antialiasing);
	QRadialGradient lit(QPointF(glass.center().x(), glass.bottom() + glass.height() * 0.25),
	                    std::max(glass.width(), glass.height()) * 0.75);
	lit.setColorAt(0, lcdBg.lighter(108));
	lit.setColorAt(0.7, lcdBg);
	lit.setColorAt(1, lcdBg.darker(106));
	p.setPen(Qt::NoPen);
	p.setBrush(lit);
	p.drawRoundedRect(glass, radius, radius);
	p.setBrush(Qt::NoBrush);
	p.setPen(QPen(rule.darker(150), 2));
	p.drawRoundedRect(glass.adjusted(1, 1, -1, -1), radius, radius);
	p.restore();
}

QIcon ledIcon(const QColor& lit) {
	auto draw = [&](bool on) {
		QPixmap pm(28, 28);
		pm.setDevicePixelRatio(2.0);
		pm.fill(Qt::transparent);
		QPainter p(&pm);
		p.setRenderHint(QPainter::Antialiasing);
		const QPointF c(7, 7);
		if (on) {
			QRadialGradient halo(c, 7);
			halo.setColorAt(0, QColor(lit.red(), lit.green(), lit.blue(), 150));
			halo.setColorAt(1, Qt::transparent);
			p.setPen(Qt::NoPen);
			p.setBrush(halo);
			p.drawEllipse(c, 7, 7);
		}
		QRadialGradient g(QPointF(6, 5.5), 5);
		const QColor base = on ? lit : QColor(0x6a, 0x5e, 0x52);
		g.setColorAt(0, on ? base.lighter(160) : base.lighter(120));
		g.setColorAt(1, on ? base : base.darker(150));
		p.setPen(QPen(QColor(0, 0, 0, 120), 0.8));
		p.setBrush(g);
		p.drawEllipse(c, 3.6, 3.6);
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(255, 255, 255, on ? 200 : 90));
		p.drawEllipse(QPointF(5.9, 5.8), 1.1, 1.1);
		return pm;
	};
	QIcon icon;
	icon.addPixmap(draw(false), QIcon::Normal, QIcon::Off);
	icon.addPixmap(draw(true), QIcon::Normal, QIcon::On);
	icon.addPixmap(draw(false), QIcon::Disabled, QIcon::Off);
	return icon;
}

QIcon transportIcon(const QString& kind) {
	auto draw = [&](const QColor& c) {
		QPixmap pm(40, 40);
		pm.setDevicePixelRatio(2.0);
		pm.fill(Qt::transparent);
		QPainter p(&pm);
		p.setRenderHint(QPainter::Antialiasing);
		p.setPen(Qt::NoPen);
		p.setBrush(c);
		if (kind == "play") {
			QPolygonF t;
			t << QPointF(6, 3) << QPointF(16.5, 10) << QPointF(6, 17);
			p.drawPolygon(t);
		} else if (kind == "stop")
			p.drawRoundedRect(QRectF(4.5, 4.5, 11, 11), 1.5, 1.5);
		else if (kind == "start") {
			p.drawRect(QRectF(4, 4, 2.4, 12));
			QPolygonF t;
			t << QPointF(16, 4) << QPointF(7.5, 10) << QPointF(16, 16);
			p.drawPolygon(t);
		} else if (kind == "undo" || kind == "redo") { // a curved arrow turning back (or forward)
			if (kind == "redo") {
				p.translate(20, 0);
				p.scale(-1, 1);
			}
			QPainterPath arc;
			arc.moveTo(6.5, 8.5);
			arc.cubicTo(9, 5.5, 15.5, 5.5, 16, 10.5);
			arc.cubicTo(16.3, 14, 13.5, 16, 10.5, 16);
			p.setBrush(Qt::NoBrush);
			p.setPen(QPen(c, 2.1, Qt::SolidLine, Qt::RoundCap));
			p.drawPath(arc);
			p.setPen(Qt::NoPen);
			p.setBrush(c);
			QPolygonF t;
			t << QPointF(3, 5.5) << QPointF(9.5, 5) << QPointF(5, 11.5);
			p.drawPolygon(t);
		} else if (kind == "export") { // an arrow going down into a tray
			p.setBrush(Qt::NoBrush);
			p.setPen(QPen(c, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
			p.drawLine(QPointF(10, 3), QPointF(10, 11.5));
			p.drawPolyline(QPolygonF() << QPointF(4, 12) << QPointF(4, 16.5) << QPointF(16, 16.5) << QPointF(16, 12));
			p.setPen(Qt::NoPen);
			p.setBrush(c);
			QPolygonF t;
			t << QPointF(5.8, 8.5) << QPointF(14.2, 8.5) << QPointF(10, 13.2);
			p.drawPolygon(t);
		} else // rec: the colour it is given (it was always red, so on the red key that is on it hardly showed)
			p.drawEllipse(QPointF(10, 10), 6, 6);
		return pm;
	};
	QIcon icon;
	// rec: a red dot on the key, a cream one on the red key when it is on
	icon.addPixmap(draw(kind == "rec" ? QColor(0xc4, 0x36, 0x1c) : ink), QIcon::Normal, QIcon::Off);
	icon.addPixmap(draw(kind == "rec" ? QColor(0xff, 0xf3, 0xe6) : accentInk), QIcon::Normal, QIcon::On);
	return icon;
}

// The keys' symbols on an 18-unit grid (the box is scaled to it): thin round lines in the ink, filled parts where a
// line would be too fine, and the accent for the one part that matters (the cut, the glide, the new knob setting).
void drawSymbol(QPainter& p, const QString& kind, const QRectF& box, const QColor& ink, const QColor& accent) {
	p.save();
	p.setRenderHint(QPainter::Antialiasing);
	p.translate(box.topLeft());
	p.scale(box.width() / 18.0, box.height() / 18.0);
	const QPen line(ink, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
	const QPen accentLine(accent, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
	p.setPen(line);
	p.setBrush(Qt::NoBrush);
	auto fill = [&](const QColor& c) {
		p.setPen(Qt::NoPen);
		p.setBrush(c);
	};
	auto poly = [&](std::initializer_list<QPointF> pts) { p.drawPolyline(QPolygonF(QList<QPointF>(pts))); };
	if (kind == "copy") { // two clips, one over the other
		p.drawRoundedRect(QRectF(3, 3, 9, 9), 2, 2);
		p.setBrush(wellHi);
		p.drawRoundedRect(QRectF(7, 7, 9, 9), 2, 2);
	} else if (kind == "loop") { // an arrow coming round again
		p.drawArc(QRectF(3, 4, 12, 11), 40 * 16, 290 * 16);
		fill(ink);
		p.drawPolygon(QPolygonF({QPointF(15.5, 3.5), QPointF(15.8, 9.2), QPointF(10.6, 6.6)}));
	} else if (kind == "cut") { // a clip in two at the playhead
		p.drawRoundedRect(QRectF(2, 5, 6, 8), 1.5, 1.5);
		p.drawRoundedRect(QRectF(10, 5, 6, 8), 1.5, 1.5);
		p.setPen(accentLine);
		p.drawLine(QPointF(9, 2), QPointF(9, 16));
	} else if (kind == "zoomout" || kind == "zoomin") { // a magnifying glass, with a minus or a plus in it
		p.drawEllipse(QPointF(7.5, 7.5), 5, 5);
		p.drawLine(QPointF(11.2, 11.2), QPointF(16, 16));
		p.drawLine(QPointF(5, 7.5), QPointF(10, 7.5));
		if (kind == "zoomin") p.drawLine(QPointF(7.5, 5), QPointF(7.5, 10));
	} else if (kind == "scissors") {
		p.drawEllipse(QPointF(5, 13), 2.6, 2.6);
		p.drawEllipse(QPointF(13, 13), 2.6, 2.6);
		p.drawLine(QPointF(6.6, 11), QPointF(13, 2.5));
		p.drawLine(QPointF(11.4, 11), QPointF(5, 2.5));
	} else if (kind == "delete") { // a bin
		p.drawLine(QPointF(3, 5), QPointF(15, 5));
		poly({{7, 5}, {7.5, 3}, {10.5, 3}, {11, 5}});
		poly({{4.5, 5}, {5.5, 16}, {12.5, 16}, {13.5, 5}});
	} else if (kind == "arrange") { // parts that build up
		fill(ink);
		p.drawRoundedRect(QRectF(2, 11, 4.5, 5), 1, 1);
		p.drawRoundedRect(QRectF(7, 7, 4.5, 9), 1, 1);
		p.drawRoundedRect(QRectF(12, 3, 4.5, 13), 1, 1);
	} else if (kind == "journey") { // a long wave rolling on, and the way onward (accent)
		QPainterPath wave;
		wave.moveTo(1.5, 11);
		wave.cubicTo(3.5, 4.5, 6, 4.5, 7.5, 9);
		wave.cubicTo(9, 13.5, 11.5, 13.5, 13.5, 9);
		p.drawPath(wave);
		p.setPen(accentLine);
		poly({{13, 5.5}, {16.5, 9}, {13, 12.5}});
	} else if (kind == "mute") { // a speaker, crossed out
		p.drawPolygon(QPolygonF(
		    {QPointF(2.5, 7), QPointF(5.5, 7), QPointF(9, 4), QPointF(9, 14), QPointF(5.5, 11), QPointF(2.5, 11)}));
		p.drawLine(QPointF(12, 7), QPointF(16, 11));
		p.drawLine(QPointF(16, 7), QPointF(12, 11));
	} else if (kind == "solo") { // headphones
		QPainterPath band;
		band.moveTo(3, 12);
		band.lineTo(3, 9.5);
		band.arcTo(QRectF(3, 3.5, 12, 12), 180, -180);
		band.lineTo(15, 12);
		p.drawPath(band);
		p.drawRoundedRect(QRectF(2.2, 10.5, 3.3, 5), 1.2, 1.2);
		p.drawRoundedRect(QRectF(12.5, 10.5, 3.3, 5), 1.2, 1.2);
	} else if (kind == "lines") { // a knob line with two points
		poly({{2, 13.5}, {6, 8}, {10, 11}, {16, 4}});
		fill(ink);
		p.drawEllipse(QPointF(6, 8), 1.3, 1.3);
		p.drawEllipse(QPointF(10, 11), 1.3, 1.3);
	} else if (kind == "stop") {
		fill(ink);
		p.drawRoundedRect(QRectF(4.5, 4.5, 9, 9), 1.5, 1.5);
	} else if (kind == "play") { // listen: the transport's triangle
		fill(ink);
		p.drawPolygon(QPolygonF({QPointF(5.5, 3.8), QPointF(14.2, 9), QPointF(5.5, 14.2)}));
	} else if (kind == "idea") { // a light bulb
		QPainterPath bulb;
		bulb.moveTo(6.9, 13.3);
		bulb.lineTo(6.9, 13);
		bulb.cubicTo(6.9, 12.3, 6.6, 11.7, 6, 11.2);
		bulb.arcTo(QRectF(4, 2.2, 10, 10), 216, -252);
		bulb.cubicTo(11.4, 11.7, 11.1, 12.3, 11.1, 13);
		bulb.lineTo(11.1, 13.3);
		bulb.closeSubpath();
		p.drawPath(bulb);
		p.drawLine(QPointF(7.2, 15.8), QPointF(10.8, 15.8));
	} else if (kind == "vary") { // two paths that swap
		QPainterPath a;
		a.moveTo(2.5, 5.5);
		a.lineTo(5.5, 5.5);
		a.cubicTo(8.5, 5.5, 9.5, 12.5, 12.5, 12.5);
		a.lineTo(15.5, 12.5);
		p.drawPath(a);
		poly({{13.5, 10.5}, {15.5, 12.5}, {13.5, 14.5}});
		QPainterPath b;
		b.moveTo(2.5, 12.5);
		b.lineTo(5.5, 12.5);
		b.cubicTo(6.5, 12.5, 7.3, 11.9, 7.9, 11);
		p.drawPath(b);
		QPainterPath c;
		c.moveTo(10.1, 7);
		c.cubicTo(10.7, 6.1, 11.5, 5.5, 12.5, 5.5);
		c.lineTo(15.5, 5.5);
		p.drawPath(c);
		poly({{13.5, 3.5}, {15.5, 5.5}, {13.5, 7.5}});
	} else if (kind == "clean") { // notes snapping onto the grid
		p.setPen(QPen(ink, 1, Qt::SolidLine, Qt::RoundCap));
		p.setOpacity(0.55);
		for (double x : {2.5, 9.0, 15.5})
			p.drawLine(QPointF(x, 3), QPointF(x, 15));
		p.setOpacity(1);
		fill(ink);
		p.drawRoundedRect(QRectF(2.5, 5, 4.5, 3), 1, 1);
		p.drawRoundedRect(QRectF(9, 10, 4.5, 3), 1, 1);
	} else if (kind == "clear") { // an eraser
		p.drawPolygon(QPolygonF(
		    {QPointF(10.5, 3.2), QPointF(15, 7.7), QPointF(8.2, 14.5), QPointF(4.5, 14.5), QPointF(2.7, 12.7)}));
		p.drawLine(QPointF(6.5, 7.2), QPointF(11, 11.7));
		p.drawLine(QPointF(8, 15.5), QPointF(15.5, 15.5));
	} else if (kind == "accent") { // the accent sign of written music
		poly({{4, 5}, {14, 9}, {4, 13}});
	} else if (kind == "slide") { // two notes and the glide between them
		p.drawRoundedRect(QRectF(1.8, 11, 4.6, 3), 1, 1);
		p.drawRoundedRect(QRectF(11.6, 4, 4.6, 3), 1, 1);
		QPainterPath g;
		g.moveTo(6.4, 12.5);
		g.cubicTo(9.5, 12.5, 8.5, 5.5, 11.6, 5.5);
		p.setPen(accentLine);
		p.drawPath(g);
	} else if (kind == "chance") { // half a coin: sometimes yes, sometimes no
		p.drawEllipse(QPointF(9, 9), 6.3, 6.3);
		QPainterPath half;
		half.moveTo(9, 2.7);
		half.arcTo(QRectF(2.7, 2.7, 12.6, 12.6), 90, -180);
		half.closeSubpath();
		fill(ink);
		p.drawPath(half);
	} else if (kind == "repeat") { // a note, then fainter echoes of it
		fill(ink);
		for (int i = 0; i < 3; i++) {
			p.setOpacity(1.0 - 0.3 * i);
			p.drawRoundedRect(QRectF(2 + 5.2 * i, 7, 3.6, 4), 1, 1);
		}
		p.setOpacity(1);
	} else if (kind == "lean") { // a note, and a step either side of it
		fill(ink);
		p.drawRoundedRect(QRectF(6, 3.5, 6, 4), 1, 1);
		p.setPen(line);
		p.setBrush(Qt::NoBrush);
		p.drawLine(QPointF(3, 12.5), QPointF(15, 12.5));
		poly({{5, 10.5}, {3, 12.5}, {5, 14.5}});
		poly({{13, 10.5}, {15, 12.5}, {13, 14.5}});
	} else if (kind == "savesound") { // an index card with a plus: a sound kept in the card index
		p.drawRoundedRect(QRectF(2.5, 4.5, 11, 9), 1.4, 1.4);
		p.drawLine(QPointF(4.8, 7.6), QPointF(9.5, 7.6));
		p.drawLine(QPointF(4.8, 10.4), QPointF(8, 10.4));
		p.setPen(accentLine);
		p.drawLine(QPointF(14, 9.5), QPointF(14, 15.5));
		p.drawLine(QPointF(11, 12.5), QPointF(17, 12.5));
		p.setPen(line);
	} else if (kind == "library") { // a card-index drawer: cards standing in an open tray
		p.drawRoundedRect(QRectF(5.5, 2.5, 8, 7), 1.2, 1.2);
		p.setPen(accentLine);
		p.setBrush(wellHi);
		p.drawRoundedRect(QRectF(3.5, 5, 8, 6.5), 1.2, 1.2);
		p.setPen(line);
		p.setBrush(wellHi);
		p.drawRoundedRect(QRectF(2, 9, 14, 6.5), 1.6, 1.6);
		p.drawLine(QPointF(7, 12.2), QPointF(11, 12.2));
		p.setBrush(Qt::NoBrush);
	} else if (kind == "keep") { // a pin
		poly({{6.5, 2.5}, {11.5, 2.5}});
		poly({{7.5, 2.5}, {7.5, 7}, {5, 10}, {13, 10}, {10.5, 7}, {10.5, 2.5}});
		p.drawLine(QPointF(9, 10), QPointF(9, 15.5));
	} else if (kind == "dice") {
		p.drawRoundedRect(QRectF(2.8, 2.8, 12.4, 12.4), 2.6, 2.6);
		fill(ink);
		for (QPointF c :
		     {QPointF(6.1, 6.1), QPointF(11.9, 6.1), QPointF(9, 9), QPointF(6.1, 11.9), QPointF(11.9, 11.9)})
			p.drawEllipse(c, 1.1, 1.1);
	} else if (kind == "mutate") { // a wave and a small plus: the knobs nudged
		QPainterPath w;
		w.moveTo(2.5, 10.5);
		w.cubicTo(4.1, 7.5, 5.7, 7.5, 7.3, 10.5);
		w.cubicTo(8.9, 13.5, 10.5, 13.5, 12.1, 10.5);
		p.drawPath(w);
		p.setPen(accentLine);
		p.drawLine(QPointF(13.5, 3), QPointF(13.5, 6.5));
		p.drawLine(QPointF(11.75, 4.75), QPointF(15.25, 4.75));
	} else if (kind == "stopall") { // the stop square in a ring
		p.drawEllipse(QPointF(9, 9), 6.5, 6.5);
		fill(ink);
		p.drawRoundedRect(QRectF(6.2, 6.2, 5.6, 5.6), 1, 1);
	} else if (kind == "column") { // an empty column, and one more
		QPen dashed = line;
		dashed.setDashPattern({1.4, 1.25});
		p.setPen(dashed);
		p.drawRoundedRect(QRectF(2.5, 2.5, 6, 13), 1.4, 1.4);
		p.setPen(line);
		p.drawLine(QPointF(13, 6), QPointF(13, 12));
		p.drawLine(QPointF(10, 9), QPointF(16, 9));
	}
	p.restore();
}

QIcon symbolIcon(const QString& kind, bool dark) {
	auto draw = [&](const QColor& ink, const QColor& accent) {
		QPixmap pm(36, 36);
		pm.setDevicePixelRatio(2.0);
		pm.fill(Qt::transparent);
		QPainter p(&pm);
		drawSymbol(p, kind, QRectF(0, 0, 18, 18), ink, accent);
		return pm;
	};
	const QColor off = dark ? keyInk : ink;
	QIcon icon;
	icon.addPixmap(draw(off, amber), QIcon::Normal, QIcon::Off);
	icon.addPixmap(draw(accentInk, accentInk), QIcon::Normal, QIcon::On); // on a lit key
	icon.addPixmap(draw(off, amber), QIcon::Active, QIcon::Off);
	icon.addPixmap(draw(accentInk, accentInk), QIcon::Active, QIcon::On);
	icon.addPixmap(draw(rule, rule), QIcon::Disabled, QIcon::Off);
	icon.addPixmap(draw(rule, rule), QIcon::Disabled, QIcon::On);
	return icon;
}

void loadKeyLook() {
	symbolsOnly = QSettings().value(QStringLiteral("keys")).toString() == QLatin1String("symbols");
	ownCursors = QSettings().value(QStringLiteral("ownCursors"), true).toBool();
}

// ---------------------------------------------------------------- cursors

namespace {

// the drawing kit for the cursors, on a 24 grid: everything is drawn twice, first as a wide light rim under all (so
// a cursor shows on the dark looks too), then the shapes with a thin ink outline
struct CursorKit {
	QPainter& p;
	bool under;
	QColor ink = QColor(0x2b, 0x26, 0x23), rim = QColor(0xff, 0xfa, 0xf2), cream = QColor(0xf6, 0xf2, 0xea);
	QColor accent;
	double o = 1.1; // the ink outline
	CursorKit(QPainter& painter, bool rimOnly, const QColor& a) : p(painter), under(rimOnly), accent(a) {}
	void fill(const QPainterPath& path, const QColor& c, double w = -1) {
		if (w < 0) w = o;
		p.setPen(QPen(under ? rim : ink, under ? w + 2.2 : w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
		p.setBrush(under ? QBrush(rim) : QBrush(c));
		p.drawPath(path);
	}
	void poly(std::initializer_list<QPointF> pts, const QColor& c, double w = -1) {
		QPainterPath path;
		path.addPolygon(QPolygonF(pts));
		path.closeSubpath();
		fill(path, c, w);
	}
	void circle(QPointF c, double r, const QColor& col, double w = -1) {
		QPainterPath path;
		path.addEllipse(c, r, r);
		fill(path, col, w);
	}
	void rounded(const QRectF& r, double radius, const QColor& c) {
		QPainterPath path;
		path.addRoundedRect(r, radius, radius);
		fill(path, c);
	}
	void line(QPointF a, QPointF b, const QColor& c, double w) { // a line with an ink edge
		p.setBrush(Qt::NoBrush);
		p.setPen(QPen(under ? rim : ink, w + 2 * o + (under ? 2.2 : 0), Qt::SolidLine, Qt::RoundCap));
		p.drawLine(a, b);
		if (under) return;
		p.setPen(QPen(c, w, Qt::SolidLine, Qt::RoundCap));
		p.drawLine(a, b);
	}
	// a filled arrowhead with its tip at t, pointing at angle degrees (0 to the right, 90 down)
	void head(QPointF t, double angle, double len, double half, const QColor& c) {
		const double a = angle * M_PI / 180;
		const QPointF d(std::cos(a), std::sin(a)), n(-d.y(), d.x());
		poly({t, t - len * d + half * n, t - len * d - half * n}, c);
	}
	void fourWays(QPointF c, double dist, double len, double half) {
		head(c + QPointF(0, -dist), -90, len, half, cream);
		head(c + QPointF(0, dist), 90, len, half, cream);
		head(c + QPointF(-dist, 0), 180, len, half, cream);
		head(c + QPointF(dist, 0), 0, len, half, cream);
	}
	// Nachtzug's knob seen from above: a ribbed black skirt, a cap, and the pointer in the accent
	void knob(QPointF c, double r) {
		if (under) {
			circle(c, r + 0.7, rim);
			return;
		}
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(0x1f, 0x1c, 0x1a));
		for (int i = 0; i < 18; i++) {
			const double a = 2 * M_PI * i / 18;
			p.drawEllipse(c + QPointF((r - 0.2) * std::cos(a), (r - 0.2) * std::sin(a)), 0.95, 0.95);
		}
		QLinearGradient skirt(c.x(), c.y() - r, c.x(), c.y() + r);
		skirt.setColorAt(0, QColor(0x55, 0x4f, 0x4a));
		skirt.setColorAt(1, QColor(0x14, 0x12, 0x11));
		p.setBrush(skirt);
		p.drawEllipse(c, r - 0.6, r - 0.6);
		QRadialGradient capG(c + QPointF(-r * 0.25, -r * 0.3), r * 0.9);
		capG.setColorAt(0, QColor(0x5a, 0x53, 0x4e));
		capG.setColorAt(1, QColor(0x1c, 0x19, 0x18));
		p.setPen(QPen(QColor(0, 0, 0, 160), 0.6));
		p.setBrush(capG);
		p.drawEllipse(c, r * 0.68, r * 0.68);
		const QPointF d(std::cos(-M_PI / 3), std::sin(-M_PI / 3)); // the pointer at about one o'clock
		p.setPen(QPen(accent, r * 0.26, Qt::SolidLine, Qt::RoundCap));
		p.drawLine(c + d * (r * 0.22), c + d * (r * 0.62));
	}
	void fader() { // a fader's cap on its slot, and up and down
		line({12, 5.5}, {12, 18.5}, QColor(0x3a, 0x34, 0x30), 1.0);
		rounded(QRectF(6.5, 9.4, 11, 5.2), 1.2, QColor(0x2f, 0x2a, 0x27));
		if (!under) {
			p.setPen(QPen(accent, 1.2, Qt::SolidLine, Qt::FlatCap));
			p.drawLine(QPointF(7.6, 12), QPointF(16.4, 12));
		}
		poly({{12, 1.5}, {15.4, 4.9}, {8.6, 4.9}}, cream);
		poly({{12, 22.5}, {15.4, 19.1}, {8.6, 19.1}}, cream);
	}
};

// one of Nachtzug's cursors on the 24 grid; returns where it points
QPointF drawCursor(CursorKit& k, const QString& kind) {
	const QColor wood(0xe9, 0xd3, 0xa6), ferrule(0xb1, 0xa3, 0x91), rubber(0xe8, 0xa0, 0x90), dark(0x2f, 0x2a, 0x27);
	const double s = std::sqrt(0.5);
	if (kind == QLatin1String("knob")) { // the knob, with up and down: the way a knob is dragged
		k.knob({12, 12}, 6.2);
		k.poly({{12, 1.5}, {15.4, 4.9}, {8.6, 4.9}}, k.cream);
		k.poly({{12, 22.5}, {15.4, 19.1}, {8.6, 19.1}}, k.cream);
	} else if (kind == QLatin1String("fader")) {
		k.fader();
	} else if (kind == QLatin1String("slider")) { // the fader lying down, for the sliders across
		k.p.save();
		k.p.translate(12, 12);
		k.p.rotate(90);
		k.p.translate(-12, -12);
		k.fader();
		k.p.restore();
	} else if (kind == QLatin1String("pencil")) { // lying from the lower left, its lead the point
		const QPointF t(2, 22), u(s, -s), v(s, s);
		const double w = 2.7;
		auto f = [&](double a, double b) { return t + a * u + b * v; };
		k.poly({f(5.4, -w), f(16.8, -w), f(16.8, w), f(5.4, w)}, k.accent);
		k.poly({f(16.8, -w), f(19.0, -w), f(19.0, w), f(16.8, w)}, ferrule);
		k.poly({f(19.0, -w), f(21.6, -w), f(21.6, w), f(19.0, w)}, rubber);
		k.poly({f(0, 0), f(5.4, -w), f(5.4, w)}, wood);
		k.poly({f(0, 0), f(2.0, -w * 0.37), f(2.0, w * 0.37)}, k.ink, 0.6);
		return t;
	} else if (kind == QLatin1String("rubber")) { // a rubber in its sleeve, rubbing out with its lower end
		const QPointF t(5, 19), u(s, -s), v(s, s);
		const double w = 4.2;
		auto f = [&](double a, double b) { return t + a * u + b * v; };
		k.poly({f(0, -w), f(7, -w), f(7, w), f(0, w)}, rubber);
		k.poly({f(7, -w), f(17, -w), f(17, w), f(7, w)}, dark);
		if (!k.under) {
			k.p.setPen(QPen(k.accent, 1.1));
			k.p.drawLine(f(9, -w + 1.4), f(15.5, -w + 1.4));
		}
		return f(0, w * 0.4);
	} else if (kind == QLatin1String("move")) { // a note, and the four ways it goes
		k.rounded(QRectF(7.2, 9.5, 9.6, 5), 1.0, k.accent);
		k.fourWays({12, 12}, 10.4, 3.4, 3.0);
	} else if (kind == QLatin1String("handle")) { // the end of a note, and the two ways it goes
		k.rounded(QRectF(10.6, 3.5, 2.8, 17), 0.8, k.accent);
		k.poly({{1.6, 12}, {6, 7.4}, {6, 10.3}, {8.8, 10.3}, {8.8, 13.7}, {6, 13.7}, {6, 16.6}}, k.cream);
		k.poly({{22.4, 12}, {18, 7.4}, {18, 10.3}, {15.2, 10.3}, {15.2, 13.7}, {18, 13.7}, {18, 16.6}}, k.cream);
	} else if (kind == QLatin1String("scissors")) { // open, pointing up and to the left; they cut where the blades meet
		const QPointF pivot(11, 11);
		const QColor steel(0xc8, 0xbf, 0xb2);
		k.line(pivot, {16.8, 19.6}, k.ink, 1.6);
		k.line(pivot, {19.6, 16.8}, k.ink, 1.6);
		k.poly({{1.6, 6.2}, {12.6, 9.7}, {11.6, 12.5}, {9.6, 11.8}}, steel);
		k.poly({{6.2, 1.6}, {9.7, 12.6}, {12.5, 11.6}, {11.8, 9.6}}, steel);
		k.circle({16.8, 20}, 2.9, k.accent, 1.2);
		k.circle({20, 16.8}, 2.9, k.accent, 1.2);
		if (!k.under) {
			k.circle({16.8, 20}, 1.1, k.cream, 0.7);
			k.circle({20, 16.8}, 1.1, k.cream, 0.7);
			k.circle(pivot, 0.9, k.ink, 0.4); // the rivet
		}
		return {3.5, 3.5};
	} else if (kind == QLatin1String("aim")) { // a point goes here
		k.line({12, 1.5}, {12, 7.5}, k.cream, 1.3);
		k.line({12, 16.5}, {12, 22.5}, k.cream, 1.3);
		k.line({1.5, 12}, {7.5, 12}, k.cream, 1.3);
		k.line({16.5, 12}, {22.5, 12}, k.cream, 1.3);
		k.circle({12, 12}, 2.4, k.accent);
	} else if (kind == QLatin1String("point")) { // a point, and the four ways it goes
		k.fourWays({12, 12}, 10.4, 3.6, 3.1);
		k.circle({12, 12}, 3.6, k.accent);
		if (!k.under) k.circle({12, 12}, 1.2, k.cream, 0.6);
	} else if (kind == QLatin1String("playhead")) { // the playhead: play from here
		k.line({12, 9}, {12, 22.5}, k.accent, 1.4);
		k.poly({{7, 1.5}, {17, 1.5}, {17, 6.5}, {12, 11}, {7, 6.5}}, k.accent);
		return {12, 14};
	} else if (kind == QLatin1String("finger")) { // a finger, to press a key or a switch
		QPainterPath h;
		h.addRoundedRect(QRectF(7.6, 1.2, 3.8, 13), 1.9, 1.9);
		QPainterPath palm;
		palm.addRoundedRect(QRectF(7.6, 9.6, 12.2, 10.4), 2.6, 2.6);
		h = h.united(palm);
		for (double x : {11.0, 14.0, 17.0}) {
			QPainterPath f;
			f.addRoundedRect(QRectF(x, 8.6, 3.0, 6), 1.5, 1.5);
			h = h.united(f);
		}
		QPainterPath thumb; // lying against the palm
		thumb.addRoundedRect(QRectF(3.2, 11.2, 7, 3.4), 1.7, 1.7);
		QTransform turn;
		turn.translate(6.7, 12.9);
		turn.rotate(35);
		turn.translate(-6.7, -12.9);
		h = h.united(turn.map(thumb));
		k.fill(h, k.cream);
		if (!k.under) {
			k.p.setPen(QPen(k.ink, 0.7, Qt::SolidLine, Qt::RoundCap));
			for (double x : {14.0, 17.0})
				k.p.drawLine(QPointF(x, 11.0), QPointF(x, 13.6));
		}
		k.rounded(QRectF(8.4, 19.4, 10.6, 3.6), 0.8, k.accent);
		return {9.5, 1.2};
	}
	return {12, 12};
}

// How big the system's cursors are, and how Qt hands a drawn cursor to it. The grid's 24 is the usual cursor size on
// Linux, and about the system's arrow on a Mac and on Windows. X11 takes the picture and the point at their pixels,
// and its cursor size (XCURSOR_SIZE) is in pixels too; Wayland shows the picture at the window's scale rounded up,
// with the size in points; the Mac and Windows take the picture's own scale.
struct CursorSize {
	int side;      // the picture's pixels
	qreal scale;   // the picture's pixels to a point
	bool pixelHot; // the point that clicks is given in pixels
};
CursorSize cursorSize() {
	qreal dpr = 1; // as sharp as the sharpest screen; Qt scales it down on the others
	for (QScreen* s : QGuiApplication::screens())
		dpr = std::max(dpr, s->devicePixelRatio());
	bool ok = false;
	const int wanted = qEnvironmentVariableIntValue("XCURSOR_SIZE", &ok);
	const bool given = ok && wanted >= 12 && wanted <= 256;
	const QString platform = QGuiApplication::platformName();
	if (platform == QLatin1String("xcb")) return {given ? wanted : int(std::lround(24 * dpr)), dpr, true};
	if (platform.startsWith(QLatin1String("wayland"))) {
		const qreal scale = std::ceil(dpr - 0.01);
		return {int(std::lround((given ? wanted : 24) * scale)), scale, false};
	}
	return {int(std::lround(24 * dpr)), dpr, false};
}

} // namespace

QCursor cursor(const QString& kind, Qt::CursorShape system) {
	if (!ownCursors) return QCursor(system);
	const CursorSize size = cursorSize();
	static QHash<QString, QCursor> made;
	const QString key = kind + QLatin1Char('@') + QString::number(size.side) + QLatin1Char('/') +
	                    QString::number(size.scale) + QLatin1Char('/') + amber.name();
	if (!made.contains(key)) {
		QImage im(size.side, size.side, QImage::Format_ARGB32_Premultiplied);
		im.fill(Qt::transparent);
		QPointF hot;
		{
			QPainter p(&im);
			p.setRenderHint(QPainter::Antialiasing);
			p.scale(size.side / 24.0, size.side / 24.0);
			for (int pass = 0; pass < 2; pass++) {
				CursorKit k(p, pass == 0, amber);
				hot = drawCursor(k, kind);
			}
		}
		im.setDevicePixelRatio(size.scale);
		const QPointF at = hot * (size.side / 24.0) / (size.pixelHot ? 1.0 : size.scale);
		made.insert(key, QCursor(QPixmap::fromImage(im), int(std::lround(at.x())), int(std::lround(at.y()))));
	}
	return made.value(key);
}

void setOwnCursor(QWidget* w, const QString& kind, Qt::CursorShape system) {
	w->setProperty("cursorKind", kind);
	w->setProperty("cursorSystem", int(system));
	w->setCursor(cursor(kind, system));
}

namespace {
// gives every slider the fader's cursor, standing or lying as the slider does, as it is about to be shown
struct SliderCursors : QObject {
	bool eventFilter(QObject* o, QEvent* e) override {
		if (e->type() == QEvent::Polish)
			if (auto* s = qobject_cast<QSlider*>(o))
				setOwnCursor(s, s->orientation() == Qt::Horizontal ? QStringLiteral("slider") : QStringLiteral("fader"),
				             Qt::ArrowCursor);
		return false;
	}
};
} // namespace

void applyCursorLook() {
	loadKeyLook();
	static SliderCursors* sliders = nullptr;
	if (!sliders && qApp) {
		sliders = new SliderCursors;
		sliders->setParent(qApp);
		qApp->installEventFilter(sliders);
	}
	for (QWidget* w : QApplication::allWidgets()) {
		const QString kind = w->property("cursorKind").toString();
		if (!kind.isEmpty()) w->setCursor(cursor(kind, Qt::CursorShape(w->property("cursorSystem").toInt())));
	}
}

void symbolKey(QAbstractButton* b, const QString& kind, const QString& word, bool alone) {
	b->setProperty("symbol", kind);
	b->setProperty("word", word);
	b->setProperty("symbolAlone", alone);
	b->setProperty("tip", b->toolTip());
	b->setAccessibleName(word);
	if (b->iconSize().width() < 18) b->setIconSize(QSize(18, 18));
	dressSymbolKeys(b);
}

// a key's icon in the colours of the look, and its word unless it shows its symbol alone; a key that loses its word
// says it in its tooltip. The arrow of a key that opens a menu stays.
void dressSymbolKeys(QWidget* root) {
	QList<QAbstractButton*> keys = root->findChildren<QAbstractButton*>();
	if (auto* self = qobject_cast<QAbstractButton*>(root)) keys.prepend(self);
	for (QAbstractButton* b : keys) {
		const QString kind = b->property("symbol").toString();
		if (kind.isEmpty()) continue;
		const QString word = b->property("word").toString(), tip = b->property("tip").toString();
		const bool alone = symbolsOnly || b->property("symbolAlone").toBool();
		const bool arrow = word.endsWith(QStringLiteral(" ▾"));
		b->setIcon(symbolIcon(kind, b->objectName() == QLatin1String("primary")));
		b->setText(alone ? (arrow ? QStringLiteral("▾") : QString()) : word);
		if (auto* tb = qobject_cast<QToolButton*>(b)) // a menu key: shows only its icon unless told
			tb->setToolButtonStyle(b->text().isEmpty() ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon);
		const QString name = arrow ? word.chopped(2) : word;
		b->setToolTip(alone && !tip.isEmpty() ? QStringLiteral("%1: %2").arg(name, tip) : alone ? name : tip);
	}
}

QIcon appIcon() {
	QIcon icon;
	for (int sz : {16, 24, 32, 48, 64, 128, 256})
		icon.addPixmap(logo(sz));
	return icon;
}

} // namespace theme
