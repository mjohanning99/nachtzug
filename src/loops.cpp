#include "loops.h"
#include <QWheelEvent>
#include "app.h"
#include "widgets.h"
#include <QContextMenuEvent>
#include <QDateTime>
#include <QHelpEvent>
#include <QMenu>
#include <QPainter>
#include <QScrollBar>
#include <QToolTip>
#include <climits>
#include <cmath>

LoopGrid::LoopGrid(App* a, QWidget* parent) : QAbstractScrollArea(parent), app(a) {
	setFrameShape(QFrame::NoFrame);
	setFocusPolicy(Qt::NoFocus);
	viewport()->setMouseTracking(true);
	setMinimumHeight(150);
	connect(app, &App::changed, this, &LoopGrid::refresh);
	connect(app, &App::selectionChanged, this, &LoopGrid::refresh);
}

int LoopGrid::rowY(int row) const {
	return TOP + row * ROW - verticalScrollBar()->value();
}
int LoopGrid::colX(int scene) const {
	return HDR + scene * CELLW - horizontalScrollBar()->value();
}
QRect LoopGrid::cellRect(int row, int scene) const {
	return QRect(colX(scene) + 4, rowY(row) + 4, CELLW - 8, ROW - 9);
}

bool LoopGrid::hit(const QPoint& p, int* row, int* scene) const {
	if (p.x() < HDR || p.y() < TOP) return false;
	const int nt = int(app->song.tracks.size());
	const int s = (p.x() - HDR + horizontalScrollBar()->value()) / CELLW;
	if (s < 0 || s >= app->song.scenes) return false;
	int r = (p.y() - TOP + verticalScrollBar()->value()) / ROW;
	if (p.y() >= viewport()->height() - FOOT)
		r = nt; // the column buttons stay at the bottom edge
	else if (r >= nt)
		return false;
	*row = r;
	*scene = s;
	return true;
}

void LoopGrid::updateScroll() {
	const int w = app->song.scenes * CELLW + 20, h = int(app->song.tracks.size()) * ROW;
	horizontalScrollBar()->setRange(0, std::max(0, w - (viewport()->width() - HDR)));
	horizontalScrollBar()->setPageStep(viewport()->width() - HDR);
	horizontalScrollBar()->setSingleStep(CELLW);
	verticalScrollBar()->setRange(0, std::max(0, h - (viewport()->height() - TOP - FOOT)));
	verticalScrollBar()->setPageStep(viewport()->height() - TOP - FOOT);
}

// Cells fill the view: taller rows when there are few tracks, wider cells when there is room, within limits
// so a cell never becomes too small to read or so big it looks empty.
void LoopGrid::fitCells() {
	const int nt = std::max(1, int(app->song.tracks.size()));
	ROW = std::clamp((viewport()->height() - TOP - FOOT) / nt, 58, 112);
	CELLW = std::clamp((viewport()->width() - HDR - 16) / std::max(1, app->song.scenes), 90, 190);
}

void LoopGrid::refresh() {
	fitCells();
	updateScroll();
	viewport()->update();
}
void LoopGrid::resizeEvent(QResizeEvent*) {
	fitCells();
	updateScroll();
}
void LoopGrid::leaveEvent(QEvent*) {
	hoverRow = hoverScene = -1;
	viewport()->update();
}

// ---------------------------------------------------------------- painting

// what every part of the loop grid is drawn with: the painter, the size, the song, the fonts, where the music is,
// whether it plays, the blink of what waits for the bar line, and where the rows end
struct LoopGrid::Frame {
	QPainter& p;
	int W;
	int H;
	const Song& s;
	int nt;
	QFont f;
	QFont fs;
	QFont fb;
	double m;
	bool playing;
	bool blink;
	int rowsBottom;
};

void LoopGrid::paintEvent(QPaintEvent*) {
	QPainter p(viewport());
	const int W = viewport()->width(), H = viewport()->height();
	const Song& s = app->song;
	const int nt = int(s.tracks.size());
	p.fillRect(0, 0, W, H, theme::canvas);
	QFont f = font();
	f.setPixelSize(11);
	QFont fs = f;
	fs.setPixelSize(10);
	QFont fb = f;
	fb.setWeight(QFont::DemiBold); // the Grotesk’s heaviest, 600
	fb.setPixelSize(12);
	const double m = app->musicalPos();
	const bool playing = app->isPlaying();
	const bool blink = (QDateTime::currentMSecsSinceEpoch() / 250) % 2 == 0;
	const int rowsBottom = H - FOOT;
	const Frame fr{p, W, H, s, nt, f, fs, fb, m, playing, blink, rowsBottom};
	paintCells(fr);
	paintColumnNumbers(fr);
	paintChords(fr);
	paintColumnKeys(fr);
	paintHeaders(fr);
}

// the cells: each track's patterns, the one playing pressed in, the one waiting blinking
void LoopGrid::paintCells(const Frame& fr) {
	QPainter& p = fr.p;
	const int W = fr.W;
	const Song& s = fr.s;
	const int nt = fr.nt;
	const QFont& f = fr.f;
	const QFont& fs = fr.fs;
	const double m = fr.m;
	const bool playing = fr.playing;
	const bool blink = fr.blink;
	const int rowsBottom = fr.rowsBottom;
	p.setRenderHint(QPainter::Antialiasing, true);
	p.setClipRect(QRect(HDR, TOP, W - HDR, rowsBottom - TOP));
	for (int i = 0; i < nt; i++) {
		const int y = rowY(i);
		if (y + ROW < TOP || y > rowsBottom) continue;
		const Track& t = s.tracks[size_t(i)];
		const QColor col = theme::trackColor(t.role);
		p.fillRect(HDR, y, W - HDR, ROW - 1, t.id == app->selTrack ? theme::laneSel : theme::lane);
		const int queued = app->queuedCell(t.id);
		for (int sc = 0; sc < s.scenes; sc++) {
			const QRect r = cellRect(i, sc);
			if (r.right() < HDR || r.left() > W) continue;
			const Clip* c = t.cell(sc);
			const bool hover = hoverRow == i && hoverScene == sc;
			if (!c) { // empty: an outline, and a plus when the mouse is over it
				p.setPen(QPen(hover ? theme::rule : theme::gridBar, 1, Qt::DashLine));
				p.setBrush(Qt::NoBrush);
				p.drawRoundedRect(r, 5, 5);
				if (hover) {
					p.setPen(theme::dim);
					p.setFont(f);
					p.drawText(r, Qt::AlignCenter, tr("+ new idea"));
				}
				if (queued == -1 && t.liveCell == sc) { /* nothing to mark on an empty cell */
				}
				continue;
			}
			const bool live = t.liveCell == sc, waiting = queued == sc, stopping = live && queued != -2 && queued != sc;
			const QColor fill = theme::blockFill(col, hover, live);
			theme::drawBlock(p, r, fill, 5, !live); // a playing cell sits pressed in
			if (live) {
				p.setPen(QPen(theme::ink, 2));
				p.setBrush(Qt::NoBrush);
				p.drawRoundedRect(QRectF(r).adjusted(1, 1, -1, -1), 5, 5);
			}
			if (c->id == app->editClip) { // the cell shown in the Notes tab: a ring just outside it
				p.setPen(QPen(theme::amber, 2));
				p.setBrush(Qt::NoBrush);
				p.drawRoundedRect(r.adjusted(-3, -3, 3, 3), 7, 7);
			}
			if ((waiting || stopping) && blink) { // waiting for the bar line
				p.setPen(QPen(waiting ? theme::ink : theme::lcd, 2, Qt::DashLine));
				p.setBrush(Qt::NoBrush);
				p.drawRoundedRect(r.adjusted(2, 2, -2, -2), 4, 4);
			}
			// the notes, small
			p.setClipRect(r.adjusted(3, 3, -3, -3).intersected(QRect(HDR, TOP, W - HDR, rowsBottom - TOP)));
			if (!c->notes.empty() && c->loopLen > 0) {
				int lo = INT_MAX, hi = INT_MIN;
				for (const Note& n : c->notes) {
					lo = std::min(lo, n.deg);
					hi = std::max(hi, n.deg);
				}
				const double span = std::max(1, hi - lo), sx = double(r.width() - 8) / c->loopLen;
				p.setPen(Qt::NoPen);
				p.setBrush(theme::clipInk);
				for (const Note& n : c->notes) {
					if (n.start >= c->loopLen) continue;
					p.drawRect(QRectF(r.left() + 4 + n.start * sx,
					                  r.bottom() - 9 - (n.deg - lo) / span * (r.height() - 26),
					                  std::max(1.5, (t.isDrums() ? 10 : n.len) * sx), 2));
				}
			}
			p.setPen(theme::clipInk);
			p.setFont(fs);
			const int bars = c->loopLen / BAR;
			const QString len = c->loopLen % BAR != 0 ? tr("½ bar") : bars == 1 ? tr("1 bar") : tr("%1 bars").arg(bars);
			const QString label = p.fontMetrics().elidedText(
			    c->ad.on ? tr("Drummer · %1").arg(ui(DRUM_STYLES[std::clamp(c->ad.style, 0, NUM_DRUM_STYLES - 1)]))
				: c->name.isEmpty() ? len
				                    : ui(c->name) + " · " + len,
			    Qt::ElideRight, r.width() - 10);
			p.drawText(r.adjusted(6, 3, -4, 0), Qt::AlignLeft | Qt::AlignTop, label);
			if (live && playing && c->loopLen > 0) { // how far through the pattern it is
				const double frac = std::fmod(m, double(c->loopLen)) / c->loopLen;
				p.setPen(Qt::NoPen);
				p.setBrush(theme::ink);
				p.drawRect(QRectF(r.left() + 3, r.bottom() - 4, (r.width() - 6) * frac, 2.5));
			}
			p.setClipRect(QRect(HDR, TOP, W - HDR, rowsBottom - TOP));
		}
	}
	p.setClipping(false);
	p.setRenderHint(QPainter::Antialiasing, false);
}

// the column numbers along the top
void LoopGrid::paintColumnNumbers(const Frame& fr) {
	QPainter& p = fr.p;
	const int W = fr.W;
	const Song& s = fr.s;
	const QFont& f = fr.f;
	// column numbers
	p.fillRect(HDR, 0, W - HDR, COLH, theme::well);
	p.setFont(f);
	for (int sc = 0; sc < s.scenes; sc++) {
		const int x = colX(sc);
		if (x + CELLW < HDR || x > W) continue;
		p.setPen(theme::dim);
		p.drawText(QRect(x, 0, CELLW, COLH), Qt::AlignCenter, tr("Column %1").arg(sc + 1));
	}
}

// the chords: the progression, one box per bar, repeating for as long as the loops play
void LoopGrid::paintChords(const Frame& fr) {
	QPainter& p = fr.p;
	const int W = fr.W;
	const Song& s = fr.s;
	const QFont& f = fr.f;
	const QFont& fb = fr.fb;
	const double m = fr.m;
	const bool playing = fr.playing;
	// chords: the progression, one box per bar, repeating for as long as the loops play
	p.fillRect(HDR, COLH, W - HDR, CHORDH, theme::wellHi);
	{
		const int n = int(s.chords.size()), now = n ? ((int(m) / BAR) % n + n) % n : 0;
		p.setClipRect(QRect(HDR, COLH, W - HDR, CHORDH));
		p.setRenderHint(QPainter::Antialiasing, true);
		for (int i = 0; i <= n; i++) {
			const QRect r(HDR + i * CHORDW - horizontalScrollBar()->value() + 3, COLH + 3, CHORDW - 6, CHORDH - 6);
			if (r.right() < HDR || r.left() > W) continue;
			if (i == n) { // one more box: make the progression a bar longer
				p.setPen(QPen(theme::gridBar, 1, Qt::DashLine));
				p.setBrush(Qt::NoBrush);
				p.drawRoundedRect(r, 3, 3);
				p.setPen(theme::dim);
				p.setFont(fb);
				p.drawText(r, Qt::AlignCenter, QStringLiteral("+"));
				break;
			}
			const bool cur = playing && i == now;
			p.setPen(cur ? QPen(Qt::NoPen) : QPen(theme::rule, 1));
			p.setBrush(cur ? theme::amber : theme::panel);
			p.drawRoundedRect(r, 3, 3);
			p.setPen(cur ? theme::accentInk : theme::ink);
			p.setFont(f);
			p.drawText(r, Qt::AlignCenter, s.chordName(s.chords[size_t(i)], s.chordAltAt(i)));
		}
		p.setRenderHint(QPainter::Antialiasing, false);
		p.setClipping(false);
	}
}

// the keys along the bottom that start a whole column
void LoopGrid::paintColumnKeys(const Frame& fr) {
	QPainter& p = fr.p;
	const int W = fr.W;
	const Song& s = fr.s;
	const int nt = fr.nt;
	const QFont& fb = fr.fb;
	const int rowsBottom = fr.rowsBottom;
	// column buttons along the bottom
	p.fillRect(HDR, rowsBottom, W - HDR, FOOT, theme::well);
	p.setRenderHint(QPainter::Antialiasing, true);
	for (int sc = 0; sc < s.scenes; sc++) {
		const int x = colX(sc);
		if (x + CELLW < HDR || x > W) continue;
		const QRect r(x + 4, rowsBottom + 6, CELLW - 8, FOOT - 12);
		bool any = false, allLive = true;
		for (const Track& t : s.tracks)
			if (t.cell(sc)) {
				any = true;
				allLive &= t.liveCell == sc;
			}
		const bool hover = hoverRow == nt && hoverScene == sc, on = any && allLive;
		p.setFont(fb);
		if (theme::tactile)
			theme::drawKey(p, r, tr("▶  %1").arg(sc + 1), on, theme::amber);
		else {
			p.setPen(on ? QPen(Qt::NoPen) : QPen(theme::rule, 1));
			p.setBrush(on ? theme::amber : hover ? theme::hover : theme::wellHi);
			p.drawRoundedRect(r, 4, 4);
			p.setPen(on ? theme::accentInk : any ? theme::ink : theme::dim);
			p.drawText(r, Qt::AlignCenter, tr("▶  %1").arg(sc + 1));
		}
	}
}

// the header column: each track's name, keys and volume, and the focus line
void LoopGrid::paintHeaders(const Frame& fr) {
	QPainter& p = fr.p;
	const int W = fr.W;
	const int H = fr.H;
	const Song& s = fr.s;
	const int nt = fr.nt;
	const QFont& f = fr.f;
	const QFont& fs = fr.fs;
	const QFont& fb = fr.fb;
	const int rowsBottom = fr.rowsBottom;
	// header column
	p.fillRect(0, 0, HDR, H, theme::panel);
	p.setPen(theme::dim);
	p.setFont(fs);
	p.drawText(QRect(12, 0, HDR - 16, COLH), Qt::AlignVCenter | Qt::AlignLeft, tr("CELLS"));
	p.drawText(QRect(12, COLH, HDR - 16, CHORDH), Qt::AlignVCenter | Qt::AlignLeft, tr("CHORDS"));
	p.drawText(QRect(12, rowsBottom, HDR - 16, FOOT), Qt::AlignVCenter | Qt::AlignLeft, tr("COLUMNS"));
	p.setClipRect(QRect(0, TOP, HDR, rowsBottom - TOP));
	auto keyFace = [&](const QRect& r, const QString& text, bool on, const QColor& onCol, const QString& symbol = {}) {
		theme::drawKey(p, r, text, on, onCol, symbol);
	};

	for (int i = 0; i < nt; i++) {
		const int y = rowY(i);
		if (y + ROW < TOP || y > rowsBottom) continue;
		const Track& t = s.tracks[size_t(i)];
		const QColor col = theme::trackColor(t.role);
		p.fillRect(0, y, HDR, ROW - 1, t.id == app->selTrack ? theme::wellHi : theme::well);
		p.fillRect(0, y, 5, ROW - 1, col);
		p.setFont(fb);
		p.setPen(theme::ink);
		theme::drawRoleIcon(p, QRectF(12, y + 7, 15, 15), t.role, theme::ink);
		p.drawText(QRect(33, y + 5, HDR - 153, 20), Qt::AlignVCenter | Qt::AlignLeft,
		           p.fontMetrics().elidedText(ui(t.name), Qt::ElideRight, HDR - 153));
		p.setFont(f);
		keyFace(QRect(HDR - 114, y + 6, 24, 18), QString(), t.mute, QColor(0xc4, 0x36, 0x1c), QStringLiteral("mute"));
		keyFace(QRect(HDR - 86, y + 6, 24, 18), QString(), t.solo, theme::amber, QStringLiteral("solo"));
		keyFace(QRect(HDR - 58, y + 6, 48, 18), QString(), false, theme::lcd, QStringLiteral("stop"));
		const QRect vr(12, y + 32, HDR - 24, 8);
		theme::drawSlider(p, QRectF(vr).adjusted(4, 1, -4, -1), app->paramValue(t, AP_VOLUME), col);
		const float pk =
		    std::clamp(app->engine->trackPeak[std::min(i, MAX_TRACKS - 1)].load(std::memory_order_relaxed), 0.f, 1.f);
		p.fillRect(QRect(12, y + 45, int((HDR - 24) * std::sqrt(pk)), 3), theme::ink);
	}
	p.setClipping(false);
	if (app->focus == App::FocusArrange) p.fillRect(0, 0, W, 2, theme::amber);
}

// ---------------------------------------------------------------- help

QString LoopGrid::helpAt(const QPoint& p) const {
	const int nt = int(app->song.tracks.size());
	if (p.x() < HDR && p.y() < COLH)
		return tr(
		    "The cells: each holds a pattern that repeats for as long as it is switched on. Click a cell to start "
		    "it on the next bar and show its notes in the Notes tab; click it again to stop it.");
	if (p.x() < HDR && p.y() >= viewport()->height() - FOOT)
		return tr("The keys beside this start a whole column of cells at once, on the next bar.");
	if (p.y() >= COLH && p.y() < TOP)
		return tr("The chords, one box per bar. They repeat in this order while the loops play, and tracks set to "
		          "“Follow chords” shift their notes to fit. Click a box to change that chord, or + to add a bar.");
	if (p.x() < HDR) {
		const int row = p.y() < TOP || p.y() >= viewport()->height() - FOOT
		                    ? -1
		                    : (p.y() - TOP + verticalScrollBar()->value()) / ROW;
		if (row < 0 || row >= nt) return QString();
		const int y = rowY(row);
		if (QRect(HDR - 114, y + 6, 24, 18).contains(p))
			return tr("Mute: silence this track. It switches on the next bar.");
		if (QRect(HDR - 86, y + 6, 24, 18).contains(p)) return tr("Solo: hear only this track.");
		if (QRect(HDR - 58, y + 6, 48, 18).contains(p)) return tr("Stop this track’s cell on the next bar.");
		if (p.y() >= y + 27 && p.y() <= y + 44) return tr("Track volume. Drag left or right.");
		return tr("Click to select this track: the letter keys then play it, and the tabs above show it.");
	}
	int row = 0, scene = 0;
	if (!hit(p, &row, &scene)) return QString();
	if (row == nt)
		return tr(
		    "Start every cell in this column together, on the next bar. Tracks with no cell in this column stop.");
	const Track& t = app->song.tracks[size_t(row)];
	if (!t.cell(scene)) return tr("Click to put a new idea here and start it.");
	return t.liveCell == scene
	           ? tr("This cell is playing. Click to stop it on the next bar. Shift + click shows its notes without "
			        "stopping it; double-click opens them.")
			   : tr("Click to start this cell on the next bar; it replaces what this track is playing, and the Notes "
			        "tab shows it. Shift + click picks it without starting it. Right-click for more.");
}

bool LoopGrid::viewportEvent(QEvent* e) {
	if (e->type() == QEvent::ToolTip) {
		auto* h = static_cast<QHelpEvent*>(e);
		const QString text = helpAt(h->pos());
		if (text.isEmpty())
			QToolTip::hideText();
		else
			QToolTip::showText(h->globalPos(), text, viewport());
		return true;
	}
	return QAbstractScrollArea::viewportEvent(e);
}

// Shift + wheel scrolls sideways through the columns, as in the timeline (there may be no scroll bar to drag)
void LoopGrid::wheelEvent(QWheelEvent* e) {
	if ((e->modifiers() & Qt::ShiftModifier) && e->angleDelta().x() == 0) {
		horizontalScrollBar()->setValue(horizontalScrollBar()->value() - e->angleDelta().y());
		e->accept();
		return;
	}
	QAbstractScrollArea::wheelEvent(e);
}

// ---------------------------------------------------------------- mouse

void LoopGrid::setVolumeFromX(int x) {
	if (volTrack >= 0) app->paramMove(volTrack, AP_VOLUME, std::clamp(float(x - 16) / float(HDR - 32), 0.f, 1.f));
}

void LoopGrid::mousePressEvent(QMouseEvent* e) {
	if (e->button() != Qt::LeftButton) return;
	app->focus = App::FocusArrange;
	const QPoint p = e->pos();
	const int nt = int(app->song.tracks.size());
	if (p.x() >= HDR && p.y() >= COLH && p.y() < TOP) { // the chord boxes
		const int i = (p.x() - HDR + horizontalScrollBar()->value()) / CHORDW, n = int(app->song.chords.size());
		if (i == n && n < 64)
			app->onBar([](Song& s) { s.chords.push_back(s.chords.empty() ? 0 : s.chords.back()); });
		else if (i >= 0 && i < n)
			chordMenu(i, e->globalPosition().toPoint());
		return;
	}
	if (p.x() < HDR) {
		if (p.y() < TOP || p.y() >= viewport()->height() - FOOT) return;
		const int row = (p.y() - TOP + verticalScrollBar()->value()) / ROW;
		if (row < 0 || row >= nt) return;
		const int y = rowY(row), id = app->song.tracks[size_t(row)].id;
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
		if (QRect(HDR - 58, y + 6, 48, 18).contains(p)) {
			app->launch(id, -1);
			return;
		}
		app->selectTrack(id);
		if (p.y() >= y + 27 && p.y() <= y + 44) {
			volTrack = id;
			app->paramPress(id, AP_VOLUME);
			setVolumeFromX(p.x());
		}
		return;
	}
	int row = 0, scene = 0;
	if (!hit(p, &row, &scene)) return;
	if (row == nt) {
		app->launchScene(scene);
		return;
	}
	const Track& t = app->song.tracks[size_t(row)];
	const int id = t.id;
	if (const Clip* c = t.cell(scene)) {
		const int cellId = c->id;
		const bool live = t.liveCell == scene && app->queuedCell(id) == -2;
		app->selectClip(cellId); // the Notes tab now shows this cell
		// a click starts a cell, or stops it if it is the one playing; Shift + click only picks it for editing
		if (!(e->modifiers() & Qt::ShiftModifier)) app->launch(id, live ? -1 : scene);
	} else if (app->newCell(id, scene) >= 0)
		app->launch(id, scene);
}

void LoopGrid::mouseMoveEvent(QMouseEvent* e) {
	if (volTrack >= 0) {
		setVolumeFromX(e->pos().x());
		return;
	}
	int row = -1, scene = -1;
	if (!hit(e->pos(), &row, &scene)) row = scene = -1;
	if (row != hoverRow || scene != hoverScene) {
		hoverRow = row;
		hoverScene = scene;
		viewport()->update();
	}
}

void LoopGrid::mouseReleaseEvent(QMouseEvent*) {
	if (volTrack >= 0) {
		app->paramRelease();
		volTrack = -1;
	}
}

void LoopGrid::mouseDoubleClickEvent(QMouseEvent* e) {
	int row = 0, scene = 0;
	if (e->button() != Qt::LeftButton || !hit(e->pos(), &row, &scene) || row >= int(app->song.tracks.size())) return;
	if (const Clip* c = app->song.tracks[size_t(row)].cell(scene)) {
		app->openClip(c->id);
		emit clipActivated();
	}
}

void LoopGrid::contextMenuEvent(QContextMenuEvent* e) {
	int row = 0, scene = 0;
	if (!hit(e->pos(), &row, &scene) || row >= int(app->song.tracks.size())) return;
	app->focus = App::FocusArrange;
	cellMenu(row, scene, e->globalPos());
}

void LoopGrid::chordMenu(int index, const QPoint& global) {
	const Song& s = app->song;
	const int n = int(s.chords.size());
	if (index < 0 || index >= n) return;
	QMenu menu(this);
	menu.addSection(tr("Chord in bar %1 of %2").arg(index + 1).arg(n));
	QList<QAction*> picks;
	for (int d = 0; d < s.sc().n; d++) {
		QAction* a = menu.addAction(s.chordName(d));
		a->setCheckable(true);
		a->setChecked(d == s.chords[size_t(index)]);
		picks << a;
	}
	menu.addSeparator();
	QAction* remove = n > 1 ? menu.addAction(tr("Remove this bar from the progression")) : nullptr;
	QAction* surprise = menu.addAction(tr("Surprise me with a new progression"));
	QAction* got = menu.exec(global);
	if (!got) return;
	if (got == surprise) {
		const std::vector<int> prog = randomProgression(s, app->rng);
		app->onBar([prog](Song& k) { k.setChords(prog); });
		return;
	}
	if (got == remove) {
		app->onBar([index](Song& k) {
			if (index < int(k.chords.size()) && k.chords.size() > 1) {
				k.chords.erase(k.chords.begin() + index);
				if (index < int(k.chordAlts.size())) k.chordAlts.erase(k.chordAlts.begin() + index);
			}
		});
		return;
	}
	const int d = int(picks.indexOf(got));
	if (d >= 0)
		app->onBar([index, d](Song& k) {
			if (index < int(k.chords.size())) {
				k.chords[size_t(index)] = d;
				if (index < int(k.chordAlts.size())) k.chordAlts[size_t(index)] = 0;
			}
		});
}

void LoopGrid::cellMenu(int row, int scene, const QPoint& global) {
	Track& t = app->song.tracks[size_t(row)];
	const int trackId = t.id;
	const Clip* c = t.cell(scene);
	QMenu menu(this);
	if (!c) {
		QAction* fresh = menu.addAction(tr("New idea here"));
		if (menu.exec(global) == fresh) app->newCell(trackId, scene);
		return;
	}
	const int cellId = c->id;
	app->selectClip(cellId);
	QAction* open = menu.addAction(tr("Edit notes"));
	QAction* next =
	    scene + 1 < app->song.scenes && !t.cell(scene + 1) ? menu.addAction(tr("Copy into the next column")) : nullptr;
	QAction* song = menu.addAction(tr("Copy into the song timeline at the playhead"));
	menu.addSeparator();
	QAction* del = menu.addAction(tr("Delete"));
	QAction* got = menu.exec(global);
	if (!got) return;
	if (got == open) {
		app->openClip(cellId);
		emit clipActivated();
	} else if (got == song)
		app->cellToTimeline(cellId);
	else if (got == del)
		app->deleteCell(cellId);
	else if (got == next)
		app->edit([&](Song& s) {
			Track* k = s.track(trackId);
			const Clip* src = k ? k->cell(scene) : nullptr;
			if (!src) return;
			Clip copy = *src;
			copy.id = s.newId();
			copy.scene = scene + 1;
			copy.adKey = 0;
			if (copy.ad.on)
				copy.ad.seed = unsigned(app->rng()); // a different take, so the next column is not identical
			k->cells.push_back(copy);
		});
}
