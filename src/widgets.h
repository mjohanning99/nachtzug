// The look and the program's own controls, in one header for every panel to include: the themes' colours and style
// sheet (theme.cpp), drawing the instrument's parts and the icons (drawing.cpp), the screwed wells, folding sections
// and rounded lists (panels.cpp), and Knob, Lcd, VuMeter, Switch, Toast, FlowLayout, Emblem, ElideLabel (controls.cpp).
#pragma once
#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QPalette>
#include <QWidget>
#include <QPainter>
#include <QAbstractButton>
#include <functional>
class QGroupBox;
class QLabel;
#include <QTimer>

#include <QLayout>
#include <QVariantAnimation>

class App;

// Lays its widgets out left to right and starts a new row when the next one does not fit, so a row of tool
// groups wraps onto a second line in a narrow window instead of making the window wider.
class FlowLayout : public QLayout {
public:
	explicit FlowLayout(QWidget* parent = nullptr, int hSpacing = 10, int vSpacing = 8)
	    : QLayout(parent), h_(hSpacing), v_(vSpacing) {
		setContentsMargins(0, 0, 0, 0);
	}
	// justify: every row is stretched to the full width (the extra shared by the items that can grow, in proportion
	// to their width) and its items get the same height, so the sections of a page end on one line
	void setJustify(bool on) { justify_ = on; }
	~FlowLayout() override {
		while (QLayoutItem* it = takeAt(0))
			delete it;
	}
	void addItem(QLayoutItem* item) override { items_.append(item); }
	void insertWidget(int i, QWidget* w) {
		addChildWidget(w);
		items_.insert(std::clamp(i, 0, int(items_.size())), new QWidgetItem(w));
		invalidate();
	}
	int count() const override { return int(items_.size()); }
	QLayoutItem* itemAt(int i) const override { return i >= 0 && i < items_.size() ? items_[i] : nullptr; }
	QLayoutItem* takeAt(int i) override { return i >= 0 && i < items_.size() ? items_.takeAt(i) : nullptr; }
	Qt::Orientations expandingDirections() const override { return {}; }
	bool hasHeightForWidth() const override { return true; }
	int heightForWidth(int w) const override { return place(QRect(0, 0, w, 0), false); }
	void setGeometry(const QRect& r) override {
		QLayout::setGeometry(r);
		place(r, true);
	}
	QSize sizeHint() const override;
	QSize minimumSize() const override;

private:
	int place(const QRect& r, bool apply) const;
	QList<QLayoutItem*> items_;
	int h_, v_;
	bool justify_ = false;
};

// Colours of the current theme. apply() fills them in; everything paints from these.
class QFormLayout;

namespace theme {
inline QColor panel, well, wellHi, rule, ink, dim, canvas, lane, laneSel, grid, gridBar;
inline QColor amber /* main accent */, accentInk /* text on the accent */, ice /* second accent */, cap, capEdge,
    capInk, lcdBg, lcd, key, keyInk, hover;
inline QColor tracks[5];
inline QColor clipInk;       // dark marks drawn on top of the coloured clips and notes, in every theme
inline bool tactile = false; // the instrument look: keys with depth, ribbed knobs, an LCD
int count();
QString name(int index);
int current();
void apply(int index);
QColor trackColor(int role);
inline bool isLight() {
	return canvas.lightness() > 128;
}
inline QColor slot() {
	return isLight() ? gridBar : canvas;
} // the empty part of sliders, faders, meters and knob rings
QString styleSheet();
QPalette palette();
QPixmap logo(int size); // the Nachtzug mark: the tz ligature on an enamel sign
QIcon appIcon();
QIcon ledIcon(const QColor& lit);         // a small indicator light: dark when off, lit when the button is on
QIcon transportIcon(const QString& kind); // "start", "play", "stop", "rec": symbols for the transport keys
// The keys' symbols, drawn on an 18-unit grid with round ends: "copy", "loop", "cut", "scissors", "delete",
// "arrange" (above the timeline), "mute", "solo", "lines", "stop", "idea", "vary", "clean", "clear", "accent",
// "slide", "chance", "repeat", "lean", "keep", "dice", "mutate", "stopall", "column". accent: the red or amber parts.
void drawSymbol(QPainter& p, const QString& kind, const QRectF& box, const QColor& ink, const QColor& accent);
// a symbol as an icon: in the ink (or, dark: the dark keys' ink), in the accent's ink on a lit key, faint when disabled
QIcon symbolIcon(const QString& kind, bool dark = false);
inline QIcon toolIcon(const QString& kind) {
	return symbolIcon(kind);
}
// Keys with a symbol show it with their word, or the symbol alone: "keys" in Settings is "words" (the symbol and the
// word, as chosen for each key) or "symbols" (only the symbol, on every key that has one; the word is in its
// tooltip). symbolKey() marks a key once; dressSymbolKeys() gives every marked key in a window its icon and text
// again, after the look or that setting changed. alone: this key shows its symbol alone either way (M and S).
inline bool symbolsOnly = false;
void loadKeyLook(); // reads "keys" from the settings into symbolsOnly
void symbolKey(QAbstractButton* b, const QString& kind, const QString& word, bool alone = false);
void dressSymbolKeys(QWidget* root);
void loadFonts();
QFont lcdFont(int pixelSize, bool bold = false);
// Drawing helpers for the instrument look; in the flat themes they draw the plain versions.
void drawBlock(QPainter& p, const QRectF& r, const QColor& c, double radius, bool raised = true); // clip, note or cell
// the colour of a clip in the timeline or a cell in the loop grid, from its track's colour: the same in both, lighter
// (or, in the flat looks, see-through) unless it is playing, a little less so under the mouse
QColor blockFill(const QColor& track, bool hover, bool playing);
QColor mix(const QColor& a, const QColor& b, double t); // a colour t of the way from a to b
void drawKey(QPainter& p, const QRectF& r, const QString& text, bool on,
             const QColor& onColour); // small M / S style key
// the same key with a symbol: alone when text is empty, else the symbol and the word side by side
void drawKey(QPainter& p, const QRectF& r, const QString& text, bool on, const QColor& onColour, const QString& symbol);
void drawSunken(QPainter& p, const QRectF& r); // shade along the top and left of a recessed area
void drawGlass(QPainter& p, const QRectF& face,
               double radius); // a pane of glass over a window: the frame's shadow, a reflection and a light edge
// a well sunk into the panel, screwed in at its four corners, as the sections of the instrument look sit in
void drawWell(QPainter& p, const QRectF& r, bool selected = false);
// ivory glass lit from behind, as in the needle meters: for the display and the readouts
void drawIvory(QPainter& p, const QRectF& glass, double radius);
void installWells(); // once, after the application is made: group boxes, top bar units and mixer strips get their wells
// A section that folds shut when its name is clicked, leaving the name and a one-line summary (by default the choices
// of its lists, or the first knobs and their values). key: where the state is remembered between runs.
void makeFoldable(QGroupBox* g, const QString& key, std::function<QString()> summary = {});
void setFolded(QGroupBox* g, bool folded);
constexpr int GROUP_TOP = 17; // the room above a section's well, for its name
void drawSlider(QPainter& p, const QRectF& slot, float value,
                const QColor& fill); // a small horizontal fader in a sunken slot
void drawRoleIcon(QPainter& p, const QRectF& r, int role,
                  const QColor& c); // a small symbol for bass, sequence, pad, lead or drums
QIcon roleIcon(int role, const QColor& c);
QIcon mixerIcon(const QColor& c); // three faders, for the Mixer tab
// a form laid out the same on every system: labels flush left, the boxes filling their column (a Mac would set the
// labels flush right and keep the boxes narrow)
void tidyForm(QFormLayout* form);
} // namespace theme

class Knob : public QWidget {
	Q_OBJECT
public:
	explicit Knob(const QString& label, QWidget* parent = nullptr);
	void setValue(float v);
	float value() const { return val; }
	void setDefault(float v) { def = v; }
	void setFormatter(std::function<QString(float)> f) { fmt = std::move(f); }
	void setAccent(const QColor& c) {
		accent = c;
		update();
	}
	void setMarked(bool m) {
		if (marked != m) {
			marked = m;
			update();
		}
	} // has recorded moves
	bool isHeld() const { return dragging; }
	QString name() const { return label; }
	QString valueText() const { return fmt ? fmt(val) : QString::number(int(std::lround(val * 100))); }
	void setScale(double s) {
		scale = s;
		setFixedSize(sizeHint());
		update();
	} // a bigger knob, for the simple sound page
	QSize sizeHint() const override { return {int(58 * scale), int((caption_ ? 50 : 78) * scale)}; }
	// the name goes in this label above the knob instead of under it, and the value while it is turned or pointed at
	// (the top bar, where every control has its name in one row above it)
	void setCaption(QLabel* caption);
	// knobs with a "midiTarget" property ask this whether a MIDI controller turns them (controller number, or -1)
	static std::function<int(const QString&)> midiLookup;
signals:
	void pressed(); // a change is about to start (one undo step)
	void moved(float v);
	void released();

protected:
	void paintEvent(QPaintEvent*) override;
	void mousePressEvent(QMouseEvent*) override;
	void mouseMoveEvent(QMouseEvent*) override;
	void mouseReleaseEvent(QMouseEvent*) override;
	void mouseDoubleClickEvent(QMouseEvent*) override;
	void enterEvent(QEnterEvent*) override;
	void leaveEvent(QEvent*) override;

private:
	void drawMidiMark(QPainter& p, const QRectF& dial) const;
	QString label;
	float val = 0.5f, def = 0.5f, startVal = 0.f;
	int startY = 0;
	bool dragging = false, hover = false, marked = false;
	double scale = 1.0;
	QLabel* caption_ = nullptr;
	void showCaption();
	QColor accent = theme::amber;
	std::function<QString(float)> fmt;
};

// An analogue level meter with a needle, for the output section.
class VuMeter : public QWidget {
	Q_OBJECT
public:
	explicit VuMeter(const QString& label, QWidget* parent = nullptr);
	void feed(float peak); // called about 30 times a second with the latest peak level
	QSize sizeHint() const override { return {82, 57}; }
	QSize minimumSizeHint() const override { return sizeHint(); }

protected:
	void paintEvent(QPaintEvent*) override;

private:
	QString label;
	float needle = 0.f; // 0..1 along the scale (-20 to +3 dB), with the slow fall of a real meter
};

// A small sliding switch for settings that stay on or off. Its name is printed above it (the transport's Loop,
// Click and In time), or beside it like a tick box's (Beside): Nachtzug has switches instead of tick boxes. The knob
// slides across when it is switched.
class Switch : public QAbstractButton {
public:
	enum Label { Above, Beside };
	explicit Switch(const QString& label, Label where = Above, QWidget* parent = nullptr);
	QSize sizeHint() const override;
	QSize minimumSizeHint() const override { return sizeHint(); }

protected:
	void paintEvent(QPaintEvent*) override;

private:
	Label where_;
	double at_ = 0.0; // where the knob is: 0 off, 1 on, in between while it slides
	QVariantAnimation* slide_;
};

// The Nachtzug mark in the top bar. It gives way when the window is narrow: smaller, then gone.
class Emblem : public QWidget {
public:
	explicit Emblem(QWidget* parent = nullptr) : QWidget(parent) {
		setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	}
	QSize sizeHint() const override { return {86, 86}; }
	QSize minimumSizeHint() const override { return {0, 86}; }

protected:
	void paintEvent(QPaintEvent*) override;
};

// A line of text that is shortened with "…" when there is not enough room, and steps aside when there is
// hardly any, instead of holding the window wide open.
class ElideLabel : public QWidget {
public:
	explicit ElideLabel(const QString& text, QWidget* parent = nullptr) : QWidget(parent), text_(text) {
		setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	}
	void setText(const QString& t) {
		text_ = t;
		updateGeometry();
		update();
	}
	QString text() const { return text_; }
	QSize sizeHint() const override { return {fontMetrics().horizontalAdvance(text_) + 4, fontMetrics().height()}; }
	QSize minimumSizeHint() const override { return {0, fontMetrics().height()}; }

protected:
	void paintEvent(QPaintEvent*) override;

private:
	QString text_;
};

// Red LED display: oscilloscope, song position, key and tempo, plus short messages.
class Lcd : public QWidget {
	Q_OBJECT
public:
	explicit Lcd(App* app, QWidget* parent = nullptr);
	void setMessage(const QString& m);
	QSize sizeHint() const override { return {380, 58}; }
	QSize minimumSizeHint() const override { return {300, 52}; }

protected:
	void paintEvent(QPaintEvent*) override;

private:
	App* app;
	QString message;
	qint64 messageUntil = 0;
	mutable bool everPlayed = false;
};

// A short note that rises at the bottom of the window after a big change ("New idea written"), with a button
// that takes it back. It fades away by itself after a few seconds; hovering keeps it.
class Toast : public QWidget {
	Q_OBJECT
public:
	explicit Toast(QWidget* parent);
	void showMessage(const QString& text, const QString& action = QString(), int ms = 5000);
signals:
	void actionClicked();

protected:
	void paintEvent(QPaintEvent*) override;
	void mousePressEvent(QMouseEvent*) override;
	void mouseMoveEvent(QMouseEvent*) override;
	void enterEvent(QEnterEvent*) override;
	void leaveEvent(QEvent*) override;
	bool eventFilter(QObject* o, QEvent* e) override;

private:
	void place();
	QRect actionRect() const;
	QString text, action;
	QTimer* hideTimer;
	bool hoverAction = false;
};
