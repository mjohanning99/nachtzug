// Shared look (colours, style sheet) and the small custom widgets: Knob and the red LED display.
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
#include <QTimer>

#include <QLayout>

class App;

// Lays its widgets out left to right and starts a new row when the next one does not fit, so a row of tool
// groups wraps onto a second line in a narrow window instead of making the window wider.
class FlowLayout : public QLayout {
public:
    explicit FlowLayout(QWidget* parent = nullptr, int hSpacing = 10, int vSpacing = 8) : QLayout(parent), h_(hSpacing), v_(vSpacing) { setContentsMargins(0, 0, 0, 0); }
    // justify: every row is stretched to the full width (the extra shared by the items that can grow, in proportion
    // to their width) and its items get the same height, so the sections of a page end on one line
    void setJustify(bool on) { justify_ = on; }
    ~FlowLayout() override { while (QLayoutItem* it = takeAt(0)) delete it; }
    void addItem(QLayoutItem* item) override { items_.append(item); }
    void insertWidget(int i, QWidget* w) { addChildWidget(w); items_.insert(std::clamp(i, 0, int(items_.size())), new QWidgetItem(w)); invalidate(); }
    int count() const override { return int(items_.size()); }
    QLayoutItem* itemAt(int i) const override { return i >= 0 && i < items_.size() ? items_[i] : nullptr; }
    QLayoutItem* takeAt(int i) override { return i >= 0 && i < items_.size() ? items_.takeAt(i) : nullptr; }
    Qt::Orientations expandingDirections() const override { return {}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override { return place(QRect(0, 0, w, 0), false); }
    void setGeometry(const QRect& r) override { QLayout::setGeometry(r); place(r, true); }
    QSize sizeHint() const override;
    QSize minimumSize() const override;
private:
    int place(const QRect& r, bool apply) const;
    QList<QLayoutItem*> items_;
    int h_, v_;
    bool justify_ = false;
};

// Colours of the current theme. apply() fills them in; everything paints from these.
namespace theme {
inline QColor panel, well, wellHi, rule, ink, dim, canvas, lane, laneSel, grid, gridBar;
inline QColor amber /* main accent */, accentInk /* text on the accent */, ice /* second accent */, cap, capEdge, capInk, lcdBg, lcd, key, keyInk, hover;
inline QColor tracks[5];
inline QColor clipInk;        // dark marks drawn on top of the coloured clips and notes, in every theme
inline bool tactile = false;  // the instrument look: keys with depth, ribbed knobs, an LCD
int count();
QString name(int index);
int current();
void apply(int index);
QColor trackColor(int role);
inline bool isLight() { return canvas.lightness() > 128; }
inline QColor slot() { return isLight() ? gridBar : canvas; }   // the empty part of sliders, faders, meters and knob rings
QString styleSheet();
QPalette palette();
QPixmap logo(int size);      // the Nachtzug mark: the tz ligature on an enamel sign
QIcon appIcon();
QIcon ledIcon(const QColor& lit);   // a small indicator light: dark when off, lit when the button is on
QIcon transportIcon(const QString& kind);   // "start", "play", "stop", "rec": symbols for the transport keys
void loadFonts();
QFont lcdFont(int pixelSize, bool bold = false);
// Drawing helpers for the instrument look; in the flat themes they draw the plain versions.
void drawBlock(QPainter& p, const QRectF& r, const QColor& c, double radius, bool raised = true);   // clip, note or cell
void drawKey(QPainter& p, const QRectF& r, const QString& text, bool on, const QColor& onColour);   // small M / S style key
void drawSunken(QPainter& p, const QRectF& r);   // shade along the top and left of a recessed area
void drawGlass(QPainter& p, const QRectF& face, double radius);   // a pane of glass over a window: the frame's shadow, a reflection and a light edge
// a well sunk into the panel, screwed in at its four corners, as the sections of the instrument look sit in
void drawWell(QPainter& p, const QRectF& r, bool selected = false);
// ivory glass lit from behind, as in the needle meters: for the display and the readouts
void drawIvory(QPainter& p, const QRectF& glass, double radius);
void installWells();   // once, after the application is made: group boxes, top bar units and mixer strips get their wells
// A section that folds shut when its name is clicked, leaving the name and a one-line summary (by default the choices
// of its lists, or the first knobs and their values). key: where the state is remembered between runs.
void makeFoldable(QGroupBox* g, const QString& key, std::function<QString()> summary = {});
void setFolded(QGroupBox* g, bool folded);
constexpr int GROUP_TOP = 17;   // the room above a section's well, for its name
void drawSlider(QPainter& p, const QRectF& slot, float value, const QColor& fill);   // a small horizontal fader in a sunken slot
void drawRoleIcon(QPainter& p, const QRectF& r, int role, const QColor& c);   // a small symbol for bass, sequence, pad, lead or drums
QIcon roleIcon(int role, const QColor& c);
}

class Knob : public QWidget {
    Q_OBJECT
public:
    explicit Knob(const QString& label, QWidget* parent = nullptr);
    void setValue(float v);
    float value() const { return val; }
    void setDefault(float v) { def = v; }
    void setFormatter(std::function<QString(float)> f) { fmt = std::move(f); }
    void setAccent(const QColor& c) { accent = c; update(); }
    void setMarked(bool m) { if (marked != m) { marked = m; update(); } }   // has recorded moves
    bool isHeld() const { return dragging; }
    QString name() const { return label; }
    QString valueText() const { return fmt ? fmt(val) : QString::number(int(std::lround(val * 100))); }
    void setScale(double s) { scale = s; setFixedSize(sizeHint()); update(); }   // a bigger knob, for the simple sound page
    QSize sizeHint() const override { return {int(58 * scale), int(78 * scale)}; }
    // knobs with a "midiTarget" property ask this whether a MIDI controller turns them (controller number, or -1)
    static std::function<int(const QString&)> midiLookup;
signals:
    void pressed();        // a change is about to start (one undo step)
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
    QColor accent = theme::amber;
    std::function<QString(float)> fmt;
};

// An analogue level meter with a needle, for the output section.
class VuMeter : public QWidget {
    Q_OBJECT
public:
    explicit VuMeter(const QString& label, QWidget* parent = nullptr);
    void feed(float peak);           // called about 30 times a second with the latest peak level
    QSize sizeHint() const override { return {96, 64}; }
    QSize minimumSizeHint() const override { return {80, 64}; }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QString label;
    float needle = 0.f;              // 0..1 along the scale, with the slow fall of a real meter
};

// A small sliding switch with its name printed above it, for settings that stay on or off.
class Switch : public QAbstractButton {
public:
    explicit Switch(const QString& label, QWidget* parent = nullptr);
    QSize sizeHint() const override { return {64, 40}; }
protected:
    void paintEvent(QPaintEvent*) override;
};

// The Nachtzug mark in the top bar. It gives way when the window is narrow: smaller, then gone.
class Emblem : public QWidget {
public:
    explicit Emblem(QWidget* parent = nullptr) : QWidget(parent) { setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed); }
    QSize sizeHint() const override { return {86, 86}; }
    QSize minimumSizeHint() const override { return {0, 86}; }
protected:
    void paintEvent(QPaintEvent*) override;
};

// A line of text that is shortened with "…" when there is not enough room, and steps aside when there is
// hardly any, instead of holding the window wide open.
class ElideLabel : public QWidget {
public:
    explicit ElideLabel(const QString& text, QWidget* parent = nullptr) : QWidget(parent), text_(text) { setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred); }
    void setText(const QString& t) { text_ = t; updateGeometry(); update(); }
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
