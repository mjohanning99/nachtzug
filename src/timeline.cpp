// The song timeline: see timeline.h.
//
// What's here: Timeline, made by the main window: the paint functions in the order they are drawn, the mouse
// (moving, stretching and cutting clips, drawing knob lines, the loop row and the chord lane), and the menus of a
// clip, a chord, a track, a knob lane and Add track. The editing itself goes through App.
// Changes in the song file: none.

#include "timeline.h"
#include "app.h"
#include "widgets.h"
#include <QContextMenuEvent>
#include <QHelpEvent>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QToolTip>
#include <climits>
#include <cmath>

static QString valueText(int param, float v) {
	return param == AP_VOLUME ? QString::number(int(std::lround(v * 100.f))) + "%" : formatParam(param, v);
}

Timeline::Timeline(App* a, QWidget* parent) : QAbstractScrollArea(parent), app(a) {
	setFrameShape(QFrame::NoFrame);
	setFocusPolicy(Qt::NoFocus);
	viewport()->setMouseTracking(true);
	setMinimumHeight(110);
	connect(app, &App::changed, this, &Timeline::refresh);
	connect(app, &App::selectionChanged, this, &Timeline::refresh);
}

int Timeline::tx(double tick) const {
	return HDR + int(std::lround(tick * ppt())) - horizontalScrollBar()->value();
}
double Timeline::xt(int x) const {
	return double(x - HDR + horizontalScrollBar()->value()) / ppt();
}
int Timeline::rowH(int row) const {
	const auto& ts = app->song.tracks;
	return ROW + (row >= 0 && row < int(ts.size()) && ts[size_t(row)].laneOpen ? LANE : 0);
}
int Timeline::rowY(int row) const {
	int y = TOP - verticalScrollBar()->value();
	for (int i = 0; i < row && i < int(app->song.tracks.size()); i++)
		y += rowH(i);
	return y;
}
int Timeline::rowAt(int y, bool* inLane) const {
	if (inLane) *inLane = false;
	if (y < TOP) return -1;
	const int nt = int(app->song.tracks.size());
	int yy = TOP - verticalScrollBar()->value();
	for (int i = 0; i < nt; i++) {
		const int h = rowH(i);
		if (y < yy + h) {
			if (inLane) *inLane = y >= yy + ROW;
			return i;
		}
		yy += h;
	}
	return y < yy + ROW ? nt : -1;
}
QRect Timeline::clipRect(const Clip& c, int row) const {
	const int x = tx(c.start);
	return QRect(x, rowY(row) + 4, std::max(6, tx(c.end()) - x - 1), ROW - 9);
}
QRect Timeline::laneRect(int row) const {
	return QRect(HDR, rowY(row) + ROW, viewport()->width() - HDR, LANE - 1);
}

Clip* Timeline::clipAt(const QPoint& p, Track** owner, bool* edge) {
	bool inLane = false;
	const int row = rowAt(p.y(), &inLane);
	if (row < 0 || row >= int(app->song.tracks.size()) || p.x() < HDR || inLane) return nullptr;
	Track& t = app->song.tracks[size_t(row)];
	for (int i = int(t.clips.size()) - 1; i >= 0; i--) {
		const QRect r = clipRect(t.clips[size_t(i)], row);
		if (r.contains(p)) {
			if (owner) *owner = &t;
			if (edge) *edge = p.x() > r.right() - 7;
			return &t.clips[size_t(i)];
		}
	}
	return nullptr;
}

// Index of the line point under p in the lane of this row, or -1. base: start tick of the repeat it was found in.
int Timeline::lanePointAt(int row, const QPoint& p, int* base) const {
	const Track& t = app->song.tracks[size_t(row)];
	const AutoLane* l = t.lane(t.laneParam);
	const int b = l && l->loop > 0 ? int(std::floor(xt(p.x()) / l->loop)) * l->loop : 0;
	if (base) *base = std::max(0, b);
	if (!l) return -1;
	const QRect r = laneRect(row);
	for (int k = -1; k <= 1; k++) { // also look in the neighbouring repeats, for points near a seam
		const int bb = b + k * l->loop;
		if (bb < 0 || (k != 0 && l->loop == 0)) continue;
		for (size_t i = 0; i < l->pts.size(); i++)
			if (std::abs(tx(bb + l->pts[i].tick) - p.x()) <= 6 && std::abs(laneY(r, l->pts[i].v) - p.y()) <= 6) {
				if (base) *base = bb;
				return int(i);
			}
	}
	return -1;
}

void Timeline::updateScroll() {
	const int total = std::max(app->song.endTick(), app->song.loopEnd) + 8 * BAR;
	const int nt = int(app->song.tracks.size());
	const int w = int(total * ppt()), h = rowY(nt) + verticalScrollBar()->value() - TOP + ROW;
	horizontalScrollBar()->setRange(0, std::max(0, w - (viewport()->width() - HDR)));
	horizontalScrollBar()->setPageStep(viewport()->width() - HDR);
	horizontalScrollBar()->setSingleStep(int(pxPerBar));
	verticalScrollBar()->setRange(0, std::max(0, h - (viewport()->height() - TOP)));
	verticalScrollBar()->setPageStep(viewport()->height() - TOP);
}

void Timeline::refresh() {
	updateScroll();
	viewport()->update();
}
void Timeline::zoom(double factor) {
	const double left = xt(HDR);
	pxPerBar = std::clamp(pxPerBar * factor, 14.0, 400.0);
	updateScroll();
	horizontalScrollBar()->setValue(int(left * ppt()));
	viewport()->update();
}
void Timeline::showWholeSong() {
	const int bars = std::max(8, (app->song.endTick() + BAR - 1) / BAR + 1);
	pxPerBar = std::clamp(double(viewport()->width() - HDR - 20) / bars, 14.0, 400.0);
	updateScroll();
	horizontalScrollBar()->setValue(0);
	viewport()->update();
}
void Timeline::resizeEvent(QResizeEvent*) {
	updateScroll();
}
void Timeline::setScissors(bool on) {
	scissors = on;
	viewport()->setCursor(on ? theme::cursor(QStringLiteral("scissors"), Qt::CrossCursor) : QCursor(Qt::ArrowCursor));
}

void Timeline::followPlayhead() {
	if (!app->isPlaying() || drag != None) return;
	const int x = tx(app->pos());
	if (x > viewport()->width() - 30 || x < HDR) horizontalScrollBar()->setValue(int(app->pos() * ppt()) - 40);
}

// ---------------------------------------------------------------- painting

// what every part of the timeline is drawn with: the painter, the size, the song, the fonts, the bars in view, the
// playhead and the chord lane's top
struct Timeline::Frame {
	QPainter& p;
	int W, H;
	const Song& s;
	QFont f, fs, fb;
	int firstBar, lastBar, nt, lanesBottom;
	double mNow;
	int px, cy;
};

void Timeline::paintEvent(QPaintEvent*) {
	QPainter p(viewport());
	const int W = viewport()->width(), H = viewport()->height();
	const Song& s = app->song;
	p.fillRect(0, 0, W, H, theme::canvas);
	QFont f = font();
	f.setPixelSize(11);
	QFont fs = f;
	fs.setPixelSize(10);
	QFont fb = f;
	fb.setWeight(QFont::DemiBold); // the Grotesk’s heaviest, 600
	fb.setPixelSize(12);

	const int firstBar = std::max(0, int(xt(HDR)) / BAR), lastBar = int(xt(W)) / BAR + 1;
	const int nt = int(s.tracks.size());
	const int lanesBottom = std::min(H, rowY(nt));
	const double mNow = app->musicalPos();
	const Frame fr{p, W, H, s, f, fs, fb, firstBar, lastBar, nt, lanesBottom, mNow, tx(app->pos()), RULER + LOOPROW};
	paintGrid(fr);
	paintClips(fr);
	paintPlayhead(fr);
	paintRuler(fr);
	paintLoopRow(fr);
	paintChords(fr);
	paintEmptyNote(fr);
	paintHeaders(fr);
}

// the track rows, the looped stretch a little lighter, and the bar lines
void Timeline::paintGrid(const Frame& fr) {
	QPainter& p = fr.p;
	const int W = fr.W;
	const int H = fr.H;
	const Song& s = fr.s;
	const int firstBar = fr.firstBar;
	const int lastBar = fr.lastBar;
	const int nt = fr.nt;
	const int lanesBottom = fr.lanesBottom;
	// track rows
	for (int i = 0; i < nt; i++) {
		const int y = rowY(i);
		if (y + rowH(i) < TOP || y > H) continue;
		const Track& t = s.tracks[size_t(i)];
		p.fillRect(HDR, y, W - HDR, ROW - 1, t.id == app->selTrack ? theme::laneSel : theme::lane);
		if (t.laneOpen) p.fillRect(HDR, y + ROW, W - HDR, LANE - 1, theme::well);
	}
	if (s.loopOn) { // the looped stretch is a little lighter
		const int a = std::max(HDR, tx(s.loopStart)), b = std::min(W, tx(s.loopEnd));
		if (b > a) p.fillRect(a, TOP, b - a, std::max(0, lanesBottom - TOP), QColor(128, 128, 128, 28));
	}
	for (int bar = firstBar; bar <= lastBar; bar++) {
		const int x = tx(bar * BAR);
		if (x < HDR) continue;
		p.setPen(bar % 4 == 0 ? theme::gridBar : theme::grid);
		p.drawLine(x, TOP, x, lanesBottom);
	}
}

// each track's clips: their colour, notes, name, repeats and transitions, and its knob line
void Timeline::paintClips(const Frame& fr) {
	QPainter& p = fr.p;
	const int W = fr.W;
	const int H = fr.H;
	const Song& s = fr.s;
	const QFont& f = fr.f;
	const int nt = fr.nt;
	p.setRenderHint(QPainter::Antialiasing, true);
	for (int i = 0; i < nt; i++) {
		const int y = rowY(i);
		if (y + rowH(i) < TOP || y > H) continue;
		const Track& t = s.tracks[size_t(i)];
		const QColor col = theme::trackColor(t.role);

		// clips
		for (const Clip& c : t.clips) {
			const QRect r = clipRect(c, i);
			if (r.right() < HDR || r.left() > W) continue;
			const bool sel = app->selClips.contains(c.id), shown = c.id == app->editClip;
			const QColor fill = theme::blockFill(col, sel, false);
			theme::drawBlock(p, r, fill, theme::tactile ? 5 : 3);
			if (sel || shown) {
				p.setPen(sel ? QPen(theme::ink, 2) : QPen(theme::tactile ? col.darker(170) : col.lighter(140), 1.5));
				p.setBrush(Qt::NoBrush);
				p.drawRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), 5, 5);
			}
			// the clip's name: the idea it came from. The ticks and notes leave its place free.
			const QString name =
			    c.ad.on ? tr("Auto drummer · %1").arg(ui(DRUM_STYLES[std::clamp(c.ad.style, 0, NUM_DRUM_STYLES - 1)]))
				        : ui(c.name);
			p.setFont(f);
			const bool named = !name.isEmpty() && r.width() > 50;
			const QString shownName =
			    named ? p.fontMetrics().elidedText(name, Qt::ElideRight, r.width() - 14) : QString();
			const QRect tag(r.left() + 3, r.top() + 3, p.fontMetrics().horizontalAdvance(shownName) + 8,
			                p.fontMetrics().height() + 1);
			const QRect inside = r.adjusted(2, 2, -2, -2);
			p.setClipRegion(named ? QRegion(inside).subtracted(QRegion(tag)) : QRegion(inside));
			p.setPen(QPen(theme::clipInk, 1)); // small ticks where the pattern repeats
			if (c.loopLen > 0 && c.loopLen * ppt() >= 6)
				for (int k = c.loopLen - c.offset; k < c.len; k += c.loopLen) {
					const int x = tx(c.start + k);
					p.drawLine(x, r.top() + 2, x, r.top() + 9);
				}
			if (!c.notes.empty()) { // the notes, small
				int lo = INT_MAX, hi = INT_MIN;
				for (const Note& n : c.notes) {
					lo = std::min(lo, n.deg);
					hi = std::max(hi, n.deg);
				}
				const double span = std::max(1, hi - lo);
				p.setPen(Qt::NoPen);
				p.setBrush(theme::clipInk);
				for (int base = -c.offset; base < c.len; base += std::max(1, c.loopLen))
					for (const Note& n : c.notes) {
						if (n.start >= c.loopLen) continue;
						const int at = base + n.start;
						if (at < 0 || at >= c.len) continue;
						const int x = tx(c.start + at), w = std::max(2, int((t.isDrums() ? 10 : n.len) * ppt()));
						const int yy = r.bottom() - 5 - int((n.deg - lo) / span * (r.height() - 22));
						p.drawRect(x, yy, w, 2);
					}
			}
			p.setClipRect(inside);
			if (named) {
				p.setPen(theme::clipInk);
				p.drawText(tag.adjusted(4, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, shownName);
			}
			// transitions: a dark wedge where the clip fades or sweeps in or out
			p.setPen(Qt::NoPen);
			p.setBrush(QColor(0, 0, 0, 90));
			if (c.inKind) {
				const int w = std::min(r.width(), int(c.inLen * ppt()));
				QPolygon tri;
				tri << r.topLeft() << QPoint(r.left() + w, r.top()) << r.bottomLeft();
				p.drawPolygon(tri);
			}
			if (c.outKind) {
				const int w = std::min(r.width(), int(c.outLen * ppt()));
				QPolygon tri;
				tri << r.topRight() << QPoint(r.right() - w, r.top()) << r.bottomRight();
				p.drawPolygon(tri);
			}
			p.setClipping(false);
		}

		// every knob line of this track, thin, over the clips
		p.setClipRect(QRect(HDR, y, W - HDR, ROW - 1));
		for (const AutoLane& l : t.lanes) {
			if (l.pts.empty()) continue;
			QPainterPath path;
			for (int x = HDR; x <= W + 4; x += 4) {
				const double yy = y + ROW - 5 - double(l.valueAt(xt(x), 0.f)) * (ROW - 10);
				if (x == HDR)
					path.moveTo(x, yy);
				else
					path.lineTo(x, yy);
			}
			const bool paused = l.param < 64 && ((t.bypass >> l.param) & 1);
			QColor lc = theme::lcd;
			lc.setAlpha(paused ? 90 : 220);
			p.setPen(QPen(lc, 1.3, paused ? Qt::DashLine : Qt::SolidLine));
			p.setBrush(Qt::NoBrush);
			p.drawPath(path);
		}
		p.setClipping(false);

		// the open line lane: one knob's line with its points
		if (t.laneOpen) {
			const QRect r = laneRect(i);
			p.setClipRect(r);
			p.setPen(QPen(theme::grid, 1));
			for (float g : {0.25f, 0.5f, 0.75f}) {
				const int yy = int(laneY(r, g));
				p.drawLine(HDR, yy, W, yy);
			}
			const AutoLane* l = t.lane(t.laneParam);
			const float base = t.laneParam == AP_VOLUME ? t.vol : t.patch.p[t.laneParam];
			if (l && l->loop > 0 && l->loop * ppt() >= 8) { // where the shape starts again
				p.setPen(QPen(theme::rule, 1, Qt::DotLine));
				for (int k = int(xt(HDR)) / l->loop * l->loop; tx(k) <= W; k += l->loop)
					if (tx(k) >= HDR) p.drawLine(tx(k), r.top(), tx(k), r.bottom());
			}
			if (!l || l->pts.empty()) {
				p.setPen(QPen(theme::dim, 1, Qt::DashLine));
				p.drawLine(HDR, int(laneY(r, base)), W, int(laneY(r, base)));
				p.setFont(f);
				p.setPen(theme::dim);
				p.drawText(r.adjusted(10, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft,
				           tr("Click here to add points and draw how %1 changes over time. Drag points to move them, "
				              "right-click a point to remove it.")
				               .arg(paramLabel(t.laneParam)));
			} else {
				QPainterPath path;
				for (int x = HDR; x <= W + 2; x += 2) {
					const double yy = laneY(r, l->valueAt(xt(x), base));
					if (x == HDR)
						path.moveTo(x, yy);
					else
						path.lineTo(x, yy);
				}
				p.setPen(QPen(theme::lcd, 2));
				p.setBrush(Qt::NoBrush);
				p.drawPath(path);
				p.setPen(QPen(theme::capInk, 1.5));
				p.setBrush(theme::lcd);
				const int step = std::max(1, l->loop);
				for (int b = l->loop > 0 ? std::max(0, int(xt(HDR)) / step * step) : 0;; b += step) {
					for (const AutoPoint& a : l->pts) {
						const int x = tx(b + a.tick);
						if (x >= HDR - 6 && x <= W + 6) p.drawEllipse(QPointF(x, laneY(r, a.v)), 4, 4);
					}
					if (l->loop == 0 || tx(b + step) > W) break;
				}
			}
			p.setClipping(false);
		}
	}
	p.setRenderHint(QPainter::Antialiasing, false);
}

// the playhead across the tracks
void Timeline::paintPlayhead(const Frame& fr) {
	QPainter& p = fr.p;
	const int H = fr.H;
	const int px = fr.px;
	// playhead
	if (px >= HDR) {
		p.setPen(QPen(theme::ink, 1));
		p.drawLine(px, 0, px, H);
	}
}

// the bar numbers along the top, and the playhead's mark in them
void Timeline::paintRuler(const Frame& fr) {
	QPainter& p = fr.p;
	const int W = fr.W;
	const QFont& f = fr.f;
	const int firstBar = fr.firstBar;
	const int lastBar = fr.lastBar;
	const int px = fr.px;
	// bar ruler
	p.fillRect(HDR, 0, W - HDR, RULER, theme::well);
	p.setFont(f);
	const int every = pxPerBar >= 44 ? 1 : pxPerBar >= 22 ? 2 : 4;
	for (int bar = firstBar; bar <= lastBar; bar++) {
		const int x = tx(bar * BAR);
		if (x < HDR - 20) continue;
		p.setPen(theme::gridBar);
		p.drawLine(x, RULER - 6, x, RULER);
		if (bar % every == 0) {
			p.setPen(theme::dim);
			p.drawText(x + 4, 14, QString::number(bar + 1));
		}
	}
	if (px >= HDR) {
		QPolygon tri;
		tri << QPoint(px - 5, 0) << QPoint(px + 5, 0) << QPoint(px, 8);
		p.setPen(Qt::NoPen);
		p.setBrush(theme::ink);
		p.drawPolygon(tri);
	}
}

// the loop row: the stretch that repeats
void Timeline::paintLoopRow(const Frame& fr) {
	QPainter& p = fr.p;
	const int W = fr.W;
	const Song& s = fr.s;
	const QFont& fs = fr.fs;
	// loop row
	p.fillRect(HDR, RULER, W - HDR, LOOPROW, theme::canvas);
	{
		const int a = tx(s.loopStart), b = tx(s.loopEnd);
		QColor lc = theme::amber;
		lc.setAlpha(s.loopOn ? 255 : 70);
		p.setPen(Qt::NoPen);
		p.setBrush(lc);
		p.drawRoundedRect(QRect(a, RULER + 2, std::max(4, b - a), LOOPROW - 4), 3, 3);
		if (b - a > 70) {
			p.setFont(fs);
			p.setPen(s.loopOn ? theme::accentInk : theme::dim);
			p.drawText(QRect(a + 6, RULER + 2, b - a - 8, LOOPROW - 4), Qt::AlignVCenter | Qt::AlignLeft,
			           s.loopOn ? tr("LOOP") : tr("LOOP (OFF)"));
		}
	}
}

// the chord lane: a block for each stretch of the same chord
void Timeline::paintChords(const Frame& fr) {
	QPainter& p = fr.p;
	const int W = fr.W;
	const Song& s = fr.s;
	const QFont& f = fr.f;
	const QFont& fs = fr.fs;
	const int firstBar = fr.firstBar;
	const int lastBar = fr.lastBar;
	const int cy = fr.cy;
	// chord lane
	p.fillRect(HDR, cy, W - HDR, CHORDS, theme::wellHi);
	{ // one block for each stretch of the same chord, shaded in turn, so the changes show at any zoom; the name
		// when it fits, in smaller letters when the bars are narrow
		auto same = [&](int a, int b) { return s.chordAt(a) == s.chordAt(b) && s.chordAltAt(a) == s.chordAltAt(b); };
		int run = 0; // which stretch this is, counted from the start, so the shading does not jump when scrolling
		for (int bar = 1; bar <= firstBar; bar++)
			if (!same(bar, bar - 1)) run++;
		QFont tiny = fs;
		tiny.setPixelSize(9);
		p.setFont(pxPerBar >= 30 ? f : pxPerBar >= 18 ? fs : tiny);
		p.setRenderHint(QPainter::Antialiasing, true);
		for (int bar = firstBar; bar <= lastBar;) {
			int end = bar + 1;
			while (end <= lastBar && same(end, bar))
				end++;
			const int x0 = tx(bar * BAR), x1 = tx(end * BAR);
			const QRectF box(x0 + 1, cy + 2.5, std::max(2, x1 - x0 - 2), CHORDS - 5);
			QColor tint = theme::amber;
			tint.setAlpha(theme::isLight() ? 46 : 60);
			p.setPen(QPen(theme::gridBar, 1));
			p.setBrush(theme::panel);
			p.drawRoundedRect(box, 2.5, 2.5);
			if (run % 2) {
				p.setPen(Qt::NoPen);
				p.setBrush(tint);
				p.drawRoundedRect(box, 2.5, 2.5);
			}
			const QString name = s.chordName(s.chordAt(bar), s.chordAltAt(bar));
			const int pad = box.width() >= 30 ? 4 : 2, room = int(box.width()) - 2 * pad;
			if (room >= 5) {
				const QString shown = p.fontMetrics().elidedText(name, Qt::ElideRight, room);
				if (shown != QStringLiteral("…")) {
					p.setPen(theme::ink);
					p.drawText(box.adjusted(pad, 0, -pad + 2, 0), Qt::AlignVCenter | Qt::AlignLeft, shown);
				}
			}
			bar = end;
			run++;
		}
		p.setRenderHint(QPainter::Antialiasing, false);
	}
}

// what an empty song says: how to begin
void Timeline::paintEmptyNote(const Frame& fr) {
	QPainter& p = fr.p;
	const int W = fr.W;
	const int nt = fr.nt;
	if (nt == 0) { // an empty song says how to begin
		QFont big = font();
		big.setPixelSize(15);
		p.setFont(big);
		p.setPen(theme::dim);
		p.drawText(QRect(HDR + 20, TOP + 30, W - HDR - 40, 80), Qt::AlignCenter | Qt::TextWordWrap,
		           tr("This song is empty.\nPress + Add track above to add a bass line, sequence, pad, lead or "
		              "drums,\nor choose File → New song with fresh ideas to get a song with everything in it."));
	}
}

// the header column: each track's name, keys and volume, and the focus line
void Timeline::paintHeaders(const Frame& fr) {
	QPainter& p = fr.p;
	const int W = fr.W;
	const int H = fr.H;
	const Song& s = fr.s;
	const QFont& f = fr.f;
	const QFont& fs = fr.fs;
	const QFont& fb = fr.fb;
	const int nt = fr.nt;
	const double mNow = fr.mNow;
	const int cy = fr.cy;
	theme::drawSunken(p, QRectF(HDR, TOP, W - HDR, H - TOP));
	// header column
	p.fillRect(0, 0, HDR, H, theme::panel);
	p.setPen(theme::dim);
	p.setFont(fs);
	p.drawText(QRect(12, 0, HDR - 16, RULER), Qt::AlignVCenter | Qt::AlignLeft, tr("BARS"));
	p.drawText(QRect(12, RULER, HDR - 16, LOOPROW), Qt::AlignVCenter | Qt::AlignLeft, tr("LOOP"));
	p.drawText(QRect(12, cy, HDR - 16, CHORDS), Qt::AlignVCenter | Qt::AlignLeft, tr("CHORDS"));
	p.setClipRect(QRect(0, TOP, HDR, H - TOP));
	auto keyFace = [&](const QRect& r, const QString& text, bool on, const QColor& onCol, const QString& symbol = {}) {
		theme::drawKey(p, r, text, on, onCol, symbol);
	};
	for (int i = 0; i < nt; i++) {
		const int y = rowY(i);
		if (y + rowH(i) < TOP || y > H) continue;
		const Track& t = s.tracks[size_t(i)];
		const QColor col = theme::trackColor(t.role);
		const bool sel = t.id == app->selTrack;
		p.fillRect(0, y, HDR, rowH(i) - 1, sel ? theme::wellHi : theme::well);
		p.fillRect(0, y, 5, rowH(i) - 1, col);
		p.setFont(fb);
		p.setPen(theme::ink);
		theme::drawRoleIcon(p, QRectF(12, y + 7, 15, 15), t.role, theme::ink);
		p.drawText(QRect(33, y + 5, HDR - 153, 20), Qt::AlignVCenter | Qt::AlignLeft,
		           p.fontMetrics().elidedText(ui(t.name), Qt::ElideRight, HDR - 153));
		p.setFont(f);
		keyFace(QRect(HDR - 114, y + 6, 24, 18), QString(), t.mute, QColor(0xc4, 0x36, 0x1c), QStringLiteral("mute"));
		keyFace(QRect(HDR - 86, y + 6, 24, 18), QString(), t.solo, theme::amber, QStringLiteral("solo"));
		keyFace(QRect(HDR - 58, y + 6, 48, 18), theme::symbolsOnly ? QString() : tr("Lines"), t.laneOpen, theme::amber,
		     QStringLiteral("lines"));
		// volume slider with the level meter underneath
		const QRect vr(12, y + 32, HDR - 24, 8);
		const float vol = app->paramValue(t, AP_VOLUME);
		theme::drawSlider(p, QRectF(vr).adjusted(4, 1, -4, -1), vol, col);
		const float pk =
		    std::clamp(app->engine->trackPeak[std::min(i, MAX_TRACKS - 1)].load(std::memory_order_relaxed), 0.f, 1.f);
		p.fillRect(QRect(12, y + 45, int((HDR - 24) * std::sqrt(pk)), 3), theme::ink);
		if (t.laneOpen) {
			const AutoLane* l = t.lane(t.laneParam);
			const float now = l ? l->valueAt(mNow, t.laneParam == AP_VOLUME ? t.vol : t.patch.p[t.laneParam])
			                    : (t.laneParam == AP_VOLUME ? t.vol : t.patch.p[t.laneParam]);
			keyFace(QRect(12, y + ROW + 8, HDR - 24, 24), tr("Line: %1  ▾").arg(paramLabel(t.laneParam)), false,
			     theme::key);
			const int lp = l ? l->loop : 0;
			keyFace(QRect(12, y + ROW + 38, HDR - 24, 24),
			     (lp == 0         ? tr("Follows the song")
			      : lp == BAR / 2 ? tr("Repeats every ½ bar")
			      : lp == BAR     ? tr("Repeats every bar")
			                      : tr("Repeats every %1 bars").arg(lp / BAR)) +
			         QStringLiteral("  ▾"),
			     false, theme::key);
			p.setFont(fs);
			p.setPen(theme::dim);
			p.drawText(QRect(HDR - 70, y + ROW - 13, 62, 12), Qt::AlignRight | Qt::AlignVCenter,
			           valueText(t.laneParam, now));
			p.setFont(f);
		}
	}
	p.setClipping(false);
	if (app->focus == App::FocusArrange) p.fillRect(0, 0, W, 2, theme::amber); // Edit-menu commands act on this view
}

// ---------------------------------------------------------------- help

QString Timeline::helpAt(const QPoint& p) {
	const int nt = int(app->song.tracks.size());
	if (p.y() < RULER) return tr("Bar numbers. Click or drag here to move the playhead; the song plays from there.");
	if (p.y() < RULER + LOOPROW)
		return tr("The amber bar is the loop: while Loop is on, the song keeps repeating this stretch. Drag here to "
		          "mark a different stretch. Click to switch looping on or off.");
	if (p.y() < TOP) {
		const int bar = std::max(0, int(xt(p.x())) / BAR);
		if (p.x() >= HDR)
			return tr("Bar %1: %2.")
			           .arg(bar + 1)
			           .arg(app->song.chordName(app->song.chordAt(bar), app->song.chordAltAt(bar))) +
			       QLatin1Char(' ') +
			       tr("The chord of each bar. Tracks set to “Follow chords” shift their notes to fit it, so one "
			          "pattern follows the harmony. Click a bar to change the chord of that bar only.");
		return tr("The chord of each bar. Tracks set to “Follow chords” shift their notes to fit it, so one pattern "
		          "follows the harmony. Click a bar to change the chord of that bar only.");
	}
	bool inLane = false;
	const int row = rowAt(p.y(), &inLane);
	if (row < 0) return QString();
	if (row == nt) return QString();
	const int y = rowY(row);
	if (p.x() < HDR) {
		if (inLane)
			return p.y() < y + ROW + 35 ? tr("Which knob this line controls. Click to pick another.")
			                            : tr("Whether the line follows the whole song, or is a short shape that "
			                                 "repeats, for example every bar.");
		if (QRect(HDR - 114, y + 6, 24, 18).contains(p))
			return tr("Mute: silence this track. While the song plays, it switches on the next bar.");
		if (QRect(HDR - 86, y + 6, 24, 18).contains(p)) return tr("Solo: hear only this track.");
		if (QRect(HDR - 58, y + 6, 48, 18).contains(p))
			return tr("Lines: open a lane under this track where you draw how a knob (for example Cutoff) changes over "
			          "time.");
		if (p.y() >= y + 27 && p.y() <= y + 44) return tr("Track volume. Drag left or right.");
		return tr("Click to select this track: the letter keys then play it. Double-click the name to rename it, "
		          "right-click for more.");
	}
	if (inLane)
		return tr("Click to add a point, drag a point to move it, right-click a point to remove it. Hold Shift for "
		          "fine steps. The sound follows this line when the song plays.");
	if (clipAt(p, nullptr, nullptr))
		return scissors ? tr("Click to cut this clip in two here.")
		                : tr("Drag to move, drag the right edge to make it longer (the pattern repeats), double-click "
		                     "to edit its notes. Right-click for more, including fades and filter sweeps into and out "
		                     "of the clip.");
	return tr("Double-click to make a new clip here.");
}

bool Timeline::viewportEvent(QEvent* e) {
	if (e->type() == QEvent::ToolTip) {
		auto* h = static_cast<QHelpEvent*>(e);
		const QString text = drag == None ? helpAt(h->pos()) : QString();
		if (text.isEmpty())
			QToolTip::hideText();
		else
			QToolTip::showText(h->globalPos(), text, viewport());
		return true;
	}
	return QAbstractScrollArea::viewportEvent(e);
}

// ---------------------------------------------------------------- mouse

void Timeline::setVolumeFromX(int x) {
	if (dragTrack < 0) return;
	app->paramMove(dragTrack, AP_VOLUME, std::clamp(float(x - 16) / float(HDR - 32), 0.f, 1.f));
}

// Moves the line point being dragged to the mouse position.
void Timeline::dragPoint(QMouseEvent* e) {
	if (pointRow < 0 || pointRow >= int(app->song.tracks.size())) return;
	Track& t = app->song.tracks[size_t(pointRow)];
	AutoLane* l = t.lane(t.laneParam);
	if (!l || pointIdx < 0 || pointIdx >= int(l->pts.size())) return;
	const QRect r = laneRect(pointRow);
	const int snap = (e->modifiers() & Qt::ShiftModifier) ? 1 : 24;
	int tick = int(std::lround((xt(e->pos().x()) - pointBase) / snap)) * snap;
	const int lo = pointIdx > 0 ? l->pts[size_t(pointIdx - 1)].tick + 1 : 0;
	int hi = pointIdx + 1 < int(l->pts.size()) ? l->pts[size_t(pointIdx + 1)].tick - 1 : INT_MAX;
	if (l->loop > 0) hi = std::min(hi, l->loop - 1);
	tick = std::clamp(tick, lo, std::max(lo, hi));
	const float v = laneV(r, e->pos().y());
	if (!began) {
		app->begin();
		began = true;
	}
	l->pts[size_t(pointIdx)] = {tick, v};
	QToolTip::showText(e->globalPosition().toPoint(), paramLabel(t.laneParam) + ": " + valueText(t.laneParam, v),
	                   viewport());
	app->touch();
}

void Timeline::mousePressEvent(QMouseEvent* e) {
	const QPoint p = e->pos();
	bool inLane = false;
	const int row = rowAt(p.y(), &inLane);
	const int nt = int(app->song.tracks.size());
	if (e->button() == Qt::RightButton) { // right-click a line point: remove it
		if (inLane && row >= 0 && row < nt && p.x() >= HDR) {
			const int idx = lanePointAt(row, p, nullptr);
			if (idx >= 0)
				app->edit([&](Song& s) {
					Track& t = s.tracks[size_t(row)];
					if (AutoLane* l = t.lane(t.laneParam)) l->pts.erase(l->pts.begin() + idx);
				});
		}
		return;
	}
	if (e->button() != Qt::LeftButton) return;
	app->focus = App::FocusArrange;
	pressPos = p;
	pressTick = xt(p.x());
	began = false;
	moved = false;
	lastDt = 0;
	if (p.x() >= HDR && p.y() < RULER) {
		drag = Seek;
		app->seek(std::max(0.0, std::round(pressTick / 24.0) * 24.0));
		return;
	}
	if (p.x() >= HDR && p.y() < RULER + LOOPROW) {
		drag = Loop;
		return;
	}
	if (p.x() >= HDR && p.y() < TOP) {
		chordMenu(std::max(0, int(pressTick)) / BAR, e->globalPosition().toPoint());
		return;
	}
	if (row < 0) return;
	if (p.x() < HDR) {
		if (row == nt) return;
		Track& t = app->song.tracks[size_t(row)];
		const int y = rowY(row), id = t.id;
		if (inLane) {
			app->selectTrack(id);
			if (QRect(12, y + ROW + 8, HDR - 24, 24).contains(p))
				laneParamMenu(id, e->globalPosition().toPoint());
			else if (QRect(12, y + ROW + 38, HDR - 24, 24).contains(p))
				laneRepeatMenu(id, e->globalPosition().toPoint());
			return;
		}
		if (QRect(HDR - 114, y + 6, 24, 18).contains(p)) {
			app->onBar([id](Song& s) {
				if (Track* k = s.track(id)) k->mute = !k->mute;
			});
			return;
		}
		if (QRect(HDR - 86, y + 6, 24, 18).contains(p)) {
			app->onBar([id](Song& s) {
				if (Track* k = s.track(id)) k->solo = !k->solo;
			});
			return;
		}
		if (QRect(HDR - 58, y + 6, 48, 18)
		        .contains(p)) { // open or close the line lane (a view setting, not an undo step)
			t.laneOpen = !t.laneOpen;
			if (t.isDrums()) t.laneParam = AP_VOLUME;
			app->selectTrack(id);
			app->touch();
			return;
		}
		app->selectTrack(id);
		if (p.y() >= y + 27 && p.y() <= y + 44) {
			drag = Volume;
			dragTrack = id;
			app->paramPress(id, AP_VOLUME);
			setVolumeFromX(p.x());
		}
		return;
	}
	if (row == nt) return;
	if (inLane) { // draw the knob line
		Track& t = app->song.tracks[size_t(row)];
		app->selectTrack(t.id);
		int base = 0;
		int idx = lanePointAt(row, p, &base);
		if (idx < 0) { // add a point here
			app->begin();
			began = true;
			if (!t.lane(t.laneParam)) {
				AutoLane fresh;
				fresh.param = t.laneParam;
				t.lanes.push_back(fresh);
			}
			AutoLane* l = t.lane(t.laneParam);
			const int snap = (e->modifiers() & Qt::ShiftModifier) ? 1 : 24;
			int tick = std::max(0, int(std::lround((pressTick - base) / snap)) * snap);
			if (l->loop > 0) tick = std::min(tick, l->loop - 1);
			std::erase_if(l->pts, [&](const AutoPoint& a) { return a.tick == tick; });
			const auto at = std::upper_bound(l->pts.begin(), l->pts.end(), tick,
			                                 [](int tk, const AutoPoint& a) { return tk < a.tick; });
			idx = int(at - l->pts.begin());
			l->pts.insert(at, AutoPoint{tick, laneV(laneRect(row), p.y())});
			if (t.laneParam < 64) t.bypass &= ~(quint64(1) << t.laneParam);
			app->touch();
		}
		drag = Point;
		pointIdx = idx;
		pointBase = base;
		pointRow = row;
		return;
	}
	Track* owner = nullptr;
	bool edge = false;
	Clip* c = clipAt(p, &owner, &edge);
	if (!c) {
		app->selectClip(-1);
		app->selectTrack(app->song.tracks[size_t(row)].id);
		return;
	}
	const int snap = (e->modifiers() & Qt::ShiftModifier) ? 24 : BEAT;
	if (scissors) {
		app->splitClip(c->id, int(std::lround(pressTick / snap)) * snap);
		return;
	}
	const int id = c->id;
	if (e->modifiers() & Qt::ControlModifier) {
		app->selectClip(id, true);
		return;
	}
	if (!app->selClips.contains(id))
		app->selectClip(id);
	else if (app->editClip != id) {
		app->editClip = id;
		if (owner) app->selTrack = owner->id;
		emit app->selectionChanged();
	}
	orig.clear();
	for (const Track& t : app->song.tracks)
		for (const Clip& k : t.clips)
			if (app->selClips.contains(k.id)) orig.push_back({k.id, k.start, k.len});
	drag = edge ? Resize : Move;
	dragTrack = row;
}

void Timeline::mouseMoveEvent(QMouseEvent* e) {
	const QPoint p = e->pos();
	if (drag == None) {
		if (scissors) return;
		bool edge = false, inLane = false;
		const int row = rowAt(p.y(), &inLane);
		if (inLane && row >= 0 && row < int(app->song.tracks.size()) && p.x() >= HDR)
			viewport()->setCursor(lanePointAt(row, p, nullptr) >= 0 ? Qt::SizeAllCursor : Qt::CrossCursor);
		else
			viewport()->setCursor(clipAt(p, nullptr, &edge) && edge
			                          ? theme::cursor(QStringLiteral("handle"), Qt::SizeHorCursor)
									  : QCursor(Qt::ArrowCursor));
		return;
	}
	if (drag == Volume) {
		setVolumeFromX(p.x());
		return;
	}
	if (drag == Point) {
		dragPoint(e);
		return;
	}
	if (drag == Seek) {
		app->seek(std::max(0.0, std::round(xt(p.x()) / 24.0) * 24.0));
		return;
	}
	if (drag == Loop) {
		if (!moved && std::abs(p.x() - pressPos.x()) < 5) return;
		moved = true;
		const int a = std::max(0, int(pressTick)) / BAR, b = std::max(0, int(xt(p.x()))) / BAR;
		if (!began) {
			app->begin();
			began = true;
		}
		app->song.loopStart = std::min(a, b) * BAR;
		app->song.loopEnd = (std::max(a, b) + 1) * BAR;
		app->song.loopOn = true;
		app->touch();
		return;
	}
	if (drag != Move && drag != Resize) return;
	const int snap = (e->modifiers() & Qt::ShiftModifier) ? 24 : BEAT;
	int dt = int(std::lround((xt(p.x()) - pressTick) / snap)) * snap;
	if (drag == Move) {
		int minS = INT_MAX;
		for (const Orig& o : orig)
			minS = std::min(minS, o.start);
		dt = std::max(dt, -minS);
	}
	const int row = rowAt(p.y());
	const bool rowChange =
	    drag == Move && orig.size() == 1 && row >= 0 && row < int(app->song.tracks.size()) && row != dragTrack;
	if (dt == lastDt && !rowChange) return;
	if (!began) {
		app->begin();
		began = true;
	}
	for (const Orig& o : orig) {
		Clip* c = app->song.clip(o.id);
		if (!c) continue;
		if (drag == Move)
			c->start = o.start + dt;
		else
			c->len = std::max(snap, o.len + dt);
	}
	if (rowChange) { // carry a single clip to another track of the same kind
		Track& from = app->song.tracks[size_t(dragTrack)];
		Track& to = app->song.tracks[size_t(row)];
		if (from.isDrums() == to.isDrums()) {
			for (size_t i = 0; i < from.clips.size(); i++)
				if (from.clips[i].id == orig[0].id) {
					const Clip carried = from.clips[i];
					from.clips.erase(from.clips.begin() + long(i));
					to.clips.push_back(carried);
					dragTrack = row;
					app->selTrack = to.id;
					break;
				}
		}
	}
	lastDt = dt;
	app->touch();
}

void Timeline::mouseReleaseEvent(QMouseEvent*) {
	if (drag == Loop && !moved)
		app->edit([](Song& s) { s.loopOn = !s.loopOn; }); // a plain click switches looping on or off
	if (drag == Volume) app->paramRelease();
	if (drag == Move && began) emit app->selectionChanged();
	if (drag == Point) QToolTip::hideText();
	drag = None;
	dragTrack = -1;
	pointIdx = -1;
	pointRow = -1;
	viewport()->update();
}

void Timeline::mouseDoubleClickEvent(QMouseEvent* e) {
	if (e->button() != Qt::LeftButton) return;
	const QPoint p = e->pos();
	bool inLane = false;
	const int row = rowAt(p.y(), &inLane), nt = int(app->song.tracks.size());
	if (row < 0 || row >= nt || inLane) return;
	if (p.x() < HDR) {
		if (p.x() < HDR - 118 && p.y() < rowY(row) + 27) renameTrack(app->song.tracks[size_t(row)].id);
		return;
	}
	drag = None;
	if (Clip* c = clipAt(p, nullptr, nullptr)) {
		app->openClip(c->id);
		emit clipActivated();
		return;
	}
	app->createClip(app->song.tracks[size_t(row)].id, std::max(0, int(xt(p.x()))) / BAR * BAR);
	app->focus = App::FocusEditor;
	emit clipActivated();
}

void Timeline::wheelEvent(QWheelEvent* e) {
	const int dy = e->angleDelta().y(), dx = e->angleDelta().x();
	if (e->modifiers() & Qt::ControlModifier) {
		const double tick = xt(int(e->position().x()));
		pxPerBar = std::clamp(pxPerBar * (dy > 0 ? 1.2 : 1 / 1.2), 14.0, 400.0);
		updateScroll();
		horizontalScrollBar()->setValue(int(tick * ppt()) - (int(e->position().x()) - HDR));
		viewport()->update();
		e->accept();
		return;
	}
	if ((e->modifiers() & Qt::ShiftModifier) || verticalScrollBar()->maximum() == 0 || dx != 0) {
		horizontalScrollBar()->setValue(horizontalScrollBar()->value() - (dx != 0 ? dx : dy));
		e->accept();
		return;
	}
	QAbstractScrollArea::wheelEvent(e);
}

// ---------------------------------------------------------------- menus

void Timeline::contextMenuEvent(QContextMenuEvent* e) {
	const QPoint p = e->pos();
	app->focus = App::FocusArrange;
	if (p.x() >= HDR && p.y() >= RULER + LOOPROW && p.y() < TOP) {
		chordMenu(std::max(0, int(xt(p.x()))) / BAR, e->globalPos());
		return;
	}
	bool inLane = false;
	const int row = rowAt(p.y(), &inLane), nt = int(app->song.tracks.size());
	if (row < 0 || row >= nt || inLane) return;
	const int trackId = app->song.tracks[size_t(row)].id;
	if (p.x() < HDR) {
		app->selectTrack(trackId);
		trackMenu(trackId, e->globalPos());
		return;
	}
	if (Clip* c = clipAt(p, nullptr, nullptr)) {
		const int id = c->id;
		if (!app->selClips.contains(id)) app->selectClip(id);
		clipMenu(id, int(std::lround(xt(p.x()) / BEAT)) * BEAT, e->globalPos());
		return;
	}
	app->selectTrack(trackId);
	QMenu menu(this);
	QAction* fresh = menu.addAction(tr("New clip here"));
	QAction* paste = menu.addAction(tr("Paste at the playhead"));
	QAction* got = menu.exec(e->globalPos());
	if (got == fresh) {
		app->createClip(trackId, std::max(0, int(xt(p.x()))) / BAR * BAR);
		emit clipActivated();
	} else if (got == paste)
		app->paste();
}

void Timeline::clipMenu(int clipId, int tick, const QPoint& global) {
	QMenu menu(this);
	QAction* open = menu.addAction(tr("Edit notes"));
	QAction* cut = menu.addAction(tr("Cut in two here"));
	QAction* dup = menu.addAction(tr("Duplicate"));
	QAction* copy = menu.addAction(tr("Copy"));
	QAction* loop = menu.addAction(tr("Loop this part"));
	menu.addSeparator();
	// transitions into and out of the clip; they apply to every selected clip
	const Clip* cur = app->song.clip(clipId);
	const std::pair<int, QString> lens[] = {{BEAT, tr("1 beat long")},
	                                        {2 * BEAT, tr("2 beats long")},
	                                        {BAR, tr("1 bar long")},
	                                        {2 * BAR, tr("2 bars long")},
	                                        {4 * BAR, tr("4 bars long")}};
	auto build = [&](const QString& title, bool out) {
		QMenu* m = menu.addMenu(title);
		const int kind = cur ? (out ? cur->outKind : cur->inKind) : 0,
		          len = cur ? (out ? cur->outLen : cur->inLen) : BAR;
		const std::pair<int, QString> kinds[] = {
		    {0, tr("No transition")},
		    {1, out ? tr("Fade out") : tr("Fade in")},
		    {2, out ? tr("Filter closes") : tr("Filter opens")},
		    {3, out ? tr("Fade out and filter closes") : tr("Fade in and filter opens")}};
		for (auto& [k, name] : kinds) {
			QAction* a = m->addAction(name);
			a->setCheckable(true);
			a->setChecked(k == kind);
			a->setData(QPoint(out ? 1 : 0, k));
		}
		m->addSeparator();
		for (auto& [l, name] : lens) {
			QAction* a = m->addAction(name);
			a->setCheckable(true);
			a->setChecked(l == len);
			a->setData(QPoint(out ? 3 : 2, l));
		}
	};
	build(tr("Transition at the start"), false);
	build(tr("Transition at the end"), true);
	menu.addSeparator();
	QAction* del = menu.addAction(tr("Delete"));
	QAction* got = menu.exec(global);
	if (got && got->data().metaType().id() == QMetaType::QPoint) {
		const QPoint what = got->data().toPoint();
		app->edit([&](Song& s) {
			for (Track& t : s.tracks)
				for (Clip& c : t.clips) {
					if (!app->selClips.contains(c.id) && c.id != clipId) continue;
					switch (what.x()) {
					case 0:
						c.inKind = what.y();
						break;
					case 1:
						c.outKind = what.y();
						break;
					case 2:
						c.inLen = what.y();
						if (!c.inKind) c.inKind = 1;
						break;
					default:
						c.outLen = what.y();
						if (!c.outKind) c.outKind = 1;
						break;
					}
				}
		});
		return;
	}
	if (got == open) {
		app->openClip(clipId);
		emit clipActivated();
	} else if (got == cut)
		app->splitClip(clipId, tick);
	else if (got == dup)
		app->duplicateSelection();
	else if (got == copy)
		app->copy();
	else if (got == loop)
		app->loopSelection();
	else if (got == del)
		app->deleteSelection();
}

void Timeline::chordMenu(int bar, const QPoint& global) {
	const Song& s = app->song;
	QMenu menu(this);
	menu.addSection(tr("Chord in bar %1").arg(bar + 1));
	const int cur = s.chordAt(bar), curAlt = s.chordAltAt(bar), n = s.sc().n;
	QList<QAction*> picks;
	for (int d = 0; d < n; d++) {
		QAction* a = menu.addAction(s.chordName(d));
		a->setCheckable(true);
		a->setChecked(d == cur && curAlt == 0);
		picks << a;
	}
	menu.addSeparator();
	// the "right wrong chord": the next bar's chord a semitone lower, sliding up into it
	const int next = s.chordAt(bar + 1), nextAlt = s.chordAltAt(bar + 1);
	QAction* lean = menu.addAction(tr("Lean into the next chord: %1").arg(s.chordName(next, nextAlt - 1)));
	lean->setCheckable(true);
	lean->setChecked(curAlt == -1 && cur == next);
	lean->setEnabled(nextAlt == 0);
	lean->setToolTip(tr(
	    "Plays the next bar’s chord a semitone lower in this bar, outside the scale, so the music slides up into it"));
	menu.setToolTipsVisible(true);
	menu.addSeparator();
	QMenu* rep = menu.addMenu(tr("Repeat the first bars across the whole song"));
	QList<QAction*> reps;
	for (int k : {1, 2, 4, 8, 16}) {
		QAction* a = rep->addAction(k == 1 ? tr("Bar 1 only (one chord throughout)") : tr("Bars 1 to %1").arg(k));
		a->setData(k);
		reps << a;
	}
	QAction* surprise = menu.addAction(tr("Surprise me with a new progression"));
	QAction* got = menu.exec(global);
	if (!got) return;
	if (got == surprise) {
		const std::vector<int> prog = randomProgression(s, app->rng);
		app->onBar([prog](Song& k) { k.setChords(prog); });
		return;
	}
	if (reps.contains(got)) {
		const int k = got->data().toInt();
		app->onBar([k](Song& sg) {
			std::vector<int> c(static_cast<size_t>(k)), a(static_cast<size_t>(k));
			for (int i = 0; i < k; i++) {
				c[size_t(i)] = sg.chordAt(i);
				a[size_t(i)] = sg.chordAltAt(i);
			}
			sg.chords = c;
			sg.chordAlts = a;
		});
		return;
	}
	if (got == lean) {
		if (lean->isChecked())
			app->onBar([bar, next](Song& sg) { sg.setChord(bar, next, -1); });
		else
			app->onBar([bar, next](Song& sg) { sg.setChord(bar, next, 0); });
		return;
	}
	const int d = int(picks.indexOf(got));
	if (d >= 0) app->onBar([bar, d](Song& sg) { sg.setChord(bar, d); });
}

void Timeline::laneParamMenu(int trackId, const QPoint& global) {
	Track* t = app->song.track(trackId);
	if (!t) return;
	QMenu menu(this);
	menu.addSection(tr("This line controls"));
	auto item = [&](QMenu* m, int param) {
		QAction* a = m->addAction(paramLabel(param) + (t->hasMoves(param) ? QStringLiteral("  •") : QString()));
		a->setData(param);
		a->setCheckable(true);
		a->setChecked(param == t->laneParam);
	};
	item(&menu, AP_VOLUME);
	if (!t->isDrums()) {
		using namespace dsp;
		for (int p : {P_CUTOFF, P_RES, P_ENVAMT, P_ACCENT})
			item(&menu, p);
		QMenu* more = menu.addMenu(tr("More knobs"));
		for (int p = 0; p < P_COUNT; p++)
			if (PARAMS[p].kind != K_CHOICE && p != P_CUTOFF && p != P_RES && p != P_ENVAMT && p != P_ACCENT)
				item(more, p);
	}
	menu.addSeparator();
	QAction* clear = menu.addAction(tr("Erase this line"));
	clear->setEnabled(t->hasMoves(t->laneParam));
	QAction* got = menu.exec(global);
	if (!got) return;
	if (got == clear) {
		app->forgetMoves(trackId, t->laneParam);
		return;
	}
	t->laneParam = got->data().toInt();
	app->touch();
}

void Timeline::laneRepeatMenu(int trackId, const QPoint& global) {
	Track* t = app->song.track(trackId);
	if (!t) return;
	const AutoLane* l = t->lane(t->laneParam);
	const int cur = l ? l->loop : 0;
	QMenu menu(this);
	const std::pair<int, QString> items[] = {{0, tr("Follows the song (one long line)")},
	                                         {BAR / 2, tr("Repeats every ½ bar")},
	                                         {BAR, tr("Repeats every bar")},
	                                         {2 * BAR, tr("Repeats every 2 bars")},
	                                         {4 * BAR, tr("Repeats every 4 bars")},
	                                         {8 * BAR, tr("Repeats every 8 bars")}};
	for (auto& [len, name] : items) {
		QAction* a = menu.addAction(name);
		a->setData(len);
		a->setCheckable(true);
		a->setChecked(len == cur);
	}
	QAction* got = menu.exec(global);
	if (!got) return;
	const int len = got->data().toInt(), param = t->laneParam;
	app->edit([&](Song& s) {
		Track* k = s.track(trackId);
		if (!k) return;
		if (!k->lane(param)) {
			AutoLane fresh;
			fresh.param = param;
			k->lanes.push_back(fresh);
		}
		AutoLane* ln = k->lane(param);
		ln->loop = len;
		if (len > 0)
			std::erase_if(ln->pts,
			              [&](const AutoPoint& a) { return a.tick >= len; }); // keep the part that fits the shape
	});
}

void Timeline::addMenu(const QPoint& global) {
	QMenu menu(this);
	const std::pair<int, QString> items[] = {{R_BASS, tr("Bass line")},
	                                         {R_SEQ, tr("Sequence (repeating pattern)")},
	                                         {R_PAD, tr("Pad (chords)")},
	                                         {R_LEAD, tr("Lead (melody)")},
	                                         {R_DRUMS, tr("Drums (auto drummer)")}};
	for (auto& [role, name] : items)
		menu.addAction(name)->setData(role);
	if (QAction* got = menu.exec(global)) {
		app->addTrack(got->data().toInt());
		emit clipActivated();
	}
}

void Timeline::renameTrack(int trackId) {
	Track* t = app->song.track(trackId);
	if (!t) return;
	bool ok = false;
	const QString name = QInputDialog::getText(this, tr("Rename track"), tr("Name"), QLineEdit::Normal, t->name, &ok);
	if (ok && !name.trimmed().isEmpty())
		app->edit([&](Song& s) {
			if (Track* k = s.track(trackId)) k->name = name.trimmed();
		});
}

void Timeline::trackMenu(int trackId, const QPoint& global) {
	Track* t = app->song.track(trackId);
	if (!t) return;
	QMenu menu(this);
	QAction* rename = menu.addAction(tr("Rename…"));
	QAction* dup = menu.addAction(tr("Duplicate track"));
	QAction* up = menu.addAction(tr("Move up"));
	QAction* down = menu.addAction(tr("Move down"));
	QAction* follow = nullptr;
	if (!t->isDrums()) {
		follow = menu.addAction(tr("Follow the chord lane"));
		follow->setCheckable(true);
		follow->setChecked(t->follow);
	}
	QAction* forget = t->lanes.empty() ? nullptr : menu.addAction(tr("Erase all knob lines of this track"));
	menu.addSeparator();
	QAction* del = menu.addAction(tr("Delete track"));
	QAction* got = menu.exec(global);
	if (!got) return;
	if (got == rename)
		renameTrack(trackId);
	else if (got == del)
		app->removeTrack(trackId);
	else if (got == forget)
		app->forgetMoves(trackId, -1);
	else if (got == follow)
		app->edit([&](Song& s) {
			if (Track* k = s.track(trackId)) k->follow = !k->follow;
		});
	else if (got == dup) {
		if (int(app->song.tracks.size()) >= MAX_TRACKS) {
			emit app->status(tr("The song already has %1 tracks, which is the most it can hold.").arg(MAX_TRACKS));
			return;
		}
		app->edit([&](Song& s) {
			const int i = s.trackIndex(trackId);
			if (i < 0) return;
			Track c = s.tracks[size_t(i)];
			c.id = s.newId();
			c.name += tr(" copy");
			for (Clip& k : c.clips)
				k.id = s.newId();
			s.tracks.insert(s.tracks.begin() + i + 1, c);
		});
	} else if (got == up || got == down) {
		app->edit([&](Song& s) {
			const int i = s.trackIndex(trackId), j = i + (got == up ? -1 : 1);
			if (i >= 0 && j >= 0 && j < int(s.tracks.size())) std::swap(s.tracks[size_t(i)], s.tracks[size_t(j)]);
		});
	}
}
