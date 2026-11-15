#include "editor.h"
#include "app.h"
#include "widgets.h"
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <functional>
#include <QHBoxLayout>
#include <QHelpEvent>
#include <QToolTip>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QScrollBar>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>
#include <climits>
#include <cmath>

// ---------------------------------------------------------------- NoteEditor

NoteEditor::NoteEditor(App* a, QWidget* parent) : QAbstractScrollArea(parent), app(a) {
    setFrameShape(QFrame::NoFrame);
    setFocusPolicy(Qt::NoFocus);
    viewport()->setMouseTracking(true);
    setMinimumHeight(100);
    viewport()->setStatusTip(tr("Click to add a note and drag to stretch it. Drag notes to move them. Right-click a note to delete it (keep the button down to wipe several). Shift + drag selects several. Ctrl + wheel zooms."));
}

bool NoteEditor::drums() const {
    const Track* t = app->song.track(app->selTrack);
    Track* owner = nullptr;
    if (app->song.clip(app->editClip, &owner) && owner) return owner->isDrums();
    return t && t->isDrums();
}
// drum rows grow to fill the height, so the eight drums are not squeezed into the top of an empty grid
int NoteEditor::rowH() const {
    if (drums()) return std::max(24, (viewport()->height() - RUL) / dsp::D_COUNT);
    return std::clamp((viewport()->height() - RUL) / (DEG_MAX - DEG_MIN + 1), 15, 22);
}
void NoteEditor::fitWidth() {
    Clip* c = app->curClip();
    if (!c || !autoFit) return;
    ppt = std::clamp(double(viewport()->width() - KEYW - 24) / std::max(24, c->loopLen), 0.12, 8.0);
}
int NoteEditor::tx(double tick) const { return KEYW + int(std::lround(tick * ppt)) - horizontalScrollBar()->value(); }
double NoteEditor::xt(int x) const { return double(x - KEYW + horizontalScrollBar()->value()) / ppt; }
int NoteEditor::degAt(int y) const {
    const int row = std::clamp((y - RUL + verticalScrollBar()->value()) / rowH(), 0, rows() - 1);
    return drums() ? dsp::D_COUNT - 1 - row : DEG_MAX - row;
}
QRect NoteEditor::noteRect(const Note& n) const {
    const int x = tx(n.start), y = RUL + rowOf(n.deg) * rowH() - verticalScrollBar()->value();
    const int w = drums() ? std::max(5, int(app->grid * ppt) - 2) : std::max(4, int(n.len * ppt) - 1);
    return QRect(x + 1, y + 1, w, rowH() - 2);
}
int NoteEditor::hitNote(const QPoint& p, bool* edge) const {
    Clip* c = app->curClip();
    if (!c) return -1;
    for (int i = int(c->notes.size()) - 1; i >= 0; i--) {
        const Note& n = c->notes[size_t(i)];
        if (n.start >= c->loopLen) continue;
        const QRect r = noteRect(n);
        if (r.contains(p)) { if (edge) *edge = !drums() && p.x() > r.right() - 6; return i; }
    }
    return -1;
}

void NoteEditor::updateScroll() {
    Clip* c = app->curClip();
    const int w = c ? int(c->loopLen * ppt) + 40 : 0, h = rows() * rowH();
    horizontalScrollBar()->setRange(0, std::max(0, w - (viewport()->width() - KEYW)));
    horizontalScrollBar()->setPageStep(viewport()->width() - KEYW);
    verticalScrollBar()->setRange(0, std::max(0, h - (viewport()->height() - RUL)));
    verticalScrollBar()->setPageStep(viewport()->height() - RUL);
}

void NoteEditor::clipChanged(bool recentre) {
    Clip* c = app->curClip();
    if (c && shownClip != c->id) autoFit = true;
    fitWidth();
    updateScroll();
    if (c && (recentre || shownClip != c->id) && !drums()) { // bring the notes into view
        int lo = 0, hi = app->song.sc().n;
        if (!c->notes.empty()) { lo = DEG_MAX; hi = DEG_MIN; for (const Note& n : c->notes) { lo = std::min(lo, n.deg); hi = std::max(hi, n.deg); } }
        const int mid = rowOf((lo + hi) / 2) * rowH();
        verticalScrollBar()->setValue(mid - (viewport()->height() - RUL) / 2);
    }
    shownClip = c ? c->id : -1;
    viewport()->update();
}

void NoteEditor::resizeEvent(QResizeEvent*) { fitWidth(); updateScroll(); }

QString NoteEditor::helpAt(const QPoint& p) const {
    Track* t = nullptr;
    Clip* c = app->curClip(&t);
    if (!c || !t) return tr("Press New idea above to get a pattern, or double-click in the timeline to make a clip.");
    if (p.y() < RUL) return c->scene >= 0 ? tr("Beats of the pattern") : tr("Click to move the playhead to this point of the clip");
    if (p.x() < KEYW) return t->isDrums() ? tr("Click to hear this drum") : tr("Click to hear this note. The dark rows are the key note of the scale.");
    if (c->ad.on) return tr("The auto drummer writes these hits. Untick “The drummer plays this clip” on the left to change single hits.");
    bool edge = false;
    const int idx = hitNote(p, &edge);
    if (idx >= 0) {
        const Note& n = c->notes[size_t(idx)];
        QString extra = n.acc ? tr(", accented") : QString();
        if (n.slide && !t->isDrums()) extra += tr(", slides into the next note");
        if (n.chance < 100) extra += n.chance >= 75 ? tr(", plays 3 times in 4") : n.chance >= 50 ? tr(", plays half the time") : tr(", plays 1 time in 4");
        if (n.rep > 1) extra += tr(", played %1 times in quick succession").arg(n.rep);
        if (n.lock) extra += tr(", kept (Vary and New idea leave it alone)");
        if (n.alt && !t->isDrums()) extra += tr(", a right wrong note leaning into %1").arg(app->song.noteName(*t, n.deg));
        if (t->isDrums()) return tr("%1%2. Click to remove this hit; Ctrl + click selects it.").arg(app->song.noteName(*t, n.deg), extra);
        const QString what = app->song.noteName(*t, n.deg, n.alt) + extra;
        return edge ? tr("%1. Drag to make it longer or shorter.").arg(what)
                    : tr("%1. Drag to move it, drag its right edge to change its length, right-click to delete it.").arg(what);
    }
    return t->isDrums() ? tr("Click to add a hit here; right-click for a whole bar of them")
                        : tr("Click to add a note here; keep the button down and drag right to make it longer. Shift + drag selects several notes. Right-click for a bar of arpeggio or chord from this note.")
                          + (t->follow ? tr("\nTinted rows are the notes of the chord, the safest choices.") : QString());
}

bool NoteEditor::viewportEvent(QEvent* e) {
    if (e->type() == QEvent::ToolTip) {
        auto* h = static_cast<QHelpEvent*>(e);
        const QString text = drag == None ? helpAt(h->pos()) : QString();
        if (text.isEmpty()) QToolTip::hideText(); else QToolTip::showText(h->globalPos(), text, viewport());
        return true;
    }
    return QAbstractScrollArea::viewportEvent(e);
}

void NoteEditor::wheelEvent(QWheelEvent* e) {
    if (e->modifiers() & Qt::ControlModifier) {
        autoFit = false;
        ppt = std::clamp(ppt * (e->angleDelta().y() > 0 ? 1.2 : 1 / 1.2), 0.12, 8.0);
        updateScroll(); viewport()->update();
        e->accept();
        return;
    }
    QAbstractScrollArea::wheelEvent(e);
}

void NoteEditor::paintEvent(QPaintEvent*) {
    QPainter p(viewport());
    const int W = viewport()->width(), H = viewport()->height();
    p.fillRect(0, 0, W, H, theme::canvas);
    Track* t = nullptr;
    Clip* c = app->curClip(&t);
    if (!c || !t) {
        p.setPen(theme::dim);
        p.drawText(QRect(0, 0, W, H), Qt::AlignCenter | Qt::TextWordWrap,
                   tr("Nothing to edit yet.\nPress “New idea” above to get a pattern for this track,\nor double-click an empty spot in the timeline below."));
        return;
    }
    const bool dr = t->isDrums();
    const int L = c->loopLen, n = app->song.sc().n, rh = rowH(), sy = verticalScrollBar()->value();
    const QColor col = theme::trackColor(t->role);
    const int xEnd = tx(L);

    // rows
    for (int r = 0; r < rows(); r++) {
        const int y = RUL + r * rh - sy;
        if (y + rh < RUL || y > H) continue;
        const int deg = dr ? dsp::D_COUNT - 1 - r : DEG_MAX - r;
        const bool root = !dr && ((deg % n) + n) % n == 0;
        p.fillRect(KEYW, y, std::max(0, std::min(W, xEnd) - KEYW), rh - 1, root ? theme::laneSel : (dr && r % 2 ? theme::laneSel : theme::lane));
        // On a track that follows the chords, the pattern is written against the chord's root, so the chord's notes
        // are always the same rows: tint them as the safe choices.
        if (!dr && t->follow) {
            const int d = ((deg % n) + n) % n;
            if (d == 0 || d == app->song.q(2) || d == app->song.q(4)) {
                QColor tint = col; tint.setAlpha(root ? 34 : 26);
                p.fillRect(KEYW, y, std::max(0, std::min(W, xEnd) - KEYW), rh - 1, tint);
            }
        }
    }
    // grid lines
    const int g = app->grid;
    for (int tick = 0; tick <= L; tick += g) {
        const int x = tx(tick);
        if (x < KEYW || x > W) continue;
        const bool bar = tick % BAR == 0, beat = tick % BEAT == 0;
        if (!beat && g * ppt < 5) continue;
        p.setPen(bar ? theme::rule : beat ? theme::gridBar : theme::grid);
        p.drawLine(x, RUL, x, H);
    }
    // notes
    p.setRenderHint(QPainter::Antialiasing, false);
    for (const Note& nt : c->notes) {
        if (nt.start >= L || (!dr && (nt.deg < DEG_MIN || nt.deg > DEG_MAX)) || (dr && (nt.deg < 0 || nt.deg >= dsp::D_COUNT))) continue;
        const QRect r = noteRect(nt);
        if (r.right() < KEYW || r.left() > W || r.bottom() < RUL || r.top() > H) continue;
        // accented notes stand out: brighter on dark themes, deeper on light ones
        QColor fill = theme::isLight() ? (nt.acc ? col.darker(135) : col) : (nt.acc ? col.lighter(125) : col.darker(120));
        if (c->ad.on) fill.setAlpha(170);
        if (theme::tactile) theme::drawBlock(p, QRectF(r), fill, 3, !c->ad.on);
        else p.fillRect(r, fill);
        if (nt.chance < 100) { // a note that plays only sometimes looks faded, with a dashed edge
            QColor veil = theme::canvas; veil.setAlpha(int(150 * (1.f - float(nt.chance) / 100.f)) + 40);
            p.fillRect(r, veil);
            p.setPen(QPen(theme::clipInk, 1, Qt::DashLine)); p.setBrush(Qt::NoBrush); p.drawRect(r.adjusted(0, 0, -1, -1));
        }
        if (nt.rep > 1) { // repeats: marks where each quick hit starts, over the stretch they are spread on
            p.setPen(QPen(theme::clipInk, 1));
            const double span = nt.repeatSpan(L, dr) * ppt;
            for (int k = 1; k < nt.rep; k++) { const int x = r.left() + int(span * k / nt.rep); if (x < r.right() - 1) p.drawLine(x, r.top() + 3, x, r.bottom() - 2); }
        }
        if (nt.acc) p.fillRect(r.left() + 2, r.top() + 1, r.width() - 4, 2, theme::ink);
        if (nt.lock && r.width() >= 9 && r.height() >= 10) { // a small padlock in the corner
            p.setRenderHint(QPainter::Antialiasing, true);
            const QRectF body(r.left() + 3, r.bottom() - 6, 6, 4.5);
            p.setPen(QPen(theme::clipInk, 1.1)); p.setBrush(Qt::NoBrush);
            p.drawArc(QRectF(body.left() + 1, body.top() - 3.5, 4, 6), 0, 180 * 16);
            p.setPen(Qt::NoPen); p.setBrush(theme::clipInk); p.drawRect(body);
            p.setRenderHint(QPainter::Antialiasing, false);
        }
        int textX = nt.lock ? 11 : 3;
        if (nt.alt && !dr && r.width() >= 8 && r.height() >= 8) { // a right wrong note: a ♭ or ♯ at its start
            QFont af = font(); af.setPixelSize(std::clamp(r.height() - 1, 8, 13)); af.setWeight(QFont::DemiBold);
            p.setFont(af); p.setPen(theme::clipInk);
            p.drawText(QRect(r.left() + textX - 1, r.top(), 10, r.height()), Qt::AlignCenter, nt.alt < 0 ? QStringLiteral("♭") : QStringLiteral("♯"));
            textX += 9;
        }
        if (nt.chance < 100 && r.width() >= 30 && r.height() >= 12) { // and says how often it plays
            QFont cf = font(); cf.setPixelSize(std::min(10, r.height() - 3)); p.setFont(cf); p.setPen(theme::clipInk);
            p.drawText(r.adjusted(textX, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, tr("%1%").arg(nt.chance));
        }
        if (nt.slide) { // a small arrow: this note glides into the next
            QPolygon tri; tri << QPoint(r.right() - 6, r.top() + 2) << QPoint(r.right(), r.center().y()) << QPoint(r.right() - 6, r.bottom() - 1);
            p.setPen(Qt::NoPen); p.setBrush(theme::clipInk); p.drawPolygon(tri);
        }
        if (nt.sel) { p.setPen(QPen(theme::ink, 2)); p.setBrush(Qt::NoBrush); p.drawRect(r.adjusted(0, 0, -1, -1)); }
    }
    // an empty pattern says what to do
    if (c->notes.empty() && !c->ad.on) {
        QFont hf = font(); hf.setPixelSize(13); p.setFont(hf);
        const QRect box(KEYW + 20, RUL + 20, std::max(200, W - KEYW - 40), 60);
        p.setPen(theme::dim);
        p.drawText(box, Qt::AlignCenter | Qt::TextWordWrap,
                   dr ? tr("No hits yet. Click in the grid to add one, or press New idea above.")
                      : tr("No notes yet. Click in the grid to add one, press New idea above, or play the letter keys with record on.\nThe tinted rows are the notes of the chord: they always fit."));
    }
    // playhead
    const double m = app->musicalPos();
    if (c->scene >= 0 ? (t->liveCell == c->scene && app->isPlaying()) : (m >= c->start && m < c->end())) {
        const int x = tx(std::fmod(c->scene >= 0 ? m : m - c->start + c->offset, double(L)));
        if (x >= KEYW) { p.setPen(QPen(theme::ink, 1)); p.drawLine(x, RUL, x, H); }
    }
    if (drag == Band) { p.setPen(QPen(theme::ink, 1, Qt::DashLine)); p.setBrush(QColor(255, 255, 255, 30)); p.drawRect(band); }

    // ruler
    p.fillRect(0, 0, W, RUL, theme::well);
    QFont f = font(); f.setPixelSize(11); p.setFont(f);
    for (int tick = 0; tick < L; tick += BEAT) {
        const int x = tx(tick);
        if (x < KEYW - 30 || x > W) continue;
        const bool bar = tick % BAR == 0;
        if (!bar && BEAT * ppt < 34) continue;
        p.setPen(bar ? theme::ink : theme::dim);
        p.drawText(x + 4, 14, bar ? QString::number(tick / BAR + 1) : QStringLiteral("%1.%2").arg(tick / BAR + 1).arg(tick % BAR / BEAT + 1));
    }
    // key column
    p.fillRect(0, 0, KEYW, H, theme::panel);
    for (int r = 0; r < rows(); r++) {
        const int y = RUL + r * rh - sy;
        if (y + rh < RUL || y > H) continue;
        const int deg = dr ? dsp::D_COUNT - 1 - r : DEG_MAX - r;
        const bool root = !dr && ((deg % n) + n) % n == 0;
        if (theme::tactile) { // keys like a keyboard's: cream with a darker edge, the key note in black
            QLinearGradient kg(2, 0, KEYW - 2, 0);
            const QColor base = root ? theme::cap : QColor(0xf4, 0xef, 0xe5);
            kg.setColorAt(0, base.darker(root ? 100 : 104)); kg.setColorAt(0.85, base); kg.setColorAt(1, base.darker(root ? 130 : 112));
            p.fillRect(2, y, KEYW - 4, rh - 1, kg);
            p.fillRect(2, y + rh - 2, KEYW - 4, 1, theme::rule);
        } else p.fillRect(2, y, KEYW - 4, rh - 1, root ? theme::cap : theme::well);
        p.setPen(root ? theme::capInk : dr ? theme::ink : theme::dim);
        p.drawText(QRect(8, y, KEYW - 12, rh - 1), Qt::AlignVCenter | Qt::AlignLeft, app->song.noteName(*t, deg));
    }
    p.fillRect(0, 0, KEYW, RUL, theme::panel);
    theme::drawSunken(p, QRectF(KEYW, RUL, W - KEYW, H - RUL));
    if (app->focus == App::FocusEditor) p.fillRect(0, 0, W, 2, theme::amber);   // Edit-menu commands act on this view
}

void NoteEditor::mousePressEvent(QMouseEvent* e) {
    app->focus = App::FocusEditor;
    Track* t = nullptr;
    Clip* c = app->curClip(&t);
    if (!c || !t) return;
    const QPoint p = e->pos();
    if (e->button() == Qt::RightButton) { // right button: delete, and keep deleting while it is held
        if (p.x() < KEYW || p.y() < RUL) return;
        if (c->ad.on) { emit app->status(tr("The auto drummer writes this clip. Untick “The drummer plays this clip” to change single hits.")); return; }
        drag = Erase; began = false; rightPress = p;
        eraseAt(p);
        return;
    }
    if (e->button() != Qt::LeftButton) return;
    const int L = c->loopLen, g = app->grid;
    if (p.y() < RUL) { // click the ruler: move the playhead inside this clip
        if (p.x() >= KEYW) {
            const int rel = ((int(xt(p.x())) / g * g - c->offset) % L + L) % L;
            if (c->scene < 0 && rel < c->len) app->seek(c->start + rel);
        }
        return;
    }
    if (p.x() < KEYW) { app->audition(t->id, degAt(p.y())); return; }
    if (c->ad.on) { emit app->status(tr("The auto drummer writes this clip. Untick “The drummer plays this clip” to change single hits.")); return; }

    bool edge = false;
    const int idx = hitNote(p, &edge);
    if (idx >= 0) {
        Note& n = c->notes[size_t(idx)];
        if (e->modifiers() & Qt::ControlModifier) { n.sel = !n.sel; viewport()->update(); return; }
        if (t->isDrums()) { app->begin(tr("Remove a hit")); c->notes.erase(c->notes.begin() + idx); app->touch(); return; }
        if (!n.sel) { for (Note& o : c->notes) o.sel = false; n.sel = true; }
        drag = edge ? Resize : Move;
        orig = c->notes; pressTick = xt(p.x()); pressDeg = degAt(p.y()); began = false; lastDt = lastDd = 0; drawIdx = idx;
        app->audition(t->id, n.deg, n.acc, n.alt);
        viewport()->update();
        return;
    }
    if (e->modifiers() & Qt::ShiftModifier) { drag = Band; bandStart = p; band = QRect(p, p); return; }
    const int tick = int(std::floor(xt(p.x()) / g)) * g, deg = degAt(p.y());
    if (tick < 0 || tick >= L) return;
    app->begin(t->isDrums() ? tr("Add a hit") : tr("Add a note"));
    for (Note& o : c->notes) o.sel = false;
    Note n; n.start = tick; n.deg = deg; n.len = t->isDrums() ? 12 : std::min(lastLen, L - tick); n.sel = !t->isDrums();
    c->notes.push_back(n);
    app->touch();
    app->audition(t->id, deg);
    if (!t->isDrums()) { drag = Draw; drawIdx = int(c->notes.size()) - 1; began = true; }
}

void NoteEditor::mouseMoveEvent(QMouseEvent* e) {
    Track* t = nullptr;
    Clip* c = app->curClip(&t);
    const QPoint p = e->pos();
    if (!c || !t) return;
    const int L = c->loopLen, g = app->grid;
    if (drag == None) {
        bool edge = false;
        viewport()->setCursor(hitNote(p, &edge) >= 0 && edge ? Qt::SizeHorCursor : Qt::ArrowCursor);
        return;
    }
    if (drag == Erase) { eraseAt(p); return; }
    if (drag == Band) { band = QRect(bandStart, p).normalized(); viewport()->update(); return; }
    if (drag == Draw) {
        if (drawIdx < 0 || drawIdx >= int(c->notes.size())) return;
        Note& n = c->notes[size_t(drawIdx)];
        const int len = std::clamp(int(std::ceil((xt(p.x()) - n.start) / g)) * g, g, L - n.start);
        if (len != n.len) { n.len = len; lastLen = len; app->touch(); }
        return;
    }
    if (orig.size() != c->notes.size()) { drag = None; return; }
    int dt = int(std::lround((xt(p.x()) - pressTick) / g)) * g, dd = drag == Move ? degAt(p.y()) - pressDeg : 0;
    if (drag == Move) { // keep the whole selection inside the clip and the pitch range
        int minS = INT_MAX, maxS = 0, minD = INT_MAX, maxD = INT_MIN;
        for (const Note& o : orig) if (o.sel) { minS = std::min(minS, o.start); maxS = std::max(maxS, o.start); minD = std::min(minD, o.deg); maxD = std::max(maxD, o.deg); }
        dt = std::clamp(dt, -minS, std::max(-minS, L - g - maxS));
        dd = std::clamp(dd, DEG_MIN - minD, DEG_MAX - maxD);
    }
    if (dt == lastDt && dd == lastDd) return;
    if (!began) { app->begin(drag == Move ? tr("Move notes") : tr("Note length")); began = true; }
    for (size_t i = 0; i < orig.size(); i++) {
        if (!orig[i].sel) continue;
        Note& n = c->notes[i];
        if (drag == Move) { n.start = orig[i].start + dt; n.deg = orig[i].deg + dd; }
        else { n.len = std::clamp(orig[i].len + dt, std::min(g, 12), std::max(std::min(g, 12), L - n.start)); if (int(i) == drawIdx) lastLen = n.len; }
    }
    if (dd != lastDd && drawIdx >= 0 && drawIdx < int(c->notes.size())) app->audition(t->id, c->notes[size_t(drawIdx)].deg, false, c->notes[size_t(drawIdx)].alt);
    lastDt = dt; lastDd = dd;
    app->touch();
}

void NoteEditor::mouseReleaseEvent(QMouseEvent* e) {
    if (drag == Erase && !began && (e->pos() - rightPress).manhattanLength() < 5) { // a right-click on nothing
        drag = None;
        stampMenu(e->pos(), e->globalPosition().toPoint());
        return;
    }
    if (drag == Band) {
        if (Clip* c = app->curClip()) {
            const bool add = e->modifiers() & Qt::ControlModifier;
            for (Note& n : c->notes) { const bool in = noteRect(n).intersects(band) && n.start < c->loopLen; n.sel = in || (add && n.sel); }
        }
    }
    drag = None;
    viewport()->update();
}

void NoteEditor::stampMenu(const QPoint& p, const QPoint& global) {
    Track* t = nullptr;
    Clip* c = app->curClip(&t);
    if (!c || !t) return;
    const Song& s = app->song;
    const int L = c->loopLen, deg = degAt(p.y());
    const int step = std::clamp(app->grid, 12, 48);
    const int tick = std::clamp(int(std::floor(xt(p.x()) / step)) * step, 0, L - step);
    const int end = std::min(L, tick + BAR);
    const bool dr = t->isDrums();
    QMenu menu(this);
    menu.addSection(dr ? tr("A bar of %1").arg(s.noteName(*t, deg)) : tr("A bar from %1").arg(s.noteName(*t, deg)));
    // the chord of the scale built on this note: root, third, fifth and the octave, kept inside the editor's range
    std::vector<int> chord{deg, deg + s.q(2), deg + s.q(4), deg + s.sc().n};
    std::erase_if(chord, [](int d) { return d > DEG_MAX; });
    struct Stamp { QString name; std::function<void(std::vector<Note>&)> make; };
    auto note = [](int start, int len, int d, bool acc = false) { Note n; n.start = start; n.len = len; n.deg = d; n.acc = acc; n.sel = true; return n; };
    auto run = [&, chord](const std::vector<int>& order) {
        return [=](std::vector<Note>& out) {
            int i = 0;
            for (int at = tick; at < end; at += step, i++) out.push_back(note(at, std::max(6, step * 3 / 4), order[size_t(i) % order.size()], i == 0));
        };
    };
    std::vector<Stamp> stamps;
    if (dr) {
        auto every = [&](int gap, int from) { return [=](std::vector<Note>& out) { for (int at = tick + from; at < end; at += gap) out.push_back(note(at, 12, deg, at == tick)); }; };
        stamps = {{tr("On every beat"), every(BEAT, 0)}, {tr("Every eighth"), every(BEAT / 2, 0)}, {tr("Every sixteenth"), every(BEAT / 4, 0)},
                  {tr("On the offbeats"), every(BEAT, BEAT / 2)}};
    } else {
        std::vector<int> down(chord.rbegin(), chord.rend()), upDown = chord;
        for (size_t i = chord.size() - 1; i-- > 1;) upDown.push_back(chord[i]);
        std::vector<int> rolling;   // the Berlin School roll: root, fifth, octave, fifth, third, fifth, octave, fifth
        if (chord.size() == 4) rolling = {chord[0], chord[2], chord[3], chord[2], chord[1], chord[2], chord[3], chord[2]};
        stamps = {{tr("Arpeggio up"), run(chord)}, {tr("Arpeggio down"), run(down)}, {tr("Arpeggio up and down"), run(upDown)}};
        if (!rolling.empty()) stamps.push_back({tr("Rolling sequence"), run(rolling)});
        stamps.push_back({tr("Chord, held for the bar"), [=](std::vector<Note>& out) { for (size_t i = 0; i < std::min<size_t>(3, chord.size()); i++) out.push_back(note(tick, end - tick, chord[i])); }});
        stamps.push_back({tr("Chord stabs on the beats"), [=](std::vector<Note>& out) {
            for (int at = tick; at < end; at += BEAT) for (size_t i = 0; i < std::min<size_t>(3, chord.size()); i++) out.push_back(note(at, BEAT / 3, chord[i], at == tick));
        }});
    }
    QList<QAction*> acts;
    for (const Stamp& st : stamps) acts << menu.addAction(st.name);
    QAction* got = menu.exec(global);
    const int i = int(acts.indexOf(got));
    if (i < 0) return;
    std::vector<Note> made;
    stamps[size_t(i)].make(made);
    const int id = app->editClip;
    app->edit([&](Song& sg) {
        Clip* k = sg.clip(id);
        if (!k) return;
        for (Note& n : k->notes) n.sel = false;
        // what was in the way on those rows goes, so the stamp is heard as it is
        std::erase_if(k->notes, [&](const Note& n) {
            return !n.lock && n.start >= tick && n.start < end && std::any_of(made.begin(), made.end(), [&](const Note& m) { return m.deg == n.deg && m.start == n.start; });
        });
        k->notes.insert(k->notes.end(), made.begin(), made.end());
    }, dr ? tr("Add a bar of hits") : tr("Add an arpeggio"));
    app->audition(t->id, deg);
}

void NoteEditor::eraseAt(const QPoint& p) {
    Clip* c = app->curClip();
    const int idx = hitNote(p, nullptr);
    if (!c || idx < 0) return;
    if (!began) { app->begin(tr("Delete notes")); began = true; }
    c->notes.erase(c->notes.begin() + idx);
    app->touch();
}

// ---------------------------------------------------------------- KeyPads

// the letters printed on the pads; the keys themselves are found by position, so any layout works
static const char* const PAD_LETTERS[3] = {QT_TRANSLATE_NOOP("Model", "ZXCVBNM,./"), QT_TRANSLATE_NOOP("Model", "ASDFGHJKL;"), QT_TRANSLATE_NOOP("Model", "QWERTYUIOP")};

KeyPads::KeyPads(App* a, QWidget* parent) : QWidget(parent), app(a) {
    setFixedHeight(sizeHint().height());
    setMouseTracking(false);
    setToolTip(tr("The letter keys on your computer keyboard and the notes they play, always in the song’s scale.\nClick a pad to hear it, or type the letters. The darker pads are the key note.\nHold Shift for the right wrong notes: a semitone outside the scale, leaning into the pad’s own note."));
}

bool KeyPads::drumsOnly() const { const Track* t = app->song.track(app->selTrack); return t && t->isDrums() && !app->chordKeys(); }

QRectF KeyPads::padRect(int row, int col) const {
    const double label = 64, gap = 4, w = (width() - label - 8 - gap * 9) / 10.0, h = 26;
    const int line = drumsOnly() ? 0 : 2 - row;       // top line: the high row; a drum track shows only its one row
    return QRectF(label + col * (w + gap), 4 + line * (h + gap), w, h);
}

bool KeyPads::padAt(const QPointF& pos, int* row, int* col) const {
    for (int r = 0; r < (drumsOnly() ? 1 : 3); r++) for (int c = 0; c < 10; c++) if (padRect(r, c).contains(pos)) { *row = r; *col = c; return true; }
    return false;
}

void KeyPads::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const Track* t = app->song.track(app->selTrack);
    const bool chords = app->chordKeys();
    const bool drumRow = drumsOnly();
    QFont f = font(); f.setPixelSize(11); p.setFont(f);
    p.setPen(theme::dim);
    for (int r = 0; r < (drumRow ? 1 : 3); r++) {
        static const char* rowNames[3] = {QT_TRANSLATE_NOOP("Model", "Low"), QT_TRANSLATE_NOOP("Model", "Middle"), QT_TRANSLATE_NOOP("Model", "High")};
        p.drawText(QRectF(0, padRect(r, 0).top(), 58, 26), Qt::AlignVCenter | Qt::AlignRight, drumRow ? (r == 0 ? tr("Drums") : QString()) : ui(rowNames[r]));
    }
    if (!t && !chords) return;
    const int n = app->song.sc().n;
    const int live = app->liveChord();
    for (int r = 0; r < (drumRow ? 1 : 3); r++) {
        for (int c = 0; c < 10; c++) {
            const QRectF pr = padRect(r, c);
            const int code = r * 16 + c;
            const int deg = drumRow ? c % dsp::D_COUNT : App::keyDegree(r, c, n);
            // in Chords mode the pad of the chord playing stays lit, as the chord does
            const bool held = app->keyHeld(code) || app->keyHeld(100 + code) || (chords && deg == live);
            const bool root = !drumRow && ((deg % n) + n) % n == 0;
            const bool unused = drumRow && (r != 0 || c >= dsp::D_COUNT);
            if (unused) { p.setPen(QPen(theme::gridBar, 1, Qt::DotLine)); p.setBrush(Qt::NoBrush); p.drawRoundedRect(pr, 3, 3); continue; }
            const QColor on = chords ? theme::amber : theme::trackColor(t->role);
            if (theme::tactile) theme::drawKey(p, pr, QString(), held, on);
            else { p.setPen(QPen(theme::rule, 1)); p.setBrush(held ? on : theme::wellHi); p.drawRoundedRect(pr, 3, 3); }
            if (root && !held) { p.setPen(Qt::NoPen); p.setBrush(theme::cap); p.drawRoundedRect(pr.adjusted(2, 2, -2, -4), 2, 2); }
            const QColor text = held ? theme::accentInk : root ? theme::capInk : theme::ink;
            QFont nf = font(); nf.setPixelSize(11); nf.setWeight(QFont::DemiBold); p.setFont(nf); p.setPen(text);
            // a chord is named by its root and kind; the octave tells the rows apart
            const int oct = deg >= 0 ? deg / n : -((-deg + n - 1) / n);
            const bool bent = app->shiftHeld && !drumRow;   // Shift: the right wrong note (or chord) of each key
            const QString name = chords ? app->song.chordName(deg, bent ? -1 : 0) + (oct > 0 ? QStringLiteral(" ") + QString(oct, QChar(0x2191)) : oct < 0 ? QStringLiteral(" ") + QString(-oct, QChar(0x2193)) : QString())
                                        : app->song.noteName(*t, deg, bent ? app->song.approachAlt(deg) : 0);
            p.drawText(pr.adjusted(5, 0, -4, -2), Qt::AlignVCenter | Qt::AlignLeft, p.fontMetrics().elidedText(name, Qt::ElideRight, int(pr.width()) - 22));
            QFont lf = theme::lcdFont(10); p.setFont(lf); p.setPen(held ? theme::accentInk : root ? theme::capInk : theme::dim);
            p.drawText(pr.adjusted(4, 0, -5, -2), Qt::AlignVCenter | Qt::AlignRight, ui(PAD_LETTERS[r]).mid(c, 1));
        }
    }
}

void KeyPads::mousePressEvent(QMouseEvent* e) {
    int r = 0, c = 0;
    if (e->button() != Qt::LeftButton || !padAt(e->position(), &r, &c)) return;
    heldCode = 100 + r * 16 + c;           // its own number, so a typed key and a clicked pad never cancel each other
    app->keyDown(heldCode, r, c, e->modifiers() & Qt::ShiftModifier);
    update();
}
void KeyPads::mouseMoveEvent(QMouseEvent* e) { // slide across the pads to play them one after another
    if (heldCode < 0) return;
    int r = 0, c = 0;
    if (!padAt(e->position(), &r, &c) || 100 + r * 16 + c == heldCode) return;
    app->keyUp(heldCode);
    heldCode = 100 + r * 16 + c;
    app->keyDown(heldCode, r, c, e->modifiers() & Qt::ShiftModifier);
    update();
}
void KeyPads::mouseReleaseEvent(QMouseEvent*) { if (heldCode >= 0) { app->keyUp(heldCode); heldCode = -1; update(); } }

// ---------------------------------------------------------------- EditorPanel

static QString selectHint(App* app) {
    const Track* t = nullptr;
    Track* owner = nullptr;
    if (app->song.clip(app->editClip, &owner)) t = owner;
    if (t && t->isDrums()) return EditorPanel::tr("Select hits first: hold Shift and drag a box around them, or hold Ctrl and click them one by one.");
    return EditorPanel::tr("Select notes first: click one, or hold Shift and drag a box around several.");
}

static QPushButton* button(const QString& text, const QString& tip) {
    auto* b = new QPushButton(text);
    b->setFocusPolicy(Qt::NoFocus);
    b->setToolTip(tip); b->setStatusTip(tip);
    return b;
}
static QLabel* dimLabel(const QString& text) { auto* l = new QLabel(text); l->setObjectName("dim"); return l; }

static QGroupBox* section(const QString& name, QHBoxLayout** lay) {
    auto* g = new QGroupBox(name.toUpper());
    *lay = new QHBoxLayout(g);
    (*lay)->setContentsMargins(8, 6, 8, 6); (*lay)->setSpacing(6);
    return g;
}
static QSlider* slider(int width, const QString& tip) {
    auto* s = new QSlider(Qt::Horizontal);
    s->setRange(0, 100); s->setFixedWidth(width); s->setFocusPolicy(Qt::NoFocus); s->setToolTip(tip);
    return s;
}

// Layout: one row of three groups (Ideas, Pattern, Selected notes) above the grid.
// For auto-drummer clips the drummer's settings sit in a panel to the left of the grid.
EditorPanel::EditorPanel(App* a, QWidget* parent) : QWidget(parent), app(a) {
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(10, 8, 10, 0); v->setSpacing(8);
    // the three tool groups sit side by side, and wrap onto a second row when the editor is narrow
    toolBox = new QWidget;
    auto* row = new FlowLayout(toolBox, 10, 8);
    QHBoxLayout* l = nullptr;

    row->addWidget(section(tr("Ideas"), &l));
    ideas = new QToolButton; ideas->setText(tr("New idea ▾")); ideas->setPopupMode(QToolButton::InstantPopup); ideas->setFocusPolicy(Qt::NoFocus);
    ideas->setObjectName("primary");
    ideas->setProperty("lesson", "ideas");
    ideas->setToolTip(tr("Let the program write a new pattern for this clip. While the song plays, it starts on the next bar."));
    l->addWidget(ideas);
    vary = button(tr("Vary"), tr("Change a note or two. Press it again for more."));
    vary->setProperty("lesson", "vary");
    l->addWidget(vary);
    l->addSpacing(6);
    l->addWidget(dimLabel(tr("Calm")));
    busy = slider(60, tr("How busy new ideas are"));
    busy->setValue(int(app->density * 100));
    l->addWidget(busy);
    l->addWidget(dimLabel(tr("Busy")));

    row->addWidget(section(tr("Pattern"), &l));
    l->addWidget(dimLabel(tr("Length")));
    length = new QComboBox; length->setFocusPolicy(Qt::NoFocus);
    for (auto [name, ticks] : {std::pair{QT_TRANSLATE_NOOP("Model", "½ bar"), BAR / 2}, {QT_TRANSLATE_NOOP("Model", "1 bar"), BAR}, {QT_TRANSLATE_NOOP("Model", "2 bars"), 2 * BAR}, {QT_TRANSLATE_NOOP("Model", "4 bars"), 4 * BAR}, {QT_TRANSLATE_NOOP("Model", "8 bars"), 8 * BAR}}) length->addItem(ui(name), ticks);
    length->setToolTip(tr("How long the pattern is before it repeats"));
    l->addWidget(length);
    l->addWidget(dimLabel(tr("Grid")));
    gridBox = new QComboBox; gridBox->setFocusPolicy(Qt::NoFocus);
    for (auto [name, ticks] : {std::pair{"1/4", 96}, {"1/8", 48}, {"1/8 triplet", 32}, {"1/16", 24}, {"1/16 triplet", 16}, {"1/32", 12}}) gridBox->addItem(QString::fromLatin1(name), ticks);
    gridBox->setCurrentIndex(3);
    gridBox->setToolTip(tr("Notes you draw, move or record snap to this grid"));
    l->addWidget(gridBox);
    tidy = button(tr("Clean up"), tr("Tidy up what you played in: notes go onto the grid, doubled notes become one, and a note pressed by accident goes. When the pattern repeats, a note you missed in one round is filled in and a wrong key is put right."));
    tidy->setProperty("lesson", "cleanUp");
    l->addWidget(tidy);
    l->addSpacing(12);
    clear = button(tr("Clear"), tr("Remove every note in this pattern"));   // set a little apart: it wipes the pattern
    l->addWidget(clear);

    notesGroup = section(tr("Notes"), &l);
    notesGroup->setToolTip(tr("These act on the selected notes, or on every note when none are selected"));
    row->addWidget(notesGroup);
    accent = button(tr("Accent"), tr("Make the selected notes louder and brighter"));
    slide = button(tr("Slide"), tr("Make the selected notes glide into the next note"));
    // how often a note plays, and quick repeats: small surprises that keep a short loop alive
    auto dropButton = [](const QString& text, const QString& tip) {
        auto* b = new QToolButton; b->setText(text + QStringLiteral(" ▾")); b->setPopupMode(QToolButton::InstantPopup);
        b->setFocusPolicy(Qt::NoFocus); b->setToolTip(tip); b->setStatusTip(tip);
        return b;
    };
    chance = dropButton(tr("Chance"), tr("How often the selected notes play. A hi-hat that plays only now and then keeps a loop from sounding the same every time."));
    repeat = dropButton(tr("Repeat"), tr("Play the selected notes two, three or four times in quick succession over their length: a drum roll, or a stuttering acid note"));
    chance->setProperty("lesson", "chance");
    lean = dropButton(tr("Lean"), tr("Make the selected notes “right wrong notes”: a semitone outside the scale, leaning into the note of their row. Holding Shift while you play the letter keys does the same."));
    keep = button(tr("Keep"), tr("Keep the selected notes: Vary and New idea leave them as they are and write around them. Press again to let them go."));
    up = button(QStringLiteral("▲"), tr("Move notes one scale step up (selected notes, or all)"));
    down = button(QStringLiteral("▼"), tr("Move notes one scale step down (selected notes, or all)"));
    octUp = button(QStringLiteral("▲"), tr("Move notes one octave up (selected notes, or all)"));
    octDown = button(QStringLiteral("▼"), tr("Move notes one octave down (selected notes, or all)"));
    for (QPushButton* b : {up, down, octUp, octDown}) { b->setFixedWidth(27); b->setStyleSheet(QStringLiteral("padding: 0;")); }
    l->addWidget(accent); l->addWidget(slide);
    l->addSpacing(3);
    l->addWidget(chance); l->addWidget(repeat); l->addWidget(lean); l->addWidget(keep);
    l->addSpacing(3);
    auto pair = [&](const QString& name, QPushButton* a, QPushButton* b) {
        auto* w = new QWidget; auto* h = new QHBoxLayout(w); h->setContentsMargins(0, 0, 0, 0); h->setSpacing(3);
        h->addWidget(dimLabel(name)); h->addSpacing(2); h->addWidget(a); h->addWidget(b);
        l->addWidget(w);
        return w;
    };
    stepBox = pair(tr("Step"), up, down);
    l->addSpacing(2);
    octBox = pair(tr("Octave"), octUp, octDown);
    {   // the menus of the two drop-down keys
        auto* cm = new QMenu(chance);
        for (auto [pct, name] : {std::pair{100, QT_TR_NOOP("Always")}, {75, QT_TR_NOOP("Often (3 times in 4)")}, {50, QT_TR_NOOP("Half the time")}, {25, QT_TR_NOOP("Now and then (1 time in 4)")}}) {
            const int v = pct;
            cm->addAction(tr(name), this, [this, v] { onSelected(tr("Chance"), [v](Note& n) { n.chance = v; }); });
        }
        chance->setMenu(cm);
        auto* rm = new QMenu(repeat);
        for (auto [times, name] : {std::pair{1, QT_TR_NOOP("Once")}, {2, QT_TR_NOOP("Twice")}, {3, QT_TR_NOOP("3 times")}, {4, QT_TR_NOOP("4 times")}}) {
            const int v = times;
            rm->addAction(tr(name), this, [this, v] { onSelected(tr("Repeat"), [v](Note& n) { n.rep = v; }); });
        }
        repeat->setMenu(rm);
        auto* lm = new QMenu(lean);
        lm->addAction(tr("In the scale"), this, [this] { onSelected(tr("Lean"), [](Note& n) { n.alt = 0; }); });
        lm->addAction(tr("The right wrong note"), this, [this] { onSelected(tr("Lean"), [this](Note& n) { n.alt = app->song.approachAlt(n.deg); }); });
        lm->addAction(tr("A semitone lower (♭)"), this, [this] { onSelected(tr("Lean"), [](Note& n) { n.alt = -1; }); });
        lm->addAction(tr("A semitone higher (♯)"), this, [this] { onSelected(tr("Lean"), [](Note& n) { n.alt = 1; }); });
        lean->setMenu(lm);
    }
    v->addWidget(toolBox);

    // the drummer's settings, as a form
    auto* body = new QHBoxLayout; body->setSpacing(10);
    auto* drummer = new QGroupBox(tr("AUTO DRUMMER"));
    drummer->setObjectName("drummerBox");
    drummer->setFixedWidth(300);
    auto* dv = new QVBoxLayout(drummer); dv->setContentsMargins(10, 10, 10, 8); dv->setSpacing(8);
    autoBox = new QCheckBox(tr("The drummer plays this clip")); autoBox->setFocusPolicy(Qt::NoFocus);
    autoBox->setToolTip(tr("On: the program writes the drum hits from the settings below.\nOff: you place the hits yourself in the grid."));
    dv->addWidget(autoBox);
    drummerBody = new QWidget;
    dv->addWidget(drummerBody);
    auto* form = new QFormLayout(drummerBody);
    form->setContentsMargins(0, 0, 0, 0); form->setVerticalSpacing(7); form->setHorizontalSpacing(10);
    // New take first, so it is in sight however short the window is
    auto* take = button(tr("New take"), tr("Same settings, different playing"));
    take->setObjectName("primary");
    take->setProperty("lesson", "drumTake");
    form->addRow(take);
    auto mk = [&](const QString& label, std::initializer_list<const char*> items, const QString& tip) {
        auto* c = new QComboBox; c->setFocusPolicy(Qt::NoFocus); c->setToolTip(tip);
        for (const char* i : items) c->addItem(ui(i));
        form->addRow(label, c);
        return c;
    };
    style = new QComboBox; style->setFocusPolicy(Qt::NoFocus);
    style->setToolTip(tr("The kind of beat the drummer plays"));
    for (const char* s : DRUM_STYLES) style->addItem(ui(s));
    form->addRow(tr("Style"), style);
    style->setProperty("lesson", "drumStyle");
    intensity = slider(150, tr("How hard the drummer plays: from a bare pulse up to full hats, crashes and extra kicks"));
    form->addRow(tr("Energy"), intensity);
    intensity->setProperty("lesson", "drumEnergy");
    complexity = slider(150, tr("How many ghost notes and surprises the drummer adds"));
    form->addRow(tr("Playfulness"), complexity);
    shuffle = slider(150, tr("Shuffle: plays every second sixteenth a little late for a swinging feel"));
    form->addRow(tr("Shuffle"), shuffle);
    fills = new QComboBox; fills->setFocusPolicy(Qt::NoFocus);
    fills->setToolTip(tr("How often the drummer plays a fill. A fill always comes at the end of the clip too."));
    for (auto [name, n] : {std::pair{QT_TRANSLATE_NOOP("Model", "Never"), 0}, {QT_TRANSLATE_NOOP("Model", "Every 2 bars"), 2}, {QT_TRANSLATE_NOOP("Model", "Every 4 bars"), 4}, {QT_TRANSLATE_NOOP("Model", "Every 8 bars"), 8}}) fills->addItem(ui(name), n);
    form->addRow(tr("Fills"), fills);
    fills->setProperty("lesson", "drumFills");
    backbeat = mk(tr("Backbeat"), {QT_TRANSLATE_NOOP("Model", "Style’s own"), QT_TRANSLATE_NOOP("Model", "Clap"), QT_TRANSLATE_NOOP("Model", "Snare"), QT_TRANSLATE_NOOP("Model", "Rim"), QT_TRANSLATE_NOOP("Model", "None")}, tr("Which drum plays the backbeat (beats 2 and 4)"));
    hatsBox = mk(tr("Hi-hats"), {QT_TRANSLATE_NOOP("Model", "Style’s own"), QT_TRANSLATE_NOOP("Model", "Eighths"), QT_TRANSLATE_NOOP("Model", "Sixteenths"), QT_TRANSLATE_NOOP("Model", "Offbeat only"), QT_TRANSLATE_NOOP("Model", "None")}, tr("What the hi-hats play"));
    shape = mk(tr("Over the clip"), {QT_TRANSLATE_NOOP("Model", "Steady"), QT_TRANSLATE_NOOP("Model", "Build up"), QT_TRANSLATE_NOOP("Model", "Wind down")}, tr("Let the drummer start soft and build up over this clip, or the other way round"));
    crash = new QCheckBox(tr("Crash after fills")); crash->setFocusPolicy(Qt::NoFocus);
    crash->setToolTip(tr("A cymbal crash on the first beat after each fill"));
    form->addRow(QString(), crash);
    // the drummer's settings scroll when the window is short, so they never hold it tall
    auto* drumScroll = new QScrollArea;
    drumScroll->setFrameShape(QFrame::NoFrame);
    drumScroll->setWidgetResizable(true);
    drumScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    drumScroll->setFocusPolicy(Qt::NoFocus);
    auto* drumInner = new QWidget;
    auto* side = new QVBoxLayout(drumInner); side->setContentsMargins(0, 0, 0, 0); side->addWidget(drummer); side->addStretch(1);
    drummerBox = drummer; drummerHome = side;
    theme::makeFoldable(drummer, QStringLiteral("inspector/drummer"), [this] { const Clip* c = app->curClip(); return c && c->ad.on ? style->currentText() : tr("off"); });
    drumScroll->setWidget(drumInner);
    drumScroll->setFixedWidth(drummer->width() + 14);
    drumScroll->setMinimumHeight(120);
    drumScroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; } QScrollArea > QWidget > QWidget { background: transparent; }"));
    drummerRow = drumScroll;
    body->addWidget(drumScroll);
    view = new NoteEditor(app);
    auto* gridCol = new QVBoxLayout; gridCol->setSpacing(6);
    gridCol->addWidget(view, 1);
    padsBox = new QWidget;
    auto* pv = new QVBoxLayout(padsBox); pv->setContentsMargins(0, 0, 0, 0); pv->setSpacing(4);
    {   // what the letter keys do: play notes, or change the chord for every track that follows the chords
        auto* kr = new QHBoxLayout; kr->setContentsMargins(0, 0, 0, 0); kr->setSpacing(0);
        auto* cap = dimLabel(tr("Keys play")); cap->setMinimumWidth(64); cap->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        kr->addWidget(cap); kr->addSpacing(8);
        keysNotes = button(tr("Notes"), tr("The letter keys play notes on the selected track"));
        keysChords = button(tr("Chords"), tr("The letter keys change the chord: every track that follows the chords moves with it at once, "
                                             "like transposing the sequencer of an old synthesiser from its keyboard. The chord stays when you let go. "
                                             "With Record on, the chords are written into the chord row."));
        keysNotes->setObjectName("segL"); keysChords->setObjectName("segR");
        keysNotes->setProperty("lesson", "noteKeys"); keysChords->setProperty("lesson", "chordKeys");
        for (QPushButton* b : {keysNotes, keysChords}) { b->setCheckable(true); kr->addWidget(b); }
        kr->addSpacing(12);
        keysHint = dimLabel(QString());
        keysHint->setMinimumWidth(10);
        kr->addWidget(keysHint, 1);
        // shows and hides the on-screen keys; in a short editor they make way by themselves until this is used
        padsToggle = button(QString(), tr("Show or hide the on-screen keys (also in the View menu)"));
        padsToggle->setObjectName("padsToggle");
        connect(padsToggle, &QPushButton::clicked, this, [this] { setPadsMode(pads->isVisible() ? 2 : 1); });
        kr->addWidget(padsToggle);
        pv->addLayout(kr);
        connect(keysNotes, &QPushButton::clicked, this, [this] { app->setChordKeys(false); sync(); });
        connect(keysChords, &QPushButton::clicked, this, [this] { app->setChordKeys(true); sync(); });
    }
    pads = new KeyPads(app);
    pv->addWidget(pads);
    gridCol->addWidget(padsBox);
    body->addLayout(gridCol, 1);
    v->addLayout(body, 1);

    connect(app, &App::changed, this, &EditorPanel::sync);
    connect(app, &App::selectionChanged, this, [this] { view->clipChanged(false); sync(); });
    connect(busy, &QSlider::valueChanged, this, [this](int x) { app->density = float(x) / 100.f; });
    connect(vary, &QPushButton::clicked, this, [this] {
        const int id = app->editClip;
        if (const Clip* c = app->curClip(); c && !c->ad.on && !c->notes.empty() &&
            std::all_of(c->notes.begin(), c->notes.end(), [](const Note& n) { return n.lock; })) {
            emit app->status(tr("Every note in this pattern is kept, so Vary leaves it alone. Select notes and press Keep to let them go."));
            return;
        }
        app->onBar([this, id](Song& s) { Track* t = nullptr; if (Clip* c = s.clip(id, &t)) varyClip(s, *t, *c, app->rng); }, true, tr("Vary"), true);
    });
    connect(length, &QComboBox::activated, this, [this](int) {
        const int ticks = length->currentData().toInt();
        app->edit([&](Song& s) { if (Clip* c = s.clip(app->editClip)) { c->loopLen = ticks; c->offset %= ticks; if (c->scene >= 0) c->len = ticks; } }, tr("Pattern length"));
        view->clipChanged(false);
    });
    connect(gridBox, &QComboBox::activated, this, [this](int) { app->grid = gridBox->currentData().toInt(); view->viewport()->update(); });
    connect(accent, &QPushButton::clicked, this, [this] { flag(false); });
    connect(slide, &QPushButton::clicked, this, [this] { flag(true); });
    connect(down, &QPushButton::clicked, this, [this] { transpose(-1); });
    connect(up, &QPushButton::clicked, this, [this] { transpose(1); });
    connect(octDown, &QPushButton::clicked, this, [this] { transpose(-app->song.sc().n); });
    connect(octUp, &QPushButton::clicked, this, [this] { transpose(app->song.sc().n); });
    connect(keep, &QPushButton::clicked, this, [this] { toggleKeep(); });
    connect(tidy, &QPushButton::clicked, this, [this] { cleanUp(); });
    connect(clear, &QPushButton::clicked, this, [this] { app->edit([&](Song& s) { if (Clip* c = s.clip(app->editClip)) { c->notes.clear(); c->ad.on = false; } }, tr("Clear pattern"), true); });

    auto onClip = [this](std::function<void(Clip&)> f, const QString& what = QString(), bool announce = false) { const int id = app->editClip; app->onBar([id, f](Song& s) { if (Clip* c = s.clip(id)) f(*c); }, true, what.isEmpty() ? tr("Drummer setting") : what, announce); };
    connect(style, &QComboBox::activated, this, [onClip](int i) { onClip([i](Clip& c) { c.ad.style = i; }); });
    connect(fills, &QComboBox::activated, this, [this, onClip](int) { const int n = fills->currentData().toInt(); onClip([n](Clip& c) { c.ad.fillEvery = n; }); });
    connect(backbeat, &QComboBox::activated, this, [onClip](int i) { onClip([i](Clip& c) { c.ad.backbeat = i; }); });
    connect(hatsBox, &QComboBox::activated, this, [onClip](int i) { onClip([i](Clip& c) { c.ad.hats = i; }); });
    connect(shape, &QComboBox::activated, this, [onClip](int i) { onClip([i](Clip& c) { c.ad.shape = i; }); });
    connect(crash, &QCheckBox::clicked, this, [onClip](bool on) { onClip([on](Clip& c) { c.ad.crash = on; }); });
    connect(take, &QPushButton::clicked, this, [this, onClip] { const unsigned seed = unsigned(app->rng()); onClip([seed](Clip& c) { c.ad.seed = seed; }, tr("New take"), true); });
    connect(autoBox, &QCheckBox::clicked, this, [this](bool on) {
        if (!on) { app->edit([&](Song& s) { if (Clip* c = s.clip(app->editClip)) c->ad.on = false; }, tr("Drummer off")); return; }   // keep the hits, edit them by hand
        const int id = app->editClip; const unsigned seed = unsigned(app->rng());
        app->onBar([id, seed](Song& s) { if (Clip* c = s.clip(id)) { c->ad.on = true; c->ad.seed = seed; c->adKey = 0; } }, true, tr("Drummer on"), true);
    });
    // sliders act at once, so the beat changes while you drag
    auto live = [this](QSlider* sl, float AutoDrum::* field) {
        connect(sl, &QSlider::sliderPressed, this, [this] { app->begin(tr("Drummer setting")); });
        connect(sl, &QSlider::sliderMoved, this, [this, field](int x) { if (Clip* c = app->curClip()) { c->ad.*field = float(x) / 100.f; app->touch(); } });
    };
    live(intensity, &AutoDrum::intensity);
    live(complexity, &AutoDrum::complexity);
    live(shuffle, &AutoDrum::shuffle);
    sync();
}

void EditorPanel::placeDrummer(QLayout* host) {
    drummerAway = host != nullptr;
    if (host) {
        drummerHome->removeWidget(drummerBox);
        drummerBox->setMinimumWidth(0); drummerBox->setMaximumWidth(QWIDGETSIZE_MAX);
        host->addWidget(drummerBox);
    } else if (drummerBox->parentWidget() != drummerRow->findChild<QWidget*>()) {
        drummerBox->setParent(nullptr);
        drummerBox->setFixedWidth(300);
        drummerHome->insertWidget(0, drummerBox);
        drummerBox->show();
    }
    sync();
}

void EditorPanel::repaintPads() { if (pads->isVisible()) pads->update(); }
void EditorPanel::repaintView() { view->viewport()->update(); if (pads->isVisible()) pads->update(); }
// 0: automatic (the keys make way in a short editor), 1: always shown, 2: hidden. Remembered between runs.
void EditorPanel::setPadsMode(int mode) {
    padsMode_ = std::clamp(mode, 0, 2);
    QSettings().setValue(QStringLiteral("padsMode"), padsMode_);
    updatePads();
}

void EditorPanel::updatePads() {
    const int h = toolBox && toolBox->layout() ? toolBox->minimumHeight() : 0;
    const bool room = height() < 50 || height() - h >= 400;
    const bool show = padsMode_ == 1 || (padsMode_ == 0 && room);
    if (pads->isHidden() == show) pads->setVisible(show);   // the Notes | Chords switch above them always stays
    padsToggle->setText(show ? tr("Keys ▾") : tr("Keys ▸"));
    padsToggle->setToolTip(show ? tr("Hide the on-screen keys, so the note grid gets their room") : tr("Show the on-screen keys under the note grid"));
    if (show != padsShownLast_) { padsShownLast_ = show; emit padsShown(show); }
}

// Beside the loop grid the editor is narrow: the tool groups then stack instead of pushing the window wider.
void EditorPanel::setCompact(bool) {}   // the tool groups wrap by themselves

// when the tool groups wrap onto more rows, the editor needs that much more height: say so, so the window keeps room
// for the note grid and the keys instead of squeezing them
void EditorPanel::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    if (!toolBox || !toolBox->layout()) return;
    const int h = toolBox->layout()->heightForWidth(toolBox->width() > 0 ? toolBox->width() : width() - 20);
    if (h > 0 && toolBox->minimumHeight() != h) toolBox->setMinimumHeight(h);
    // in a short editor (a laptop, the timeline on top) the on-screen keys make way, so the note grid keeps its room
    updatePads();
}

void EditorPanel::rebuildIdeas() {
    Track* t = app->curTrack();
    const int role = t ? t->role : -1;
    if (role == ideasRole && ideas->menu()) return;
    ideasRole = role;
    auto* menu = new QMenu(ideas);
    // where a new idea goes when the track has nothing selected: a clip in the song, or a cell in the loop grid
    auto makeHolder = [this](Track* tk) {
        if (!app->song.liveMode) { app->createClip(tk->id, app->song.loopStart); return; }
        for (int sc = 0; sc < app->song.scenes; sc++) if (!tk->cell(sc)) { app->newCell(tk->id, sc); return; }
    };
    if (t) {
        if (t->isDrums()) {
            QAction* a = menu->addAction(tr("Auto drummer  —  plays by itself, with fills you can shape"));
            connect(a, &QAction::triggered, this, [this, makeHolder] {
                Track* tr_ = app->curTrack(); if (!tr_) return;
                if (!app->curClip()) makeHolder(tr_);
                const int id = app->editClip; const unsigned seed = unsigned(app->rng());
                app->onBar([id, seed](Song& s) { if (Clip* c = s.clip(id)) { c->ad.on = true; c->ad.seed = seed; c->adKey = 0; } }, true, tr("New idea"), true);
            });
            menu->addSeparator();
        }
        for (const GenDef& g : generatorsFor(role)) {
            QAction* a = menu->addAction(QStringLiteral("%1  —  %2").arg(ui(g.name), ui(g.about)));
            const int gid = g.id;
            connect(a, &QAction::triggered, this, [this, gid, makeHolder] {
                Track* tr_ = app->curTrack(); if (!tr_) return;
                Track* owner = nullptr;
                if (!app->curClip(&owner) || owner != tr_) makeHolder(tr_);   // nothing to fill yet: make a clip or cell
                const int id = app->editClip; const float d = app->density;
                app->onBar([this, id, gid, d](Song& s) { Track* o = nullptr; if (Clip* c = s.clip(id, &o)) generate(s, *o, *c, gid, d, app->rng); }, true, tr("New idea"), true);
            });
        }
    }
    if (QMenu* old = ideas->menu()) old->deleteLater();
    ideas->setMenu(menu);
}

void EditorPanel::flag(bool isSlide) {
    Clip* c = app->curClip();
    if (!c) return;
    bool any = false, allOn = true;
    for (const Note& n : c->notes) if (n.sel) { any = true; allOn &= isSlide ? n.slide : n.acc; }
    if (!any) { emit app->status(selectHint(app)); return; }
    app->edit([&](Song& s) { if (Clip* k = s.clip(app->editClip)) for (Note& n : k->notes) if (n.sel) (isSlide ? n.slide : n.acc) = !allOn; }, isSlide ? tr("Slide") : tr("Accent"));
}

bool EditorPanel::onSelected(const QString& what, const std::function<void(Note&)>& f) {
    Clip* c = app->curClip();
    if (!c) return false;
    if (c->ad.on) { emit app->status(tr("The auto drummer writes this clip. Untick “The drummer plays this clip” to change single hits.")); return false; }
    if (std::none_of(c->notes.begin(), c->notes.end(), [](const Note& n) { return n.sel; })) {
        emit app->status(selectHint(app));
        return false;
    }
    const int id = app->editClip;
    app->edit([&](Song& s) { if (Clip* k = s.clip(id)) for (Note& n : k->notes) if (n.sel) f(n); }, what);
    return true;
}

void EditorPanel::toggleKeep() {
    Clip* c = app->curClip();
    if (!c) return;
    bool allKept = true, any = false;
    for (const Note& n : c->notes) if (n.sel) { any = true; allKept &= n.lock; }
    if (!any) { onSelected(tr("Keep"), [](Note&) {}); return; }   // says what to do
    onSelected(allKept ? tr("Let notes go") : tr("Keep notes"), [allKept](Note& n) { n.lock = !allKept; });
    emit app->status(allKept ? tr("Vary and New idea may change these notes again.") : tr("Kept: Vary and New idea leave these notes as they are."));
}

void EditorPanel::cleanUp() {
    Track* t = nullptr;
    Clip* c = app->curClip(&t);
    if (!c || !t) return;
    if (c->ad.on) { emit app->status(tr("The auto drummer writes this clip, so it is always tidy.")); return; }
    Clip trial = *c;   // try it first, so a pattern that is already tidy makes no undo step
    const CleanReport r = cleanUpClip(trial, t->isDrums(), app->grid);
    if (r.total() == 0) { emit app->status(tr("Nothing to clean up: the pattern is already tidy.")); return; }
    const int id = app->editClip;
    const bool drums = t->isDrums();
    const int grid = app->grid;
    app->edit([&](Song& s) { if (Clip* k = s.clip(id)) cleanUpClip(*k, drums, grid); }, tr("Clean up"));
    QStringList what;
    auto say = [&](int n, const QString& one, const QString& many) { if (n) what << (n == 1 ? one : many.arg(n)); };
    say(r.snapped, tr("1 note put on the grid"), tr("%1 notes put on the grid"));
    say(r.merged, tr("1 doubled note merged"), tr("%1 doubled notes merged"));
    say(r.fixed, tr("1 wrong key put right"), tr("%1 wrong keys put right"));
    say(r.filled, tr("1 missed note filled in"), tr("%1 missed notes filled in"));
    say(r.removed, tr("1 stray note removed"), tr("%1 stray notes removed"));
    emit app->status(tr("Cleaned up: %1. Ctrl+Z brings back what you played.").arg(QLocale().createSeparatedList(what)));
}

void EditorPanel::transpose(int steps) {
    Clip* c = app->curClip();
    if (!c) return;
    bool any = false; for (const Note& n : c->notes) any |= n.sel;
    const int id = app->editClip;
    app->edit([&](Song& s) {
        if (Clip* k = s.clip(id)) for (Note& n : k->notes) if (!any || n.sel) n.deg = std::clamp(n.deg + steps, DEG_MIN, DEG_MAX);
    }, tr("Move notes"));
    view->clipChanged(true);
}

void EditorPanel::sync() {
    Track* t = nullptr;
    Clip* c = app->curClip(&t);
    if (!t) t = app->curTrack();
    rebuildIdeas();
    const bool dr = t && t->isDrums(), autoOn = c && c->ad.on;
    drummerRow->setVisible(dr && c && !drummerAway);   // drum clips always show the drummer, switched on or off
    if (drummerAway) drummerBox->setVisible(dr && c);
    autoBox->setChecked(autoOn);
    drummerBody->setEnabled(autoOn);
    notesGroup->setVisible(c && !autoOn);          // nothing to select in a clip the drummer writes
    slide->setVisible(!dr); lean->setVisible(!dr); stepBox->setVisible(!dr); octBox->setVisible(!dr);
    length->setEnabled(c && !autoOn);
    clear->setEnabled(c && !autoOn);
    tidy->setEnabled(c && !autoOn);
    vary->setEnabled(c != nullptr);
    vary->setToolTip(autoOn ? tr("Ask the drummer for a new take") : tr("Change a note or two. Press it again for more."));
    ideas->setEnabled(t != nullptr);
    if (c) {
        length->blockSignals(true); length->setCurrentIndex(length->findData(c->loopLen)); length->blockSignals(false);
    }
    if (autoOn) {
        style->blockSignals(true); style->setCurrentIndex(c->ad.style); style->blockSignals(false);
        fills->blockSignals(true); fills->setCurrentIndex(std::max(0, fills->findData(c->ad.fillEvery))); fills->blockSignals(false);
        if (!intensity->isSliderDown()) intensity->setValue(int(c->ad.intensity * 100));
        if (!complexity->isSliderDown()) complexity->setValue(int(c->ad.complexity * 100));
        if (!shuffle->isSliderDown()) shuffle->setValue(int(c->ad.shuffle * 100));
        for (auto [box, value] : {std::pair{backbeat, c->ad.backbeat}, {hatsBox, c->ad.hats}, {shape, c->ad.shape}}) { box->blockSignals(true); box->setCurrentIndex(value); box->blockSignals(false); }
        crash->setChecked(c->ad.crash);
    }
    const bool chords = app->chordKeys();
    keysNotes->setChecked(!chords); keysChords->setChecked(chords);
    if (chords) {
        const int live = app->liveChord();
        keysHint->setText(live == Engine::NO_CHORD ? tr("Press a key to change the chord. Tracks that follow the chords move with it. With Shift, the chord comes a semitone lower and leans into it.")
                                                   : tr("Playing %1 instead of the song’s chords. Esc goes back to them.").arg(app->song.chordName(live, app->liveChordAlt())));
    } else keysHint->setText(dr ? tr("The bottom row of letter keys plays the drums") : tr("Three rows of letter keys, three octaves, always in the song’s scale. Hold Shift for the right wrong notes."));
    pads->setFixedHeight(dr && !chords ? 26 + 8 : pads->sizeHint().height());
    pads->update();
    view->clipChanged(false);
}
