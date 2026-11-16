// Sound panel: every knob of the selected track's synth or drum kit, laid out in the order the sound flows
// (oscillators -> filter -> amp, then movement and effects), with a live picture in each section.
#pragma once
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
class FlowLayout;

class SoundPanel : public QWidget {
	Q_OBJECT
public:
	explicit SoundPanel(App* app, QWidget* parent = nullptr);
	void setSimple(bool on); // the simple page: a few big knobs in plain words, instead of every knob
	bool isSimple() const { return simple; }
	QGroupBox* buildQuick();           // the inspector's Sound section
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
	QWidget* buildSimpleDrums();
	QGroupBox* buildMaster();
	Knob* kitKnob(const QString& label, int idx, float def);
	void spread(QWidget* page);
	void rollSound();
	void mutateSound();
	QComboBox* makePresetBox();
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
};
