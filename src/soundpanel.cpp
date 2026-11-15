#include "soundpanel.h"
#include "chopdialog.h"
#include "midi.h"
#include "app.h"
#include "widgets.h"
#include <QCheckBox>
#include <QComboBox>
#include <QAbstractItemView>
#include <QTimer>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QSettings>
#include <QStyle>
#include "samples.h"
#include "settingsdialog.h"
#include <array>
#include <complex>
#include <QDateTime>
#include <QFontMetricsF>
#include <QLineF>

using namespace dsp;

static QString pct(float v) { return QString::number(int(std::lround(v * 100.f))) + "%"; }
static QLabel* dimLabel(const QString& text) { auto* l = new QLabel(text); l->setObjectName("dim"); return l; }
static QPushButton* button(const QString& text, const QString& tip) {
    auto* b = new QPushButton(text); b->setFocusPolicy(Qt::NoFocus); b->setToolTip(tip); b->setStatusTip(tip); return b;
}
static QGroupBox* group(const QString& name, QHBoxLayout** lay) {
    auto* g = new QGroupBox(name.toUpper());
    *lay = new QHBoxLayout(g);
    (*lay)->setContentsMargins(8, 8, 8, 4); (*lay)->setSpacing(4);
    return g;
}
// Every row of sections spans the full width: each section gets width in proportion to what it holds,
// and inside a section the controls are spread out evenly, so rows end on the same line.
static void fillRow(QHBoxLayout* row) {
    for (int i = 0; i < row->count(); i++) {
        QWidget* w = row->itemAt(i)->widget();
        if (!w) continue;
        row->setStretch(i, std::max(1, w->sizeHint().width()));
        if (auto* inner = qobject_cast<QHBoxLayout*>(w->layout())) {
            const int n = inner->count();
            for (int k = n; k >= 0; k--) inner->insertStretch(k, 1);
        }
    }
}

static QWidget* stacked(std::initializer_list<std::pair<QString, QWidget*>> rows) { // labelled combos on top of each other
    auto* w = new QWidget; auto* g = new QGridLayout(w);
    g->setContentsMargins(0, 0, 6, 0); g->setSpacing(4);
    int r = 0;
    for (auto& [label, widget] : rows) { g->addWidget(dimLabel(label), r, 0); g->addWidget(widget, r, 1); r++; }
    g->setRowStretch(r, 1);
    return w;
}

// ---------------------------------------------------------------- live pictures

// A small drawing of what a section's knobs add up to. It reads the same values the knobs show,
// so it moves along with drawn or recorded knob lines. In the instrument look it is graph paper behind a pane
// of glass, with scale marks; the filter curve and the envelope points can be dragged.
class Graph : public QWidget {
public:
    enum Kind { Wave, Filter, FilterEnv, AmpEnv, Lfo };
    Graph(App* a, Kind k) : app(a), kind(k) {
        setFixedSize(theme::tactile ? QSize(128, 84) : QSize(104, 70));
        setFocusPolicy(Qt::NoFocus);
        setMouseTracking(true);
        if (kind == Filter) {
            setCursor(Qt::CrossCursor);
            setToolTip(SoundPanel::tr("Which pitches the filter lets through, from 20 Hz on the left to 20 kHz on the right.\nDrag here: left and right move the cutoff, up and down the resonance."));
        } else if (kind == Wave) setToolTip(SoundPanel::tr("The waveform the oscillators make together"));
        else if (kind == Lfo) setToolTip(SoundPanel::tr("The LFO: a slow, repeating movement you can send to the filter, the pitch or the pulse width"));
        else setToolTip(SoundPanel::tr("The envelope: how the sound rises when a note starts (A), falls to its held level (D, S), and fades when the note ends (R).\nDrag the points to change it."));
    }
protected:
    float pv(int param) const { const Track* t = app->song.track(app->selTrack); return t ? app->paramValue(*t, param) : PARAMS[param].def; }
    bool amp() const { return kind == AmpEnv; }

    // ---- envelope geometry, shared by drawing and dragging
    static double squeeze(double sec) { return std::pow(sec, 0.35); }      // 1 ms .. 8 s brought into view
    static double unsqueeze(double w) { return std::pow(std::max(w, 0.0), 1.0 / 0.35); }
    static float timeToParam(double sec) { return float(std::clamp(std::log(std::max(sec, 0.001) / 0.001) / std::log(8000.0), 0.0, 1.0)); }
    QRectF area() const { return QRectF(rect()).adjusted(theme::tactile ? 9 : 6, theme::tactile ? 9 : 7, theme::tactile ? -9 : -6, theme::tactile ? -16 : -7); }
    // the time scale fits the envelope, so short and long sounds both fill the window; it holds still while a point is dragged
    double unit() const {
        if (frozenUnit > 0) return frozenUnit;
        const double total = squeeze(envTime(pv(amp() ? P_AA : P_FA))) + squeeze(envTime(pv(amp() ? P_AD : P_FD))) + 0.5 + squeeze(envTime(pv(P_AR)));
        return area().width() / std::max(total * 1.12, 1.6);
    }
    // the four points: end of attack, end of decay (on the sustain level), end of the held part, end of release
    std::array<QPointF, 4> envPoints() const {
        const QRectF in = area();
        const double a = envTime(pv(amp() ? P_AA : P_FA)), d = envTime(pv(amp() ? P_AD : P_FD)), r = envTime(pv(P_AR)), sus = pv(amp() ? P_AS : P_FS);
        const double u = unit(), xa = in.left() + squeeze(a) * u, xd = xa + squeeze(d) * u, xh = xd + 0.5 * u, xr = xh + squeeze(r) * u;
        const double ys = in.bottom() - sus * in.height();
        return {QPointF(xa, in.top()), QPointF(xd, ys), QPointF(xh, ys), QPointF(xr, in.bottom())};
    }

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        const Track* t = app->song.track(app->selTrack);
        const bool tactile = theme::tactile;
        if (tactile) { // a deep frame, and graph paper below the glass
            QLinearGradient outer(r.topLeft(), r.bottomLeft()); outer.setColorAt(0, QColor(0xe3, 0xda, 0xcb)); outer.setColorAt(1, QColor(0x9f, 0x91, 0x7f));
            p.setPen(Qt::NoPen); p.setBrush(outer); p.drawRoundedRect(r, 6, 6);
            const QRectF bev = r.adjusted(3, 3, -3, -3);
            QLinearGradient inner(bev.topLeft(), bev.bottomLeft()); inner.setColorAt(0, QColor(0x7d, 0x70, 0x5f)); inner.setColorAt(1, QColor(0xd5, 0xcb, 0xbb));
            p.setBrush(inner); p.drawRoundedRect(bev, 4.5, 4.5);
        }
        const QRectF paper = tactile ? r.adjusted(5, 5, -5, -5) : r;
        if (tactile) {
            p.setBrush(QColor(0xf7, 0xf0, 0xde)); p.drawRoundedRect(paper, 3, 3);
            const QColor line(0xc9, 0x8f, 0x5a);
            for (int i = 1; i < 20; i++) { // fine lines, every fifth one stronger
                QColor c = line; c.setAlphaF(i % 5 == 0 ? 0.42 : 0.15);
                p.setPen(QPen(c, i % 5 == 0 ? 0.9 : 0.5));
                const double gx = paper.left() + paper.width() * i / 20;
                p.drawLine(QPointF(gx, paper.top()), QPointF(gx, paper.bottom()));
            }
            for (int i = 1; i < 12; i++) {
                QColor c = line; c.setAlphaF(i % 4 == 0 ? 0.42 : 0.15);
                p.setPen(QPen(c, i % 4 == 0 ? 0.9 : 0.5));
                const double gy = paper.top() + paper.height() * i / 12;
                p.drawLine(QPointF(paper.left(), gy), QPointF(paper.right(), gy));
            }
        } else {
            p.setPen(QPen(theme::rule, 1)); p.setBrush(theme::canvas);
            p.drawRoundedRect(r, 4, 4);
        }
        if (!t || t->isDrums()) return;
        const QColor col = tactile ? theme::trackColor(t->role) : theme::trackColor(t->role);
        const QColor ink = tactile ? QColor(0x2b, 0x26, 0x23) : col;
        const QRectF in = area();
        const double W = in.width(), H = in.height(), X = in.left(), top = in.top(), bottom = in.bottom();
        QFont small = theme::lcdFont(8); small.setPixelSize(8);
        const QColor labelInk(0x5f, 0x4a, 0x37);
        QPainterPath path;
        double baseline = bottom;
        p.save();
        p.setClipRect(paper.adjusted(1, 1, -1, -1));

        if (kind == Wave) {
            baseline = in.center().y();
            const int w1 = int(pv(P_WAVE1) + 0.5f), w2 = int(pv(P_WAVE2) + 0.5f);
            static const float tune2[4] = {0.5f, 1.f, 1.5f, 2.f};
            const float ratio = tune2[std::clamp(int(pv(P_TUNE2) + 0.5f), 0, 3)];
            const float mix = pv(P_MIX), g1 = std::min(1.f, 2.f * (1.f - mix)), g2 = std::min(1.f, 2.f * mix), gs = pv(P_SUB);
            const float pw = 0.5f - 0.42f * pv(P_PW), fm = pv(P_FM) * pv(P_FM) * 1.5f;
            auto wave = [pw](int w, float ph) {
                ph -= std::floor(ph);
                switch (w) { case 0: return 2.f * ph - 1.f; case 1: return ph < pw ? 1.f : -1.f; case 2: return 1.f - 4.f * std::fabs(ph - 0.5f); default: return std::sin(TAU * ph); }
            };
            const float norm = 1.f / std::max(1.f, g1 + g2 + gs);
            for (int x = 0; x <= int(W); x++) {
                const float ph = float(x) / float(W) * 2.f;
                const float b = wave(w2, ph * ratio), a = wave(w1, ph + b * fm);
                const float v = (a * g1 + b * g2 + wave(2, ph * 0.5f) * gs) * norm;
                const QPointF pt(X + x, baseline - double(v) * H * 0.42);
                if (x == 0) path.moveTo(pt); else path.lineTo(pt);
            }
        } else if (kind == Filter) {
            const float wc = std::min(cutoffHz(pv(P_CUTOFF)), 20000.f), k = pv(P_RES) * 4.15f;
            const int mode = std::clamp(int(pv(P_SLOPE) + 0.5f), 0, 3);
            for (int x = 0; x <= int(W); x++) {
                const float f = 20.f * std::pow(1000.f, float(x) / float(W));        // 20 Hz .. 20 kHz
                const std::complex<float> G = 1.f / std::complex<float>(1.f, f / wc), G2 = G * G, G4 = G2 * G2;
                const std::complex<float> num = mode == 1 ? G2 : mode == 2 ? 2.f * (G - G2) : mode == 3 ? (1.f - G) * (1.f - G) : G4;
                const float mag = std::abs(num / (1.f + k * G4)) * (1.f + 0.45f * k);
                const float db = std::clamp(20.f * std::log10(std::max(mag, 1e-4f)), -40.f, 24.f);
                const QPointF pt(X + x, bottom - double(db + 40.f) / 64.0 * H);
                if (x == 0) path.moveTo(pt); else path.lineTo(pt);
            }
        } else if (kind == FilterEnv || kind == AmpEnv) {
            const auto pts = envPoints();
            path.moveTo(X, bottom);
            path.lineTo(pts[0]);
            path.quadTo(pts[0].x() + (pts[1].x() - pts[0].x()) * 0.25, pts[1].y(), pts[1].x(), pts[1].y());
            path.lineTo(pts[2]);
            path.quadTo(pts[2].x() + (pts[3].x() - pts[2].x()) * 0.25, bottom, pts[3].x(), pts[3].y());
            path.lineTo(in.right() + 4, bottom);
        } else {
            baseline = in.center().y();
            const int shape = int(pv(P_LFOWAVE) + 0.5f);
            const float cycles = 1.f + pv(P_LFORATE) * 5.f;
            const float depth = 0.25f + 0.75f * std::max({pv(P_LFOCUT), pv(P_LFOPITCH), pv(P_LFOPW)});
            dsp::Rng steps;
            float held = steps.next(); int lastCycle = 0;
            auto value = [&](float ph) { const float fr = ph - std::floor(ph); return shape == 0 ? 1.f - 4.f * std::fabs(fr - 0.5f) : shape == 1 ? (fr < 0.5f ? 1.f : -1.f) : held; };
            for (int x = 0; x <= int(W); x++) {
                const float ph = float(x) / float(W) * cycles;
                if (int(ph) != lastCycle) { lastCycle = int(ph); held = steps.next(); }
                const QPointF pt(X + x, baseline - double(value(ph) * depth) * H * 0.42);
                if (x == 0) path.moveTo(pt); else path.lineTo(pt);
            }
            if (tactile) { // a dot running along the movement, at the LFO's real speed
                const double hz = lfoHz(pv(P_LFORATE));
                const double frac = std::fmod(double(QDateTime::currentMSecsSinceEpoch()) / 1000.0 * hz / cycles, 1.0);
                const double x = X + frac * W;
                const QPointF at = path.pointAtPercent(std::clamp(frac, 0.0, 1.0));
                p.setPen(QPen(ink, 1.2)); p.setBrush(col); p.drawEllipse(QPointF(x, at.y()), 3.6, 3.6);
            }
        }
        if (kind == Filter || kind == FilterEnv || kind == AmpEnv) { // a light fill in the track's colour under the curve
            QPainterPath area = path;
            area.lineTo(path.currentPosition().x(), baseline); area.lineTo(X, baseline); area.closeSubpath();
            QColor fill = col; fill.setAlpha(tactile ? 72 : 50);
            p.fillPath(area, fill);
        }
        p.setPen(QPen(ink, tactile ? 1.6 : 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p.setBrush(Qt::NoBrush);
        p.drawPath(path);

        if (tactile) { // scale marks, values and the points you can drag
            p.setFont(small);
            if (kind == Filter) {
                const double cx = X + std::log10(std::min(cutoffHz(pv(P_CUTOFF)), 20000.f) / 20.0) / 3.0 * W;
                p.setPen(QPen(col, 1.2)); p.drawLine(QPointF(cx, top - 2), QPointF(cx, bottom));
                const QString hz = formatParam(P_CUTOFF, pv(P_CUTOFF));
                const double tw = QFontMetricsF(small).horizontalAdvance(hz) + 8;
                const QRectF tag(std::min(cx + 3, paper.right() - tw - 2), paper.top() + 3, tw, 11);
                p.setPen(Qt::NoPen); p.setBrush(col); p.drawRoundedRect(tag, 2, 2);
                p.setPen(QColor(0xff, 0xf8, 0xee)); p.drawText(tag, Qt::AlignCenter, hz);
                p.setPen(labelInk);
                for (auto [f, label] : {std::pair{100.0, "100"}, {1000.0, "1k"}, {10000.0, "10k"}})
                    p.drawText(QRectF(X + std::log10(f / 20.0) / 3.0 * W - 12, paper.bottom() - 10, 24, 9), Qt::AlignCenter, QString::fromLatin1(label));
            } else if (kind == FilterEnv || kind == AmpEnv) {
                const auto pts = envPoints();
                const char* names = "ADSR";
                const int count = amp() ? 4 : 3;   // the filter envelope's release is the amp's release
                for (int i = 0; i < count; i++) {
                    const bool hot = i == hover || i == dragging;
                    p.setPen(Qt::NoPen); p.setBrush(QColor(0, 0, 0, 45)); p.drawEllipse(pts[size_t(i)] + QPointF(0, 1), 4.2, 4.2);
                    p.setPen(QPen(ink, 1.2)); p.setBrush(hot ? col.lighter(125) : col); p.drawEllipse(pts[size_t(i)], hot ? 4.6 : 3.8, hot ? 4.6 : 3.8);
                    // letters under the points, skipped where two points sit too close to label both
                    if (i == 0 || pts[size_t(i)].x() - pts[size_t(i - 1)].x() > 9) {
                        p.setPen(labelInk);
                        p.drawText(QRectF(pts[size_t(i)].x() - 6, paper.bottom() - 10, 12, 9), Qt::AlignCenter, QString(QChar(names[i])));
                    }
                }
            } else if (kind == Lfo) {
                p.setPen(labelInk);
                p.drawText(QRectF(paper.left(), paper.top() + 2, paper.width() - 4, 9), Qt::AlignRight | Qt::AlignVCenter, formatParam(P_LFORATE, pv(P_LFORATE)));
            } else {
                static const char* names[4] = {QT_TRANSLATE_NOOP("Model", "SAW"), QT_TRANSLATE_NOOP("Model", "PULSE"), QT_TRANSLATE_NOOP("Model", "TRIANGLE"), QT_TRANSLATE_NOOP("Model", "SINE")};
                const float mix = pv(P_MIX);
                const QString a = ui(names[std::clamp(int(pv(P_WAVE1) + 0.5f), 0, 3)]), b = ui(names[std::clamp(int(pv(P_WAVE2) + 0.5f), 0, 3)]);
                const QString label = mix < 0.08f ? a : mix > 0.92f ? b : a == b ? a + " × 2" : a + " + " + b;
                p.setPen(labelInk);
                p.drawText(QRectF(paper.left() + 4, paper.top() + 2, paper.width() - 8, 9), Qt::AlignLeft | Qt::AlignVCenter, label);
            }
        }
        p.restore();

        if (tactile) theme::drawGlass(p, paper, 3);
    }

    // ---- dragging: the filter curve, or the envelope's points
    int envPointAt(const QPointF& pos) const {
        if (kind != FilterEnv && kind != AmpEnv) return -1;
        const auto pts = envPoints();
        const int count = amp() ? 4 : 3;
        int best = -1; double bestD = 9.0;
        for (int i = 0; i < count; i++) { const double d = QLineF(pos, pts[size_t(i)]).length(); if (d < bestD) { bestD = d; best = i; } }
        return best;
    }
    void dragEnvelope(const QPointF& pos) {
        Track* t = app->curTrack();
        if (!t || t->isDrums() || dragging < 0) return;
        const QRectF in = area();
        const auto pts = envPoints();
        const double u = unit();
        float* pp = t->patch.p;
        const float sus = float(std::clamp((in.bottom() - pos.y()) / in.height(), 0.0, 1.0));
        switch (dragging) {
        case 0: pp[amp() ? P_AA : P_FA] = timeToParam(unsqueeze((pos.x() - in.left()) / u)); break;
        case 1: pp[amp() ? P_AD : P_FD] = timeToParam(unsqueeze((pos.x() - pts[0].x()) / u)); pp[amp() ? P_AS : P_FS] = sus; break;
        case 2: pp[amp() ? P_AS : P_FS] = sus; break;
        default: pp[P_AR] = timeToParam(unsqueeze((pos.x() - pts[2].x()) / u)); break;
        }
        t->patch.name = SoundPanel::tr("Custom sound");
        app->touch();
    }
    void setFromMouse(QMouseEvent* e) {
        Track* t = app->curTrack();
        if (!t || t->isDrums()) return;
        const QRectF in = area();
        t->patch.p[P_CUTOFF] = std::clamp(float((e->position().x() - in.left()) / in.width()), 0.f, 1.f);
        t->patch.p[P_RES] = std::clamp(1.f - float((e->position().y() - in.top()) / in.height()), 0.f, 1.f);
        app->touch();
    }
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() != Qt::LeftButton) return;
        if (kind == Filter) { held = true; app->begin(SoundPanel::tr("Filter")); setFromMouse(e); return; }
        if (theme::tactile && (dragging = envPointAt(e->position())) >= 0) { frozenUnit = unit(); app->begin(SoundPanel::tr("Envelope")); update(); }
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        if (held) { setFromMouse(e); return; }
        if (dragging >= 0) { dragEnvelope(e->position()); return; }
        if (theme::tactile) {
            const int h = envPointAt(e->position());
            if (h != hover) { hover = h; setCursor(h >= 0 ? Qt::SizeAllCursor : (kind == Filter ? Qt::CrossCursor : Qt::ArrowCursor)); update(); }
        }
    }
    void mouseReleaseEvent(QMouseEvent*) override { held = false; dragging = -1; frozenUnit = 0; update(); }
    void leaveEvent(QEvent*) override { hover = -1; update(); }

private:
    App* app;
    Kind kind;
    bool held = false;
    int hover = -1, dragging = -1;
    double frozenUnit = 0;
};

// ---------------------------------------------------------------- panel

SoundPanel::SoundPanel(App* a, QWidget* parent) : QWidget(parent), app(a) {
    auto* h = new QHBoxLayout(this);
    h->setContentsMargins(10, 8, 10, 8);
    pages = new QStackedWidget;
    pages->addWidget(buildSynthPage());     // 0
    pages->addWidget(buildDrumPage());      // 1
    pages->addWidget(buildSimpleSynth());   // 2
    pages->addWidget(buildSimpleDrums());   // 3
    h->addWidget(pages, 1);
    simple = QSettings().value(QStringLiteral("simpleSound"), false).toBool();
    connect(app, &App::changed, this, &SoundPanel::sync);
    connect(app, &App::selectionChanged, this, &SoundPanel::sync);
    sync();
}

QWidget* SoundPanel::graph(int kind) {
    auto* g = new Graph(app, Graph::Kind(kind));
    graphs.push_back(g);
    return g;
}

Knob* SoundPanel::patchKnob(int param) {
    auto* k = new Knob(ui(PARAMS[param].label));
    k->setProperty("param", param);   // the lessons find knobs by what they turn
    if (PARAMS[param].kind != K_CHOICE) k->setProperty("midiTarget", Midi::targetForParam(param));
    k->setDefault(PARAMS[param].def);
    k->setFormatter([param](float v) { return formatParam(param, v); });
    k->setToolTip(paramTip(param) + tr("\n\nDrag up or down, Shift for small steps, double-click to reset. Right-click to draw how it changes over time."));
    connect(k, &Knob::pressed, this, [this, param] { app->paramPress(app->selTrack, param); });
    connect(k, &Knob::moved, this, [this, param](float v) { app->paramMove(app->selTrack, param, v); });
    connect(k, &Knob::released, this, [this] { app->paramRelease(); });
    k->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(k, &QWidget::customContextMenuRequested, this, [this, k, param](const QPoint& pt) {
        Track* t = app->curTrack();
        if (!t) return;
        QMenu menu;
        QAction* draw = menu.addAction(tr("Draw a line for %1 in the timeline").arg(paramLabel(param)));
        QAction* forget = t->hasMoves(param) ? menu.addAction(tr("Erase the line of %1").arg(paramLabel(param))) : nullptr;
        if (PARAMS[param].kind != K_CHOICE) addMidiActions(&menu, app, Midi::targetForParam(param), paramLabel(param));
        QAction* got = menu.exec(k->mapToGlobal(pt));
        if (!got) return;
        if (got == forget) app->forgetMoves(t->id, param);
        else if (got == draw) { t->laneParam = param; t->laneOpen = true; app->touch(); emit app->status(tr("The line lane for %1 is open under the track in the timeline. Click there to add points.").arg(paramLabel(param))); }
    });
    knobs.push_back({k, [this, param] { Track* t = app->curTrack(); return t ? app->paramValue(*t, param) : 0.f; }});
    patchKnobs.push_back({param, k});
    return k;
}

Knob* SoundPanel::freeKnob(const QString& label, std::function<float()> get, std::function<void(float)> set, std::function<QString(float)> fmt, float def, const QString& tip) {
    auto* k = new Knob(label);
    if (!tip.isEmpty()) k->setToolTip(tip + tr("\n\nDrag up or down, Shift for small steps, double-click to reset."));
    k->setDefault(def);
    k->setFormatter(std::move(fmt));
    connect(k, &Knob::pressed, this, [this, label] { app->begin(label); });
    connect(k, &Knob::moved, this, [this, set](float v) { set(v); app->touch(); });
    knobs.push_back({k, std::move(get)});
    return k;
}

QComboBox* SoundPanel::combo(const QStringList& items, std::function<int()> get, std::function<void(int)> set, const QString& tip) {
    auto* c = new QComboBox;
    c->addItems(items);
    c->setFocusPolicy(Qt::NoFocus);
    if (!tip.isEmpty()) { c->setToolTip(tip); c->setStatusTip(tip); }
    connect(c, &QComboBox::activated, this, [this, set, tip](int i) { app->begin(tr("Setting")); set(i); app->touch(); });
    combos.push_back({c, std::move(get)});
    return c;
}

QComboBox* SoundPanel::patchCombo(int param) {
    return combo(ui(PARAMS[param].choices).split('|'),
                 [this, param] { Track* t = app->curTrack(); return t ? int(t->patch.p[param] + 0.5f) : 0; },
                 [this, param](int i) { if (Track* t = app->curTrack()) t->patch.p[param] = float(i); }, paramTip(param));
}

QWidget* SoundPanel::buildSynthPage() {
    // the sections wrap onto as many rows as the width needs, each row stretched to the full width, in the order the
    // sound flows; a section folds shut when its name is clicked
    auto* page = new QWidget;
    auto* flow = new FlowLayout(page, 10, 8);
    flow->setJustify(true);
    QHBoxLayout* l = nullptr;
    auto add = [&](QGroupBox* g, const char* key) { theme::makeFoldable(g, QStringLiteral("synth/") + QLatin1String(key)); flow->addWidget(g); return g; };

    add(group(tr("Sound"), &l), "sound");
    l->setSpacing(6);
    presetBox = makePresetBox();
    l->addWidget(presetBox);
    auto* roll = button(tr("Roll new sound"), tr("Build a brand new sound that suits this kind of track"));
    roll->setObjectName("primary");
    auto* mutate = button(tr("Mutate"), tr("Nudge every knob a little"));
    connect(roll, &QPushButton::clicked, this, [this] { rollSound(); });
    connect(mutate, &QPushButton::clicked, this, [this] { mutateSound(); });
    l->addWidget(roll); l->addWidget(mutate);
    resume = button(tr("Resume lines"), tr("Some knobs were set by hand, which paused their lines. This brings the lines back."));
    resume->setCheckable(true); resume->setChecked(true);
    connect(resume, &QPushButton::clicked, this, [this] { app->resumeMoves(app->selTrack); });
    l->addWidget(resume);

    // how notes are played belongs to the track: it lives in the inspector, and comes back here when that is hidden
    howGroup = new QGroupBox(tr("How notes are played").toUpper());
    {
        auto* hf = new FlowLayout(howGroup, 10, 6);
        hf->setContentsMargins(8, 8, 8, 4);
        auto pair = [&](const QString& label, std::initializer_list<QWidget*> ws) {
            auto* w = new QWidget; auto* h = new QHBoxLayout(w); h->setContentsMargins(0, 0, 0, 0); h->setSpacing(6);
            if (!label.isEmpty()) h->addWidget(dimLabel(label));
            for (QWidget* x : ws) h->addWidget(x);
            hf->addWidget(w);
        };
        pair(tr("Octave"), {combo({"-2", "-1", "0", "+1", "+2", "+3", "+4"}, [this] { Track* t = app->curTrack(); return t ? t->octave + 2 : 2; },
                                  [this](int i) { if (Track* t = app->curTrack()) t->octave = i - 2; }, tr("Moves this track’s notes, and the letter keys, up or down by whole octaves"))});
        QStringList chords; for (const char* c : CHORD_NAMES) chords << ui(c);
        pair(tr("Each note plays"), {combo(chords, [this] { Track* t = app->curTrack(); return t ? t->chord : 0; }, [this](int i) { if (Track* t = app->curTrack()) t->chord = i; },
                                           tr("Turns every note into a chord that fits the scale"))});
        QStringList arps; for (const char* c : ARP_NAMES) arps << ui(c);
        QStringList rates; for (const char* c : RATE_NAMES) rates << QString::fromLatin1(c);
        pair(tr("Arpeggio"), {combo(arps, [this] { Track* t = app->curTrack(); return t ? t->arp : 0; }, [this](int i) { if (Track* t = app->curTrack()) t->arp = i; },
                                    tr("Plays held notes and chords one after another"))});
        pair(QString(), {combo(rates, [this] { Track* t = app->curTrack(); return t ? t->arpRate : 2; }, [this](int i) { if (Track* t = app->curTrack()) t->arpRate = i; }, tr("Arpeggio speed"))});
        pair(QString(), {combo({tr("1 octave"), tr("2 octaves"), tr("3 octaves")}, [this] { Track* t = app->curTrack(); return t ? t->arpOct : 0; },
                                    [this](int i) { if (Track* t = app->curTrack()) t->arpOct = i; }, tr("Arpeggio range"))});
        follow = new QCheckBox(tr("Follow chords")); follow->setFocusPolicy(Qt::NoFocus);
        follow->setToolTip(tr("Shift this track’s notes with the chord lane in the timeline"));
        connect(follow, &QCheckBox::clicked, this, [this](bool on) { app->edit([&](Song& s) { if (Track* t = s.track(app->selTrack)) t->follow = on; }); });
        pair(QString(), {follow});
    }
    theme::makeFoldable(howGroup, QStringLiteral("synth/how"));
    howFlow = flow;
    flow->addWidget(howGroup);

    add(group(tr("1 · Oscillators"), &l), "osc");
    l->addWidget(graph(Graph::Wave));
    l->addSpacing(6);
    l->addWidget(stacked({{tr("Wave 1"), patchCombo(P_WAVE1)}, {tr("Wave 2"), patchCombo(P_WAVE2)}, {tr("Osc 2"), patchCombo(P_TUNE2)}}));
    for (int p : {P_MIX, P_DETUNE, P_FM, P_PW, P_SUB, P_NOISE}) l->addWidget(patchKnob(p));
    add(group(tr("Unison"), &l), "unison");
    l->addWidget(stacked({{tr("Voices"), patchCombo(P_UNISON)}}));
    for (int p : {P_UDETUNE, P_WIDTH}) l->addWidget(patchKnob(p));
    add(group(tr("Voices and glide"), &l), "voices");
    l->addWidget(stacked({{tr("Play"), patchCombo(P_MONO)}}));
    l->addWidget(patchKnob(P_GLIDE));
    add(group(tr("2 · Filter"), &l), "filter");
    l->addWidget(graph(Graph::Filter));
    l->addSpacing(6);
    l->addWidget(stacked({{tr("Type"), patchCombo(P_SLOPE)}}));
    for (int p : {P_CUTOFF, P_RES, P_ENVAMT, P_KEYTRK, P_ACCENT}) l->addWidget(patchKnob(p));
    add(group(tr("Filter envelope"), &l), "fenv");
    l->addWidget(graph(Graph::FilterEnv));
    l->addSpacing(6);
    for (int p : {P_FA, P_FD, P_FS, P_PENV}) l->addWidget(patchKnob(p));
    add(group(tr("3 · Amp envelope"), &l), "aenv");
    l->addWidget(graph(Graph::AmpEnv));
    l->addSpacing(6);
    for (int p : {P_AA, P_AD, P_AS, P_AR}) l->addWidget(patchKnob(p));
    add(group(tr("LFO"), &l), "lfo");
    l->addWidget(graph(Graph::Lfo));
    l->addSpacing(6);
    l->addWidget(stacked({{tr("Shape"), patchCombo(P_LFOWAVE)}}));
    for (int p : {P_LFORATE, P_LFOCUT, P_LFOPITCH, P_LFOPW}) l->addWidget(patchKnob(p));
    add(group(tr("4 · Effects"), &l), "fx");
    for (int p : {P_DRIVE, P_PHASER, P_PHRATE, P_CHORUS, P_DELAY, P_REVERB}) l->addWidget(patchKnob(p));
    add(buildMaster(), "master");
    spread(page);
    return page;
}

QWidget* SoundPanel::buildDrumPage() {
    auto* page = new QWidget;
    auto* flow = new FlowLayout(page, 10, 8);
    flow->setJustify(true);
    QHBoxLayout* l = nullptr;
    auto add = [&](QGroupBox* g, const QString& key) { theme::makeFoldable(g, QStringLiteral("drums/") + key); flow->addWidget(g); return g; };
    add(group(tr("Drum kit"), &l), QStringLiteral("kit"));
    l->setSpacing(6);
    kitBox = makeKitBox();
    l->addWidget(kitBox);
    l->addWidget(dimLabel(tr("The bottom row of letter keys, Z to M and the comma, plays the eight drums.")));
    for (int d = 0; d < D_COUNT; d++) {
        add(group(ui(DRUM_NAMES[d]), &l), QString::number(d));
        auto* hear = button(QStringLiteral("▶"), tr("Hear this drum"));
        hear->setFixedWidth(34);
        connect(hear, &QPushButton::clicked, this, [this, d] { app->audition(app->selTrack, d, true); });
        auto* col = new QVBoxLayout; col->setSpacing(4); col->addWidget(hear); col->addWidget(sampleButton(d)); col->addStretch(1);
        l->addLayout(col);
        l->addWidget(kitKnob(tr("Level"), d * 3, 0.6f));
        l->addWidget(kitKnob(tr("Tune"), d * 3 + 1, 0.5f));
        l->addWidget(kitKnob(tr("Decay"), d * 3 + 2, 0.4f));
    }
    add(group(tr("Effects"), &l), QStringLiteral("fx"));
    l->addWidget(kitKnob(tr("Drive"), DP_DRIVE, 0.2f));
    l->addWidget(kitKnob(tr("Echo"), DP_DELAY, 0.05f));
    l->addWidget(kitKnob(tr("Reverb"), DP_REVERB, 0.12f));
    add(buildMaster(), QStringLiteral("master"));
    spread(page);
    return page;
}

// a knob for one setting of the drum kit
Knob* SoundPanel::kitKnob(const QString& label, int idx, float def) {
    static const char* const what[] = {QT_TRANSLATE_NOOP("Model", "How loud this drum is."), QT_TRANSLATE_NOOP("Model", "Tunes this drum higher or lower."), QT_TRANSLATE_NOOP("Model", "How long this drum rings.")};
    const QString tip = idx < D_COUNT * 3 ? tr("%1: %2").arg(ui(DRUM_NAMES[idx / 3]), ui(what[idx % 3]))
                      : idx == DP_DRIVE ? tr("Distortion on the whole kit: warmer at first, grittier further up.")
                      : idx == DP_DELAY ? tr("How much of the kit goes to the shared echo.") : tr("How much of the kit goes to the shared reverb.");
    return freeKnob(label, [this, idx] { Track* t = app->curTrack(); return t ? t->kit.p[idx] : 0.f; },
                    [this, idx](float x) { if (Track* t = app->curTrack()) { t->kit.p[idx] = x; t->kit.name = tr("Custom kit"); } }, pct, def, tip);
}

// when a row is stretched, the controls inside each section spread out evenly instead of bunching at the left
void SoundPanel::spread(QWidget* page) {
    for (QGroupBox* g : page->findChildren<QGroupBox*>())
        if (auto* inner = qobject_cast<QHBoxLayout*>(g->layout())) { const int n = inner->count(); for (int k = n; k >= 0; k--) inner->insertStretch(k, 1); }
}

void SoundPanel::rollSound() {
    app->edit([&](Song& s) { if (Track* t = s.track(app->selTrack)) t->patch = rollPatch(t->role, app->rng); }, tr("Roll new sound"), true);
}
void SoundPanel::mutateSound() {
    app->edit([&](Song& s) { if (Track* t = s.track(app->selTrack)) t->patch = mutatePatch(t->patch, app->rng); }, tr("Mutate"), true);
}

// The inspector's Sound section: the sound's name, Roll new sound and Mutate, and the five knobs turned most
// (for drums: the kit and its effects). It syncs with everything else here.
QGroupBox* SoundPanel::buildQuick() {
    auto* g = new QGroupBox(tr("Sound").toUpper());
    auto* v = new QVBoxLayout(g); v->setContentsMargins(8, 8, 8, 4); v->setSpacing(6);
    quickPages = new QStackedWidget;
    {   // synthesizer
        auto* w = new QWidget; auto* sv = new QVBoxLayout(w); sv->setContentsMargins(0, 0, 0, 0); sv->setSpacing(6);
        auto* top = new QHBoxLayout; top->setSpacing(6);
        QComboBox* pb = makePresetBox(); pb->setMinimumWidth(100);
        top->addWidget(pb, 1);
        auto* mutate = button(tr("Mutate"), tr("Nudge every knob a little"));
        connect(mutate, &QPushButton::clicked, this, [this] { mutateSound(); });
        top->addWidget(mutate);
        sv->addLayout(top);
        auto* roll = button(tr("Roll new sound"), tr("Build a brand new sound that suits this kind of track"));
        roll->setObjectName("primary");
        connect(roll, &QPushButton::clicked, this, [this] { rollSound(); });
        sv->addWidget(roll);
        auto* ks = new QHBoxLayout; ks->setSpacing(0);
        for (int p : {P_CUTOFF, P_RES, P_ENVAMT, P_AD, P_DRIVE}) { Knob* k = patchKnob(p); k->setScale(0.76); ks->addWidget(k); }
        sv->addLayout(ks);
        quickPages->addWidget(w);
    }
    {   // drums
        auto* w = new QWidget; auto* dv = new QVBoxLayout(w); dv->setContentsMargins(0, 0, 0, 0); dv->setSpacing(6);
        dv->addWidget(makeKitBox());
        auto* ks = new QHBoxLayout; ks->setSpacing(0);
        ks->addWidget(kitKnob(tr("Drive"), DP_DRIVE, 0.2f)); ks->addWidget(kitKnob(tr("Echo"), DP_DELAY, 0.05f)); ks->addWidget(kitKnob(tr("Reverb"), DP_REVERB, 0.12f));
        dv->addLayout(ks);
        quickPages->addWidget(w);
    }
    v->addWidget(quickPages);
    theme::makeFoldable(g, QStringLiteral("inspector/sound"), [this] { Track* t = app->curTrack(); return t ? (t->isDrums() ? ui(t->kit.name) : ui(t->patch.name)) : QString(); });
    sync();
    return g;
}

// the How notes are played section, into the inspector (host) or back onto the Sound tab (nullptr)
void SoundPanel::placeHowGroup(QLayout* host) {
    if (host) { howFlow->removeWidget(howGroup); host->addWidget(howGroup); }
    else if (howGroup->parentWidget() != pages->widget(0)) { howGroup->setParent(nullptr); howFlow->insertWidget(1, howGroup); }
    howGroup->show();
}

// The echo and reverb are shared by every track; each track's Echo and Reverb knobs set how much it sends.
QGroupBox* SoundPanel::buildMaster() {
    QHBoxLayout* l = nullptr;
    auto* g = group(tr("Shared echo and reverb"), &l);
    g->setToolTip(tr("One echo and one reverb for the whole song. Each track’s Echo and Reverb knobs set how much of it that track gets."));
    QStringList times; for (const char* c : DTIME_NAMES) times << ui(c);
    auto* et = new QVBoxLayout; et->setSpacing(4);   // label above the box keeps this group narrow
    et->addWidget(dimLabel(tr("Echo time")));
    et->addWidget(combo(times, [this] { return app->song.dtime; }, [this](int i) { app->song.dtime = i; }, tr("Echo time, locked to the tempo")));
    et->addStretch(1);
    l->addLayout(et);
    l->addSpacing(4);
    l->addWidget(freeKnob(tr("Echo repeats"), [this] { return app->song.dfb; }, [this](float x) { app->song.dfb = x; }, pct, 0.45f,
                          tr("How many times the echo repeats before it fades away")));
    l->addWidget(freeKnob(tr("Reverb size"), [this] { return app->song.rsize; }, [this](float x) { app->song.rsize = x; },
                          [](float x) { return QString::number(double(0.5f + x * x * 11.f), 'f', 1) + " s"; }, 0.55f, tr("How large the room sounds: from a small room to a huge hall")));
    l->addWidget(freeKnob(tr("Reverb damping"), [this] { return app->song.rdamp; }, [this](float x) { app->song.rdamp = x; }, pct, 0.45f,
                          tr("How dark the reverb is: high damping takes the brightness out of the echoes of the room")));
    return g;
}

void SoundPanel::sync() {
    Track* t = app->curTrack();
    pages->setCurrentIndex((t && t->isDrums() ? 1 : 0) + (simple ? 2 : 0));
    if (quickPages) quickPages->setCurrentIndex(t && t->isDrums() ? 1 : 0);
    if (howGroup) howGroup->setVisible(t && !t->isDrums());
    pages->setEnabled(t != nullptr);
    for (auto& b : knobs) if (!b.k->isHeld()) b.k->setValue(b.get());
    for (auto& b : combos) { b.c->blockSignals(true); b.c->setCurrentIndex(b.get()); b.c->blockSignals(false); }
    for (QWidget* g : graphs) g->update();
    if (t) {
        int pi = 0;
        for (int i = 0; i < presetBox->count(); i++) { // separators carry no number and are skipped
            const QVariant d = presetBox->itemData(i);
            if (d.isValid() && d.toInt() >= 0 && presets()[size_t(d.toInt())].name == t->patch.name) pi = i;
        }
        for (QComboBox* b : presetBoxes) { b->blockSignals(true); b->setCurrentIndex(pi); b->blockSignals(false); }
        for (QComboBox* b : kitBoxes) { b->blockSignals(true); b->setCurrentIndex(std::max(0, b->findText(ui(t->kit.name)))); b->blockSignals(false); }
        for (auto& [d, b] : sampleBtns) {
            const QString n = t->isDrums() ? t->kit.sample[d] : QString();
            const bool missing = !n.isEmpty() && !t->kit.data[d];
            b->setText(n.isEmpty() ? tr("Built-in") : b->fontMetrics().elidedText(samples::displayName(n), Qt::ElideRight, 86));
            b->setToolTip(n.isEmpty() ? tr("This drum uses its own sound. Click to play one of your samples instead.")
                          : missing ? tr("%1 is not in the sample library, so the built-in sound plays. Click to choose another.").arg(samples::displayName(n))
                                    : tr("This drum plays the sample %1. Tune changes its speed, Decay shortens it. Click to change.").arg(samples::displayName(n)));
            b->setProperty("sampleOn", !n.isEmpty());
            b->style()->unpolish(b); b->style()->polish(b);
        }
        follow->setChecked(t->follow);
        resume->setVisible(t->bypass != 0);
        const QColor col = theme::trackColor(t->role);
        for (auto& b : knobs) b.k->setAccent(col);
        for (auto& [param, k] : patchKnobs) k->setMarked(t->hasMoves(param));

    } else resume->setVisible(false);
}

void SoundPanel::syncMoving() {
    Track* t = app->curTrack();
    if (!t || t->isDrums()) return;
    for (QWidget* g : graphs) g->update();   // the LFO dot runs all the time; the curves follow knob lines
    if (t->lanes.empty()) return;
    for (auto& [param, k] : patchKnobs) if (!k->isHeld() && t->hasMoves(param)) k->setValue(app->paramValue(*t, param));

    for (QWidget* g : graphs) g->update();
}

// ---------------------------------------------------------------- shared pieces

// A list of sounds that plays one while the mouse rests on it in the open list, and goes quiet when the list closes.
namespace {
class HearCombo : public QComboBox {
public:
    // hear gets the list and the row the mouse rests on; quiet is called when the list closes
    HearCombo(std::function<void(QComboBox*, int)> hear, std::function<void()> quiet) : hear_(std::move(hear)), quiet_(std::move(quiet)) {
        wait_.setSingleShot(true); wait_.setInterval(160);   // a short wait, so sweeping down the list is not a jumble of sounds
        QObject::connect(&wait_, &QTimer::timeout, this, [this] { if (view() && view()->isVisible()) hear_(this, pending_); });
        QObject::connect(this, qOverload<int>(&QComboBox::highlighted), this, [this](int i) { pending_ = i; wait_.start(); });
    }
    void hidePopup() override { QComboBox::hidePopup(); wait_.stop(); quiet_(); }
private:
    std::function<void(QComboBox*, int)> hear_;
    std::function<void()> quiet_;
    QTimer wait_;
    int pending_ = -1;
};
}

QComboBox* SoundPanel::makePresetBox() {
    auto* box = new HearCombo([this](QComboBox* b, int row) {
        const QVariant d = b->itemData(row);
        if (d.isValid() && d.toInt() >= 0) app->previewSound(&presets()[size_t(d.toInt())], nullptr);
    }, [this] { app->previewSound(nullptr, nullptr); });
    box->setFocusPolicy(Qt::NoFocus); box->setMinimumWidth(170);
    box->setToolTip(tr("Ready-made sounds. Pick one as a starting point, then change it with the knobs. In the open list, rest the mouse on a sound to hear it."));
    // presets in groups, each under a heading that cannot be picked
    box->addItem(tr("Custom sound"), -1);
    for (const char* group : {"Bass", "Sequence and pluck", "Pad", "Lead"}) {
        box->insertSeparator(box->count());
        box->addItem(ui(group).toUpper(), -2);
        if (auto* model = qobject_cast<QStandardItemModel*>(box->model())) {
            QStandardItem* head = model->item(box->count() - 1);
            head->setFlags(head->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
            QFont hf = head->font(); hf.setPixelSize(10); hf.setBold(true); head->setFont(hf);
        }
        for (size_t i = 0; i < presets().size(); i++)
            if (presetGroup(presets()[i].name) == QLatin1String(group)) box->addItem(QStringLiteral("   ") + ui(presets()[i].name), int(i));
    }
    connect(box, &QComboBox::activated, this, [this, box](int i) {
        const QVariant d = box->itemData(i);
        const int idx = d.isValid() ? d.toInt() : -1;
        if (idx >= 0) app->edit([&](Song& s) { if (Track* t = s.track(app->selTrack)) t->patch = presets()[size_t(idx)]; }, tr("Preset"));
    });
    presetBoxes.push_back(box);
    return box;
}

QComboBox* SoundPanel::makeKitBox() {
    auto* box = new HearCombo([this](QComboBox* b, int row) {
        const QVariant d = b->itemData(row);
        if (d.isValid() && d.toInt() >= 0) app->previewSound(nullptr, &drumKits()[size_t(d.toInt())]);
    }, [this] { app->previewSound(nullptr, nullptr); });
    box->setFocusPolicy(Qt::NoFocus); box->setMinimumWidth(170);
    box->setToolTip(tr("Ready-made drum kits. Pick one, then change each drum with its knobs below. In the open list, rest the mouse on a kit to hear it."));
    box->setProperty("lesson", "kit");
    box->addItem(tr("Custom kit"), -1);
    bool sampledYet = false;
    for (size_t i = 0; i < drumKits().size(); i++) {
        const bool sampled = !drumKits()[i].sample[0].isEmpty();
        if (sampled && !sampledYet) { box->insertSeparator(box->count()); sampledYet = true; }   // synthesized kits first, then the ones made of samples
        box->addItem(ui(drumKits()[i].name), int(i));
    }
    connect(box, &QComboBox::activated, this, [this, box](int i) {
        const int idx = box->itemData(i).isValid() ? box->itemData(i).toInt() : -1;
        if (idx < 0) return;
        app->edit([&](Song& s) {
            Track* t = s.track(app->selTrack);
            if (!t) return;
            t->kit = chooseKit(t->kit, drumKits()[size_t(idx)]);
        }, tr("Drum kit"));
    });
    kitBoxes.push_back(box);
    return box;
}

// Simple or Full: which page the Sound tab shows (View > Simple sound page, or the settings)
void SoundPanel::setSimple(bool on) {
    const bool changed = on != simple;
    simple = on;
    QSettings().setValue(QStringLiteral("simpleSound"), on);
    sync();
    if (changed) emit simpleChanged(on);
}

// ---------------------------------------------------------------- your own samples on the drums

QPushButton* SoundPanel::sampleButton(int drum) {
    auto* b = new QPushButton(tr("Built-in"));
    b->setObjectName("sample");
    b->setFocusPolicy(Qt::NoFocus);
    b->setFixedWidth(96);
    connect(b, &QPushButton::clicked, this, [this, drum, b] { sampleMenu(drum, b); });
    sampleBtns.push_back({drum, b});
    return b;
}

void SoundPanel::sampleMenu(int drum, QPushButton* under) {
    Track* t = app->curTrack();
    if (!t || !t->isDrums()) return;
    const QString now = t->kit.sample[drum];
    QMenu menu;
    menu.setToolTipsVisible(true);
    QAction* builtIn = menu.addAction(tr("Built-in %1").arg(ui(DRUM_NAMES[drum])));
    builtIn->setCheckable(true); builtIn->setChecked(now.isEmpty());
    auto pickable = [&](QMenu* in, const QString& id, const QString& text, const QString& tip) {
        QAction* a = in->addAction(text);
        a->setData(id); a->setCheckable(true); a->setChecked(id == now);
        if (!tip.isEmpty()) a->setToolTip(tip);
        return a;
    };
    // headings the menu style draws everywhere (a section's title is not shown by every style)
    auto heading = [](QMenu* in, const QString& text) {
        in->addSeparator();
        QAction* h = in->addAction(text.toUpper());
        QFont f = h->font(); f.setPointSizeF(f.pointSizeF() * 0.85); f.setBold(true); f.setLetterSpacing(QFont::AbsoluteSpacing, 1.0); h->setFont(f);
        h->setEnabled(false);
    };
    // the Nachtzug library, one submenu per kind of sound; resting on a sound plays it
    heading(&menu, tr("Nachtzug library"));
    QList<QMenu*> subs;
    for (const QString& cat : samples::libraryCategories()) {
        QMenu* sub = menu.addMenu(cat);
        sub->setToolTipsVisible(true);
        bool current = false;
        for (const samples::LibrarySound& ls : samples::library())
            if (ls.category == cat) { pickable(sub, ls.id, ls.name, ls.about); current |= ls.id == now; }
        if (current) sub->setTitle(QStringLiteral("● ") + cat);   // the sound the drum plays now is in here
        subs << sub;
    }
    const QStringList names = samples::list();
    heading(&menu, tr("Your samples"));
    for (const QString& n : names) pickable(&menu, n, samples::displayName(n), QString());
    if (names.isEmpty()) { QAction* none = menu.addAction(tr("(none yet)")); none->setEnabled(false); }
    menu.addSeparator();
    QAction* importA = menu.addAction(tr("Import a sound file…"));
    QAction* recordA = menu.addAction(tr("Record a sample…"));
    QAction* chopA = menu.addAction(tr("Chop a loop across the drums…"));
    chopA->setStatusTip(tr("Cut a drum loop into pieces at its hits and put one on each drum pad"));
    QAction* library = menu.addAction(tr("Sample library…"));
    auto preview = [this](QAction* a) { const QString id = a->data().toString(); if (!id.isEmpty()) app->previewSample(id); };
    connect(&menu, &QMenu::hovered, this, preview);
    for (QMenu* sub : subs) connect(sub, &QMenu::hovered, this, preview);
    QAction* got = menu.exec(under->mapToGlobal(QPoint(0, under->height())));
    app->previewSample(QString());
    if (!got) return;
    QString pick;
    if (got == builtIn) pick = QString();
    else if (got == importA) {
        QSettings st;
        QString dir = st.value(QStringLiteral("samples/importDir")).toString();
        const QString f = QFileDialog::getOpenFileName(this, tr("Import a sound file"), dir, tr("Sound files (*.wav *.aif *.aiff *.aifc *.WAV *.AIF *.AIFF);;All files (*)"));
        if (f.isEmpty()) return;
        st.setValue(QStringLiteral("samples/importDir"), QFileInfo(f).absolutePath());
        QString err;
        pick = samples::import(f, err);
        if (pick.isEmpty()) { QMessageBox::warning(this, tr("Import a sound file"), err); return; }
    } else if (got == recordA) {
        RecordDialog dlg(app, this);
        dlg.exec();
        pick = dlg.savedName();
        if (pick.isEmpty()) return;
    } else if (got == library) { emit openSampleLibrary(); return; }
    else if (got == chopA) { ChopDialog::chooseAndChop(app, this); return; }
    else pick = got->data().toString();
    const int id = app->selTrack;
    app->edit([&](Song& s) {
        if (Track* k = s.track(id)) {
            k->kit.sample[drum] = pick; k->kit.data[drum].reset(); k->kit.name = tr("Custom kit");
            if (!pick.isEmpty()) { k->kit.p[drum * 3 + 1] = 0.5f; k->kit.p[drum * 3 + 2] = 1.f; }   // a new sample starts at its own pitch, playing out in full
        }
    }, tr("Drum sample"));
    app->audition(id, drum, true);
}

// ---------------------------------------------------------------- the simple pages

Knob* SoundPanel::macroKnob(const QString& label, const QString& tip, std::vector<std::pair<int, float>> parts) {
    auto* k = new Knob(label);
    k->setScale(1.45);
    const int primary = parts.front().first;
    k->setProperty("param", primary);
    midiMenuFor(k, app, Midi::targetForParam(primary), label);   // a controller knob turns its main setting
    k->setDefault(PARAMS[primary].def);
    k->setFormatter([](float v) { return pct(v); });
    k->setToolTip(tip + tr("\n\nDrag up or down, Shift for small steps, double-click to reset."));
    auto startV = std::make_shared<float>(0.f), startVoices = std::make_shared<float>(0.f);
    connect(k, &Knob::pressed, this, [this, k, parts, startV, startVoices] {
        Track* t = app->curTrack();
        if (!t) return;
        app->begin(k->name());
        macroBase.clear();
        for (auto& [param, w] : parts) macroBase.push_back(t->patch.p[param]);
        *startV = k->value();
        *startVoices = t->patch.p[P_UNISON];
    });
    connect(k, &Knob::moved, this, [this, parts, startV, startVoices](float v) {
        Track* t = app->curTrack();
        if (!t || macroBase.size() != parts.size()) return;
        const float dv = v - *startV;
        for (size_t i = 0; i < parts.size(); i++) {
            const int param = parts[i].first;
            t->patch.p[param] = std::clamp(macroBase[i] + dv * parts[i].second, 0.f, 1.f);
            if (t->hasMoves(param)) t->bypass |= quint64(1) << param;   // set by hand: its line pauses, as with any knob
        }
        // a thick sound needs more than one voice per note: turning up adds voices, turning down takes them away,
        // so a preset that already has many keeps them until you thin it out
        if (parts.front().first == P_UDETUNE) {
            const float wanted = v < 0.08f ? 0.f : v < 0.45f ? 2.f : 3.f;
            t->patch.p[P_UNISON] = dv >= 0.f ? std::max(*startVoices, wanted) : std::min(*startVoices, wanted);
        }
        app->touch();
    });
    knobs.push_back({k, [this, primary] { Track* t = app->curTrack(); return t ? app->paramValue(*t, primary) : 0.f; }});
    return k;
}

// a big knob with what its two ends do written under it
static QWidget* withEnds(Knob* k, const QString& low, const QString& high) {
    auto* w = new QWidget;
    auto* v = new QVBoxLayout(w); v->setContentsMargins(0, 0, 0, 0); v->setSpacing(0);
    v->addWidget(k, 0, Qt::AlignHCenter);
    auto* ends = new QLabel(QStringLiteral("%1  ↔  %2").arg(low, high));
    ends->setObjectName("dim"); ends->setAlignment(Qt::AlignHCenter);
    v->addWidget(ends);
    return w;
}

QWidget* SoundPanel::buildSimpleSynth() {
    auto* page = new QWidget;
    auto* v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0); v->setSpacing(10);
    QHBoxLayout* l = nullptr;
    auto* top = new QHBoxLayout; top->setSpacing(10);
    top->addWidget(group(tr("Sound"), &l));
    l->setSpacing(8);
    l->addWidget(makePresetBox());
    auto* roll = button(tr("Roll new sound"), tr("Build a brand new sound that suits this kind of track"));
    roll->setObjectName("primary");
    auto* mutate = button(tr("Mutate"), tr("Nudge every knob a little"));
    connect(roll, &QPushButton::clicked, this, [this] { app->edit([&](Song& s) { if (Track* t = s.track(app->selTrack)) t->patch = rollPatch(t->role, app->rng); }, tr("Roll new sound"), true); });
    connect(mutate, &QPushButton::clicked, this, [this] { app->edit([&](Song& s) { if (Track* t = s.track(app->selTrack)) t->patch = mutatePatch(t->patch, app->rng); }, tr("Mutate"), true); });
    l->addWidget(roll); l->addWidget(mutate);
    l->addSpacing(16);
    l->addWidget(dimLabel(tr("Octave")));
    l->addWidget(combo({"-2", "-1", "0", "+1", "+2", "+3", "+4"}, [this] { Track* t = app->curTrack(); return t ? t->octave + 2 : 2; },
                       [this](int i) { if (Track* t = app->curTrack()) t->octave = i - 2; }, tr("Moves this track’s notes, and the letter keys, up or down by whole octaves")));
    l->addStretch(1);
    v->addLayout(top);

    auto* body = new QHBoxLayout; body->setSpacing(10);
    auto* look = group(tr("What it sounds like"), &l);
    l->setSpacing(12);
    auto* filter = static_cast<Graph*>(graph(Graph::Filter)); filter->setFixedSize(220, 140);
    auto* wave = static_cast<Graph*>(graph(Graph::Wave)); wave->setFixedSize(220, 140);
    auto* pics = new QVBoxLayout; pics->setSpacing(8); pics->addWidget(wave); pics->addWidget(filter); pics->addStretch(1);
    l->addLayout(pics);
    body->addWidget(look);

    auto* knobsBox = group(tr("Shape the sound"), &l);
    auto* grid = new QGridLayout; grid->setHorizontalSpacing(18); grid->setVerticalSpacing(14);
    const std::vector<QWidget*> cells = {
        withEnds(macroKnob(tr("Brightness"), tr("Opens or closes the filter: dark and muffled on the left, bright and buzzy on the right."), {{P_CUTOFF, 1.f}}), tr("dark"), tr("bright")),
        withEnds(macroKnob(tr("Squelch"), tr("The acid sound: the filter rings and sweeps on every note. Turn it up on a bass line and listen."),
                           {{P_RES, 1.f}, {P_ENVAMT, 0.7f}, {P_ACCENT, 0.5f}}), tr("smooth"), tr("squelchy")),
        withEnds(macroKnob(tr("Length"), tr("How long each note rings: short plucks on the left, long notes on the right."),
                           {{P_AD, 1.f}, {P_FD, 0.8f}, {P_AR, 0.6f}}), tr("short"), tr("long")),
        withEnds(macroKnob(tr("Thickness"), tr("Stacks slightly detuned copies of the sound and adds a lower octave, for a wide, full sound."),
                           {{P_UDETUNE, 1.f}, {P_SUB, 0.5f}, {P_DETUNE, 0.3f}}), tr("thin"), tr("thick")),
        withEnds(macroKnob(tr("Grit"), tr("Distortion: warmer at first, then rough and loud."), {{P_DRIVE, 1.f}}), tr("clean"), tr("dirty")),
        withEnds(macroKnob(tr("Space"), tr("How much echo and reverb: close and dry on the left, far away in a big hall on the right."),
                           {{P_REVERB, 1.f}, {P_DELAY, 0.6f}}), tr("dry"), tr("far away")),
        withEnds(macroKnob(tr("Movement"), tr("A slow wobble in the filter and a gentle chorus, so the sound breathes."),
                           {{P_LFOCUT, 1.f}, {P_CHORUS, 0.5f}}), tr("still"), tr("moving")),
        withEnds(macroKnob(tr("Glide"), tr("How long notes take to slide from one pitch to the next, like a 303 bass."), {{P_GLIDE, 1.f}}), tr("jumps"), tr("slides")),
    };
    for (size_t i = 0; i < cells.size(); i++) grid->addWidget(cells[i], int(i / 4), int(i % 4), Qt::AlignHCenter);
    l->addLayout(grid, 1);
    body->addWidget(knobsBox, 1);
    v->addLayout(body);
    auto* hint = dimLabel(tr("These big knobs each move several settings at once. For every single knob, switch to Full at the top right."));
    hint->setWordWrap(true);
    v->addWidget(hint);
    v->addStretch(1);
    return page;
}

QWidget* SoundPanel::buildSimpleDrums() {
    auto* page = new QWidget;
    auto* v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0); v->setSpacing(10);
    QHBoxLayout* l = nullptr;
    auto* top = new QHBoxLayout; top->setSpacing(10);
    top->addWidget(group(tr("Drum kit"), &l));
    l->setSpacing(8);
    l->addWidget(makeKitBox());
    l->addWidget(dimLabel(tr("The bottom row of letter keys, Z to M and the comma, plays the eight drums.")));
    l->addStretch(1);
    v->addLayout(top);

    auto* drums = group(tr("How loud each drum is"), &l);
    l->setSpacing(6);
    for (int d = 0; d < D_COUNT; d++) {
        auto* col = new QVBoxLayout; col->setSpacing(4);
        auto* k = freeKnob(ui(DRUM_NAMES[d]), [this, d] { Track* t = app->curTrack(); return t ? t->kit.p[d * 3] : 0.f; },
                           [this, d](float x) { if (Track* t = app->curTrack()) { t->kit.p[d * 3] = x; t->kit.name = tr("Custom kit"); } }, pct, 0.6f,
                           tr("%1: how loud it is").arg(ui(DRUM_NAMES[d])));
        k->setScale(1.3);
        col->addWidget(k, 0, Qt::AlignHCenter);
        auto* hear = button(QStringLiteral("▶"), tr("Hear this drum"));
        hear->setFixedWidth(34);
        connect(hear, &QPushButton::clicked, this, [this, d] { app->audition(app->selTrack, d, true); });
        auto* row = new QHBoxLayout; row->setSpacing(4); row->addStretch(1); row->addWidget(hear); row->addWidget(sampleButton(d)); row->addStretch(1);
        col->addLayout(row);
        col->addStretch(1);
        l->addLayout(col, 1);
    }
    v->addWidget(drums);

    auto* feel = group(tr("The whole kit"), &l);
    l->setSpacing(18);
    auto kitMacro = [this](const QString& label, int idx, float def, const QString& tip, const QString& lo, const QString& hi) {
        auto* k = freeKnob(label, [this, idx] { Track* t = app->curTrack(); return t ? t->kit.p[idx] : 0.f; },
                           [this, idx](float x) { if (Track* t = app->curTrack()) { t->kit.p[idx] = x; t->kit.name = tr("Custom kit"); } }, pct, def, tip);
        k->setScale(1.45);
        return withEnds(k, lo, hi);
    };
    l->addWidget(kitMacro(tr("Grit"), DP_DRIVE, 0.2f, tr("Distortion on the whole kit: warmer at first, grittier further up."), tr("clean"), tr("dirty")));
    l->addWidget(kitMacro(tr("Echo"), DP_DELAY, 0.05f, tr("How much of the kit goes to the shared echo."), tr("none"), tr("lots")));
    l->addWidget(kitMacro(tr("Space"), DP_REVERB, 0.12f, tr("How much of the kit goes to the shared reverb."), tr("dry"), tr("far away")));
    l->addStretch(1);
    v->addWidget(feel);
    auto* hint = dimLabel(tr("Every drum can play your own sound: click Built-in under it. Full, at the top right, has tuning and length for each drum."));
    hint->setWordWrap(true);
    v->addWidget(hint);
    v->addStretch(1);
    return page;
}
