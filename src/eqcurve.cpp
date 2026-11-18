// The equaliser's picture on the Sound tab: see eqcurve.h.
//
// What's here: EqCurve, the curve the five equaliser knobs add up to (computed by dsp::Eq, so it is the sound's own
// curve), drawn like the other pictures of the Sound tab (graph paper under glass in the instrument look, a plain box
// in the flat looks), with a point for each band: the low cut moves left and right, Low and High up and down, Mid
// both ways. The keys work too: Tab picks a point, the arrows move it, Delete puts it back. Made by SoundPanel::
// buildEq() for the synthesiser's and the drum kit's Complete page.
// Changes in the song file: none (the five settings are a sound's knobs, songfile.cpp).

#include "eqcurve.h"
#include "app.h"
#include "widgets.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QFontMetricsF>
#include <QLineF>
#include <cmath>

using namespace dsp;

static constexpr double DB_RANGE = 15.0; // the picture shows −15 to +15 dB
static constexpr float SR = 48000.f;     // the picture's sample rate: the curve's shape hardly depends on it

EqCurve::EqCurve(App* a, QWidget* parent) : QWidget(parent), app(a) {
	setFixedSize(theme::tactile ? QSize(200, 84) : QSize(168, 70));
	setFocusPolicy(Qt::StrongFocus); // Tab reaches it, so it can be set with the keys
	setMouseTracking(true);
	theme::setOwnCursor(this, QStringLiteral("point"), Qt::CrossCursor);
	setAccessibleName(tr("Equaliser"));
	setToolTip(tr("The equaliser: how loud each pitch comes out, the deepest on the left, the highest on the right.\n"
	              "Drag a point: up for more, down for less. The middle point and the low cut on the left also move "
	              "sideways.\nDouble-click a point to put it back. With the keys: Tab picks a point, the arrows move "
	              "it, Delete puts it back."));
}

bool EqCurve::values(float v[5]) const {
	const Track* t = app->song.track(app->selTrack);
	if (!t) return false;
	for (int i = 0; i < 5; i++)
		v[i] = t->isDrums() ? t->kit.p[DP_EQCUT + i] : app->paramValue(*t, P_EQCUT + i);
	return true;
}

void EqCurve::set(int index, float x) {
	Track* t = app->curTrack();
	if (!t) return;
	x = std::clamp(x, 0.f, 1.f);
	if (t->isDrums())
		t->kit.p[DP_EQCUT + index] = x;
	else {
		const int param = P_EQCUT + index;
		t->patch.p[param] = x;
		if (t->hasMoves(param)) t->bypass |= quint64(1) << param; // set by hand: its line pauses, as with any knob
	}
}

void EqCurve::refresh() {
	float v[5] = {};
	const bool any = values(v);
	const int id = any ? app->selTrack : -1;
	if (id == shownTrack && std::equal(v, v + 5, shown)) return;
	update();
}

QRectF EqCurve::area() const {
	return QRectF(rect()).adjusted(theme::tactile ? 9 : 6, theme::tactile ? 9 : 7, theme::tactile ? -9 : -6,
	                               theme::tactile ? -16 : -7);
}
double EqCurve::xOf(double hz) const {
	const QRectF in = area();
	return in.left() + std::log10(std::clamp(hz, 20.0, 20000.0) / 20.0) / 3.0 * in.width();
}
double EqCurve::hzAt(double x) const {
	const QRectF in = area();
	return 20.0 * std::pow(1000.0, std::clamp((x - in.left()) / in.width(), 0.0, 1.0));
}
double EqCurve::yOf(double db) const {
	const QRectF in = area();
	return in.center().y() - db / DB_RANGE * in.height() / 2;
}
double EqCurve::dbAt(double y) const {
	const QRectF in = area();
	return (in.center().y() - y) / (in.height() / 2) * DB_RANGE;
}

QPointF EqCurve::pointPos(int point, const float v[5]) const {
	switch (point) {
	case 0: // the low cut sits on the line, at its pitch; at the left edge when it is off
		return {v[0] > 0.f ? xOf(lowCutHz(v[0])) : area().left(), yOf(0)};
	case 1:
		return {xOf(EQ_LOW_HZ), yOf(eqDb(v[1]))};
	case 2:
		return {xOf(eqMidHz(v[3])), yOf(eqDb(v[2]))};
	default:
		return {xOf(EQ_HIGH_HZ), yOf(eqDb(v[4]))};
	}
}

int EqCurve::pointAt(const QPointF& pos) const {
	float v[5];
	if (!values(v)) return -1;
	int best = -1;
	double bestD = 12.0;
	for (int i = 0; i < 4; i++) {
		const double d = QLineF(pos, pointPos(i, v)).length();
		if (d < bestD) {
			bestD = d;
			best = i;
		}
	}
	return best;
}

QString EqCurve::pointText(int point, const float v[5]) const {
	switch (point) {
	case 0:
		return ui(PARAMS[P_EQCUT].label) + QStringLiteral(" ") + formatParam(P_EQCUT, v[0]);
	case 1:
		return ui(PARAMS[P_EQLOW].label) + QStringLiteral(" ") + formatParam(P_EQLOW, v[1]);
	case 2:
		return ui(PARAMS[P_EQMID].label) + QStringLiteral(" ") + formatParam(P_EQMID, v[2]) + QStringLiteral(" · ") +
		       formatParam(P_EQFREQ, v[3]);
	default:
		return ui(PARAMS[P_EQHIGH].label) + QStringLiteral(" ") + formatParam(P_EQHIGH, v[4]);
	}
}

void EqCurve::paintEvent(QPaintEvent*) {
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);
	const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
	const bool tactile = theme::tactile;
	// the frame and the paper, as the Sound tab's other pictures have them
	if (tactile) {
		QLinearGradient outer(r.topLeft(), r.bottomLeft());
		outer.setColorAt(0, QColor(0xe3, 0xda, 0xcb));
		outer.setColorAt(1, QColor(0x9f, 0x91, 0x7f));
		p.setPen(Qt::NoPen);
		p.setBrush(outer);
		p.drawRoundedRect(r, 6, 6);
		const QRectF bev = r.adjusted(3, 3, -3, -3);
		QLinearGradient inner(bev.topLeft(), bev.bottomLeft());
		inner.setColorAt(0, QColor(0x7d, 0x70, 0x5f));
		inner.setColorAt(1, QColor(0xd5, 0xcb, 0xbb));
		p.setBrush(inner);
		p.drawRoundedRect(bev, 4.5, 4.5);
	}
	const QRectF paper = tactile ? r.adjusted(5, 5, -5, -5) : r;
	const QColor line(0xc9, 0x8f, 0x5a);
	if (tactile) {
		p.setBrush(QColor(0xf7, 0xf0, 0xde));
		p.drawRoundedRect(paper, 3, 3);
		for (int i = 1; i < 20; i++) {
			QColor c = line;
			c.setAlphaF(i % 5 == 0 ? 0.42 : 0.15);
			p.setPen(QPen(c, i % 5 == 0 ? 0.9 : 0.5));
			const double gx = paper.left() + paper.width() * i / 20;
			p.drawLine(QPointF(gx, paper.top()), QPointF(gx, paper.bottom()));
		}
		for (int i = 1; i < 12; i++) {
			QColor c = line;
			c.setAlphaF(i % 4 == 0 ? 0.42 : 0.15);
			p.setPen(QPen(c, i % 4 == 0 ? 0.9 : 0.5));
			const double gy = paper.top() + paper.height() * i / 12;
			p.drawLine(QPointF(paper.left(), gy), QPointF(paper.right(), gy));
		}
	} else {
		p.setPen(QPen(theme::rule, 1));
		p.setBrush(theme::canvas);
		p.drawRoundedRect(r, 4, 4);
	}
	float v[5] = {};
	const bool any = values(v);
	shownTrack = any ? app->selTrack : -1;
	std::copy(v, v + 5, shown);
	if (!any) return;
	const Track* t = app->song.track(app->selTrack);
	const QColor col = theme::trackColor(t->role);
	const QColor ink = tactile ? QColor(0x2b, 0x26, 0x23) : col;
	const QColor labelInk = tactile ? QColor(0x5f, 0x4a, 0x37) : theme::dim;
	const QRectF in = area();
	p.save();
	p.setClipRect(paper.adjusted(1, 1, -1, -1));
	// the flat line: 0 dB
	QColor zero = tactile ? line : theme::rule;
	if (tactile) zero.setAlphaF(0.8);
	p.setPen(QPen(zero, 1, Qt::DashLine));
	p.drawLine(QPointF(paper.left(), yOf(0)), QPointF(paper.right(), yOf(0)));
	// the curve, from the equaliser itself
	Eq eq;
	eq.setNow(v, SR);
	QPainterPath path;
	const int w = int(in.width());
	for (int x = 0; x <= w; x++) {
		const double hz = 20.0 * std::pow(1000.0, double(x) / w);
		const double db = std::clamp(double(eq.responseDb(float(hz), SR)), -DB_RANGE * 1.4, DB_RANGE * 1.4);
		const QPointF pt(in.left() + x, yOf(db));
		if (x == 0)
			path.moveTo(pt);
		else
			path.lineTo(pt);
	}
	QPainterPath fill = path; // a light fill in the track's colour between the curve and the flat line
	fill.lineTo(in.right(), yOf(0));
	fill.lineTo(in.left(), yOf(0));
	fill.closeSubpath();
	QColor fc = col;
	fc.setAlpha(tactile ? 72 : 50);
	p.fillPath(fill, fc);
	p.setPen(QPen(ink, tactile ? 1.6 : 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
	p.setBrush(Qt::NoBrush);
	p.drawPath(path);
	// the scale under it
	QFont small = theme::lcdFont(8);
	small.setPixelSize(8);
	p.setFont(small);
	if (tactile) {
		p.setPen(labelInk);
		for (auto [f, label] : {std::pair{100.0, "100"}, {1000.0, "1k"}, {10000.0, "10k"}})
			p.drawText(QRectF(xOf(f) - 12, paper.bottom() - 10, 24, 9), Qt::AlignCenter, QString::fromLatin1(label));
	}
	// the points, the one the keys move ringed while the picture has the focus
	for (int i = 0; i < 4; i++) {
		const QPointF c = pointPos(i, v);
		const bool hot = i == hover || i == dragging;
		const bool off = i == 0 && v[0] <= 0.f;
		if (hasFocus() && i == chosen) {
			p.setPen(QPen(theme::amber, 2));
			p.setBrush(Qt::NoBrush);
			p.drawEllipse(c, 7.0, 7.0);
		}
		if (tactile) {
			p.setPen(Qt::NoPen);
			p.setBrush(QColor(0, 0, 0, 45));
			p.drawEllipse(c + QPointF(0, 1), 4.2, 4.2);
		}
		p.setPen(QPen(ink, 1.2));
		p.setBrush(off ? (tactile ? QColor(0xf7, 0xf0, 0xde) : theme::canvas) : hot ? col.lighter(125) : col);
		p.drawEllipse(c, hot ? 4.6 : 3.8, hot ? 4.6 : 3.8);
	}
	// what the point under the mouse, or being moved, is set to
	const int tagged = dragging >= 0 ? dragging : hover >= 0 ? hover : hasFocus() ? chosen : -1;
	if (tagged >= 0) {
		const QString text = pointText(tagged, v);
		const double tw = QFontMetricsF(small).horizontalAdvance(text) + 8;
		const QRectF tag(paper.right() - tw - 3, paper.top() + 3, tw, 11);
		p.setPen(Qt::NoPen);
		p.setBrush(col);
		p.drawRoundedRect(tag, 2, 2);
		p.setPen(tactile ? QColor(0xff, 0xf8, 0xee) : theme::accentInk);
		p.drawText(tag, Qt::AlignCenter, text);
	}
	p.restore();
	if (tactile) theme::drawGlass(p, paper, 3);
}

void EqCurve::dragTo(const QPointF& pos) {
	const float gain = float(std::clamp(dbAt(pos.y()), -12.0, 12.0) / 24.0 + 0.5);
	switch (dragging) {
	case 0: { // left of 22 Hz is off
		const double hz = hzAt(pos.x());
		set(0, hz < 22.0 ? 0.f : float(std::log(hz / 20.0) / std::log(40.0)));
		break;
	}
	case 1:
		set(1, gain);
		break;
	case 2:
		set(2, gain);
		set(3, float(std::log(std::clamp(hzAt(pos.x()), 150.0, 6000.0) / 150.0) / std::log(40.0)));
		break;
	default:
		set(4, gain);
		break;
	}
	app->touch();
}

void EqCurve::resetPoint(int point) {
	switch (point) {
	case 0:
		set(0, 0.f);
		break;
	case 1:
		set(1, 0.5f);
		break;
	case 2:
		set(2, 0.5f);
		set(3, 0.5f);
		break;
	default:
		set(4, 0.5f);
		break;
	}
}

void EqCurve::mousePressEvent(QMouseEvent* e) {
	if (e->button() != Qt::LeftButton || !app->curTrack()) return;
	const int at = pointAt(e->position());
	if (at < 0) return;
	dragging = chosen = at;
	keyRun = false;
	app->begin(tr("Equaliser"));
	update();
}
void EqCurve::mouseMoveEvent(QMouseEvent* e) {
	if (dragging >= 0) {
		dragTo(e->position());
		return;
	}
	const int h = pointAt(e->position());
	if (h != hover) {
		hover = h;
		update();
	}
}
void EqCurve::mouseReleaseEvent(QMouseEvent*) {
	dragging = -1;
	update();
}
void EqCurve::mouseDoubleClickEvent(QMouseEvent* e) {
	const int at = pointAt(e->position());
	if (at < 0 || !app->curTrack()) return;
	app->begin(tr("Equaliser"));
	resetPoint(at);
	app->touch();
}
void EqCurve::leaveEvent(QEvent*) {
	hover = -1;
	update();
}
void EqCurve::focusInEvent(QFocusEvent* e) {
	keyRun = false;
	// only a click or the Tab key gives it the focus: never the window opening, or the arrows would turn the
	// equaliser where they did nothing before
	if (e->reason() != Qt::TabFocusReason && e->reason() != Qt::BacktabFocusReason &&
	    e->reason() != Qt::MouseFocusReason) {
		clearFocus();
		return;
	}
	if (e->reason() == Qt::BacktabFocusReason) chosen = 3;
	if (e->reason() == Qt::TabFocusReason) chosen = 0;
	update();
}
void EqCurve::focusOutEvent(QFocusEvent*) {
	keyRun = false;
	update();
}

// Tab and Shift+Tab go from point to point, and on out of the picture after the last one
bool EqCurve::focusNextPrevChild(bool next) {
	if (hasFocus() && (next ? chosen < 3 : chosen > 0)) {
		chosen += next ? 1 : -1;
		keyRun = false;
		update();
		return true;
	}
	return QWidget::focusNextPrevChild(next);
}

void EqCurve::keyPressEvent(QKeyEvent* e) {
	float v[5];
	if (!app->curTrack() || !values(v)) return QWidget::keyPressEvent(e);
	const bool fine = e->modifiers() & Qt::ShiftModifier;
	const float gainStep = (fine ? 0.25f : 1.f) / 24.f, pitchStep = fine ? 0.005f : 0.025f;
	int index = -1;
	float to = 0.f;
	switch (e->key()) {
	case Qt::Key_Up:
	case Qt::Key_Down: {
		const float d = e->key() == Qt::Key_Up ? gainStep : -gainStep;
		if (chosen == 0) return e->accept(); // the low cut only moves sideways
		index = chosen == 1 ? 1 : chosen == 2 ? 2 : 4;
		to = v[index] + d;
		break;
	}
	case Qt::Key_Left:
	case Qt::Key_Right: {
		const float d = e->key() == Qt::Key_Right ? pitchStep : -pitchStep;
		if (chosen == 1 || chosen == 3) return e->accept(); // Low and High stay where they are
		index = chosen == 0 ? 0 : 3;
		to = v[index] + d;
		if (index == 0 && v[0] <= 0.f && d > 0) to = 0.1f; // switched on, at about 30 Hz
		if (index == 0 && to < 0.05f) to = 0.f;            // and off again below that
		break;
	}
	case Qt::Key_Delete:
	case Qt::Key_Backspace:
	case Qt::Key_Home:
		if (!keyRun) app->begin(tr("Equaliser"));
		keyRun = true;
		resetPoint(chosen);
		app->touch();
		return e->accept();
	default:
		return QWidget::keyPressEvent(e);
	}
	if (!keyRun) app->begin(tr("Equaliser")); // one undo step for a run of keys
	keyRun = true;
	set(index, to);
	app->touch();
	e->accept();
}
