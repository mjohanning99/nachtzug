// Note editor: a scale-locked piano roll for synth clips, a hit grid for drum clips,
// and the toolbar around it (new ideas, auto drummer, accents, transposing).
#pragma once
class QLayout;
class QVBoxLayout;
class QGroupBox;
#include "model.h"
#include <QAbstractScrollArea>
#include <QWidget>
#include <functional>

class App;
class QComboBox;
class QLabel;
class QSlider;
class QToolButton;
class QPushButton;
class Switch;

class NoteEditor : public QAbstractScrollArea {
	Q_OBJECT
public:
	explicit NoteEditor(App* app, QWidget* parent = nullptr);
	void clipChanged(bool recentre);
	bool notesInView() const; // every note of the clip is on a row the grid shows (the GUI test asks)
	void repaintMoving();     // about 30 times a second while something moves: the playhead

protected:
	void paintEvent(QPaintEvent*) override;
	void mousePressEvent(QMouseEvent*) override;
	void mouseMoveEvent(QMouseEvent*) override;
	void mouseReleaseEvent(QMouseEvent*) override;
	void wheelEvent(QWheelEvent*) override;
	void resizeEvent(QResizeEvent*) override;
	void showEvent(QShowEvent*) override;
	bool viewportEvent(QEvent*) override;

private:
	static constexpr int KEYW = 76, RUL = 20;
	int playheadX() const;  // where the playhead is drawn, or -1 when this clip is not playing
	int shownPx = -1;       // where repaintMoving() last saw it
	unsigned shownSong = 0; // and how often the song had been published then
	QString helpAt(const QPoint& p) const;
	bool drums() const;
	int rows() const { return drums() ? dsp::D_COUNT : DEG_MAX - DEG_MIN + 1; }
	int rowH() const;
	void fitWidth();
	int rowOf(int deg) const { return drums() ? dsp::D_COUNT - 1 - deg : DEG_MAX - deg; }
	int degAt(int y) const;
	int tx(double tick) const;
	double xt(int x) const;
	QRect noteRect(const Note& n) const;
	int hitNote(const QPoint& p, bool* edge) const;
	void updateScroll();
	void eraseAt(const QPoint& p);
	// right-click on an empty spot: a bar of arpeggio or chord from that note (a bar of hits on a drum row)
	void stampMenu(const QPoint& p, const QPoint& global);

	App* app;
	double ppt = 0.6; // pixels per tick
	enum Drag { None, Draw, Move, Resize, Band, Erase } drag = None;
	std::vector<Note> orig;
	double pressTick = 0;
	int pressDeg = 0, drawIdx = -1, lastLen = 24, lastDt = 0, lastDd = 0, shownClip = -1;
	bool began = false, autoFit = true; // autoFit: the pattern is zoomed to fill the width until you zoom by hand
	// centrePending: the notes are to be brought into view, but the grid has no height yet to scroll in (when the
	// window opens, or a tab is shown for the first time); the next resize does it
	bool centrePending = false;
	bool keepCentred = false; // centred, and not scrolled by hand since: a resize centres the notes again
	void centreOnNotes();
	QPoint bandStart, rightPress;
	QRect band;
};

// Three rows of pads, laid out like the letter keys that play them: shows which key plays which note,
// lights up while a key is held, and can be played with the mouse.
class KeyPads : public QWidget {
	Q_OBJECT
public:
	explicit KeyPads(App* app, QWidget* parent = nullptr);
	QSize sizeHint() const override { return {600, 3 * 26 + 2 * 4 + 8}; }

protected:
	void paintEvent(QPaintEvent*) override;
	void mousePressEvent(QMouseEvent*) override;
	void mouseMoveEvent(QMouseEvent*) override;
	void mouseReleaseEvent(QMouseEvent*) override;

private:
	QRectF padRect(int row, int col) const;
	bool drumsOnly() const; // a drum track uses only the bottom row of keys
	bool padAt(const QPointF& p, int* row, int* col) const;
	App* app;
	int heldCode = -1; // the pad the mouse is holding down
};

class EditorPanel : public QWidget {
	Q_OBJECT
public:
	explicit EditorPanel(App* app, QWidget* parent = nullptr);
	void repaintView();       // moves the playhead line
	void repaintPads();       // Shift went down or up: the pads show the bent notes
	void setCompact(bool on); // stacked tool groups, for the narrow editor beside the loop grid
	// the auto drummer's settings: into the inspector (host), or back beside the note grid (nullptr)
	void placeDrummer(QLayout* host);
	QGroupBox* drummerSection() const { return drummerBox; }
public slots:
	void sync();

protected:
	void resizeEvent(QResizeEvent* e) override;

private:
	void rebuildIdeas();
	QWidget* toolBox = nullptr; // the tool groups, wrapping onto more rows when narrow
	void transpose(int steps);
	void flag(bool slide);
	// chance (percent), repeats or keep on the selected notes; false and a hint if none are selected
	bool onSelected(const QString& what, const std::function<void(Note&)>& f);
	void toggleKeep();
	void cleanUp();
	App* app;
	NoteEditor* view;
	QToolButton* ideas;
	QSlider* busy;
	QPushButton *vary, *accent, *slide, *up, *down, *octUp, *octDown, *clear, *keep, *tidy;
	QToolButton *chance, *repeat, *lean;
	QWidget *stepBox, *octBox;
	QComboBox *length, *gridBox;
	QWidget *drummerRow, *notesGroup;
	KeyPads* pads;
	QWidget* padsBox; // the pads with the Notes | Chords switch above them
	QPushButton *keysNotes, *keysChords;
	QLabel* keysHint;

public:
	void setPadsMode(int mode); // 0 automatic, 1 shown, 2 hidden
	int padsMode() const { return padsMode_; }
signals:
	void padsShown(bool shown);

private:
	void updatePads();
	QPushButton* padsToggle = nullptr;
	int padsMode_ = 0;
	bool padsShownLast_ = true;

public:
private:
	QComboBox *style, *fills, *backbeat, *hatsBox, *shape;
	QSlider *intensity, *complexity, *shuffle;
	Switch *crash, *autoBox;
	QWidget* drummerBody;
	QGroupBox* drummerBox = nullptr;
	QVBoxLayout* drummerHome = nullptr;
	bool drummerAway = false;

	int ideasRole = -1;
};
