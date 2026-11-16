// Mixer: one channel strip per track (volume fader with level meter, pan, echo and reverb sends, mute, solo)
// and a master strip for the whole song.
#pragma once
#include <QColor>
#include <QWidget>
#include <vector>

class App;
struct LiveInput;
class Knob;
class QFrame;
class QHBoxLayout;
class QPushButton;

class Fader : public QWidget {
	Q_OBJECT
public:
	explicit Fader(QWidget* parent = nullptr);
	void setValue(float v);
	void setPeak(float p);
	void setAccent(const QColor& c) {
		accent = c;
		update();
	}
	bool isHeld() const { return dragging; }
	QSize sizeHint() const override { return {46, 190}; }
signals:
	void pressed();
	void moved(float v);
	void released();

protected:
	void paintEvent(QPaintEvent*) override;
	void mousePressEvent(QMouseEvent*) override;
	void mouseMoveEvent(QMouseEvent*) override;
	void mouseReleaseEvent(QMouseEvent*) override;
	void mouseDoubleClickEvent(QMouseEvent*) override;

private:
	float val = 0.75f, peak = 0.f, startVal = 0.f;
	int startY = 0;
	bool dragging = false;
	QColor accent;
};

class MixerPanel : public QWidget {
	Q_OBJECT
public:
	explicit MixerPanel(App* app, QWidget* parent = nullptr);
	void tick(); // meters, and faders following their lines
public slots:
	void sync();

private:
	struct Strip {
		int id;
		QFrame* box;
		QPushButton *name, *mute, *solo;
		Knob *pan, *echo, *reverb, *pump;
		Fader* fader;
	};
	void rebuild();
	App* app;
	QHBoxLayout* row;
	std::vector<Strip> strips;
	std::vector<int> signature; // track ids and roles the strips were built for
	QFrame* masterBox = nullptr;
	// the live input
	QFrame* inputBox = nullptr;
	QPushButton *inputOn = nullptr, *inputMute = nullptr;
	Fader* inputFader = nullptr;
	std::vector<std::pair<Knob*, float LiveInput::*>> inputKnobs;
	Fader* masterFader = nullptr;
	Knob *echoRepeats = nullptr, *reverbSize = nullptr;
};
