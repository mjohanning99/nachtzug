// Song timeline: track headers on the left; a bar ruler, the loop row and the chord lane on top; clips you can
// move, stretch, cut, copy and paste; and under each track an optional lane where a knob's line is drawn by hand.
#pragma once
#include "model.h"
#include <QAbstractScrollArea>

class App;

class Timeline : public QAbstractScrollArea {
    Q_OBJECT
public:
    explicit Timeline(App* app, QWidget* parent = nullptr);
    void setScissors(bool on);
    void followPlayhead();
    void refresh();
    void zoom(double factor);
    void showWholeSong();   // zoom out so every clip fits, from bar 1
    void addMenu(const QPoint& global);
signals:
    void clipActivated();     // a clip was opened: show the note editor
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
    static constexpr int HDR = 214, RULER = 20, LOOPROW = 16, CHORDS = 22, TOP = RULER + LOOPROW + CHORDS, ROW = 58, LANE = 72;
    double ppt() const { return pxPerBar / BAR; }
    int tx(double tick) const;
    double xt(int x) const;
    int rowH(int row) const;
    int rowY(int row) const;
    int rowAt(int y, bool* inLane = nullptr) const;   // track index under y, or -1; tracks.size() for the "add track" row
    QRect clipRect(const Clip& c, int row) const;
    QRect laneRect(int row) const;
    static double laneY(const QRect& r, float v) { return r.top() + 6 + double(1.f - v) * (r.height() - 12); }
    static float laneV(const QRect& r, int y) { return std::clamp(1.f - float(y - r.top() - 6) / float(r.height() - 12), 0.f, 1.f); }
    int lanePointAt(int row, const QPoint& p, int* base) const;
    Clip* clipAt(const QPoint& p, Track** owner, bool* edge);
    QString helpAt(const QPoint& p);
    void updateScroll();
    void chordMenu(int bar, const QPoint& global);
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
    enum Drag { None, Seek, Loop, Move, Resize, Volume, Point } drag = None;
    struct Orig { int id, start, len; };
    std::vector<Orig> orig;
    double pressTick = 0;
    QPoint pressPos;
    int dragTrack = -1, lastDt = 0, pointIdx = -1, pointBase = 0, pointRow = -1;
    bool began = false, moved = false;
};
