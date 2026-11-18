// The colour themes (Bakelite, Ember and the rest): their colours, the fonts, the palette and the style sheet
// that dresses every standard control in them, with the small pictures it uses (drawn as PNG: no SVG plugin).
//
// What's here: count(), name(), current(), apply(), trackColor(), loadFonts(), mix(), palette(), styleSheet();
// apply() and styleSheet() are called at start-up and when the look changes (Settings, View).
// Changes in the song file: none.
#include "widgets.h"
#include <QDir>
#include <QFile>
#include <QComboBox>
#include <QFontDatabase>
#include <QFrame>
#include <QGroupBox>
#include <QPainter>
#include <QImage>
#include <QMenu>
#include <QAbstractItemView>
#include <functional>

namespace theme {

struct Def {
	const char* name;
	unsigned panel, well, wellHi, rule, ink, dim, canvas, lane, laneSel, grid, gridBar;
	unsigned accent, accentInk, second, cap, capEdge, capInk, key, keyInk, hover, lcdBg, lcd;
	unsigned tracks[5];   // bass, sequence, pad, lead, drums
	bool tactile = false; // keys with depth, ribbed knobs, LCD
};
// clang-format off: one colour theme per row, as a table
static const Def DEFS[] = {
	// the instrument look: warm enamel panel, ribbed black knobs, a warm LCD
	{QT_TRANSLATE_NOOP("Model", "Bakelite"), 0xebe5d8, 0xe1d9c9, 0xe9e2d4, 0xb1a391, 0x2b2623, 0x6e6054, 0xf6f2ea, 0xeee8dc, 0xe4dccc, 0xe0d7c7, 0xbfb2a0,
	 0xd8642a, 0xfff8ee, 0xd98a2b, 0x2a2624, 0x141211, 0xe8743a, 0x2f2a27, 0xf3ece0, 0xdcd3c3, 0xf4e8c9, 0x3b2a1b,
	 {0xd98a2b, 0xb79b5c, 0xb77a8e, 0xc95a33, 0x7f9356}, true},
	{QT_TRANSLATE_NOOP("Model", "Ember"), 0x2b2623, 0x1d1917, 0x3a322c, 0x8a7565, 0xf3ece0, 0xc2b3a1, 0x141110, 0x1f1b18, 0x2b2520, 0x302924, 0x57483c,
	 0xed9d24, 0x2b2623, 0xe9d5a1, 0x161210, 0xcdbfae, 0xf3ece0, 0xece8df, 0x2b2623, 0x4a3f37, 0x190807, 0xff5a36,
	 {0xed9d24, 0xe9d5a1, 0xd79a8c, 0xe2643b, 0xb9c27a}},
	// a real light theme: cream paper, light knobs and display, rust and amber for emphasis
	{QT_TRANSLATE_NOOP("Model", "Parchment"), 0xece8df, 0xdcd5c7, 0xe5dfd2, 0xa89a8a, 0x2b2623, 0x6a5d51, 0xf8f5ee, 0xefe9dd, 0xe3dbcb, 0xe0d8c9, 0xb9ac9a,
	 0xa9270a, 0xfff8ee, 0xed9d24, 0xfbf8f1, 0x9a8b7b, 0x2b2623, 0xed9d24, 0x2b2623, 0xd6cdbc, 0xfbf8f1, 0xa9270a,
	 {0xd98a1c, 0xa9853c, 0xb0627f, 0xc2401f, 0x6f8a45}},
	{QT_TRANSLATE_NOOP("Model", "Wine"), 0x3b1f26, 0x2a141a, 0x4a2a32, 0x9c6f78, 0xf6ebe6, 0xd1b3b0, 0x1a0d11, 0x24131a, 0x321c24, 0x38202a, 0x64404a,
	 0xe8a23a, 0x3b1f26, 0xf0c9a8, 0x16090d, 0xd9bfb8, 0xf6ebe6, 0xf2e6dc, 0x3b1f26, 0x5a343e, 0x190807, 0xff5a36,
	 {0xe8a23a, 0xf0c9a8, 0xd98fb0, 0xf0714f, 0xbfc48a}},
	{QT_TRANSLATE_NOOP("Model", "Charcoal"), 0x2e2d2d, 0x1f1f1f, 0x3a3938, 0x7d7872, 0xf0ede8, 0xb9b3aa, 0x151515, 0x1e1e1e, 0x2a2928, 0x2f2e2d, 0x504d49,
	 0xdba047, 0x1f1f1f, 0xd9d2c5, 0x111111, 0xc8c2b8, 0xf0ede8, 0xe6e2da, 0x1f1f1f, 0x48463f, 0x190807, 0xff5a36,
	 {0xdba047, 0xd9d2c5, 0xc79a8f, 0xd4683c, 0xa9b58a}},
	{QT_TRANSLATE_NOOP("Model", "Blue panel"), 0x1f4b8e, 0x163868, 0x1d4680, 0x7fa0d2, 0xf1f4f8, 0xb9cae4, 0x0e1c36, 0x122647, 0x18315a, 0x1f3d6e, 0x345c9c,
	 0xffb53d, 0x15171c, 0xa6ecff, 0x15171c, 0xc9ced6, 0xf1f4f8, 0xe9ecf0, 0x15171c, 0x2a5ba6, 0x190807, 0xff5a36,
	 {0xffb53d, 0xa6ecff, 0xc9a7ff, 0xff8f6b, 0x9fe08c}},
};
// clang-format on
static int cur = 0;

int count() {
	return int(sizeof(DEFS) / sizeof(DEFS[0]));
}
QString name(int i) {
	return QString::fromLatin1(DEFS[std::clamp(i, 0, count() - 1)].name);
}
int current() {
	return cur;
}

void apply(int index) {
	cur = std::clamp(index, 0, count() - 1);
	const Def& d = DEFS[cur];
	auto c = [](unsigned v) { return QColor(int((v >> 16) & 0xff), int((v >> 8) & 0xff), int(v & 0xff)); };
	panel = c(d.panel);
	well = c(d.well);
	wellHi = c(d.wellHi);
	rule = c(d.rule);
	ink = c(d.ink);
	dim = c(d.dim);
	canvas = c(d.canvas);
	lane = c(d.lane);
	laneSel = c(d.laneSel);
	grid = c(d.grid);
	gridBar = c(d.gridBar);
	amber = c(d.accent);
	accentInk = c(d.accentInk);
	ice = c(d.second);
	cap = c(d.cap);
	capEdge = c(d.capEdge);
	capInk = c(d.capInk);
	key = c(d.key);
	keyInk = c(d.keyInk);
	hover = c(d.hover);
	lcdBg = c(d.lcdBg);
	lcd = c(d.lcd);
	clipInk = QColor(0x22, 0x1c, 0x18);
	tactile = d.tactile;
	for (int i = 0; i < 5; i++)
		tracks[i] = c(d.tracks[i]);
}

QColor trackColor(int role) {
	return tracks[std::clamp(role, 0, 4)];
}

void loadFonts() {
	for (const char* f : {":/fonts/nachtzug/NachtzugGrotesk-Regular.ttf", ":/fonts/nachtzug/NachtzugGrotesk-Medium.ttf",
	                      ":/fonts/nachtzug/NachtzugGrotesk-SemiBold.ttf",
	                      ":/fonts/nachtzug/NachtzugAntiqua-Regular.ttf", ":/fonts/nachtzug/NachtzugAntiqua-Bold.ttf",
	                      ":/fonts/CourierPrime-Regular.ttf", ":/fonts/CourierPrime-Bold.ttf"})
		QFontDatabase::addApplicationFont(QString::fromLatin1(f));
}

QFont lcdFont(int pixelSize, bool bold) {
	QFont f(QStringLiteral("Courier Prime"));
	f.setStyleHint(QFont::Monospace);
	f.setPixelSize(pixelSize);
	f.setBold(bold);
	return f;
}

QColor mix(const QColor& a, const QColor& b, double t) {
	return QColor::fromRgbF(float(a.redF() * (1 - t) + b.redF() * t), float(a.greenF() * (1 - t) + b.greenF() * t),
	                        float(a.blueF() * (1 - t) + b.blueF() * t));
}

QPalette palette() {
	QPalette pal;
	pal.setColor(QPalette::Window, panel);
	pal.setColor(QPalette::WindowText, ink);
	pal.setColor(QPalette::Base, well);
	pal.setColor(QPalette::AlternateBase, wellHi);
	pal.setColor(QPalette::Text, ink);
	pal.setColor(QPalette::Button, key);
	pal.setColor(QPalette::ButtonText, keyInk);
	pal.setColor(QPalette::Highlight, amber);
	pal.setColor(QPalette::HighlightedText, accentInk);
	pal.setColor(QPalette::ToolTipBase, lcdBg);
	pal.setColor(QPalette::ToolTipText, lcd);
	return pal;
}

// A small picture for the style sheet (a drop-down arrow, the fader cap, a divider's grip), drawn here and saved next
// to the program's temporary files at one, two and three times its size: name.png, name@2x.png and name@3x.png, so
// Qt can take the sharp one on any screen. Drawn rather than written as SVG, so the downloads need no SVG plugin.
// Change the "p1" in the names when a picture changes, or the old files are used. Returns the path of name.png.
static QString stylePicture(const QString& name, QSize size, const std::function<void(QPainter&)>& draw) {
	const QString base = QDir::temp().filePath(QStringLiteral("nachtzug-p1-") + name);
	for (int scale = 1; scale <= 3; scale++) {
		const QString path =
		    base + (scale == 1 ? QString() : QStringLiteral("@%1x").arg(scale)) + QStringLiteral(".png");
		if (QFile::exists(path)) continue;
		QImage img(size * scale, QImage::Format_ARGB32_Premultiplied);
		img.fill(Qt::transparent);
		QPainter p(&img);
		p.setRenderHint(QPainter::Antialiasing);
		p.scale(scale, scale);
		draw(p);
		p.end();
		img.save(path);
	}
	return base + QStringLiteral(".png");
}

QString styleSheet() {
	QString s = QStringLiteral(R"(
QWidget { color: @ink@; font-size: 13px; }
QMainWindow, QDialog { background: @panel@; }
QMenuBar { background: @well@; }
QMenuBar::item:selected, QMenu::item:selected { background: @hover@; }
QMenu { background: @well@; border: 1px solid @rule@; border-radius: 6px; padding: 4px 0; }
QMenu::item { padding: 4px 22px 4px 14px; border-radius: 4px; margin: 0 4px; }
QMenu::separator { height: 1px; background: @gridBar@; margin: 4px 8px; }
QStatusBar { background: @well@; color: @dim@; }
QToolTip { background: @lcdBg@; color: @lcdText@; border: 1px solid @lcd@; padding: 4px; }
QLabel#dim { color: @dim@; }
QLabel#cap { color: @dim@; font-size: 13px; }
QToolButton[transport="true"] { min-height: 30px; max-height: 30px; padding: 0 6px; }
QToolButton#exportKey { min-height: 62px; max-height: 62px; padding: 4px 4px 2px 4px; }
QPushButton#tap { min-height: 26px; max-height: 26px; padding: 0 10px; font-weight: 600; }
QToolButton#help { min-height: 16px; max-height: 16px; min-width: 20px; max-width: 20px; padding: 0; font-weight: 600; font-size: 11px; }
QToolButton#inspectorTab { min-height: 0; max-height: 16000px; padding: 0; border-radius: 0 6px 6px 0; border-left: none; font-size: 15px; } /* the tab on the left edge, as tall as the window */
/* the mixer strips' keys sit between the screws, so they are narrower: less room beside the word */
QFrame#strip QPushButton#mute, QFrame#strip QPushButton[strip="true"] { padding: 0 2px; }
QFrame#strip QPushButton#mute:pressed, QFrame#strip QPushButton#mute:checked, QFrame#strip QPushButton[strip="true"]:pressed,
  QFrame#strip QPushButton[strip="true"]:checked { padding: 2px 2px 1px 2px; }
QPushButton#segL { border-top-right-radius: 0; border-bottom-right-radius: 0; margin-right: 0; }
QPushButton#segR { border-top-left-radius: 0; border-bottom-left-radius: 0; border-left: none; margin-left: 0; }
QPushButton#segM { border-radius: 0; border-left: none; margin-left: 0; margin-right: 0; }
QFrame#soundCard { background: @field@; border: 1px solid @rule@; border-radius: 6px; }
QFrame#soundCard[on="true"] { border: 2px solid @amber@; }
QLabel#cardName { font-weight: 600; }
QLabel#cardGone { color: @dim@; font-weight: 600; }
QFrame#soundChosen { background: @wellHi@; border: 1px solid @rule@; border-radius: 6px; }
QLabel#chosenName { font-size: 19px; font-weight: 600; }
QPushButton#mute:checked { background: #c4361c; color: #fff8ee; border-color: #7a2010; }
QLabel#title { font-weight: 600; letter-spacing: 1px; }
QGroupBox { border: 1px solid @rule@; border-radius: 6px; margin-top: 9px; padding: 8px 6px 4px 6px; }
QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; color: @dim@; font-size: 10px; font-weight: 600; letter-spacing: 2px; }
QPushButton, QToolButton { background: @wellHi@; color: @ink@; border: 1px solid @rule@; border-radius: 4px; padding: 0 12px; min-height: 28px; max-height: 28px; font-weight: 600; }
QPushButton:hover, QToolButton:hover { background: @hover@; }
QPushButton:pressed, QToolButton:pressed { background: @well@; }
QPushButton:checked, QToolButton:checked { background: @amber@; color: @accentInk@; border-color: @amber@; }
QPushButton:disabled, QToolButton:disabled { background: transparent; color: @rule@; border-color: @gridBar@; }
QPushButton#primary, QToolButton#primary { background: @key@; color: @keyInk@; border-color: @key@; }
QPushButton#primary:hover, QToolButton#primary:hover { background: @keyHover@; }
QPushButton#primary:disabled, QToolButton#primary:disabled { background: transparent; color: @rule@; border-color: @gridBar@; }
QPushButton#primary:pressed, QToolButton#primary:pressed { background: @keyPress@; }
QToolButton#primary:checked { background: @amber@; color: @accentInk@; border-color: @amber@; }
QToolButton#rec:checked { background: @lcd@; color: @accentInk@; border-color: @lcd@; }
QWidget#sep { background: @rule@; }
QWidget#topbar { border-bottom: 1px solid @rule@; }
QToolButton[big="true"] { min-height: 32px; max-height: 32px; font-size: 14px; padding: 0 14px; }
QLabel#plate { color: @ink@; }
QFrame#strip { background: @well@; border: 1px solid @gridBar@; border-radius: 6px; padding: 1px; }
QFrame#strip[sel="true"] { border: 2px solid @amber@; padding: 0; } /* the thicker border takes the padding's place: nothing inside moves */
QPushButton#stripName { background: transparent; border: none; border-radius: 0; font-weight: 600; }
QPushButton#stripName:hover { background: @hover@; }
QTextBrowser { background: @canvas@; border: 1px solid @rule@; border-radius: 6px; padding: 8px; }
QToolButton::menu-indicator { image: none; }
QComboBox, QSpinBox { background: @well@; border: 1px solid @rule@; border-radius: 4px; padding: 0 8px; min-height: 28px; max-height: 28px; }
QSpinBox::up-button, QSpinBox::down-button { width: 0; height: 0; border: none; }
QComboBox:disabled { color: @dim@; }
/* a drop-down's open list: the window round it draws the rounded body (WellPainter), the list is see-through on
   it, and its items light up as the menus' do */
QComboBox QAbstractItemView { background: transparent; border: none; padding: 3px; outline: none; selection-background-color: @hover@; selection-color: @ink@; }
QComboBox QAbstractItemView::item { min-height: 24px; padding: 0 8px; border: none; border-radius: 4px; }
QComboBox QAbstractItemView::item:selected, QComboBox QAbstractItemView::item:hover { background: @hover@; color: @ink@; }
QComboBox::drop-down { border: none; width: 16px; }
QComboBox::down-arrow { image: url(@arrowInk@); width: 10px; height: 6px; margin-right: 6px; }
QCheckBox { spacing: 6px; }
/* tabs like a card index: the chosen one is the page's own colour and runs into it, the others stand back a little;
   neighbours share their border (margin -1px), the last one keeps its own */
QTabWidget::pane { border: none; border-top: 1px solid @rule@; top: -1px; }
QTabBar::tab { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @well@, stop:1 @slot@); color: @dim@; border: 1px solid @rule@;
  border-bottom: none; border-top-left-radius: 9px; border-top-right-radius: 9px; padding: 6px 20px; margin-top: 5px; margin-right: -1px;
  font-weight: 600; }
QTabBar::tab:last { margin-right: 0; }
QTabBar::tab:selected { background: @panel@; color: @ink@; margin-top: 0; padding-bottom: 11px; }
QScrollBar:horizontal { height: 12px; background: @canvas@; }
QScrollBar:vertical { width: 12px; background: @canvas@; }
QScrollBar::handle { background: @gridBar@; border-radius: 5px; min-width: 24px; min-height: 24px; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollArea { border: none; background: @panel@; }
/* a divider that can be dragged: a fine line with a raised handle in the middle, like a drawer pull; amber under the
   mouse */
QSplitter::handle:vertical { border: none; height: 14px; image: url(@gripH@);
  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 transparent, stop:0.49 transparent, stop:0.5 @rule@, stop:0.57 @rule@, stop:0.58 transparent, stop:1 transparent); }
QSplitter::handle:horizontal { border: none; width: 14px; image: url(@gripV@);
  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 transparent, stop:0.49 transparent, stop:0.5 @rule@, stop:0.57 @rule@, stop:0.58 transparent, stop:1 transparent); }
QSplitter::handle:vertical:hover { image: url(@gripHHot@);
  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 transparent, stop:0.46 transparent, stop:0.47 @amber@, stop:0.6 @amber@, stop:0.61 transparent, stop:1 transparent); }
QSplitter::handle:horizontal:hover { image: url(@gripVHot@);
  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 transparent, stop:0.46 transparent, stop:0.47 @amber@, stop:0.6 @amber@, stop:0.61 transparent, stop:1 transparent); }
)");
	if (tactile)
		s += QStringLiteral(R"(
QWidget { font-family: "@uiFont@"; font-size: @uiSize@px; }
QMainWindow, QDialog { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @panelHi@, stop:1 @panel@); }
QMenuBar { background: @panelHi@; border-bottom: 1px solid @rule@; }
QGroupBox { background: transparent; border: none; margin-top: @groupTop@px; padding: 5px 8px 3px 8px; }
QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 10px; top: 1px; padding: 0; color: @ink@; font-size: 10px; font-weight: 500; letter-spacing: 2.5px; }
/* keys: the dark lip under each key is the bottom band of its own gradient, so it follows the key's rounded corners
   exactly (a thicker bottom border would join the corners badly). Pressed or lit keys show a thinner lip. */
QPushButton, QToolButton { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @keyTop@, stop:0.895 @keyBot@, stop:0.900 @keyDeep@, stop:1 @keyDeep@);
  color: @ink@; border: 1px solid @keyEdge@; border-radius: 4px;
  padding: 0 12px 3px 12px; min-height: 25px; max-height: 25px; font-weight: 500; letter-spacing: 0.5px; } /* 30 in all: 25, the 3 px lip, the borders (measured) */
QPushButton:hover, QToolButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:0.895 @keyTop@, stop:0.900 @keyDeep@, stop:1 @keyDeep@); }
QPushButton:pressed, QToolButton:pressed { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @keyBot@, stop:0.962 @keyBot@, stop:0.967 @keyDeep@, stop:1 @keyDeep@); padding: 2px 12px 1px 12px; }
QPushButton:checked, QToolButton:checked { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @amberHi@, stop:0.962 @amber@, stop:0.967 @amberDeep@, stop:1 @amberDeep@);
  color: @accentInk@; border-color: @amberDeep@; padding: 2px 12px 1px 12px; }
QPushButton:disabled, QToolButton:disabled { background: transparent; color: @rule@; border-color: @gridBar@; }
/* joined keys (rounded on one side only) are painted another way, and their gradient falls a little lower: at 1.5×
   and 2× their lip started a device pixel below the other keys'. These stops, 0.0125 earlier, put it on the same
   line at every scale from 1× to 3× (measured) */
QPushButton#segL, QPushButton#segR { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @keyTop@, stop:0.8825 @keyBot@, stop:0.8875 @keyDeep@, stop:1 @keyDeep@); }
QPushButton#segL:hover, QPushButton#segR:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:0.8825 @keyTop@, stop:0.8875 @keyDeep@, stop:1 @keyDeep@); }
QPushButton#segL:pressed, QPushButton#segR:pressed { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @keyBot@, stop:0.9495 @keyBot@, stop:0.9545 @keyDeep@, stop:1 @keyDeep@); }
QPushButton#segL:checked, QPushButton#segR:checked { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @amberHi@, stop:0.9495 @amber@, stop:0.9545 @amberDeep@, stop:1 @amberDeep@); }
QPushButton#segL:disabled, QPushButton#segR:disabled { background: transparent; }
QPushButton#primary, QToolButton#primary { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @keyHover@, stop:0.895 @key@, stop:0.900 #120f0d, stop:1 #120f0d); color: @keyInk@; border-color: #000000; }
QPushButton#primary:hover, QToolButton#primary:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @keyHover@, stop:0.895 @keyHover@, stop:0.900 #120f0d, stop:1 #120f0d); }
QPushButton#primary:pressed, QToolButton#primary:pressed { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @key@, stop:0.962 @key@, stop:0.967 #120f0d, stop:1 #120f0d); padding: 2px 12px 1px 12px; }
QToolButton#primary:checked { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @amberHi@, stop:0.962 @amber@, stop:0.967 @amberDeep@, stop:1 @amberDeep@); color: @accentInk@; border-color: @amberDeep@; }
QToolButton#rec:checked { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #e0563c, stop:0.962 #b8361f, stop:0.967 #7a2010, stop:1 #7a2010); color: #fff8ee; border-color: #7a2010; }
QToolButton[transport="true"] { min-height: 23px; max-height: 23px; padding: 0 6px 3px 6px; } /* 28 in all (23, the 3 px lip, the borders), 2 less than a key */
QToolButton[transport="true"]:pressed, QToolButton[transport="true"]:checked { padding: 2px 6px 1px 6px; }
QToolButton[big="true"] { min-height: 31px; max-height: 31px; padding: 0 14px 3px 14px; }
QToolButton[big="true"]:pressed, QToolButton[big="true"]:checked { padding: 2px 14px 1px 14px; }
QToolButton#exportKey { min-height: 0; max-height: 999px; padding: 4px 4px 5px 4px; background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @keyTop@, stop:0.950 @keyBot@, stop:0.955 @keyDeep@, stop:1 @keyDeep@); }
QToolButton#exportKey:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:0.950 @keyTop@, stop:0.955 @keyDeep@, stop:1 @keyDeep@); }
QToolButton#exportKey:pressed { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @keyBot@, stop:0.980 @keyBot@, stop:0.985 @keyDeep@, stop:1 @keyDeep@); padding: 6px 4px 3px 4px; }
QToolButton#help { min-height: 15px; max-height: 15px; padding: 0 0 2px 0; }
QToolButton#help:pressed { padding: 1px 0 1px 0; }
/* a field sits a little into the panel: the shade under its top edge is painted inside one even border, so it follows
   the rounded corners (a thicker top border met the corners in a step, which a Retina screen shows plainly) */
QComboBox, QSpinBox { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @fieldShade@, stop:0.14 @field@, stop:1 @field@);
  border: 1px solid @rule@; border-radius: 4px; padding: 0 8px; min-height: 26px; max-height: 26px; }
QComboBox::drop-down { border: none; width: 18px; }
QComboBox::down-arrow { image: url(@arrowInk@); width: 10px; height: 6px; margin-right: 6px; }
QComboBox#readout::down-arrow { image: url(@arrowLcd@); }
/* key and scale in the top bar: keys with the value on them, which open their list */
QComboBox#selector { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @keyTop@, stop:0.895 @keyBot@, stop:0.900 @keyDeep@, stop:1 @keyDeep@);
  color: @ink@; border: 1px solid @keyEdge@; border-radius: 4px; padding: 0 6px 3px 10px; min-height: 23px; max-height: 23px; font-weight: 500; letter-spacing: 0.5px; }
QComboBox#selector:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:0.895 @keyTop@, stop:0.900 @keyDeep@, stop:1 @keyDeep@); }
QComboBox#selector::drop-down { border: none; width: 18px; padding-bottom: 3px; }
QSlider { min-height: 26px; }
QSlider::groove:horizontal { height: 8px; border-radius: 4px; border: 1px solid @rule@;
  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @slotDeep@, stop:0.45 @slot@, stop:1 @slotHi@); }
QSlider::sub-page:horizontal { height: 8px; border-radius: 4px; border: 1px solid @amberDeep@;
  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @amber@, stop:1 @amberHi@); }
QSlider::handle:horizontal { width: 18px; height: 26px; margin: -10px -1px; image: url(@faderCap@); background: transparent; border: none; }
QTabBar::tab { letter-spacing: 1px; }
QFrame#strip, QFrame#strip[sel="true"] { background: transparent; border: none; padding: 0; }
QPushButton#stripName { background: qradialgradient(cx:0.5, cy:1.3, radius:1.1, fx:0.5, fy:1.3, stop:0 @ivoryHi@, stop:0.75 @lcdBg@, stop:1 @ivoryLo@);
  border: 2px solid @bezel@; border-radius: 5px; min-height: 24px; max-height: 24px; margin: 4px 6px 0 6px; }
QPushButton#stripName:hover { background: @ivoryHi@; }
QStatusBar { background: @panelHi@; border-top: 1px solid @rule@; }
QPushButton#mute:checked { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #e0563c, stop:0.962 #b8361f, stop:0.967 #7a2010, stop:1 #7a2010); color: #fff8ee; border-color: #7a2010; padding: 2px 12px 1px 12px; }
QToolButton#inspectorTab {
  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 @keyBot@, stop:0.6 @keyTop@, stop:0.82 @keyTop@, stop:0.86 @keyDeep@, stop:1 @keyDeep@); }
/* the tab on the left edge is a key on its side: lit across its face, its depth along the edge that stands out */
QToolButton#inspectorTab:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 @keyTop@, stop:0.6 #ffffff, stop:0.82 #ffffff, stop:0.86 @keyDeep@, stop:1 @keyDeep@); }
QToolButton#inspectorTab:pressed { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 @keyBot@, stop:0.92 @keyBot@, stop:0.95 @keyDeep@, stop:1 @keyDeep@); }
)");
	s += QStringLiteral(R"(
QSpinBox#readout, QComboBox#readout { background: @lcdBg@; color: @lcd@; font-family: "Courier Prime"; font-weight: 700; font-size: 14px;
  border: 1px solid @frame@; border-radius: 4px; padding: 0 6px; selection-background-color: @lcd@; selection-color: @lcdBg@; }
QLabel#engraved { color: @ink@; font-size: 9px; font-weight: 500; letter-spacing: 1.4px; }
QComboBox#readout QAbstractItemView { font-family: "Nachtzug Grotesk"; font-weight: 400; color: @ink@; background: transparent; }
QFrame#unit { background: @zoneU@; border: 1px solid @rule@; border-radius: 8px; }
QLabel#unitTitle { color: @ink@; font-size: 13px; font-weight: 400; letter-spacing: 0.5px; }
)");
	if (tactile)
		s += QStringLiteral(R"(
QFrame#unit { background: transparent; border: none; }
QSpinBox#readout, QComboBox#readout { background: qradialgradient(cx:0.5, cy:1.3, radius:1.1, fx:0.5, fy:1.3, stop:0 @ivoryHi@, stop:0.75 @lcdBg@, stop:1 @ivoryLo@); }
)");
	s += QStringLiteral(R"(
QPushButton#card { background: @well@; border: 1px solid @rule@; border-radius: 8px; padding: 0; min-height: 88px; max-height: 400px; text-align: left; }
QPushButton#card:hover { background: @hover@; }
QPushButton#card:checked { background: @wellHi@; border: 2px solid @amber@; padding: 0; }
QLabel#cardHead { color: @ink@; font-size: 15px; font-weight: 600; background: transparent; }
QLabel#cardText { color: @dim@; font-size: 13px; background: transparent; }
QPushButton#startCard { background: @field@; border: 1px solid @rule@; border-radius: 6px; padding: 0; min-height: 54px; max-height: 54px; text-align: left; }
QPushButton#startCard[small="true"] { min-height: 42px; max-height: 42px; }
QPushButton#startCard:hover { background: @hover@; }
QPushButton#startCard:pressed { background: @wellHi@; }
QPushButton#startCard:focus { border: 2px solid @amber@; }
QPushButton#startCard:disabled { background: transparent; border-color: @gridBar@; }
QLabel#startTitle { color: @ink@; font-size: 19px; font-weight: 600; }
QPushButton#sample { padding: 0 6px; font-weight: 400; }
QPushButton#sample[sampleOn="true"] { color: @amber@; font-weight: 600; }
)");
	// drop-down arrows in the ink and display colours
	auto arrow = [](const QColor& c) {
		return stylePicture(QStringLiteral("arrow-") + c.name().mid(1), QSize(10, 6), [&](QPainter& p) {
			p.setPen(QPen(c, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
			p.drawPolyline(QPolygonF({QPointF(1, 1), QPointF(5, 5), QPointF(9, 1)}));
		});
	};
	// a fader cap like the mixer's: dark moulded body, a light grip line across it, a soft shadow under it
	s.replace(QLatin1String("@faderCap@"),
	          stylePicture(QStringLiteral("fader-") + amber.name().mid(1), QSize(18, 26), [&](QPainter& p) {
		          p.setPen(Qt::NoPen);
		          p.setBrush(QColor(0, 0, 0, 56));
		          p.drawRoundedRect(QRectF(2, 4, 14, 21), 3, 3);
		          QLinearGradient body(0, 1.5, 0, 22.5);
		          body.setColorAt(0, QColor(0x57, 0x50, 0x4b));
		          body.setColorAt(0.5, QColor(0x2e, 0x2a, 0x27));
		          body.setColorAt(1, QColor(0x1a, 0x17, 0x16));
		          p.setPen(QPen(QColor(0x0d, 0x0b, 0x0a), 1));
		          p.setBrush(body);
		          p.drawRoundedRect(QRectF(1.5, 1.5, 15, 21), 3, 3);
		          p.setPen(QPen(QColor(255, 255, 255, 46), 1));
		          p.drawLine(QPointF(4, 3.2), QPointF(14, 3.2));
		          p.setPen(QPen(amber, 2, Qt::SolidLine, Qt::RoundCap));
		          p.drawLine(QPointF(9, 5), QPointF(9, 19));
	          }));
	// grips on the dividers: three small dots, so it is clear they can be dragged
	// the dividers' handle: a small raised pull with three grip lines; hot, the lines and its edge are amber
	auto grip = [&](bool across, bool hot) {
		const QColor edge = hot ? amber : rule.darker(115), lines = hot ? amber : dim;
		return stylePicture(QStringLiteral("pull-%1-%2-%3-%4")
		                        .arg(across ? "h" : "v", edge.name().mid(1), wellHi.name().mid(1), lines.name().mid(1)),
		                    across ? QSize(64, 14) : QSize(14, 64), [&](QPainter& p) {
			                    const QRectF body = across ? QRectF(1, 1, 62, 11) : QRectF(1, 1, 11, 62);
			                    QLinearGradient g(0, 0, across ? 0 : 14, across ? 14 : 0);
			                    g.setColorAt(0, wellHi.lighter(104));
			                    g.setColorAt(1, mix(wellHi, rule, 0.25));
			                    p.setPen(QPen(edge, 1));
			                    p.setBrush(g);
			                    p.drawRoundedRect(body, 5.5, 5.5);
			                    p.setPen(QPen(lines, 1.6, Qt::SolidLine, Qt::RoundCap));
			                    for (int i = -1; i <= 1; i++) {
				                    const double at = 32 + i * 6;
				                    if (across)
					                    p.drawLine(QPointF(at, 3.5), QPointF(at, 9.5));
				                    else
					                    p.drawLine(QPointF(3.5, at), QPointF(9.5, at));
			                    }
		                    });
	};
	s.replace(QLatin1String("@groupTop@"), QString::number(GROUP_TOP));
	{ // the type of the window: Nachtzug Grotesk throughout. For comparing pictures, NZ_UI_FONT=mixed sets the
		// names of sections, tabs and units in Nachtzug Antiqua, and NZ_UI_FONT=antiqua sets everything in it
		const QString which = qEnvironmentVariable("NZ_UI_FONT");
		const bool all = which == QLatin1String("antiqua"), mixed = which == QLatin1String("mixed");
		s.replace(QLatin1String("@uiFont@"),
		          all ? QStringLiteral("Nachtzug Antiqua") : QStringLiteral("Nachtzug Grotesk"));
		s.replace(QLatin1String("@uiSize@"), all ? QStringLiteral("14") : QStringLiteral("13"));
		if (all || mixed)
			s += QStringLiteral(R"(
QGroupBox { font-family: "Nachtzug Antiqua"; font-size: 12px; font-weight: 700; }
QLabel#unitTitle, QLabel#inspectorHead { font-family: "Nachtzug Antiqua"; font-size: 15px; }
QTabBar { font-family: "Nachtzug Antiqua"; font-size: 14px; font-weight: 700; }
)");
	}
	s.replace(QLatin1String("@gripHHot@"), grip(true, true));
	s.replace(QLatin1String("@gripVHot@"), grip(false, true));
	s.replace(QLatin1String("@gripH@"), grip(true, false));
	s.replace(QLatin1String("@gripV@"), grip(false, false));
	s.replace(QLatin1String("@arrowInk@"), arrow(ink));
	s.replace(QLatin1String("@arrowLcd@"), arrow(lcd));
	const bool darkKeys = key.lightness() < 128;
	const std::pair<const char*, QColor> map[] = {{"@accentInk@", accentInk},
	                                              {"@ink@", ink},
	                                              {"@panel@", panel},
	                                              {"@wellHi@", wellHi},
	                                              {"@well@", well},
	                                              {"@hover@", hover},
	                                              {"@rule@", rule},
	                                              {"@gridBar@", gridBar},
	                                              {"@dim@", dim},
	                                              {"@lcdBg@", lcdBg},
	                                              {"@lcdText@", lcdBg.lightness() > 128 ? ink : lcd.lighter(125)},
	                                              {"@lcd@", lcd},
	                                              {"@keyInk@", keyInk},
	                                              {"@keyHover@", darkKeys ? key.lighter(150) : key.lighter(108)},
	                                              {"@keyPress@", darkKeys ? key.lighter(190) : key.darker(118)},
	                                              {"@keyTop@", QColor(0xf3, 0xee, 0xe4)},
	                                              {"@keyBot@", QColor(0xe2, 0xda, 0xcb)},
	                                              {"@keyEdge@", rule},
	                                              {"@keyDeep@", rule.darker(125)},
	                                              {"@amberHi@", amber.lighter(118)},
	                                              {"@amberDeep@", amber.darker(150)},
	                                              {"@panelHi@", panel.lighter(103)},
	                                              {"@zoneU@", panel.darker(tactile ? 102 : 100)},
	                                              {"@bezel@", rule},
	                                              {"@frame@", rule.darker(125)},
	                                              {"@zone@", panel.darker(102)},
	                                              {"@field@", canvas.lighter(102)},
	                                              {"@fieldShade@", mix(canvas.lighter(102), rule, 0.55)},
	                                              {"@slotDeep@", gridBar.darker(118)},
	                                              {"@slotHi@", gridBar.lighter(110)},
	                                              {"@slot@", gridBar},
	                                              {"@ivoryHi@", lcdBg.lighter(108)},
	                                              {"@ivoryLo@", lcdBg.darker(106)},
	                                              {"@key@", key},
	                                              {"@amber@", amber},
	                                              {"@canvas@", canvas}};
	for (const auto& [token, colour] : map)
		s.replace(QLatin1String(token), colour.name());
	return s;
}

} // namespace theme
