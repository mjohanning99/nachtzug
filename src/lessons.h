// Lessons: short guided tours that show where things are and wait until you have tried each one. A soft shade
// lies over the window with a lit-up hole around the thing to use next, and a card beside it says what to do.
// The shade lets every click through, so you work the real program; a step moves on by itself once it is done.
#pragma once
#include <QPointer>
#include <QWidget>
#include <functional>
#include <vector>

class App;
class QLabel;
class QPushButton;
class QTimer;

struct LessonStep {
    QString text;                          // what to do, in a sentence or two (rich text)
    std::function<QRect()> target;         // where it is, in the window's coordinates; empty: no spotlight
    std::function<bool()> done;            // true once it is done; empty: the card has a Next button
    std::function<void()> enter;           // run when the step starts (remember where things were, open a tab)
};

struct Lesson {
    QString id, title, about;
    std::function<void()> prepare;         // run before the first step (for instance, add a track the lesson needs)
    std::vector<LessonStep> steps;
};

class LessonOverlay : public QWidget {
    Q_OBJECT
public:
    explicit LessonOverlay(QWidget* window);
    void start(const Lesson& lesson);
    void stop();
    bool running() const { return !lesson_.steps.empty(); }
    int step() const { return index_; }
    QString lessonId() const { return lesson_.id; }
    void next();                           // for tests: as if the step was done
signals:
    void finished(const QString& id, bool completed);
protected:
    void paintEvent(QPaintEvent*) override;
    bool eventFilter(QObject* o, QEvent* e) override;
private:
    void show(int i);
    void place();
    void fitCard();
    void poll();
    Lesson lesson_;
    int index_ = -1;
    QRect hole_;
    bool doneShown_ = false;
    QTimer* timer_;
    QWidget* card_;
    QLabel *count_, *title_, *text_, *tick_;
    QPushButton *back_, *next_, *end_;
    double pulse_ = 0.0;
};

// The lessons there are, built against the main window's widgets.
class MainWindow;
std::vector<Lesson> makeLessons(App* app, QWidget* window);
