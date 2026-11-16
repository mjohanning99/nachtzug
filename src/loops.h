// Loop grid: one row per track, one cell per column. A cell holds a pattern that repeats while it is switched on.
// Click a cell to start it on the next bar; the buttons along the bottom start a whole column.
#pragma once
#include "model.h"
#include <QAbstractScrollArea>

class App;

class LoopGrid : public QAbstractScrollArea {
	Q_OBJECT
public:
	explicit LoopGrid(App* app, QWidget* parent = nullptr);
	void refresh();
signals:
	void clipActivated(); // a cell was opened: show the note editor
protected:
	void paintEvent(QPaintEvent*) override;
	void mousePressEvent(QMouseEvent*) override;
	void mouseMoveEvent(QMouseEvent*) override;
	void mouseReleaseEvent(QMouseEvent*) override;
	void mouseDoubleClickEvent(QMouseEvent*) override;
	void contextMenuEvent(QContextMenuEvent*) override;
	void resizeEvent(QResizeEvent*) override;
	void leaveEvent(QEvent*) override;
	void wheelEvent(QWheelEvent*) override;
	bool viewportEvent(QEvent*) override;

private:
	struct Frame;
	void paintCells(
	    const Frame& fr); // the cells: each track's patterns, the one playing pressed in, the one waiting blinking
	void paintColumnNumbers(const Frame& fr); // the column numbers along the top
	void paintChords(
	    const Frame& fr); // the chords: the progression, one box per bar, repeating for as long as the loops play
	void paintColumnKeys(const Frame& fr); // the keys along the bottom that start a whole column
	void paintHeaders(const Frame& fr);    // the header column: each track's name, keys and volume, and the focus line
	static constexpr int HDR = 214, COLH = 22, CHORDH = 26, TOP = COLH + CHORDH, FOOT = 44, CHORDW = 52;
	int ROW = 58, CELLW = 104; // grow with the space available, see fitCells()
	void fitCells();
	int rowY(int row) const;
	int colX(int scene) const;
	QRect cellRect(int row, int scene) const;
	bool hit(const QPoint& p, int* row, int* scene) const; // row == tracks.size() means the column buttons
	QString helpAt(const QPoint& p) const;
	void updateScroll();
	void cellMenu(int row, int scene, const QPoint& global);
	void chordMenu(int index, const QPoint& global);
	void setVolumeFromX(int x);

	App* app;
	int hoverRow = -1, hoverScene = -1, volTrack = -1;
};
