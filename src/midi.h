// MIDI: a keyboard plays the selected track (or the chords, in Chords mode), the knobs and faders of a controller
// turn the knobs on screen they were taught to (right-click a knob, "Learn MIDI control"), and the clock keeps
// Nachtzug in step with drum machines and other programs, either way round. The choices are kept between runs.
#pragma once
#include "midiio.h"
#include <QHash>
#include <QObject>
#include <QTimer>
#include <atomic>
#include <functional>
#include <thread>

class App;
struct Track;
class QMenu;
class QWidget;

// "Learn MIDI control" (or "Forget …") for a knob's right-click menu; nothing when there is no MIDI
void addMidiActions(QMenu* menu, App* app, const QString& target, const QString& label);
// the same menu on its own, for a knob or fader that has no menu of its own yet
void midiMenuFor(QWidget* w, App* app, const QString& target, const QString& label);

class Midi : public QObject {
    Q_OBJECT
public:
    explicit Midi(App* app, bool remember = true);   // remember: keep the choices in the settings (not in tests)
    ~Midi() override;
    MidiIO& io() { return io_; }

    QString input() const { return input_; }          // "" none, "*" every device, or a device's name
    QString output() const { return output_; }
    bool sendClock() const { return sendClock_; }
    bool followClock() const { return followClock_; }
    void setInput(const QString& name);
    void setOutput(const QString& name);
    void setSendClock(bool on);
    void setFollowClock(bool on);

    // what a controller knob can turn: "p:cutoff" (that knob of the selected track), "vol:2" (the third track's
    // fader), "pump:2", "master", "swing"
    void learn(const QString& target, const QString& label);   // the next knob turned on the controller is tied to it
    void cancelLearn();
    QString learning() const { return learnTarget_; }
    void forget(const QString& target);
    void forgetAll();
    int controllerFor(const QString& target) const;   // -1 if none
    int learnedCount() const { return int(map_.size()); }
    QString lastHeard() const { return last_; }
    float clockBpm() const;                                  // the tempo of the clock coming in, 0 if none
    static QString targetForParam(int param);
    // for tests: a message as if it came from a device (any thread), and a stand-in for the output
    void incoming(const uint8_t* bytes, int length);
    std::function<void(const uint8_t*, int)> sendHook;

signals:
    void heard(const QString& what);           // something came in (for the light on the settings page)
    void mappingsChanged();

private:
    void received(const QByteArray& m);        // on the program's own thread
    void control(int cc, int value);
    void apply(const QString& target, float v);
    void release();
    Track* trackAt(const QString& target);   // "pan:2" and the like: the third track
    void startClockThread();
    void stopClockThread();
    void save();
    void out(const uint8_t* b, int n);
    App* app;
    MidiIO io_;
    bool remember_;
    QString input_, output_;
    bool sendClock_ = false, followClock_ = false;
    QHash<int, QString> map_;                  // controller number -> target
    QString learnTarget_, learnLabel_;
    QString held_;                             // the target being turned now: one undo step per turn
    QTimer idle_;
    QString last_;
    // clock out
    std::thread clockThread_;
    std::atomic<bool> clockRun_{false};
    // clock in, counted on the MIDI thread
    std::atomic<float> heardBpm_{0.f};
    std::atomic<double> lastClockSeen_{0.0};
    double lastClock_ = 0.0, clockAvg_ = 0.0;
    int clockCount_ = 0;
};
