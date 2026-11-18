// Main window: transport bar on top, note editor and sound knobs in the middle, song timeline at the bottom.
#pragma once
#include <QHash>
#include <QMainWindow>
#include <vector>

class App;
class EditorPanel;
class Knob;
class Lcd;
class VuMeter;
class QAbstractButton;
class LoopGrid;
class QStackedWidget;
class QPushButton;
class MixerPanel;
class QAction;
class QComboBox;
class QKeyEvent;
class QLabel;
class QSlider;
class QSpinBox;
class QTabWidget;
class QToolButton;
class SoundPanel;
class Timeline;

class MainWindow : public QMainWindow {
	Q_OBJECT
public:
	explicit MainWindow(App* app);
	void showHelp();
	void showSettings(int page = 0);
	void showDialog(const QString& which); // by name, for pictures and tests: settings, sound card, samples, export
	bool maybeSave();                      // asks to save unsaved changes; false if the person cancelled
	void restartProgram();
	void applyScrollBars(); // hides or shows the scroll bars of the window's scroll areas (Settings: "scrollBars")
	void applyKeyLook();    // the keys' symbols in the look's colours, with or without their words (Settings: "keys")
	// The top bar's rows: every control in a row of controls as tall as a key of the current look, and what spans
	// rows (the display, the meters, the knobs, Export) sized from that. Again after the look changes.
	void fitTopBar();
	void showStart();                    // the start window: a starter song, a recent song, an empty song or Open…
	void startLesson(const QString& id); // one of the lessons in Help > Lessons
	void openManual();                   // the operation manual (PDF) in the browser, in the program's language
	class LessonOverlay* lessons() const { return lessonOverlay; }

protected:
	bool eventFilter(QObject* o, QEvent* e) override;
	void closeEvent(QCloseEvent* e) override;

private:
	void buildMenus();
	QWidget* buildTopBar();
	QWidget* keysPage(const QString& style);   // the help's second tab: every key and mouse move worth knowing
	QList<QWidget*> rowControls, spanControls; // the top bar's: one row of controls high, and both rows high
	QList<class QFrame*> unitFrames;
	QWidget* tapKey = nullptr; // a plain key of the current look, whose height the rows take
	void sync();
	void frame();
	bool saveSong(bool askName);
	void openSong();
	void openRecent(const QString& path); // a song from File > Open recent or the start window
	void fillRecentMenu();
	class QMenu* recentMenu = nullptr;
	void newSong(bool starter);
	void exportWav();
	void arrange();
	void continueSong(); // “Continue for … minutes”: the long journey
	void setTheme(int index);
	static bool mapKey(const QKeyEvent* k, int& row, int& col);

	App* app;
	Lcd* lcd;
	Timeline* timeline;
	LoopGrid* loops;
	QStackedWidget* bottomStack;
	class QSplitter* split;
	QWidget* bottomArea = nullptr;
	bool shownLive = false;
	QList<QWidget*> songTools, loopTools; // the groups of keys above the timeline, and above the loop grid
	QPushButton *songMode, *loopMode;
	void setLoopsMode(bool on);
	EditorPanel* editor;
	SoundPanel* sound;
	MixerPanel* mixer;
	QTabWidget* tabs;
	void applyLook();
	QToolButton *playBtn, *stopBtn, *recBtn, *undoBtn, *redoBtn, *exportBtn;
	std::vector<std::pair<QPushButton*, QString>> toolKeys; // the keys above the timeline and their symbols
	class Toast* toast;
	QAbstractButton *loopBtn, *clickBtn, *timeBtn; // sliding switches
	VuMeter *vuL, *vuR;
	std::vector<qint64> taps; // times of the last presses on TAP
	QSpinBox* tempo;
	QComboBox *keyBox, *scaleBox;
	Knob *swing, *volume;
	class LessonOverlay* lessonOverlay = nullptr;
	class QMenu* lessonMenu = nullptr;
	QString nextLesson; // the toast's button starts this lesson instead of undoing
	void fillLessonMenu();
	QAction *undoAct, *redoAct, *recAct, *loopAct, *clickAct, *timeAct, *snapAct, *chordAct, *padsAct = nullptr;
	// the panels: the inspector on the left; the timeline (or loop grid) and the tabs above or below each other,
	// or the tabs in a window of their own
	class Inspector* inspector = nullptr;
	QToolButton* inspectorTab = nullptr; // the slim » tab that brings the inspector back
	class QSplitter* hsplit = nullptr;
	class QTimer* keepWidth = nullptr; // remembers the inspector's width once its divider has rested
	void keepInspectorWidth();
	// what frame() last saw of the things that move on the timeline and the note grid, and how many frames since
	// anything moved
	struct FrameState {
		double pos;
		int chord, chordAlt, chordPad, pending;
		bool operator==(const FrameState&) const = default;
	};
	FrameState lastFrame{};
	int restFrames = 0;
	QMainWindow* editorWindow = nullptr;
	bool timelineTop = true;
	QString shownArrangement; // which arrangement the divider sizes below belong to
	QHash<QString, QList<int>> splitSizes;
	QAction *inspAct = nullptr, *ownWinAct = nullptr, *topAct = nullptr;
	void arrangePanels();
	void fitTimelineHeight();
	void setInspectorShown(bool on);
	void setEditorWindow(bool on);
	void resetLayout();
};
