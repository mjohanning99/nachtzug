// The equaliser's picture on the Sound tab: the curve its knobs add up to, with a point for each band to drag.
#pragma once
#include <QWidget>

class App;

class EqCurve : public QWidget {
	Q_OBJECT
public:
	explicit EqCurve(App* app, QWidget* parent = nullptr);
	void refresh(); // repaints only if the selected track's equaliser looks different now

protected:
	void paintEvent(QPaintEvent*) override;
	void mousePressEvent(QMouseEvent* e) override;
	void mouseMoveEvent(QMouseEvent* e) override;
	void mouseReleaseEvent(QMouseEvent* e) override;
	void mouseDoubleClickEvent(QMouseEvent* e) override;
	void keyPressEvent(QKeyEvent* e) override;
	void focusInEvent(QFocusEvent* e) override;
	void focusOutEvent(QFocusEvent* e) override;
	void leaveEvent(QEvent* e) override;
	bool focusNextPrevChild(bool next) override;

private:
	bool values(float v[5]) const; // the selected track's five settings (as the knobs show them); false: no track
	void set(int index, float x);  // one of them, by hand
	QRectF area() const;           // where the curve is drawn
	double xOf(double hz) const;
	double hzAt(double x) const;
	double yOf(double db) const;
	double dbAt(double y) const;
	QPointF pointPos(int point, const float v[5]) const; // 0 low cut, 1 low, 2 mid, 3 high
	int pointAt(const QPointF& pos) const;
	void dragTo(const QPointF& pos);
	void resetPoint(int point);
	QString pointText(int point, const float v[5]) const; // the point's name and value, for the tag and the status
	App* app;
	int hover = -1, dragging = -1, chosen = 2; // chosen: the point the keys move
	bool keyRun = false;                       // the keys have started an undo step that is still going on
	float shown[5] = {};                       // what was painted last, so a refresh repaints only a change
	int shownTrack = -2;
};
