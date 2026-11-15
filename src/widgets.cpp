#include "widgets.h"
#include "model.h"
#include "app.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QPointer>
#include <QSettings>
#include <QFontDatabase>
#include <QFrame>
#include <QGroupBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace theme {

struct Def {
    const char* name;
    unsigned panel, well, wellHi, rule, ink, dim, canvas, lane, laneSel, grid, gridBar;
    unsigned accent, accentInk, second, cap, capEdge, capInk, key, keyInk, hover, lcdBg, lcd;
    unsigned tracks[5];   // bass, sequence, pad, lead, drums
    bool tactile = false; // keys with depth, ribbed knobs, LCD
};
static const Def DEFS[] = {
    // the instrument look: warm enamel panel, ribbed black knobs, a warm LCD
    {QT_TRANSLATE_NOOP("Model", "Bakelite"), 0xebe5d8, 0xe1d9c9, 0xe9e2d4, 0xb1a391, 0x2b2623, 0x6e6054, 0xf6f2ea, 0xeee8dc, 0xe4dccc, 0xe0d7c7, 0xbfb2a0,
     0xd8642a, 0xfff8ee, 0xd98a2b, 0x2a2624, 0x141211, 0xe8743a, 0x2f2a27, 0xf3ece0, 0xdcd3c3, 0xf0e1ba, 0x3b2a1b,
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
static int cur = 0;

int count() { return int(sizeof(DEFS) / sizeof(DEFS[0])); }
QString name(int i) { return QString::fromLatin1(DEFS[std::clamp(i, 0, count() - 1)].name); }
int current() { return cur; }

void apply(int index) {
    cur = std::clamp(index, 0, count() - 1);
    const Def& d = DEFS[cur];
    auto c = [](unsigned v) { return QColor(int((v >> 16) & 0xff), int((v >> 8) & 0xff), int(v & 0xff)); };
    panel = c(d.panel); well = c(d.well); wellHi = c(d.wellHi); rule = c(d.rule); ink = c(d.ink); dim = c(d.dim);
    canvas = c(d.canvas); lane = c(d.lane); laneSel = c(d.laneSel); grid = c(d.grid); gridBar = c(d.gridBar);
    amber = c(d.accent); accentInk = c(d.accentInk); ice = c(d.second); cap = c(d.cap); capEdge = c(d.capEdge); capInk = c(d.capInk);
    key = c(d.key); keyInk = c(d.keyInk); hover = c(d.hover);
    lcdBg = c(d.lcdBg); lcd = c(d.lcd);
    clipInk = QColor(0x22, 0x1c, 0x18);
    tactile = d.tactile;
    for (int i = 0; i < 5; i++) tracks[i] = c(d.tracks[i]);
}

QColor trackColor(int role) { return tracks[std::clamp(role, 0, 4)]; }

QPixmap logo(int size) {
    // the enamel sign; at 24 px and below its simpler version, without the rule, so the tz still reads
    static const QImage picture(QStringLiteral(":/logo.png")), small(QStringLiteral(":/logo-small.png"));
    const QImage& source = size <= 24 && !small.isNull() ? small : picture;
    if (!source.isNull()) {
        QPixmap pm = QPixmap::fromImage(source.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        return pm;
    }
    QPixmap pm(size, size);   // fallback: the first, drawn version of the mark
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const double s = size / 64.0;
    p.scale(s, s);
    const QColor night(0x2b, 0x26, 0x23), moon(0xed, 0x9d, 0x24), cream(0xec, 0xe8, 0xdf), rust(0xa9, 0x27, 0x0a);
    p.setPen(Qt::NoPen);
    p.setBrush(night); p.drawRoundedRect(QRectF(2, 2, 60, 60), 14, 14);
    p.setBrush(moon); p.drawEllipse(QPointF(45, 19), 9, 9);
    QPainterPath body;
    body.moveTo(8, 46); body.lineTo(8, 34); body.quadTo(8, 30, 12, 30); body.lineTo(42, 30); body.quadTo(54, 31, 57, 43); body.lineTo(57, 46); body.closeSubpath();
    p.setBrush(cream); p.drawPath(body);
    p.setBrush(rust); p.drawRoundedRect(QRectF(6, 49, 52, 3.5), 1.75, 1.75);
    return pm;
}

void loadFonts() {
    for (const char* f : {":/fonts/nachtzug/NachtzugGrotesk-Regular.ttf", ":/fonts/nachtzug/NachtzugGrotesk-Medium.ttf", ":/fonts/nachtzug/NachtzugGrotesk-SemiBold.ttf", ":/fonts/nachtzug/NachtzugAntiqua-Regular.ttf", ":/fonts/nachtzug/NachtzugAntiqua-Bold.ttf", ":/fonts/CourierPrime-Regular.ttf", ":/fonts/CourierPrime-Bold.ttf"})
        QFontDatabase::addApplicationFont(QString::fromLatin1(f));
}

QFont lcdFont(int pixelSize, bool bold) {
    QFont f(QStringLiteral("Courier Prime"));
    f.setStyleHint(QFont::Monospace);
    f.setPixelSize(pixelSize);
    f.setBold(bold);
    return f;
}

void drawBlock(QPainter& p, const QRectF& r, const QColor& c, double radius, bool raised) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    if (!tactile) { p.setPen(Qt::NoPen); p.setBrush(c); p.drawRoundedRect(r, radius, radius); p.restore(); return; }
    if (raised && r.height() > 6) { // soft shadow under the block
        p.setPen(Qt::NoPen);
        for (int i = 0; i < 3; i++) { p.setBrush(QColor(0, 0, 0, 26 - i * 8)); p.drawRoundedRect(r.translated(0, 1.0 + i).adjusted(-i * 0.5, 0, i * 0.5, 0), radius + i, radius + i); }
    }
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0, c.lighter(106)); g.setColorAt(1, c.darker(108));
    p.setPen(QPen(c.darker(140), 1)); p.setBrush(g);
    p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    if (r.height() > 8) { // a thin light edge along the top, like a moulded part
        p.setPen(QPen(QColor(255, 255, 255, 70), 1));
        p.drawLine(QPointF(r.left() + radius, r.top() + 1.5), QPointF(r.right() - radius, r.top() + 1.5));
    }
    p.restore();
}

void drawKey(QPainter& p, const QRectF& r, const QString& text, bool on, const QColor& onColour) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    if (!tactile) {
        p.setPen(on ? QPen(Qt::NoPen) : QPen(rule, 1)); p.setBrush(on ? onColour : panel); p.drawRoundedRect(r, 3, 3);
        p.setPen(!on ? ink : onColour == key ? keyInk : accentInk); p.drawText(r, Qt::AlignCenter, text);
        p.restore(); return;
    }
    const double depth = on ? 1.0 : 2.5;
    p.setPen(Qt::NoPen); p.setBrush(on ? onColour.darker(150) : rule.darker(120));
    p.drawRoundedRect(r.adjusted(0, 1, 0, 0), 3.5, 3.5);                       // the side of the key
    const QRectF face = r.adjusted(0, on ? 1.5 : 0, 0, -depth);
    QLinearGradient g(face.topLeft(), face.bottomLeft());
    if (on) { g.setColorAt(0, onColour.lighter(118)); g.setColorAt(1, onColour); }
    else { g.setColorAt(0, QColor(0xf5, 0xf0, 0xe7)); g.setColorAt(1, QColor(0xe2, 0xda, 0xcb)); }
    p.setPen(QPen(on ? onColour.darker(140) : rule, 0.8)); p.setBrush(g);
    p.drawRoundedRect(face, 3, 3);
    p.setPen(on ? (onColour == key ? keyInk : accentInk) : ink);
    p.drawText(face, Qt::AlignCenter, text);
    p.restore();
}

void drawSlider(QPainter& p, const QRectF& r, float value, const QColor& fill) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const double rad = r.height() / 2;
    if (!tactile) {
        p.setPen(Qt::NoPen); p.setBrush(slot()); p.drawRoundedRect(r, rad, rad);
        p.setBrush(fill); p.drawRoundedRect(QRectF(r.left(), r.top(), r.width() * double(value), r.height()), rad, rad);
        p.restore(); return;
    }
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0, gridBar.darker(118)); g.setColorAt(0.45, gridBar); g.setColorAt(1, gridBar.lighter(110));
    p.setPen(QPen(rule, 1)); p.setBrush(g); p.drawRoundedRect(r, rad, rad);
    const QRectF filled(r.left(), r.top(), std::max(r.height(), r.width() * double(value)), r.height());
    QLinearGradient fg(filled.topLeft(), filled.bottomLeft());
    fg.setColorAt(0, fill); fg.setColorAt(1, fill.lighter(115));
    p.setPen(QPen(fill.darker(140), 1)); p.setBrush(fg); p.drawRoundedRect(filled, rad, rad);
    // the cap
    const double x = r.left() + r.width() * double(value);
    const QRectF cap(x - 6, r.center().y() - 8, 12, 16);
    p.setPen(Qt::NoPen); p.setBrush(QColor(0, 0, 0, 50)); p.drawRoundedRect(cap.translated(0, 1.5), 2.5, 2.5);
    QLinearGradient cg(cap.topLeft(), cap.bottomLeft());
    cg.setColorAt(0, QColor(0x57, 0x50, 0x4b)); cg.setColorAt(0.5, QColor(0x2e, 0x2a, 0x27)); cg.setColorAt(1, QColor(0x1a, 0x17, 0x16));
    p.setPen(QPen(QColor(0x0d, 0x0b, 0x0a), 1)); p.setBrush(cg); p.drawRoundedRect(cap, 2.5, 2.5);
    p.setPen(QPen(amber, 1.6, Qt::SolidLine, Qt::RoundCap)); p.drawLine(QPointF(x, cap.top() + 3), QPointF(x, cap.bottom() - 3));
    p.restore();
}

// Little symbols, drawn in a 16 x 16 box: bass a low wave, sequence a row of steps, pad a stack of held notes,
// lead a single rising line of notes, drums a drum seen from the side.
void drawRoleIcon(QPainter& p, const QRectF& r, int role, const QColor& c) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.translate(r.topLeft());
    p.scale(r.width() / 16.0, r.height() / 16.0);
    QPen pen(c, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen); p.setBrush(Qt::NoBrush);
    switch (role) {
    case R_BASS: { QPainterPath w; w.moveTo(1, 9); w.cubicTo(4, 2, 6, 2, 8, 9); w.cubicTo(10, 16, 12, 16, 15, 9); p.drawPath(w); break; }
    case R_SEQ: { p.setPen(Qt::NoPen); p.setBrush(c); const double hs[4] = {6, 10, 4, 8}; for (int i = 0; i < 4; i++) p.drawRoundedRect(QRectF(1 + i * 3.8, 14 - hs[i], 2.8, hs[i]), 1, 1); break; }
    case R_PAD: { for (int i = 0; i < 3; i++) p.drawLine(QPointF(2, 4 + i * 4), QPointF(14, 4 + i * 4)); break; }
    case R_LEAD: { p.setPen(Qt::NoPen); p.setBrush(c); p.drawEllipse(QPointF(5, 12), 2.6, 2.1); p.setPen(pen); p.drawLine(QPointF(7.4, 12), QPointF(7.4, 2.5)); p.drawLine(QPointF(7.4, 2.5), QPointF(12, 5)); break; }
    default: { p.drawEllipse(QRectF(2, 3, 12, 4)); p.drawLine(QPointF(2, 5), QPointF(2, 12)); p.drawLine(QPointF(14, 5), QPointF(14, 12)); p.drawArc(QRectF(2, 9.5, 12, 4.5), 180 * 16, 180 * 16); break; }
    }
    p.restore();
}

QIcon roleIcon(int role, const QColor& c) {
    QPixmap pm(32, 32); pm.setDevicePixelRatio(2.0); pm.fill(Qt::transparent);
    QPainter p(&pm); drawRoleIcon(p, QRectF(0, 0, 16, 16), role, c);
    return QIcon(pm);
}

void drawGlass(QPainter& p, const QRectF& face, double radius) {
    p.save();
    p.setPen(Qt::NoPen);
    QLinearGradient sh(0, face.top(), 0, face.top() + 8); sh.setColorAt(0, QColor(0, 0, 0, 70)); sh.setColorAt(1, Qt::transparent);
    p.setBrush(sh); p.drawRoundedRect(QRectF(face.left(), face.top(), face.width(), 8), radius, radius);
    QLinearGradient sl(face.left(), 0, face.left() + 6, 0); sl.setColorAt(0, QColor(0, 0, 0, 45)); sl.setColorAt(1, Qt::transparent);
    p.setBrush(sl); p.drawRect(QRectF(face.left(), face.top(), 6, face.height()));
    QLinearGradient refl(face.topLeft(), face.bottomRight());
    refl.setColorAt(0, QColor(255, 255, 255, 80)); refl.setColorAt(0.38, QColor(255, 255, 255, 14)); refl.setColorAt(0.39, Qt::transparent); refl.setColorAt(1, Qt::transparent);
    p.setBrush(refl); p.drawRoundedRect(face, radius, radius);
    QPolygonF streak;
    streak << QPointF(face.left() + face.width() * 0.52, face.top()) << QPointF(face.left() + face.width() * 0.60, face.top())
           << QPointF(face.left() + face.width() * 0.28, face.bottom()) << QPointF(face.left() + face.width() * 0.22, face.bottom());
    p.setBrush(QColor(255, 255, 255, 26)); p.drawPolygon(streak);
    p.setPen(QPen(QColor(255, 255, 255, 140), 1)); p.setBrush(Qt::NoBrush); p.drawRoundedRect(face.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    p.restore();
}

void drawSunken(QPainter& p, const QRectF& r) {
    if (!tactile) return;
    p.save();
    QLinearGradient top(0, r.top(), 0, r.top() + 7);
    top.setColorAt(0, QColor(0, 0, 0, 48)); top.setColorAt(1, Qt::transparent);
    p.fillRect(QRectF(r.left(), r.top(), r.width(), 7), top);
    QLinearGradient left(r.left(), 0, r.left() + 5, 0);
    left.setColorAt(0, QColor(0, 0, 0, 30)); left.setColorAt(1, Qt::transparent);
    p.fillRect(QRectF(r.left(), r.top(), 5, r.height()), left);
    p.restore();
}

static QColor mix(const QColor& a, const QColor& b, double t) {
    return QColor::fromRgbF(float(a.redF() * (1 - t) + b.redF() * t), float(a.greenF() * (1 - t) + b.greenF() * t), float(a.blueF() * (1 - t) + b.blueF() * t));
}

static void drawScrew(QPainter& p, const QPointF& c, double angle) {
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 170)); p.drawEllipse(c + QPointF(0, 0.8), 3.6, 3.6);   // light catching the lower rim of the hole
    QRadialGradient g(c - QPointF(1.2, 1.2), 4.5);
    g.setColorAt(0, mix(panel, Qt::white, 0.25)); g.setColorAt(1, mix(panel, rule, 1.0).darker(135));
    p.setBrush(g); p.setPen(QPen(QColor(0, 0, 0, 90), 0.6)); p.drawEllipse(c, 3.5, 3.5);
    p.save(); p.translate(c); p.rotate(angle);
    p.setPen(QPen(QColor(60, 45, 30, 150), 1.0, Qt::SolidLine, Qt::RoundCap)); p.drawLine(QPointF(-2.4, 0), QPointF(2.4, 0));
    p.restore();
}

void drawWell(QPainter& p, const QRectF& r0, bool selected) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = r0.adjusted(0.5, 1.5, -0.5, -1.5);
    const double rad = 8;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 200)); p.drawRoundedRect(r.translated(0, 1), rad, rad);   // the light lower lip
    p.setBrush(QColor(110, 90, 65, 70)); p.drawRoundedRect(r.translated(0, -1), rad, rad);     // the shadowed upper lip
    QLinearGradient floor(0, r.top(), 0, r.bottom());
    floor.setColorAt(0, mix(panel, rule, 0.42)); floor.setColorAt(0.6, mix(panel, rule, 0.24)); floor.setColorAt(1, mix(panel, rule, 0.15));
    p.setBrush(floor); p.setPen(QPen(rule.darker(128), 1)); p.drawRoundedRect(r, rad, rad);
    QPainterPath inside; inside.addRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), rad, rad);
    p.setClipPath(inside);
    QLinearGradient lip(0, r.top(), 0, r.top() + 10);   // the shadow the top edge throws onto the floor
    lip.setColorAt(0, QColor(70, 45, 20, 100)); lip.setColorAt(1, Qt::transparent);
    p.fillRect(QRectF(r.left(), r.top(), r.width(), 10), lip);
    QLinearGradient side(r.left(), 0, r.left() + 5, 0); side.setColorAt(0, QColor(70, 45, 20, 40)); side.setColorAt(1, Qt::transparent);
    p.fillRect(QRectF(r.left(), r.top(), 5, r.height()), side);
    p.setPen(QPen(QColor(255, 255, 255, 140), 1.2)); p.drawLine(QPointF(r.left() + rad, r.bottom() - 1), QPointF(r.right() - rad, r.bottom() - 1));
    p.setClipping(false);
    // a screw in each corner, each turned its own way, as if put in by hand
    const double in = 9.5;
    const QPointF corner[4] = {{r.left() + in, r.top() + in}, {r.right() - in, r.top() + in}, {r.left() + in, r.bottom() - in}, {r.right() - in, r.bottom() - in}};
    const double turn[4] = {30, -50, 75, 10};
    for (int i = 0; i < 4; i++) drawScrew(p, corner[i], turn[i] + (int(r.width()) % 7) * 9);
    if (selected) { p.setBrush(Qt::NoBrush); p.setPen(QPen(amber, 2)); p.drawRoundedRect(r.adjusted(-2, -2, 2, 2), rad + 2, rad + 2); }
    p.restore();
}

void drawIvory(QPainter& p, const QRectF& glass, double radius) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    QRadialGradient lit(QPointF(glass.center().x(), glass.bottom() + glass.height() * 0.25), std::max(glass.width(), glass.height()) * 0.75);
    lit.setColorAt(0, lcdBg.lighter(108)); lit.setColorAt(0.7, lcdBg); lit.setColorAt(1, lcdBg.darker(106));
    p.setPen(Qt::NoPen); p.setBrush(lit); p.drawRoundedRect(glass, radius, radius);
    p.setBrush(Qt::NoBrush); p.setPen(QPen(rule.darker(150), 2)); p.drawRoundedRect(glass.adjusted(1, 1, -1, -1), radius, radius);
    p.restore();
}

namespace {
class WellPainter : public QObject {
public:
    using QObject::QObject;
    bool eventFilter(QObject* o, QEvent* e) override {
        if (e->type() != QEvent::Paint || !tactile || !o->isWidgetType()) return false;
        auto* w = static_cast<QWidget*>(o);
        if (qobject_cast<QGroupBox*>(w)) {
            if (w->property("folded").toBool()) return false;   // folded shut: only its name and summary
            QPainter p(w); drawWell(p, QRectF(w->rect()).adjusted(0, GROUP_TOP, 0, 0));
        } else if (qobject_cast<QFrame*>(w)) {
            const QString& n = w->objectName();
            if (n == QLatin1String("unit") || n == QLatin1String("strip")) { QPainter p(w); drawWell(p, QRectF(w->rect()), w->property("sel").toBool()); }
        }
        return false;
    }
};
}

// ---- folding sections
namespace {
class Folder : public QObject {
public:
    Folder(QGroupBox* g, QString key, std::function<QString()> summary) : QObject(g), g_(g), key_(std::move(key)), summary_(std::move(summary)) {
        base_ = g->title();
        setObjectName(QStringLiteral("nzFolder"));
        g->installEventFilter(this);
        g->setMouseTracking(true);
    }
    bool folded() const { return folded_; }
    void set(bool on) {
        if (on == folded_) return;
        folded_ = on;
        if (on) {
            hidden_.clear();
            for (QObject* o : g_->children())
                if (auto* w = qobject_cast<QWidget*>(o); w && !w->isWindow() && w->isVisibleTo(g_)) { hidden_.push_back(w); w->hide(); }
            const QFontMetrics fm(g_->font());
            const int w = fm.horizontalAdvance(base_) + 3 * base_.size() + 34 + std::min(220, fm.horizontalAdvance(summary()) + 24);
            g_->setFixedSize(w, GROUP_TOP + 4);
        } else {
            for (QWidget* w : hidden_) if (w) w->show();
            hidden_.clear();
            g_->setMinimumSize(0, 0); g_->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        }
        g_->setProperty("folded", on);
        g_->setTitle((on ? QStringLiteral("▸ ") : QStringLiteral("▾ ")) + base_);
        if (!key_.isEmpty()) QSettings().setValue(QStringLiteral("fold/") + key_, on);
        g_->updateGeometry();
        if (QWidget* p = g_->parentWidget()) { if (p->layout()) p->layout()->invalidate(); p->updateGeometry(); }
    }
    QString summary() const {
        if (summary_) return summary_();
        QStringList parts;
        for (QComboBox* c : g_->findChildren<QComboBox*>()) if (c->count()) parts << c->currentText().trimmed();
        if (parts.isEmpty())
            for (Knob* k : g_->findChildren<Knob*>()) { if (parts.size() == 3) break; parts << QStringLiteral("%1 %2").arg(k->name(), k->valueText()); }
        return parts.join(QStringLiteral(" · "));
    }
    bool eventFilter(QObject*, QEvent* e) override {
        switch (e->type()) {
        case QEvent::MouseButtonPress: {
            auto* m = static_cast<QMouseEvent*>(e);
            if (m->button() == Qt::LeftButton && m->position().y() < GROUP_TOP + (folded_ ? 4 : 0)) { set(!folded_); return true; }
            break;
        }
        case QEvent::MouseMove: {
            auto* m = static_cast<QMouseEvent*>(e);
            const bool over = m->position().y() < GROUP_TOP + (folded_ ? 4 : 0);
            if (over != hot_) { hot_ = over; if (over) g_->setCursor(Qt::PointingHandCursor); else g_->unsetCursor(); }
            break;
        }
        case QEvent::Leave: if (hot_) { hot_ = false; g_->unsetCursor(); } break;
        case QEvent::Paint:
            if (folded_) {   // the summary, after the name
                QPainter p(g_);
                QFont f = g_->font(); f.setPixelSize(10); p.setFont(f); p.setPen(dim);
                const QFontMetrics fm(g_->font());
                const int x0 = fm.horizontalAdvance(g_->title()) + 3 * g_->title().size() + 24;
                p.drawText(QRect(x0, 0, g_->width() - x0 - 4, GROUP_TOP + 2), Qt::AlignVCenter | Qt::AlignRight, QFontMetrics(f).elidedText(summary(), Qt::ElideRight, g_->width() - x0 - 4));
            }
            break;
        default: break;
        }
        return false;
    }
private:
    QGroupBox* g_;
    QString key_, base_;
    std::function<QString()> summary_;
    std::vector<QPointer<QWidget>> hidden_;
    bool folded_ = false, hot_ = false;
};
}

void makeFoldable(QGroupBox* g, const QString& key, std::function<QString()> summary) {
    if (!g || g->findChild<QObject*>(QStringLiteral("nzFolder"), Qt::FindDirectChildrenOnly)) return;
    auto* f = new Folder(g, key, std::move(summary));
    g->setTitle(QStringLiteral("▾ ") + g->title());
    g->setToolTip(g->toolTip().isEmpty() ? QCoreApplication::translate("Model", "Click the name to fold this section shut or open it again") : g->toolTip());
    if (!key.isEmpty() && QSettings().value(QStringLiteral("fold/") + key, false).toBool())
        QTimer::singleShot(0, g, [f] { f->set(true); });   // once the section is filled
}

void setFolded(QGroupBox* g, bool folded) {
    if (auto* f = g ? static_cast<Folder*>(g->findChild<QObject*>(QStringLiteral("nzFolder"), Qt::FindDirectChildrenOnly)) : nullptr) f->set(folded);
}

void installWells() {
    static bool done = false;
    if (done || !qApp) return;
    done = true;
    qApp->installEventFilter(new WellPainter(qApp));
}

QIcon ledIcon(const QColor& lit) {
    auto draw = [&](bool on) {
        QPixmap pm(28, 28);
        pm.setDevicePixelRatio(2.0);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        const QPointF c(7, 7);
        if (on) { QRadialGradient halo(c, 7); halo.setColorAt(0, QColor(lit.red(), lit.green(), lit.blue(), 150)); halo.setColorAt(1, Qt::transparent); p.setPen(Qt::NoPen); p.setBrush(halo); p.drawEllipse(c, 7, 7); }
        QRadialGradient g(QPointF(6, 5.5), 5);
        const QColor base = on ? lit : QColor(0x6a, 0x5e, 0x52);
        g.setColorAt(0, on ? base.lighter(160) : base.lighter(120)); g.setColorAt(1, on ? base : base.darker(150));
        p.setPen(QPen(QColor(0, 0, 0, 120), 0.8)); p.setBrush(g);
        p.drawEllipse(c, 3.6, 3.6);
        p.setPen(Qt::NoPen); p.setBrush(QColor(255, 255, 255, on ? 200 : 90)); p.drawEllipse(QPointF(5.9, 5.8), 1.1, 1.1);
        return pm;
    };
    QIcon icon;
    icon.addPixmap(draw(false), QIcon::Normal, QIcon::Off);
    icon.addPixmap(draw(true), QIcon::Normal, QIcon::On);
    icon.addPixmap(draw(false), QIcon::Disabled, QIcon::Off);
    return icon;
}

QIcon transportIcon(const QString& kind) {
    auto draw = [&](const QColor& c) {
        QPixmap pm(40, 40);
        pm.setDevicePixelRatio(2.0);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen); p.setBrush(c);
        if (kind == "play") { QPolygonF t; t << QPointF(6, 3) << QPointF(16.5, 10) << QPointF(6, 17); p.drawPolygon(t); }
        else if (kind == "stop") p.drawRoundedRect(QRectF(4.5, 4.5, 11, 11), 1.5, 1.5);
        else if (kind == "start") { p.drawRect(QRectF(4, 4, 2.4, 12)); QPolygonF t; t << QPointF(16, 4) << QPointF(7.5, 10) << QPointF(16, 16); p.drawPolygon(t); }
        else if (kind == "undo" || kind == "redo") {   // a curved arrow turning back (or forward)
            if (kind == "redo") { p.translate(20, 0); p.scale(-1, 1); }
            QPainterPath arc; arc.moveTo(6.5, 8.5); arc.cubicTo(9, 5.5, 15.5, 5.5, 16, 10.5); arc.cubicTo(16.3, 14, 13.5, 16, 10.5, 16);
            p.setBrush(Qt::NoBrush); p.setPen(QPen(c, 2.1, Qt::SolidLine, Qt::RoundCap)); p.drawPath(arc);
            p.setPen(Qt::NoPen); p.setBrush(c);
            QPolygonF t; t << QPointF(3, 5.5) << QPointF(9.5, 5) << QPointF(5, 11.5); p.drawPolygon(t);
        } else if (kind == "export") {   // an arrow going down into a tray
            p.setBrush(Qt::NoBrush); p.setPen(QPen(c, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawLine(QPointF(10, 3), QPointF(10, 11.5));
            p.drawPolyline(QPolygonF() << QPointF(4, 12) << QPointF(4, 16.5) << QPointF(16, 16.5) << QPointF(16, 12));
            p.setPen(Qt::NoPen); p.setBrush(c);
            QPolygonF t; t << QPointF(5.8, 8.5) << QPointF(14.2, 8.5) << QPointF(10, 13.2); p.drawPolygon(t);
        } else { p.setBrush(QColor(0xc4, 0x36, 0x1c)); p.drawEllipse(QPointF(10, 10), 6, 6); }
        return pm;
    };
    QIcon icon;
    icon.addPixmap(draw(ink), QIcon::Normal, QIcon::Off);
    icon.addPixmap(draw(kind == "rec" ? QColor(0xff, 0xf3, 0xe6) : accentInk), QIcon::Normal, QIcon::On);
    return icon;
}

QIcon appIcon() {
    QIcon icon;
    for (int sz : {16, 24, 32, 48, 64, 128, 256}) icon.addPixmap(logo(sz));
    return icon;
}

QPalette palette() {
    QPalette pal;
    pal.setColor(QPalette::Window, panel); pal.setColor(QPalette::WindowText, ink);
    pal.setColor(QPalette::Base, well); pal.setColor(QPalette::AlternateBase, wellHi);
    pal.setColor(QPalette::Text, ink); pal.setColor(QPalette::Button, key); pal.setColor(QPalette::ButtonText, keyInk);
    pal.setColor(QPalette::Highlight, amber); pal.setColor(QPalette::HighlightedText, accentInk);
    pal.setColor(QPalette::ToolTipBase, lcdBg); pal.setColor(QPalette::ToolTipText, lcd);
    return pal;
}

QString styleSheet() {
    QString s = QStringLiteral(R"(
QWidget { color: @ink@; font-size: 13px; }
QMainWindow, QDialog { background: @panel@; }
QMenuBar { background: @well@; }
QMenuBar::item:selected, QMenu::item:selected { background: @hover@; }
QMenu { background: @well@; border: 1px solid @rule@; }
QMenu::separator { height: 1px; background: @gridBar@; margin: 4px 8px; }
QStatusBar { background: @well@; color: @dim@; }
QToolTip { background: @lcdBg@; color: @lcdText@; border: 1px solid @lcd@; padding: 4px; }
QLabel#dim { color: @dim@; }
QLabel#cap { color: @dim@; font-size: 13px; }
QToolButton[transport="true"] { min-height: 30px; max-height: 30px; padding: 0 6px; }
QToolButton#exportKey { min-height: 62px; max-height: 62px; padding: 4px 4px 2px 4px; }
QPushButton#tap { min-height: 26px; max-height: 26px; padding: 0 10px; font-weight: 600; }
QToolButton#help { min-height: 16px; max-height: 16px; min-width: 20px; max-width: 20px; padding: 0; border-radius: 10px; font-weight: 700; font-size: 11px; }
QPushButton#segL { border-top-right-radius: 0; border-bottom-right-radius: 0; margin-right: 0; }
QPushButton#segR { border-top-left-radius: 0; border-bottom-left-radius: 0; border-left: none; margin-left: 0; }
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
QPushButton#primary:pressed, QToolButton#primary:pressed { background: @keyPress@; }
QToolButton#primary:checked { background: @amber@; color: @accentInk@; border-color: @amber@; }
QToolButton#rec:checked { background: @lcd@; color: @accentInk@; border-color: @lcd@; }
QWidget#sep { background: @rule@; }
QWidget#topbar { border-bottom: 1px solid @rule@; }
QToolButton[big="true"] { min-height: 32px; max-height: 32px; font-size: 14px; padding: 0 14px; }
QLabel#plate { color: @ink@; }
QFrame#strip { background: @well@; border: 1px solid @gridBar@; border-radius: 6px; }
QFrame#strip[sel="true"] { border: 2px solid @amber@; }
QPushButton#stripName { background: transparent; border: none; border-radius: 0; font-weight: 700; }
QPushButton#stripName:hover { background: @hover@; }
QLabel#chip { font-weight: 700; letter-spacing: 1px; padding: 0 14px 0 10px; }
QTextBrowser { background: @canvas@; border: 1px solid @rule@; border-radius: 6px; padding: 8px; }
QToolButton::menu-indicator { image: none; }
QComboBox, QSpinBox { background: @well@; border: 1px solid @rule@; border-radius: 4px; padding: 0 8px; min-height: 28px; max-height: 28px; }
QSpinBox::up-button, QSpinBox::down-button { width: 0; height: 0; border: none; }
QComboBox:disabled { color: @dim@; }
QComboBox QAbstractItemView { background: @well@; selection-background-color: @hover@; border: 1px solid @rule@; }
QComboBox::drop-down { border: none; width: 16px; }
QComboBox::down-arrow { image: url(@arrowInk@); width: 10px; height: 6px; margin-right: 6px; }
QCheckBox { spacing: 6px; }
QTabWidget::pane { border: none; }
QTabBar::tab { background: @well@; color: @dim@; padding: 7px 16px; margin-right: 2px; border-top-left-radius: 4px; border-top-right-radius: 4px; font-weight: 600; }
QTabBar::tab:selected { background: @canvas@; color: @ink@; }
QScrollBar:horizontal { height: 12px; background: @canvas@; }
QScrollBar:vertical { width: 12px; background: @canvas@; }
QScrollBar::handle { background: @gridBar@; border-radius: 5px; min-width: 24px; min-height: 24px; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollArea { border: none; background: @panel@; }
QSplitter::handle:vertical { background: transparent; border-top: 1px solid @rule@; height: 10px; image: url(@gripH@); }
QSplitter::handle:horizontal { background: transparent; border-left: 1px solid @rule@; width: 10px; image: url(@gripV@); }
QSplitter::handle:hover { background: @hover@; }
)");
    if (tactile) s += QStringLiteral(R"(
QWidget { font-family: "@uiFont@"; font-size: @uiSize@px; }
QMainWindow, QDialog { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @panelHi@, stop:1 @panel@); }
QMenuBar { background: @panelHi@; border-bottom: 1px solid @rule@; }
QGroupBox { background: transparent; border: none; margin-top: @groupTop@px; padding: 5px 8px 3px 8px; }
QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 10px; top: 1px; padding: 0; color: @ink@; font-size: 10px; font-weight: 500; letter-spacing: 2.5px; }
/* keys: the dark lip under each key is the bottom band of its own gradient, so it follows the key's rounded corners
   exactly (a thicker bottom border would join the corners badly). Pressed or lit keys show a thinner lip. */
QPushButton, QToolButton { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @keyTop@, stop:0.895 @keyBot@, stop:0.900 @keyDeep@, stop:1 @keyDeep@);
  color: @ink@; border: 1px solid @keyEdge@; border-radius: 4px;
  padding: 0 12px 3px 12px; min-height: 25px; max-height: 25px; font-weight: 500; letter-spacing: 0.5px; }
QPushButton:hover, QToolButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:0.895 @keyTop@, stop:0.900 @keyDeep@, stop:1 @keyDeep@); }
QPushButton:pressed, QToolButton:pressed { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @keyBot@, stop:0.962 @keyBot@, stop:0.967 @keyDeep@, stop:1 @keyDeep@); padding: 2px 12px 1px 12px; }
QPushButton:checked, QToolButton:checked { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @amberHi@, stop:0.962 @amber@, stop:0.967 @amberDeep@, stop:1 @amberDeep@);
  color: @accentInk@; border-color: @amberDeep@; padding: 2px 12px 1px 12px; }
QPushButton:disabled, QToolButton:disabled { background: transparent; color: @rule@; border-color: @gridBar@; }
QPushButton#primary, QToolButton#primary { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @keyHover@, stop:0.895 @key@, stop:0.900 #120f0d, stop:1 #120f0d); color: @keyInk@; border-color: #000000; }
QPushButton#primary:hover, QToolButton#primary:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @keyHover@, stop:0.895 @keyHover@, stop:0.900 #120f0d, stop:1 #120f0d); }
QPushButton#primary:pressed, QToolButton#primary:pressed { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @key@, stop:0.962 @key@, stop:0.967 #120f0d, stop:1 #120f0d); padding: 2px 12px 1px 12px; }
QToolButton#primary:checked { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @amberHi@, stop:0.962 @amber@, stop:0.967 @amberDeep@, stop:1 @amberDeep@); color: @accentInk@; border-color: @amberDeep@; }
QToolButton#rec:checked { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #e0563c, stop:0.962 #b8361f, stop:0.967 #7a2010, stop:1 #7a2010); color: #fff8ee; border-color: #7a2010; }
QToolButton[transport="true"] { min-height: 29px; max-height: 29px; padding: 0 6px 3px 6px; }
QToolButton[transport="true"]:pressed, QToolButton[transport="true"]:checked { padding: 2px 6px 1px 6px; }
QToolButton[big="true"] { min-height: 31px; max-height: 31px; padding: 0 14px 3px 14px; }
QToolButton[big="true"]:pressed, QToolButton[big="true"]:checked { padding: 2px 14px 1px 14px; }
QToolButton#exportKey { min-height: 61px; max-height: 61px; padding: 4px 4px 5px 4px; background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @keyTop@, stop:0.950 @keyBot@, stop:0.955 @keyDeep@, stop:1 @keyDeep@); }
QToolButton#exportKey:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ffffff, stop:0.950 @keyTop@, stop:0.955 @keyDeep@, stop:1 @keyDeep@); }
QToolButton#exportKey:pressed { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @keyBot@, stop:0.980 @keyBot@, stop:0.985 @keyDeep@, stop:1 @keyDeep@); padding: 6px 4px 3px 4px; }
QToolButton#help { min-height: 15px; max-height: 15px; padding: 0 0 2px 0; }
QToolButton#help:pressed { padding: 1px 0 1px 0; }
QComboBox, QSpinBox { background: @field@; border: 1px solid @rule@; border-top: 2px solid @fieldShade@; border-radius: 4px; padding: 0 8px; min-height: 26px; max-height: 26px; }
QComboBox::drop-down { border: none; width: 18px; }
QComboBox::down-arrow { image: url(@arrowInk@); width: 10px; height: 6px; margin-right: 6px; }
QComboBox#readout::down-arrow { image: url(@arrowLcd@); }
QSlider { min-height: 26px; }
QSlider::groove:horizontal { height: 8px; border-radius: 4px; border: 1px solid @rule@;
  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @slotDeep@, stop:0.45 @slot@, stop:1 @slotHi@); }
QSlider::sub-page:horizontal { height: 8px; border-radius: 4px; border: 1px solid @amberDeep@;
  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @amber@, stop:1 @amberHi@); }
QSlider::handle:horizontal { width: 18px; height: 26px; margin: -10px -1px; image: url(@faderCap@); background: transparent; border: none; }
QTabBar::tab { background: @well@; color: @dim@; padding: 7px 16px; margin-right: 3px; border: 1px solid @rule@; border-bottom: none;
  border-top-left-radius: 6px; border-top-right-radius: 6px; font-weight: 600; letter-spacing: 1px; }
QTabBar::tab:selected { background: @canvas@; color: @ink@; }
QFrame#strip, QFrame#strip[sel="true"] { background: transparent; border: none; }
QPushButton#stripName { background: qradialgradient(cx:0.5, cy:1.3, radius:1.1, fx:0.5, fy:1.3, stop:0 @ivoryHi@, stop:0.75 @lcdBg@, stop:1 @ivoryLo@);
  border: 2px solid @bezel@; border-radius: 5px; min-height: 24px; max-height: 24px; margin: 4px 6px 0 6px; }
QPushButton#stripName:hover { background: @ivoryHi@; }
QStatusBar { background: @panelHi@; border-top: 1px solid @rule@; }
QPushButton#mute:checked { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #e0563c, stop:0.962 #b8361f, stop:0.967 #7a2010, stop:1 #7a2010); color: #fff8ee; border-color: #7a2010; padding: 2px 12px 1px 12px; }
QToolButton#inspectorTab { min-height: 0; max-height: 16777215; padding: 0; border-radius: 0 6px 6px 0; border-left: none; font-size: 15px; }
)");
    s += QStringLiteral(R"(
QSpinBox#readout, QComboBox#readout { background: @lcdBg@; color: @lcd@; font-family: "Courier Prime"; font-weight: 700; font-size: 14px;
  border: 3px solid @bezel@; border-radius: 5px; padding: 0 6px; min-height: 24px; max-height: 24px; selection-background-color: @lcd@; selection-color: @lcdBg@; }
QComboBox#readout QAbstractItemView { font-family: "Nachtzug Grotesk"; font-weight: 400; color: @ink@; background: @well@; }
QFrame#unit { background: @zoneU@; border: 1px solid @rule@; border-radius: 8px; }
QLabel#unitTitle { color: @ink@; font-size: 13px; font-weight: 400; letter-spacing: 0.5px; }
)");
    if (tactile) s += QStringLiteral(R"(
QFrame#unit { background: transparent; border: none; }
QSpinBox#readout, QComboBox#readout { background: qradialgradient(cx:0.5, cy:1.3, radius:1.1, fx:0.5, fy:1.3, stop:0 @ivoryHi@, stop:0.75 @lcdBg@, stop:1 @ivoryLo@); }
)");
    s += QStringLiteral(R"(
QPushButton#card { background: @well@; border: 1px solid @rule@; border-radius: 8px; padding: 0; min-height: 88px; max-height: 400px; text-align: left; }
QPushButton#card:hover { background: @hover@; }
QPushButton#card:checked { background: @wellHi@; border: 2px solid @amber@; padding: 0; }
QLabel#cardHead { color: @ink@; font-size: 15px; font-weight: 600; background: transparent; }
QLabel#cardText { color: @dim@; font-size: 13px; background: transparent; }
QPushButton#sample { padding: 0 6px; font-weight: 400; }
QPushButton#sample[sampleOn="true"] { color: @amber@; font-weight: 600; }
)");
    // drop-down arrows in the ink and display colours, written next to the program's temporary files
    auto arrow = [](const QColor& c) {
        const QString path = QDir::temp().filePath(QStringLiteral("nachtzug-arrow-%1.svg").arg(c.name().mid(1)));
        QFile f(path);
        if (!f.exists() && f.open(QIODevice::WriteOnly))
            f.write(QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"10\" height=\"6\" viewBox=\"0 0 10 6\"><path d=\"M1 1 L5 5 L9 1\" fill=\"none\" stroke=\"%1\" stroke-width=\"1.7\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/></svg>").arg(c.name()).toUtf8());
        return path;
    };
    // a fader cap like the mixer's: dark moulded body, a light grip line across it, a soft shadow under it
    {
        const QString path = QDir::temp().filePath(QStringLiteral("nachtzug-fader-%1.svg").arg(amber.name().mid(1)));
        QFile f(path);
        if (!f.exists() && f.open(QIODevice::WriteOnly))
            f.write(QStringLiteral(
                "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"18\" height=\"26\" viewBox=\"0 0 18 26\">"
                "<defs><linearGradient id=\"b\" x1=\"0\" y1=\"0\" x2=\"0\" y2=\"1\"><stop offset=\"0\" stop-color=\"#57504b\"/><stop offset=\"0.5\" stop-color=\"#2e2a27\"/><stop offset=\"1\" stop-color=\"#1a1716\"/></linearGradient></defs>"
                "<rect x=\"2\" y=\"4\" width=\"14\" height=\"21\" rx=\"3\" fill=\"#000\" opacity=\"0.22\"/>"
                "<rect x=\"1.5\" y=\"1.5\" width=\"15\" height=\"21\" rx=\"3\" fill=\"url(#b)\" stroke=\"#0d0b0a\"/>"
                "<line x1=\"4\" y1=\"3.2\" x2=\"14\" y2=\"3.2\" stroke=\"#ffffff\" stroke-opacity=\"0.18\"/>"
                "<line x1=\"9\" y1=\"5\" x2=\"9\" y2=\"19\" stroke=\"%1\" stroke-width=\"2\" stroke-linecap=\"round\"/>"
                "</svg>").arg(amber.name()).toUtf8());
        s.replace(QLatin1String("@faderCap@"), path);
    }
    // grips on the dividers: three small dots, so it is clear they can be dragged
    auto grip = [&](bool across) {
        const QString path = QDir::temp().filePath(QStringLiteral("nachtzug-grip-%1-%2.svg").arg(across ? "h" : "v", dim.name().mid(1)));
        QFile f(path);
        if (!f.exists() && f.open(QIODevice::WriteOnly)) {
            QString dots;
            for (int i = -1; i <= 1; i++) dots += across ? QStringLiteral("<circle cx=\"%1\" cy=\"4\" r=\"1.6\"/>").arg(14 + i * 7) : QStringLiteral("<circle cx=\"4\" cy=\"%1\" r=\"1.6\"/>").arg(14 + i * 7);
            f.write(QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%1\" height=\"%2\" viewBox=\"0 0 %1 %2\"><g fill=\"%3\">%4</g></svg>")
                        .arg(across ? 28 : 8).arg(across ? 8 : 28).arg(dim.name(), dots).toUtf8());
        }
        return path;
    };
    s.replace(QLatin1String("@groupTop@"), QString::number(GROUP_TOP));
    {   // the type of the window: Nachtzug Grotesk throughout. For comparing pictures, NZ_UI_FONT=mixed sets the
        // names of sections, tabs and units in Nachtzug Antiqua, and NZ_UI_FONT=antiqua sets everything in it
        const QString which = qEnvironmentVariable("NZ_UI_FONT");
        const bool all = which == QLatin1String("antiqua"), mixed = which == QLatin1String("mixed");
        s.replace(QLatin1String("@uiFont@"), all ? QStringLiteral("Nachtzug Antiqua") : QStringLiteral("Nachtzug Grotesk"));
        s.replace(QLatin1String("@uiSize@"), all ? QStringLiteral("14") : QStringLiteral("13"));
        if (all || mixed)
            s += QStringLiteral(R"(
QGroupBox { font-family: "Nachtzug Antiqua"; font-size: 12px; font-weight: 700; }
QLabel#unitTitle, QLabel#inspectorHead { font-family: "Nachtzug Antiqua"; font-size: 15px; }
QTabBar { font-family: "Nachtzug Antiqua"; font-size: 14px; font-weight: 700; }
)");
    }
    s.replace(QLatin1String("@gripH@"), grip(true));
    s.replace(QLatin1String("@gripV@"), grip(false));
    s.replace(QLatin1String("@arrowInk@"), arrow(ink));
    s.replace(QLatin1String("@arrowLcd@"), arrow(lcd));
    const bool darkKeys = key.lightness() < 128;
    const std::pair<const char*, QColor> map[] = {
        {"@accentInk@", accentInk}, {"@ink@", ink}, {"@panel@", panel}, {"@wellHi@", wellHi}, {"@well@", well}, {"@hover@", hover}, {"@rule@", rule}, {"@gridBar@", gridBar},
        {"@dim@", dim}, {"@lcdBg@", lcdBg}, {"@lcdText@", lcdBg.lightness() > 128 ? ink : lcd.lighter(125)}, {"@lcd@", lcd}, {"@keyInk@", keyInk},
        {"@keyHover@", darkKeys ? key.lighter(150) : key.lighter(108)}, {"@keyPress@", darkKeys ? key.lighter(190) : key.darker(118)},
        {"@keyTop@", QColor(0xf3, 0xee, 0xe4)}, {"@keyBot@", QColor(0xe2, 0xda, 0xcb)}, {"@keyEdge@", rule}, {"@keyDeep@", rule.darker(125)},
        {"@amberHi@", amber.lighter(118)}, {"@amberDeep@", amber.darker(150)}, {"@panelHi@", panel.lighter(103)},
        {"@zoneU@", panel.darker(tactile ? 102 : 100)}, {"@bezel@", rule}, {"@zone@", panel.darker(102)}, {"@field@", canvas.lighter(102)}, {"@fieldShade@", rule}, {"@slotDeep@", gridBar.darker(118)}, {"@slotHi@", gridBar.lighter(110)}, {"@slot@", gridBar},
        {"@ivoryHi@", lcdBg.lighter(108)}, {"@ivoryLo@", lcdBg.darker(106)}, {"@key@", key}, {"@amber@", amber}, {"@canvas@", canvas}};
    for (const auto& [token, colour] : map) s.replace(QLatin1String(token), colour.name());
    return s;
}
} // namespace theme

// ---------------------------------------------------------------- Knob

Knob::Knob(const QString& l, QWidget* parent) : QWidget(parent), label(l) {
    setFixedSize(sizeHint());
    setCursor(Qt::SizeVerCursor);
    setFocusPolicy(Qt::NoFocus);
    setToolTip(tr("Drag up or down. Hold Shift for small steps. Double-click to reset."));
}

void Knob::setValue(float v) {
    v = std::clamp(v, 0.f, 1.f);
    if (v == val) return;
    val = v;
    update();
}

std::function<int(const QString&)> Knob::midiLookup;

// a knob a MIDI controller turns has a small ring at its top left
void Knob::drawMidiMark(QPainter& p, const QRectF& dial) const {
    const QString target = property("midiTarget").toString();
    if (target.isEmpty() || !midiLookup || midiLookup(target) < 0) return;
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(theme::ice, 1.6)); p.setBrush(Qt::NoBrush);
    p.drawEllipse(QPointF(dial.left() - 1, dial.top() + 4), 3, 3);
    p.restore();
}

void Knob::paintEvent(QPaintEvent*) {
    QPainter p(this);
    if (!isEnabled()) p.setOpacity(0.45);   // a knob that does nothing just now looks faded
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(scale, scale);   // everything below is drawn at the normal size
    const int W = int(width() / scale);
    const QRectF dial((W - 44) / 2.0, 2, 44, 44);
    if (theme::tactile) { // a ribbed black knob on a printed scale, with a coloured ring showing the value
        const QPointF c = dial.center();
        const double a0 = 225.0, span = 270.0;
        p.setPen(QPen(theme::dim, 1));
        for (int i = 0; i <= 10; i++) { // printed scale
            const double a = (a0 - span * i / 10) * M_PI / 180.0, r1 = 20.5, r2 = i % 5 == 0 ? 23.0 : 22.0;
            p.setPen(QPen(theme::dim, i % 5 == 0 ? 1.4 : 0.9, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(c.x() + r1 * std::cos(a), c.y() - r1 * std::sin(a)), QPointF(c.x() + r2 * std::cos(a), c.y() - r2 * std::sin(a)));
        }
        const QRectF ring(c.x() - 18, c.y() - 18, 36, 36);
        p.setPen(QPen(theme::gridBar, 2.5, Qt::SolidLine, Qt::FlatCap)); p.drawArc(ring, int(a0 * 16), int(-span * 16));
        p.setPen(QPen(accent, 2.5, Qt::SolidLine, Qt::FlatCap)); p.drawArc(ring, int(a0 * 16), int(-span * 16 * val));
        // soft shadow under the knob
        p.setPen(Qt::NoPen);
        for (int i = 0; i < 4; i++) { p.setBrush(QColor(0, 0, 0, 22 - i * 5)); p.drawEllipse(c + QPointF(0, 2.0 + i * 0.6), 14.5 + i, 14.5 + i); }
        // ribbed skirt
        p.setBrush(QColor(0x1f, 0x1c, 0x1a));
        for (int i = 0; i < 28; i++) { const double a = 2 * M_PI * i / 28; p.drawEllipse(QPointF(c.x() + 13.4 * std::cos(a), c.y() + 13.4 * std::sin(a)), 1.7, 1.7); }
        QLinearGradient skirt(c.x(), c.y() - 14, c.x(), c.y() + 14);
        skirt.setColorAt(0, QColor(0x45, 0x40, 0x3c)); skirt.setColorAt(1, QColor(0x14, 0x12, 0x11));
        p.setBrush(skirt); p.drawEllipse(c, 13.4, 13.4);
        QRadialGradient capG(c + QPointF(-3, -4), 13);
        capG.setColorAt(0, QColor(0x4e, 0x48, 0x44)); capG.setColorAt(1, QColor(0x1c, 0x19, 0x18));
        p.setPen(QPen(QColor(0, 0, 0, 140), 0.8)); p.setBrush(capG); p.drawEllipse(c, 10, 10);
        const double a = (a0 - span * double(val)) * M_PI / 180.0;
        p.setPen(QPen(theme::capInk, 2.4, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(c.x() + 4 * std::cos(a), c.y() - 4 * std::sin(a)), QPointF(c.x() + 9.5 * std::cos(a), c.y() - 9.5 * std::sin(a)));
        if (marked) { p.setPen(Qt::NoPen); p.setBrush(QColor(0xc0, 0x36, 0x1c)); p.drawEllipse(QPointF(dial.right() + 1, dial.top() + 3), 3, 3); }
        drawMidiMark(p, dial);
        QFont f = font(); f.setPixelSize(11); f.setLetterSpacing(QFont::AbsoluteSpacing, 0.3);
        p.setFont(f);
        const bool showValue = (dragging || hover) && fmt;
        p.setPen(showValue ? theme::ink : theme::dim);
        p.drawText(QRect(0, 48, W, 30), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, showValue ? fmt(val) : label);
        return;
    }
    QPen track(theme::slot(), 4, Qt::SolidLine, Qt::RoundCap);
    p.setPen(track);
    p.drawArc(dial.adjusted(2, 2, -2, -2), 225 * 16, -270 * 16);
    p.setPen(QPen(accent, 4, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(dial.adjusted(2, 2, -2, -2), 225 * 16, int(-270.f * 16.f * val));
    const QPointF c = dial.center();
    p.setPen(QPen(theme::capEdge, 2));
    p.setBrush(theme::cap);
    p.drawEllipse(c, 15, 15);
    const double a = (225.0 - 270.0 * double(val)) * M_PI / 180.0;
    p.setPen(QPen(theme::capInk, 2, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(c.x() + 5 * std::cos(a), c.y() - 5 * std::sin(a)), QPointF(c.x() + 13 * std::cos(a), c.y() - 13 * std::sin(a)));
    if (marked) { p.setPen(Qt::NoPen); p.setBrush(theme::lcd); p.drawEllipse(QPointF(dial.right() + 1, dial.top() + 4), 3, 3); }
    drawMidiMark(p, dial);
    QFont f = font(); f.setPixelSize(11);
    p.setFont(f);
    const bool showValue = (dragging || hover) && fmt;
    p.setPen(showValue ? theme::ink : theme::dim);
    p.drawText(QRect(0, 48, W, 30), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, showValue ? fmt(val) : label);
}

void Knob::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) { QWidget::mousePressEvent(e); return; }
    dragging = true; startY = int(e->globalPosition().y()); startVal = val;
    emit pressed();
    update();
}
void Knob::mouseMoveEvent(QMouseEvent* e) {
    if (!dragging) return;
    const float range = (e->modifiers() & Qt::ShiftModifier) ? 700.f : 170.f;
    const float v = std::clamp(startVal + float(startY - int(e->globalPosition().y())) / range, 0.f, 1.f);
    if (v != val) { val = v; update(); emit moved(v); }
}
void Knob::mouseReleaseEvent(QMouseEvent*) {
    if (!dragging) return;
    dragging = false;
    emit released();
    update();
}
void Knob::mouseDoubleClickEvent(QMouseEvent*) {
    emit pressed(); val = def; emit moved(def); emit released(); update();
}
void Knob::enterEvent(QEnterEvent*) { hover = true; update(); }
void Knob::leaveEvent(QEvent*) { hover = false; update(); }

// ---------------------------------------------------------------- Lcd

Lcd::Lcd(App* a, QWidget* parent) : QWidget(parent), app(a) {
    setMinimumSize(210, 58);
    setToolTip(tr("The display: where the song is (bar and beat), which track the letter keys play, and short messages. The wave on the left is what you hear."));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void Lcd::setMessage(const QString& m) {
    message = m;
    messageUntil = QDateTime::currentMSecsSinceEpoch() + 3500;
    update();
}

void Lcd::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (theme::tactile) { // a bezel, and the glass sunk into it
        QLinearGradient bz(0, 0, 0, height());
        bz.setColorAt(0, theme::rule.darker(115)); bz.setColorAt(1, theme::rule.lighter(112));
        p.setPen(Qt::NoPen); p.setBrush(bz); p.drawRoundedRect(QRectF(rect()), 6, 6);
        const QRectF glass = QRectF(rect()).adjusted(3, 3, -3, -3);
        theme::drawIvory(p, glass, 4);
        QLinearGradient shade(0, glass.top(), 0, glass.top() + 8);
        shade.setColorAt(0, QColor(0, 0, 0, 60)); shade.setColorAt(1, Qt::transparent);
        p.setBrush(shade); p.drawRoundedRect(glass, 4, 4);
    } else {
        p.setPen(theme::lcdBg.lightness() > 128 ? QPen(theme::rule, 1) : QPen(Qt::NoPen)); p.setBrush(theme::lcdBg);
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
    }

    // oscilloscope
    const bool wide = width() >= 300;      // the wave picture only when there is room for it beside the text
    const QRect sr(10, 5, wide ? 84 : 0, height() - 10);
    const Engine* e = app->engine;
    const int w = e->scopeW.load(std::memory_order_relaxed), N = Engine::SCOPE_N;
    int startIdx = (w + N - 600) % N;
    for (int i = 0; i < 200; i++) { // start on a rising zero crossing so the picture stands still
        const int a = (startIdx + i) % N, b = (a + 1) % N;
        if (e->scope[a].load(std::memory_order_relaxed) < 0.f && e->scope[b].load(std::memory_order_relaxed) >= 0.f) { startIdx = b; break; }
    }
    QPainterPath path;
    for (int x = 0; x < sr.width(); x++) {
        const float v = std::clamp(e->scope[(startIdx + x * 3) % N].load(std::memory_order_relaxed), -1.f, 1.f);
        const QPointF pt(sr.left() + x, sr.center().y() - double(v) * sr.height() * 0.48);
        if (x == 0) path.moveTo(pt); else path.lineTo(pt);
    }
    p.setPen(QPen(theme::lcd, 1.5)); p.setBrush(Qt::NoBrush);
    if (wide) p.drawPath(path);

    QFont f = theme::lcdFont(17, true);
    QFont f2 = theme::lcdFont(12, false);
    p.setFont(f);
    const double m = app->musicalPos();
    const int bar = int(m) / BAR + 1, beat = int(m) % BAR / BEAT + 1;
    // a sign for what the transport is doing, then the position
    const QString state = app->isPlaying() ? (app->recording ? QStringLiteral("● REC") : QStringLiteral("▶")) : (app->recording ? QStringLiteral("○ REC") : QStringLiteral("■"));
    const QString line1 = QStringLiteral("%1  ").arg(state) + tr("BAR %1 · BEAT %2").arg(bar).arg(beat);
    QString line2;
    if (QDateTime::currentMSecsSinceEpoch() < messageUntil) line2 = message;
    else if (app->pendingChanges() > 0) line2 = tr("CHANGE LANDS ON THE NEXT BAR");
    else if (app->recording && app->isPlaying()) line2 = tr("RECORDING: PLAY KEYS, TURN KNOBS");
    else if (app->recording) line2 = tr("RECORD IS ARMED: PRESS PLAY");
    else if (!everPlayed && !app->isPlaying()) line2 = tr("PRESS SPACE TO PLAY");
    else if (const Track* t = app->song.track(app->selTrack)) line2 = tr("KEYS PLAY: %1").arg(ui(t->name).toUpper());
    if (app->isPlaying()) everPlayed = true;
    const int tx = wide ? sr.right() + 14 : 12;
    const int tw = width() - tx - 20;   // keep clear of the level meter on the right
    if (height() < 50) { // one line: the position on the left, messages on the right
        const int w1 = p.fontMetrics().horizontalAdvance(line1) + 24;
        p.drawText(QRect(tx, 0, tw, height()), Qt::AlignLeft | Qt::AlignVCenter, line1);
        p.setFont(f2);
        p.drawText(QRect(tx + w1, 0, tw - w1, height()), Qt::AlignRight | Qt::AlignVCenter, p.fontMetrics().elidedText(line2.toUpper(), Qt::ElideLeft, tw - w1));
    } else {
        const int lh = (height() - 8) / 2;
        // in a narrow display (or a language with longer words) the position gets a smaller type rather than cut short
        while (f.pixelSize() > 12 && QFontMetrics(f).horizontalAdvance(line1) > tw) f.setPixelSize(f.pixelSize() - 1);
        p.setFont(f);
        p.drawText(QRect(tx, 4, tw, lh), Qt::AlignLeft | Qt::AlignVCenter, p.fontMetrics().elidedText(line1, Qt::ElideRight, tw));
        p.setFont(f2);
        p.drawText(QRect(tx, 4 + lh, tw, lh), Qt::AlignLeft | Qt::AlignVCenter, p.fontMetrics().elidedText(line2.toUpper(), Qt::ElideRight, tw));
    }
    if (theme::tactile) p.drawLine(QPointF(tx - 8, 10), QPointF(tx - 8, height() - 10));   // a printed line between the wave and the text

    // output level
    const float pk = std::max(e->peakL.load(std::memory_order_relaxed), e->peakR.load(std::memory_order_relaxed));
    const int mh = int(std::clamp(pk, 0.f, 1.f) * (height() - 12));
    p.setPen(Qt::NoPen); p.setBrush(theme::lcd);
    p.drawRect(width() - 8, height() - 6 - mh, 3, mh);
    if (theme::tactile) {   // the light on the glass, over everything shown behind it
        const QRectF glass = QRectF(rect()).adjusted(3, 3, -3, -3);
        QLinearGradient refl(glass.topLeft(), QPointF(glass.left() + glass.width() * 0.25, glass.bottom()));
        refl.setColorAt(0, QColor(255, 255, 255, 110)); refl.setColorAt(0.44, QColor(255, 255, 255, 18)); refl.setColorAt(0.45, Qt::transparent); refl.setColorAt(1, Qt::transparent);
        p.setBrush(refl); p.drawRoundedRect(glass, 4, 4);
    }
}

// ---------------------------------------------------------------- VuMeter

VuMeter::VuMeter(const QString& l, QWidget* parent) : QWidget(parent), label(l) {
    setFixedSize(sizeHint());
    setToolTip(tr("How loud the song is. When the needle stays in the red, turn tracks down in the mixer."));
}

void VuMeter::feed(float peak) {
    // decibels from -24 to +3 along the scale; the needle rises fast and falls slowly
    const float db = 20.f * std::log10(std::max(peak, 1e-4f));
    const float target = std::clamp((db + 24.f) / 27.f, 0.f, 1.f);
    needle = target > needle ? needle + (target - needle) * 0.55f : needle + (target - needle) * 0.12f;
    update();
}

void VuMeter::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QRectF face;
    if (theme::tactile) { // the deep frame of the Sound tab's windows
        QLinearGradient outer(r.topLeft(), r.bottomLeft()); outer.setColorAt(0, QColor(0xe3, 0xda, 0xcb)); outer.setColorAt(1, QColor(0x9f, 0x91, 0x7f));
        p.setPen(Qt::NoPen); p.setBrush(outer); p.drawRoundedRect(r, 6, 6);
        const QRectF bev = r.adjusted(3, 3, -3, -3);
        QLinearGradient inner(bev.topLeft(), bev.bottomLeft()); inner.setColorAt(0, QColor(0x7d, 0x70, 0x5f)); inner.setColorAt(1, QColor(0xd5, 0xcb, 0xbb));
        p.setBrush(inner); p.drawRoundedRect(bev, 4.5, 4.5);
        face = r.adjusted(5, 5, -5, -5);
    } else {
        QLinearGradient bz(0, 0, 0, height());
        bz.setColorAt(0, theme::rule.darker(115)); bz.setColorAt(1, theme::rule.lighter(110));
        p.setPen(Qt::NoPen); p.setBrush(bz); p.drawRoundedRect(r, 5, 5);
        face = r.adjusted(3, 3, -3, -3);
    }
    QLinearGradient fg(0, face.top(), 0, face.bottom());
    fg.setColorAt(0, QColor(0xf6, 0xea, 0xc8)); fg.setColorAt(1, QColor(0xec, 0xdc, 0xb2));
    p.setBrush(fg); p.drawRoundedRect(face, 3, 3);
    p.save();
    p.setClipRect(face);
    const QPointF pivot(face.center().x(), face.bottom() + 12);
    const double R = face.height() + 4;
    auto at = [&](double t, double rr) { const double a = (140.0 - t * 100.0) * M_PI / 180.0; return QPointF(pivot.x() + rr * std::cos(a), pivot.y() - rr * std::sin(a)); };
    for (int i = 0; i <= 9; i++) { // scale marks; the last stretch is red
        const double t = i / 9.0;
        const bool red = t > 24.0 / 27.0 - 0.01;
        p.setPen(QPen(red ? QColor(0xc4, 0x36, 0x1c) : QColor(0x3a, 0x33, 0x2e), i % 3 == 0 ? 1.4 : 1.0));
        p.drawLine(at(t, R - 10), at(t, R - (i % 3 == 0 ? 3 : 6)));
    }
    p.setPen(QPen(QColor(0xc4, 0x36, 0x1c), 3, Qt::SolidLine, Qt::FlatCap));
    QPainterPath arc;
    arc.moveTo(at(24.0 / 27.0, R - 7));
    for (int k = 1; k <= 8; k++) arc.lineTo(at(24.0 / 27.0 + (3.0 / 27.0) * k / 8, R - 7));
    p.drawPath(arc);
    QFont f = font(); f.setPixelSize(9); f.setBold(true); f.setLetterSpacing(QFont::AbsoluteSpacing, 1);
    p.setFont(f); p.setPen(QColor(0x3a, 0x33, 0x2e));
    p.drawText(QRectF(face.left(), face.bottom() - 16, face.width(), 12), Qt::AlignCenter, label);
    p.setPen(QPen(QColor(0x1d, 0x1a, 0x18), 1.3, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(pivot, at(needle, R - 2));
    p.restore();
    if (theme::tactile) theme::drawGlass(p, face, 3);   // the same glass as the windows of the Sound tab
    else {
        QLinearGradient glass(0, face.top(), 0, face.top() + face.height() * 0.5);   // a little shade under the bezel
        glass.setColorAt(0, QColor(0, 0, 0, 45)); glass.setColorAt(1, Qt::transparent);
        p.setPen(Qt::NoPen); p.setBrush(glass); p.drawRoundedRect(face, 3, 3);
    }
}

// ---------------------------------------------------------------- Switch

Switch::Switch(const QString& l, QWidget* parent) : QAbstractButton(parent) {
    setText(l);
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setFixedSize(sizeHint());
}

void Switch::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QFont f = font(); f.setPixelSize(12);
    p.setFont(f);
    p.setPen(isEnabled() ? theme::dim : theme::rule);
    p.drawText(QRect(0, 0, width(), 15), Qt::AlignCenter, text());
    const QRectF track((width() - 34) / 2.0, 19, 34, 16);
    const bool on = isChecked();
    p.setPen(QPen(on ? theme::amber.darker(130) : theme::rule, 1));
    p.setBrush(on ? theme::amber : theme::slot());
    p.drawRoundedRect(track, 8, 8);
    if (!on) { QLinearGradient sh(0, track.top(), 0, track.top() + 5); sh.setColorAt(0, QColor(0, 0, 0, 40)); sh.setColorAt(1, Qt::transparent); p.setPen(Qt::NoPen); p.setBrush(sh); p.drawRoundedRect(track, 8, 8); }
    const QPointF knobAt(on ? track.right() - 8 : track.left() + 8, track.center().y());
    p.setPen(Qt::NoPen); p.setBrush(QColor(0, 0, 0, 40)); p.drawEllipse(knobAt + QPointF(0, 1.2), 7, 7);
    QRadialGradient kg(knobAt + QPointF(-2, -2), 8);
    kg.setColorAt(0, QColor(0xff, 0xfd, 0xf8)); kg.setColorAt(1, QColor(0xdf, 0xd7, 0xc8));
    p.setPen(QPen(QColor(0, 0, 0, 70), 0.8)); p.setBrush(kg); p.drawEllipse(knobAt, 6.5, 6.5);
}

// ---------------------------------------------------------------- Toast

Toast::Toast(QWidget* parent) : QWidget(parent), hideTimer(new QTimer(this)) {
    hideTimer->setSingleShot(true);
    connect(hideTimer, &QTimer::timeout, this, &QWidget::hide);
    setMouseTracking(true);
    setAttribute(Qt::WA_NoSystemBackground);
    parent->installEventFilter(this);   // stays at the bottom centre when the window is resized
    hide();
}

void Toast::showMessage(const QString& t, const QString& a, int ms) {
    text = t; action = a; hoverAction = false;
    QFont f = font(); f.setPixelSize(14); f.setWeight(QFont::DemiBold);
    const QFontMetrics fm(f);
    const int w = 28 + fm.horizontalAdvance(text) + (action.isEmpty() ? 0 : 22 + fm.horizontalAdvance(action) + 26) + 6;
    resize(std::min(w, parentWidget()->width() - 40), 44);
    place();
    show(); raise(); update();
    hideTimer->start(ms);
}

void Toast::place() {
    const QWidget* p = parentWidget();
    move((p->width() - width()) / 2, p->height() - height() - 44);
}

QRect Toast::actionRect() const {
    if (action.isEmpty()) return {};
    QFont f = font(); f.setPixelSize(14); f.setWeight(QFont::DemiBold);
    const int aw = QFontMetrics(f).horizontalAdvance(action) + 26;
    return {width() - aw - 8, 8, aw, height() - 16};
}

void Toast::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 60)); p.drawRoundedRect(r.translated(0, 2), 8, 8);   // a soft shadow under it
    p.setBrush(theme::lcdBg); p.setPen(QPen(theme::lcd.darker(160), 1));
    p.drawRoundedRect(r.adjusted(0, 0, 0, -2), 8, 8);
    QFont f = font(); f.setPixelSize(14); f.setWeight(QFont::DemiBold); p.setFont(f);
    p.setPen(theme::lcd);
    const QRect ar = actionRect();
    p.drawText(QRect(14, 0, (ar.isNull() ? width() : ar.left()) - 20, height() - 2), Qt::AlignVCenter | Qt::AlignLeft, p.fontMetrics().elidedText(text, Qt::ElideRight, (ar.isNull() ? width() : ar.left()) - 24));
    if (!ar.isNull()) {
        p.setBrush(hoverAction ? theme::amber : Qt::transparent);
        p.setPen(QPen(theme::amber, 1.2));
        p.drawRoundedRect(QRectF(ar).adjusted(0.5, 0.5, -0.5, -2.5), 5, 5);
        p.setPen(hoverAction ? theme::accentInk : theme::amber);
        p.drawText(ar.adjusted(0, 0, 0, -2), Qt::AlignCenter, action);
    }
}

void Toast::mousePressEvent(QMouseEvent* e) {
    if (actionRect().contains(e->pos())) { hide(); emit actionClicked(); }
    else hide();   // a click anywhere else on it dismisses it
}
void Toast::mouseMoveEvent(QMouseEvent* e) {
    const bool h = actionRect().contains(e->pos());
    if (h != hoverAction) { hoverAction = h; setCursor(h ? Qt::PointingHandCursor : Qt::ArrowCursor); update(); }
}
void Toast::enterEvent(QEnterEvent*) { hideTimer->stop(); }
void Toast::leaveEvent(QEvent*) { hoverAction = false; update(); hideTimer->start(2500); }
bool Toast::eventFilter(QObject* o, QEvent* e) {
    if (o == parentWidget() && e->type() == QEvent::Resize && isVisible()) place();
    return QWidget::eventFilter(o, e);
}

// ---------------------------------------------------------------- FlowLayout

QSize FlowLayout::sizeHint() const {   // everything on one row
    int w = 0, hgt = 0, n = 0;
    for (QLayoutItem* it : items_) {
        if (it->isEmpty()) continue;
        const QSize s = it->sizeHint();
        w += s.width() + (n++ ? h_ : 0);
        hgt = std::max(hgt, s.height());
    }
    const QMargins m = contentsMargins();
    return QSize(w + m.left() + m.right(), hgt + m.top() + m.bottom());
}

QSize FlowLayout::minimumSize() const {   // as narrow as the widest single item
    QSize s;
    for (QLayoutItem* it : items_) if (!it->isEmpty()) s = s.expandedTo(it->sizeHint());
    const QMargins m = contentsMargins();
    return s + QSize(m.left() + m.right(), m.top() + m.bottom());
}

int FlowLayout::place(const QRect& r, bool apply) const {
    const QMargins m = contentsMargins();
    const QRect area = r.adjusted(m.left(), m.top(), -m.right(), -m.bottom());
    // first the rows: as many items as fit side by side
    std::vector<std::vector<QLayoutItem*>> rows(1);
    int x = 0;
    for (QLayoutItem* it : items_) {
        if (it->isEmpty()) continue;
        const int w = it->sizeHint().width();
        if (!rows.back().empty() && x + w > area.width()) { rows.emplace_back(); x = 0; }
        rows.back().push_back(it);
        x += w + h_;
    }
    int y = area.y();
    for (const auto& row : rows) {
        if (row.empty()) continue;
        int used = -h_, lineH = 0, growW = 0;
        for (QLayoutItem* it : row) {
            const QSize s = it->sizeHint();
            used += s.width() + h_;
            lineH = std::max(lineH, justify_ ? std::max(s.height(), it->heightForWidth(s.width())) : s.height());
            if (it->maximumSize().width() > s.width()) growW += s.width();
        }
        const int extra = justify_ && growW > 0 ? std::max(0, area.width() - used) : 0;
        if (apply) {
            int cx = area.x(), given = 0, growSeen = 0;
            for (QLayoutItem* it : row) {
                const QSize s = it->sizeHint();
                int w = s.width();
                if (extra > 0 && it->maximumSize().width() > s.width()) {   // a share of the extra, the last one takes what is left
                    growSeen += s.width();
                    const int upTo = int(std::lround(double(extra) * growSeen / growW));
                    w += upTo - given; given = upTo;
                }
                it->setGeometry(QRect(cx, y, w, justify_ ? lineH : s.height()));
                cx += w + h_;
            }
        }
        y += lineH + v_;
    }
    if (y == area.y()) return m.top() + m.bottom();   // nothing to show
    return y - v_ - r.y() + m.bottom();
}

// ---------------------------------------------------------------- Emblem, ElideLabel

void Emblem::paintEvent(QPaintEvent*) {
    const int side = std::min(width(), height());
    if (side < 40) return;   // too small to be worth drawing
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const QRect r((width() - side) / 2, (height() - side) / 2, side, side);
    p.drawPixmap(r, theme::logo(192));
}

void ElideLabel::paintEvent(QPaintEvent*) {
    if (width() < 60) return;
    QPainter p(this);
    p.setPen(theme::dim);
    p.setFont(font());
    p.drawText(rect(), Qt::AlignVCenter | Qt::AlignRight, fontMetrics().elidedText(text_, Qt::ElideRight, width()));
}
