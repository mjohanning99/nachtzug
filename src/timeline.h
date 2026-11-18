// Song timeline: track headers on the left; the named parts, a bar ruler, the loop row and the chord lane on top; clips
// you can move, stretch, cut, copy and paste; and under each track an optional lane where a knob's line is drawn by
// hand.
#pragma once
#include "model.h"
#include <QAbstractScrollArea>
#include <QImage>

class App;

class Timeline : public QAbstractScrollArea {
	Q_OBJECT
public:
	explicit Timeline(App* app, QWidget* parent = nullptr);
	void setScissors(bool on);
	void followPlayhead();
	void refresh();
	void repaintMoving(); // about 30 times a second while something moves: the playhead and the header column
	// the header column's still parts (names, keys, the rows) are kept as a picture between repaints, so while the
	// song plays only the sliders and meters on top are drawn again; the GUI test compares it with drawing afresh
	static inline bool keepHeader = true;
	void zoom(double factor);
	void showWholeSong(); // zoom out so every clip fits, from bar 1
	void addMenu(const QPoint& global);
	void renamePart(int bar);    // asks for a new name for the part starting at this bar
	void addPartAt(int bar);     // asks for a name and adds a part from this bar on
	QRect partRect(int i) const; // where the named part i is shown in the parts lane (for the tests and lessons)
	static int heightForRows(int rows) { return TOP + rows * ROW; } // the rows on top and this many tracks
signals:
	void clipActivated(); // a clip was opened: show the note editor
protected:
	void paintEvent(QPaintEvent*) override;
	void mousePressEvent(QMouseEvent*) override;
	void mouseMoveEvent(QMouseEvent*) override;
	void mouseReleaseEvent(QMouseEvent*) override;
	void mouseDoubleClickEvent(QMouseEvent*) override;
	void contextMenuEvent(QContextMenuEvent*) override;
	void wheelEvent(QWheelEvent*) override;
	void resizeEvent(QResizeEvent*) override;
	bool viewportEvent(QEvent*) override;

private:
	struct Frame;
	int shownPx = -100;              // where repaintMoving() last saw the playhead
	unsigned shownSong = 0;          // and how often the song had been published then
	void paintGrid(const Frame& fr); // the track rows, the looped stretch a little lighter, and the bar lines
	void paintClips(
	    const Frame& fr); // each track's clips: their colour, notes, name, repeats and transitions, and its knob line
	void paintPlayhead(const Frame& fr);  // the playhead across the tracks
	void paintRuler(const Frame& fr);     // the bar numbers along the top, and the playhead's mark in them
	void paintLoopRow(const Frame& fr);   // the loop row: the stretch that repeats
	void paintChords(const Frame& fr);    // the chord lane: a block for each stretch of the same chord
	void paintParts(const Frame& fr);     // the parts lane: a long key for each named part
	void paintEmptyNote(const Frame& fr); // what an empty song says: how to begin
	void paintHeaders(const Frame& fr);   // the header column: each track's name, keys and volume, and the focus line
	void paintHeaderStill(QPainter& p, const Frame& fr); // its names, keys and rows: everything that does not move
	class QTimer* partJump = nullptr; // a click on a part while playing jumps once it is not a double-click
	QImage headPic;                                      // those, kept (see paintHeaders)
	QString headKey;                                     // and what they were drawn from
	// the rows on top, from the top down: the parts, the bar ruler, the loop row and the chord lane
	static constexpr int HDR = 214, PARTS = 22, RULER = 20, LOOPROW = 16, CHORDS = 22, RULERY = PARTS,
	                     LOOPY = RULERY + RULER, CHORDY = LOOPY + LOOPROW, TOP = CHORDY + CHORDS, ROW = 58, LANE = 72;
	double ppt() const { return pxPerBar / BAR; }
	int tx(double tick) const;
	double xt(int x) const;
	int rowH(int row) const;
	int rowY(int row) const;
	int rowAt(int y, bool* inLane = nullptr) const; // track index under y, or -1; tracks.size() for the "add track" row
	QRect clipRect(const Clip& c, int row) const;
	QRect laneRect(int row) const;
	static double laneY(const QRect& r, float v) { return r.top() + 6 + double(1.f - v) * (r.height() - 12); }
	static float laneV(const QRect& r, int y) {
		return std::clamp(1.f - float(y - r.top() - 6) / float(r.height() - 12), 0.f, 1.f);
	}
	int lanePointAt(int row, const QPoint& p, int* base) const;
	Clip* clipAt(const QPoint& p, Track** owner, bool* edge);
	QString helpAt(const QPoint& p);
	void updateScroll();
	void chordMenu(int bar, const QPoint& global);
	int partUnder(int x) const;  // index of the named part under x, or -1
	int partEdgeAt(int x) const; // index of the part whose left edge is within reach of x, or -1
	void partMenu(int bar, const QPoint& global);
	QString askPartName(const QString& title, const QString& current); // empty when cancelled
	void trackMenu(int trackId, const QPoint& global);
	void clipMenu(int clipId, int tick, const QPoint& global);
	void laneParamMenu(int trackId, const QPoint& global);
	void laneRepeatMenu(int trackId, const QPoint& global);
	void renameTrack(int trackId);
	void setVolumeFromX(int x);
	void dragPoint(QMouseEvent* e);

	App* app;
	double pxPerBar = 64;
	bool scissors = false;
	enum Drag { None, Seek, Loop, Move, Resize, Volume, Point, PartEdge } drag = None;
	struct Orig {
		int id, start, len;
	};
	std::vector<Orig> orig;
	double pressTick = 0;
	QPoint pressPos;
	int dragTrack = -1, lastDt = 0, pointIdx = -1, pointBase = 0, pointRow = -1;
	bool began = false, moved = false;
};
