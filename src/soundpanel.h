// Sound panel: every knob of the selected track's synth or drum kit, laid out in the order the sound flows
// (oscillators -> filter -> amp, then movement and effects), with a live picture in each section.
#pragma once
#include "mysounds.h"
#include "model.h"
#include <QWidget>
#include <functional>
#include <vector>

class App;
class Knob;
class QComboBox;
class Switch;
class QPushButton;
class QStackedWidget;
class QGroupBox;
class QHBoxLayout;
class FlowLayout;
class QGridLayout;
class EqCurve;

class SoundPanel : public QWidget {
	Q_OBJECT
public:
	explicit SoundPanel(App* app, QWidget* parent = nullptr);
	void setSimple(bool on); // the simple page: a few big knobs in plain words, instead of every knob
	bool isSimple() const { return simple; }
	QGroupBox* buildQuick(); // the inspector's Sound section
	void openLibrary(); // "All sounds…" and File > Sound library: the sound library, for the selected track's sound
	void placeHowGroup(QLayout* host); // How notes are played: into the inspector, or back onto this tab (nullptr)
signals:
	void openSampleLibrary(); // the person wants the sample page of the settings
	void simpleChanged(bool simple);
public slots:
	void sync();       // after the song or the selection changed
	void syncMoving(); // called often while playing: knobs and pictures follow their lines
private:
	struct KnobBind {
		Knob* k;
		std::function<float()> get;
	};
	struct ComboBind {
		QComboBox* c;
		std::function<int()> get;
	};
	Knob* patchKnob(int param);
	Knob* freeKnob(const QString& label, std::function<float()> get, std::function<void(float)> set,
	               std::function<QString(float)> fmt, float def, const QString& tip = QString());
	QComboBox* combo(const QStringList& items, std::function<int()> get, std::function<void(int)> set,
	                 const QString& tip = QString());
	QComboBox* patchCombo(int param);
	QWidget* graph(int kind);
	QWidget* buildSynthPage();
	QWidget* buildDrumPage();
	QWidget* buildSimpleSynth();
	// the Simple page's Tone knob and Low cut switch: a fifth column beside the other big knobs where the window is
	// wide enough, else a row of their own under them, so the page never runs past the window's right edge
	void placeTone();
	bool eventFilter(QObject* watched, QEvent* e) override;
	void showEvent(QShowEvent* e) override;
	QWidget* simpleSynthPage = nullptr;
	QGridLayout* simpleGrid = nullptr;
	QWidget *toneCell = nullptr, *lowCutCell = nullptr;
	bool toneBeside = true;
	int toneWide = 0; // how wide the page wanted to be with Tone beside the others
	QWidget* buildSimpleDrums();
	QGroupBox* buildMaster();
	// the shared echo's own controls (on the Complete pages and, the time, on the Simple one)
	QComboBox* echoTimeCombo();
	Knob* echoToneKnob();
	Switch* pingPongSwitch();
	Knob* kitKnob(const QString& label, int idx, float def);
	void spread(QWidget* page);
	void rollSound();
	void mutateSound();
	QComboBox* makePresetBox();
	void fillPresetBox(QComboBox* box);     // the ready-made sounds in groups, your own on top
	void refreshMine();                     // your own sounds read again (after saving one), every preset list refilled
	void saveSound();                       // "Save sound…"
	QHBoxLayout* libraryPair(bool compact); // Save sound… and Library, joined
	static constexpr int MINE = 10000;      // a preset list entry at MINE + i is mine[i]
	std::vector<mysounds::Sound> mine;      // your own sounds, as listed
	QComboBox* makeKitBox();
	QPushButton* sampleButton(int drum);
	void sampleMenu(int drum, QPushButton* under);
	// one big knob that moves several settings together; parts are (parameter, how far it moves)
	Knob* macroKnob(const QString& label, const QString& tip, std::vector<std::pair<int, float>> parts);
	bool simple = false;
	std::vector<QComboBox*> presetBoxes, kitBoxes;
	std::vector<std::pair<int, QPushButton*>> sampleBtns; // (drum, button)
	std::vector<float> macroBase;                         // values of the parts when a macro knob was pressed

	App* app;
	QStackedWidget* pages;
	QComboBox *presetBox, *kitBox;
	QPushButton* resume;
	Switch* follow;
	QGroupBox* howGroup = nullptr;
	FlowLayout* howFlow = nullptr;
	QStackedWidget* quickPages = nullptr;
	std::vector<KnobBind> knobs;
	std::vector<ComboBind> combos;
	std::vector<std::pair<int, Knob*>> patchKnobs; // (parameter, knob) for the moving display
	std::vector<QWidget*> graphs;
	std::vector<Switch*> pingPongs; // the echo's ping-pong switch, one on the synth page and one on the drums'

	// the equaliser (soundpanel.cpp, at the end): its section on the Complete pages, and Tone and Low cut on the
	// Simple ones
	QGroupBox* buildEq(bool drums);
	QWidget* toneKnob(bool drums); // one big knob, darker to brighter, with its ends written under it
	Switch* lowCutSwitch(bool drums);
	void syncEq(bool moving); // after every change; while playing, only the pictures and knobs that follow lines
	std::vector<EqCurve*> eqCurves;
	std::vector<std::pair<Switch*, bool>> lowCuts; // (switch, on a drum page)
	std::vector<float> toneBase;                   // low and high when the Tone knob was pressed
};
