// Drawing the instrument's parts that the style sheet can't: clips and keys with depth, the screwed wells, the
// ivory displays and their glass, sliders, and the icons (tracks, transport, the keys above the timeline, the
// mixer, lamps) and the Nachtzug logo.
//
// What's here: logo(), blockFill(), drawBlock(), drawKey(), drawSlider(), drawRoleIcon(), roleIcon(), mixerIcon(),
// drawGlass(), drawSunken(), drawWell(), drawIvory(), ledIcon(), transportIcon(), drawSymbol(), symbolIcon(),
// symbolKey(), dressSymbolKeys(), appIcon(); used by
// every panel that paints itself.
// Changes in the song file: none.
#include "widgets.h"
#include "model.h"
#include <QPainter>
#include <QPainterPath>
#include <QImage>
#include <QSettings>
#include <QToolButton>

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
