#include "lessons.h"
#include "app.h"
#include "widgets.h"
#include <QCoreApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QPushButton>
#include <QSet>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>
#include <memory>

// ---------------------------------------------------------------- the shade and the card

LessonOverlay::LessonOverlay(QWidget* window) : QWidget(window) {
    setAttribute(Qt::WA_TransparentForMouseEvents);   // every click goes through to the program underneath
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::NoFocus);
    hide();
    window->installEventFilter(this);

    // the card is a separate window-less widget on top, so its buttons can be clicked
    card_ = new QWidget(window);
    card_->setObjectName("lessonCard");
    card_->setAttribute(Qt::WA_StyledBackground);
    card_->setFixedWidth(380);
    auto* v = new QVBoxLayout(card_);
    v->setContentsMargins(16, 12, 16, 12); v->setSpacing(8);
    auto* top = new QHBoxLayout; top->setSpacing(8);
    title_ = new QLabel; title_->setObjectName("lessonTitle");
    count_ = new QLabel; count_->setObjectName("dim");
    top->addWidget(title_, 1); top->addWidget(count_);
    v->addLayout(top);
    text_ = new QLabel; text_->setWordWrap(true); text_->setTextFormat(Qt::RichText);
    v->addWidget(text_);
    tick_ = new QLabel; tick_->setObjectName("lessonTick"); tick_->hide();
    v->addWidget(tick_);
    auto* buttons = new QHBoxLayout; buttons->setSpacing(6);
    end_ = new QPushButton(tr("End lesson")); back_ = new QPushButton(tr("Back")); next_ = new QPushButton(tr("Next"));
    for (QPushButton* b : {end_, back_, next_}) b->setFocusPolicy(Qt::NoFocus);   // the letter keys and Space stay with the music
    end_->setToolTip(tr("Stop the lesson. You can start it again from Help > Lessons."));
    back_->setToolTip(tr("Go back to the step before"));
    next_->setObjectName("primary");
    buttons->addWidget(end_); buttons->addStretch(1); buttons->addWidget(back_); buttons->addWidget(next_);
    v->addLayout(buttons);
    card_->hide();
    connect(end_, &QPushButton::clicked, this, [this] { const QString id = lesson_.id; stop(); emit finished(id, false); });
    connect(back_, &QPushButton::clicked, this, [this] { if (index_ > 0) show(index_ - 1); });
    connect(next_, &QPushButton::clicked, this, [this] { next(); });

    timer_ = new QTimer(this);
    timer_->setInterval(60);
    connect(timer_, &QTimer::timeout, this, &LessonOverlay::poll);
}

void LessonOverlay::start(const Lesson& lesson) {
    stop();
    lesson_ = lesson;
    if (lesson_.prepare) lesson_.prepare();
    QWidget* win = parentWidget();
    setGeometry(win->rect());
    QWidget::show(); raise();
    card_->show(); card_->raise();
    // the card in the theme's colours
    card_->setStyleSheet(QStringLiteral("#lessonCard { background: %1; border: 2px solid %2; border-radius: 10px; }"
                                        "#lessonTitle { font-weight: 700; color: %2; letter-spacing: 1px; }"
                                        "#lessonTick { font-weight: 700; color: %2; }").arg(theme::panel.name(), theme::amber.name()));
    timer_->start();
    show(0);
}

void LessonOverlay::stop() {
    timer_->stop();
    lesson_ = Lesson();
    index_ = -1;
    hide(); card_->hide();
}

void LessonOverlay::next() {
    if (!running()) return;
    if (index_ + 1 >= int(lesson_.steps.size())) { const QString id = lesson_.id; stop(); emit finished(id, true); return; }
    show(index_ + 1);
}

void LessonOverlay::show(int i) {
    index_ = std::clamp(i, 0, int(lesson_.steps.size()) - 1);
    const LessonStep& st = lesson_.steps[size_t(index_)];
    if (st.enter) st.enter();
    doneShown_ = false;
    title_->setText(lesson_.title.toUpper());
    count_->setText(tr("Step %1 of %2").arg(index_ + 1).arg(lesson_.steps.size()));
    text_->setText(st.text);
    tick_->hide();
    back_->setVisible(index_ > 0);
    const bool last = index_ + 1 == int(lesson_.steps.size());
    // a step that waits for you can still be skipped, so nobody gets stuck
    next_->setText(st.done ? tr("Skip") : last ? tr("Finish") : tr("Next"));
    next_->setObjectName(st.done ? QString() : QStringLiteral("primary"));
    next_->style()->unpolish(next_); next_->style()->polish(next_);
    next_->setEnabled(true);
    next_->setToolTip(st.done ? tr("Go on without doing this step") : QString());
    fitCard();
    poll();
}

// the card is as tall as its text needs at its width
void LessonOverlay::fitCard() {
    const int h = card_->layout()->hasHeightForWidth() ? card_->layout()->heightForWidth(card_->width()) : card_->layout()->sizeHint().height();
    if (h > 0 && card_->height() != h) card_->setFixedHeight(h);
}

// where the card goes: under the lit-up spot if there is room, otherwise above it, otherwise beside it
void LessonOverlay::place() {
    QWidget* win = parentWidget();
    const QRect area = win->rect().adjusted(10, 10, -10, -10);
    fitCard();
    const QSize cs = card_->size();
    QPoint at;
    if (!hole_.isValid()) at = QPoint(area.center().x() - cs.width() / 2, area.center().y() - cs.height() / 2);
    else if (hole_.bottom() + 16 + cs.height() <= area.bottom()) at = QPoint(hole_.center().x() - cs.width() / 2, hole_.bottom() + 16);
    else if (hole_.top() - 16 - cs.height() >= area.top()) at = QPoint(hole_.center().x() - cs.width() / 2, hole_.top() - 16 - cs.height());
    else if (hole_.right() + 16 + cs.width() <= area.right()) at = QPoint(hole_.right() + 16, hole_.center().y() - cs.height() / 2);
    else at = QPoint(hole_.left() - 16 - cs.width(), hole_.center().y() - cs.height() / 2);
    at.setX(std::clamp(at.x(), area.left(), std::max(area.left(), area.right() - cs.width())));
    at.setY(std::clamp(at.y(), area.top(), std::max(area.top(), area.bottom() - cs.height())));
    if (card_->pos() != at) card_->move(at);
}

void LessonOverlay::poll() {
    if (!running()) return;
    const LessonStep& st = lesson_.steps[size_t(index_)];
    const QRect h = st.target ? st.target() : QRect();
    const QRect padded = h.isValid() ? h.adjusted(-6, -6, 6, 6) : QRect();
    if (padded != hole_) { hole_ = padded; update(); }
    place();
    pulse_ += 0.12;
    if (hole_.isValid()) update(hole_.adjusted(-8, -8, 8, 8));
    if (!doneShown_ && st.done && st.done()) {
        // done: say so for a moment, then the next step
        doneShown_ = true;
        tick_->setText(QStringLiteral("✓  ") + tr("Well done"));
        tick_->show();
        next_->setEnabled(false);
        fitCard();
        const int at = index_;
        QTimer::singleShot(1100, this, [this, at] { if (running() && index_ == at) next(); });
    }
}

bool LessonOverlay::eventFilter(QObject* o, QEvent* e) {
    if (o == parentWidget() && (e->type() == QEvent::Resize || e->type() == QEvent::LayoutRequest) && running()) {
        setGeometry(parentWidget()->rect());
        raise(); card_->raise();
        place();
    }
    return QWidget::eventFilter(o, e);
}

void LessonOverlay::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath shade;
    shade.addRect(rect());
    if (hole_.isValid()) {
        QPainterPath hole; hole.addRoundedRect(QRectF(hole_), 8, 8);
        shade = shade.subtracted(hole);
    }
    p.fillPath(shade, QColor(20, 14, 8, 105));
    if (hole_.isValid()) {   // a gently pulsing ring around what to use
        QColor ring = theme::amber;
        ring.setAlphaF(0.55 + 0.45 * std::sin(pulse_));
        p.setPen(QPen(ring, 3)); p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(hole_).adjusted(-1.5, -1.5, 1.5, 1.5), 9, 9);
    }
}

// ---------------------------------------------------------------- the lessons

namespace {
QWidget* byLesson(QWidget* win, const char* key) {
    for (QWidget* w : win->findChildren<QWidget*>())
        if (w->isVisible() && w->property("lesson").toString() == QLatin1String(key)) return w;
    return nullptr;
}
// where w is in the window; a control scrolled out of sight is scrolled into view first, so the lesson never points at nothing
QRect rectIn(QWidget* win, QWidget* w) {
    if (!w || !w->isVisible()) return QRect();
    QRect r(w->mapTo(win, QPoint(0, 0)), w->size());
    for (QWidget* p = w->parentWidget(); p && p != win; p = p->parentWidget())
        if (auto* sa = qobject_cast<QScrollArea*>(p)) {
            sa->ensureWidgetVisible(w, 8, 8);
            r = QRect(w->mapTo(win, QPoint(0, 0)), w->size()).intersected(QRect(sa->viewport()->mapTo(win, QPoint(0, 0)), sa->viewport()->size()));
        }
    return r;
}
QWidget* knobFor(QWidget* win, int param) {
    for (Knob* k : win->findChildren<Knob*>())
        if (k->isVisible() && k->property("param").isValid() && k->property("param").toInt() == param) return k;
    return nullptr;
}
QTabWidget* tabsOf(QWidget* win) { return win->findChild<QTabWidget*>(); }
QRect tabRect(QWidget* win, int i) {
    QTabWidget* t = tabsOf(win);
    if (!t) return {};
    QTabBar* b = t->tabBar();
    const QRect r = b->tabRect(i);
    return QRect(b->mapTo(win, r.topLeft()), r.size());
}
int tabNow(QWidget* win) { QTabWidget* t = tabsOf(win); return t ? t->currentIndex() : -1; }
QRect timelineRect(QWidget* win) {
    for (QWidget* w : win->findChildren<QWidget*>()) {
        const QString cls = QString::fromLatin1(w->metaObject()->className());
        if ((cls == QLatin1String("Timeline") || cls == QLatin1String("LoopGrid")) && w->isVisible()) return rectIn(win, w);
    }
    return {};
}
// what to point at: the thing itself, or the tab it is on while that tab is closed
QRect orTab(QWidget* win, const QRect& r, int tab) { return r.isValid() ? r : tabRect(win, tab); }
int trackWithRole(App* app, int role) { for (const Track& t : app->song.tracks) if (t.role == role) return t.id; return -1; }
bool roleSelected(App* app, int role) { const Track* t = app->curTrack(); return t && t->role == role; }
float paramOf(App* app, int p) { const Track* t = app->curTrack(); return t && !t->isDrums() ? t->patch.p[p] : 0.f; }
// a lesson needs a track of this kind: add one if the song has none
void needTrack(App* app, int role) { if (trackWithRole(app, role) < 0 && int(app->song.tracks.size()) < MAX_TRACKS) app->addTrack(role); }
}

std::vector<Lesson> makeLessons(App* app, QWidget* win) {
    // the names the program gives these changes (in the language in use), to count them
    const QString newIdea = QCoreApplication::translate("EditorPanel", "New idea"), varied = QCoreApplication::translate("EditorPanel", "Vary");
    const QString newTake = QCoreApplication::translate("EditorPanel", "New take"), drumKit = QCoreApplication::translate("SoundPanel", "Drum kit");
    const QString arranged = QCoreApplication::translate("MainWindow", "Arrange for me");
    std::vector<Lesson> out;
    // pieces used by several lessons
    auto pickTrack = [&](int role, const QString& text) {
        LessonStep s;
        s.text = text;
        s.target = [win] { return timelineRect(win); };
        s.done = [app, role] { return roleSelected(app, role); };
        return s;
    };
    auto pressPlay = [&](const QString& text) {
        LessonStep s;
        s.text = text;
        s.target = [win] { return rectIn(win, byLesson(win, "play")); };
        s.done = [app] { return app->isPlaying(); };
        return s;
    };
    auto openTab = [&](int tab, const QString& text) {
        LessonStep s;
        s.text = text;
        s.target = [win, tab] { return tabRect(win, tab); };
        s.done = [win, tab] { return tabNow(win) == tab; };
        return s;
    };
    // a knob turned far enough from where it was when the step began
    auto turnKnob = [&](int param, const QString& text) {
        LessonStep s;
        auto start = std::make_shared<float>(0.f);
        s.text = text;
        s.enter = [app, param, start] { *start = paramOf(app, param); };
        s.target = [win, param] { return orTab(win, rectIn(win, knobFor(win, param)), 1); };
        s.done = [app, param, start] { return std::fabs(paramOf(app, param) - *start) > 0.1f; };
        return s;
    };
    auto counted = [&](const char* key, const QString& editName, int times, const QString& text, std::function<void()> enter = {}) {
        LessonStep s;
        auto start = std::make_shared<int>(0);
        s.text = text;
        s.enter = [app, editName, start, enter] { *start = app->editCount(editName); if (enter) enter(); };
        s.target = [win, key] { return rectIn(win, byLesson(win, key)); };
        s.done = [app, editName, start, times] { return app->editCount(editName) >= *start + times; };
        if (QLatin1String(key) == QLatin1String("ideas") || QLatin1String(key) == QLatin1String("vary") || QLatin1String(key) == QLatin1String("drumTake"))
            s.target = [win, key] { return orTab(win, rectIn(win, byLesson(win, key)), 0); };
        return s;
    };
    auto info = [&](const QString& text, std::function<QRect()> target = {}) { LessonStep s; s.text = text; s.target = std::move(target); return s; };
    auto showNotes = [win] { if (QTabWidget* t = tabsOf(win)) t->setCurrentIndex(0); };

    {   // 1: an acid line
        Lesson l;
        l.id = QStringLiteral("acid");
        l.title = LessonOverlay::tr("Your first acid line");
        l.about = LessonOverlay::tr("Make a bass line squelch: new ideas, Vary, notes that play only sometimes, and the filter. About five minutes.");
        l.prepare = [app] { needTrack(app, R_BASS); };
        l.steps.push_back(pressPlay(LessonOverlay::tr("<b>Press Space</b> or the ▶ key to start the song. It keeps playing while you work, so you hear every change at once.")));
        l.steps.push_back(pickTrack(R_BASS, LessonOverlay::tr("Click the orange <b>Bass</b> track in the timeline at the bottom: its name on the left, or its clip.")));
        l.steps.push_back(counted("ideas", newIdea, 1, LessonOverlay::tr("Open <b>New idea ▾</b> and pick <b>Acid line</b>. The program writes a fresh bass line; while the song plays it starts on the next bar."), showNotes));
        l.steps.push_back(counted("vary", varied, 2, LessonOverlay::tr("Press <b>Vary</b> two or three times. Each press changes a note or two, so the line grows step by step. Liked it better before? Press ↶ beside the display.")));
        {
            LessonStep s;
            s.text = LessonOverlay::tr("Hold <b>Shift</b> and drag a box around a few notes in the grid. Then open <b>Chance ▾</b> and pick <b>Half the time</b>: those notes now play only now and then, so the loop never sounds quite the same twice.");
            s.target = [win] { return orTab(win, rectIn(win, byLesson(win, "chance")), 0); };
            s.done = [app] { const Clip* c = app->curClip(); if (!c) return false; for (const Note& n : c->notes) if (n.chance < 100) return true; return false; };
            l.steps.push_back(s);
        }
        l.steps.push_back(openTab(1, LessonOverlay::tr("Open the <b>Sound</b> tab: the knobs of the bass sound.")));
        l.steps.push_back(turnKnob(dsp::P_CUTOFF, LessonOverlay::tr("Drag the <b>Cutoff</b> knob slowly up and down (on the simple page it is called <b>Brightness</b>). That is the filter, the heart of the acid sound.")));
        l.steps.push_back(turnKnob(dsp::P_RES, LessonOverlay::tr("Now turn up <b>Resonance</b> (<b>Squelch</b> on the simple page). The filter starts to ring and squeal on every note.")));
        l.steps.push_back(info(LessonOverlay::tr("That is an acid line. Save it with <b>File ▸ Save</b> (Ctrl+S), or carry on with the next lesson, <b>Build a beat</b>.")));
        out.push_back(l);
    }
    {   // 2: a beat
        Lesson l;
        l.id = QStringLiteral("beat");
        l.title = LessonOverlay::tr("Build a beat");
        l.about = LessonOverlay::tr("Shape what the auto drummer plays, pick a drum kit by ear, and make the other tracks pump along.");
        l.prepare = [app] { needTrack(app, R_DRUMS); };
        LessonStep pick = pickTrack(R_DRUMS, LessonOverlay::tr("Click the green <b>Drums</b> track in the timeline at the bottom."));
        pick.enter = showNotes;
        l.steps.push_back(pick);
        {
            LessonStep s;
            auto start = std::make_shared<int>(-1);
            s.text = LessonOverlay::tr("The auto drummer plays this clip by itself. Pick another <b>Style</b> in the list on the left and listen.");
            s.enter = [app, start, showNotes] { showNotes(); const Clip* c = app->curClip(); *start = c ? c->ad.style : -1; };
            s.target = [win] { return rectIn(win, byLesson(win, "drumStyle")); };
            s.done = [app, start] { const Clip* c = app->curClip(); return c && c->ad.on && c->ad.style != *start; };
            l.steps.push_back(s);
        }
        {
            LessonStep s;
            auto start = std::make_shared<float>(0.f);
            s.text = LessonOverlay::tr("Drag <b>Energy</b> to the right for a busier beat, or to the left for a bare pulse.");
            s.enter = [app, start] { const Clip* c = app->curClip(); *start = c ? c->ad.intensity : 0.f; };
            s.target = [win] { return rectIn(win, byLesson(win, "drumEnergy")); };
            s.done = [app, start] { const Clip* c = app->curClip(); return c && std::fabs(c->ad.intensity - *start) > 0.15f; };
            l.steps.push_back(s);
        }
        {
            LessonStep s;
            s.text = LessonOverlay::tr("Set <b>Fills</b> to <b>Every 2 bars</b>: the drummer rolls into every second bar.");
            s.target = [win] { return rectIn(win, byLesson(win, "drumFills")); };
            s.done = [app] { const Clip* c = app->curClip(); return c && c->ad.fillEvery == 2; };
            l.steps.push_back(s);
        }
        l.steps.push_back(counted("drumTake", newTake, 1, LessonOverlay::tr("Press <b>New take</b>: the same beat, played a little differently.")));
        {
            LessonStep s;
            auto start = std::make_shared<int>(0);
            s.text = LessonOverlay::tr("Open the <b>Sound</b> tab, then open the list of drum kits. Rest the mouse on a kit to hear it, and click the one you like.");
            s.enter = [app, start, drumKit] { *start = app->editCount(drumKit); };
            s.target = [win] { return tabNow(win) != 1 ? tabRect(win, 1) : rectIn(win, byLesson(win, "kit")); };
            s.done = [app, start, drumKit] { return app->editCount(drumKit) > *start; };
            l.steps.push_back(s);
        }
        {
            LessonStep s;
            s.text = LessonOverlay::tr("Open the <b>Mixer</b> tab and turn up <b>Pump</b> on the pad or the bass. On every kick the track ducks away and swells back: the breathing sound of dance music.");
            s.target = [win] { return tabNow(win) != 2 ? tabRect(win, 2) : rectIn(win, byLesson(win, "pump")); };
            s.done = [app] { for (const Track& t : app->song.tracks) if (t.duck > 0.3f) return true; return false; };
            l.steps.push_back(s);
        }
        l.steps.push_back(info(LessonOverlay::tr("A beat that breathes. Next: <b>Make it move</b>, where you record a filter sweep.")));
        out.push_back(l);
    }
    {   // 3: a recorded knob move
        Lesson l;
        l.id = QStringLiteral("move");
        l.title = LessonOverlay::tr("Make it move");
        l.about = LessonOverlay::tr("Record a filter sweep while the song plays, then see it as a line you can change.");
        l.prepare = [app] { needTrack(app, R_BASS); };
        l.steps.push_back(pickTrack(R_BASS, LessonOverlay::tr("Click the orange <b>Bass</b> track in the timeline.")));
        l.steps.push_back(openTab(1, LessonOverlay::tr("Open the <b>Sound</b> tab.")));
        {
            LessonStep s;
            s.text = LessonOverlay::tr("Switch <b>Record</b> on with the ● key (or Ctrl+R).");
            s.target = [win] { return rectIn(win, byLesson(win, "rec")); };
            s.done = [app] { return app->recording; };
            l.steps.push_back(s);
        }
        l.steps.push_back(pressPlay(LessonOverlay::tr("Start the song with <b>Space</b>, if it is not playing already.")));
        {
            LessonStep s;
            s.text = LessonOverlay::tr("While it plays, turn <b>Cutoff</b> (<b>Brightness</b> on the simple page) slowly up and down for a bar or two. Every move is written into the song.");
            s.target = [win] { return orTab(win, rectIn(win, knobFor(win, dsp::P_CUTOFF)), 1); };
            s.done = [app] { const Track* t = app->curTrack(); const AutoLane* ln = t ? t->lane(dsp::P_CUTOFF) : nullptr; return ln && ln->pts.size() >= 8; };
            l.steps.push_back(s);
        }
        {
            LessonStep s;
            s.text = LessonOverlay::tr("Switch <b>Record</b> off again, so later turns are not written down.");
            s.target = [win] { return rectIn(win, byLesson(win, "rec")); };
            s.done = [app] { return !app->recording; };
            l.steps.push_back(s);
        }
        l.steps.push_back(info(LessonOverlay::tr("Listen: the knob now moves by itself. Press <b>Lines</b> on the Bass track in the timeline to see the line; drag its points or draw new ones. Turning the knob by hand pauses the line until you press <b>Resume lines</b>."),
                               [win] { return timelineRect(win); }));
        out.push_back(l);
    }
    {   // 4: chords from the keys
        Lesson l;
        l.id = QStringLiteral("chords");
        l.title = LessonOverlay::tr("Berlin School: play the chords");
        l.about = LessonOverlay::tr("Move a running sequence from chord to chord with the letter keys, the way it was done on the synthesisers of the seventies.");
        l.prepare = [app] { needTrack(app, R_SEQ); };
        l.steps.push_back(pickTrack(R_SEQ, LessonOverlay::tr("Click the <b>Sequence</b> track in the timeline.")));
        l.steps.push_back(pressPlay(LessonOverlay::tr("Start the song with <b>Space</b>.")));
        {
            LessonStep s;
            s.text = LessonOverlay::tr("Under the notes, switch <b>Keys play</b> to <b>Chords</b>.");
            s.enter = showNotes;
            s.target = [win] { return rectIn(win, byLesson(win, "chordKeys")); };
            s.done = [app] { return app->chordKeys(); };
            l.steps.push_back(s);
        }
        {
            LessonStep s;
            auto seen = std::make_shared<QSet<int>>();
            s.text = LessonOverlay::tr("Press a few letter keys of the middle row, one after another, such as <b>D</b>, <b>F</b> and <b>G</b>. Every track that follows the chords moves to the new chord at once, and stays there when you let go.");
            s.enter = [seen] { seen->clear(); };
            s.target = [win] { for (QWidget* w : win->findChildren<QWidget*>()) if (QLatin1String(w->metaObject()->className()) == QLatin1String("KeyPads") && w->isVisible()) return rectIn(win, w); return QRect(); };
            s.done = [app, seen] { if (app->liveChord() != Engine::NO_CHORD) seen->insert(app->liveChord()); return seen->size() >= 3; };
            l.steps.push_back(s);
        }
        {
            LessonStep s;
            s.text = LessonOverlay::tr("Press <b>Esc</b> to go back to the song’s own chords.");
            s.done = [app] { return app->liveChord() == Engine::NO_CHORD; };
            l.steps.push_back(s);
        }
        {
            LessonStep s;
            s.text = LessonOverlay::tr("Switch back to <b>Notes</b> when you want the keys to play notes again. With <b>Record</b> on, the chords you press are written into the chord row of the timeline.");
            s.target = [win] { return rectIn(win, byLesson(win, "noteKeys")); };
            s.done = [app] { return !app->chordKeys(); };
            l.steps.push_back(s);
        }
        l.steps.push_back(info(LessonOverlay::tr("That is the trick behind many Berlin School pieces: one sequence, moved around by hand while it runs.")));
        out.push_back(l);
    }
    {   // 5: a whole song
        Lesson l;
        l.id = QStringLiteral("song");
        l.title = LessonOverlay::tr("From loop to song");
        l.about = LessonOverlay::tr("Let the program arrange your patterns into a whole song, listen to it, and export it.");
        l.steps.push_back(counted("arrange", arranged, 1, LessonOverlay::tr("Press <b>Arrange for me</b> above the timeline and choose a length. Your patterns become an intro, a build-up, a main part, a break and an ending.")));
        l.steps.push_back(pressPlay(LessonOverlay::tr("Press <b>Space</b> to listen from the start.")));
        l.steps.push_back(info(LessonOverlay::tr("When you like it, press <b>Export</b> at the top right: one sound file to play anywhere, a file for each track, or the notes as MIDI."),
                               [win] { return rectIn(win, byLesson(win, "export")); }));
        out.push_back(l);
    }
    return out;
}
