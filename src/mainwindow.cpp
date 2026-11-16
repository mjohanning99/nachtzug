#include "mainwindow.h"
#include "inspector.h"
#include "app.h"
#include "editor.h"
#include "loops.h"
#include "mixer.h"
#include "soundpanel.h"
#include "timeline.h"
#include "widgets.h"
#include "exportdialog.h"
#include "settingsdialog.h"
#include "lessons.h"
#include "chopdialog.h"
#include "midi.h"
#include "samples.h"
#include <QAbstractSpinBox>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDateTime>
#include <QDir>
#include <QFrame>
#include <QTextBrowser>
#include <QUrl>
#include <QDesktopServices>
#include <QMenu>
#include <cmath>
#include <QFileDialog>
#include <QGroupBox>
#include <QGridLayout>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QProgressDialog>
#include <QPushButton>
#include <QProcess>
#include <QActionGroup>
#include <QButtonGroup>
#include <QRadioButton>
#include <QScrollArea>
#include <QTextEdit>
#include <QAbstractItemView>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

static QLabel* dimLabel(const QString& text) {
	auto* l = new QLabel(text);
	l->setObjectName("dim");
	return l;
}

// the top bar's grid: the row of names, the gap under it, the gap between the two rows of controls, the margins
constexpr int TB_LABEL = 11, TB_GAP_TOP = 4, TB_GAP_ROWS = 6, TB_PAD = 8;

MainWindow::MainWindow(App* a) : app(a) {
	theme::loadKeyLook(); // before the panels: their keys show their symbols as set
	resize(1560, 980);
	buildMenus();

	auto* central = new QWidget;
	auto* v = new QVBoxLayout(central);
	v->setContentsMargins(0, 0, 0, 0);
	v->setSpacing(0);
	v->addWidget(buildTopBar()); // sets up lcd, so it comes before anything that reports to it

	split = new QSplitter(Qt::Vertical);
	tabs = new QTabWidget;
	tabs->setFocusPolicy(Qt::NoFocus);
	editor = new EditorPanel(app);
	sound = new SoundPanel(app);
	auto* scroll = new QScrollArea;
	scroll->setWidget(sound);
	scroll->setWidgetResizable(true);
	scroll->setFocusPolicy(Qt::NoFocus);
	tabs->addTab(editor, tr("NOTES"));
	tabs->addTab(scroll, tr("SOUND"));
	mixer = new MixerPanel(app);
	auto* mixScroll = new QScrollArea;
	mixScroll->setWidget(mixer);
	mixScroll->setWidgetResizable(true);
	mixScroll->setFocusPolicy(Qt::NoFocus);
	tabs->addTab(mixScroll, tr("MIXER"));
	tabs->tabBar()->setFocusPolicy(Qt::NoFocus);
	tabs->setTabToolTip(0, tr("The pattern of the selected clip: its notes or drum hits"));
	tabs->setTabToolTip(1, tr("The sound of the selected track: all its knobs"));
	tabs->setTabToolTip(2, tr("All tracks side by side: volume, pan, echo, reverb, mute and solo"));
	split->addWidget(tabs);

	auto* bottom = new QWidget;
	bottomArea = bottom;
	auto* bv = new QVBoxLayout(bottom);
	bv->setContentsMargins(0, 4, 0, 0);
	bv->setSpacing(4);
	// the bar above the bottom area: Song / Loops switch, Add track, then the tools of whichever view is shown
	auto* bar = new QHBoxLayout;
	bar->setContentsMargins(8, 0, 8, 0);
	bar->setSpacing(6);
	auto plain = [](const QString& text, const QString& tip) {
		auto* b = new QPushButton(text);
		b->setFocusPolicy(Qt::NoFocus);
		b->setToolTip(tip);
		return b;
	};
	songMode = plain(tr("Song"), tr("The song timeline: lay clips out from start to finish"));
	loopMode = plain(tr("Loops"),
	                 tr("The loop grid: switch patterns on and off while everything keeps playing, like a live set"));
	songMode->setCheckable(true);
	loopMode->setCheckable(true);
	songMode->setChecked(true);
	songMode->setObjectName("segL");
	loopMode->setObjectName("segR"); // one switch with two positions
	auto* seg = new QHBoxLayout;
	seg->setSpacing(0);
	seg->addWidget(songMode);
	seg->addWidget(loopMode);
	bar->addLayout(seg);
	bar->addSpacing(10);
	auto* addTrack = plain(tr("+  Add track"),
	                       tr("Add a bass line, sequence, pad, lead or drums. Each comes with a ready pattern."));
	addTrack->setObjectName("primary");
	bar->addWidget(addTrack);
	bar->addSpacing(14);

	songTools = new QWidget;
	auto* st = new QHBoxLayout(songTools);
	st->setContentsMargins(0, 0, 0, 0);
	st->setSpacing(6);
	// a symbol and a short word on each key; the tooltip says what it does in full (the scissors need no word)
	auto* scissors = plain(QString(), tr("Scissors: while this is on, clicking a clip cuts it in two at that spot"));
	scissors->setCheckable(true);
	auto* cutHere = plain(tr("Cut", "key: cut the selected clips at the playhead"),
	                      tr("Cut the selected clips in two where the playhead is (Ctrl+E)"));
	auto* dup = plain(tr("Copy", "key: a copy of the selected clips after them"),
	                  tr("Duplicate: put a copy of the selected clips right after them (Ctrl+D)"));
	auto* del = plain(tr("Delete"), tr("Delete the selected clips (Delete key)"));
	auto* loopSel =
	    plain(tr("Loop", "key: put the loop round the selected clips"),
		      tr("Loop this part: put the loop around the selected clips, so only that part of the song plays"));
	auto* arrangeBtn =
	    plain(tr("Arrange", "key: Arrange for me"), tr("Arrange for me: turn your patterns into a whole song, with an "
		                                               "intro, a build-up, a main part, a break and an ending"));
	toolKeys = {{dup, QStringLiteral("copy")},    {loopSel, QStringLiteral("loop")},
	            {cutHere, QStringLiteral("cut")}, {scissors, QStringLiteral("scissors")},
	            {del, QStringLiteral("delete")},  {arrangeBtn, QStringLiteral("arrange")}};
	for (auto& [key, kind] : toolKeys)
		theme::symbolKey(key, kind, key->text(), kind == QLatin1String("scissors"));
	arrangeBtn->setProperty("lesson", "arrange");
	auto* zoomOut = plain(QStringLiteral("−"), tr("Show more of the song (Ctrl + mouse wheel)"));
	zoomOut->setFixedWidth(34);
	auto* zoomIn = plain(QStringLiteral("+"), tr("Show the song larger (Ctrl + mouse wheel)"));
	zoomIn->setFixedWidth(34);
	st->addWidget(dup);
	st->addWidget(loopSel);
	st->addWidget(cutHere);
	st->addWidget(scissors);
	st->addSpacing(8);
	st->addWidget(del);
	st->addSpacing(8);
	st->addWidget(arrangeBtn);
	st->addStretch(1);
	st->addSpacing(12);
	st->addWidget(dimLabel(tr("Zoom")));
	st->addWidget(zoomOut);
	st->addWidget(zoomIn);
	bar->addWidget(songTools, 1);

	loopTools = new QWidget;
	auto* lt = new QHBoxLayout(loopTools);
	lt->setContentsMargins(0, 0, 0, 0);
	lt->setSpacing(6);
	auto* stopAll = plain(tr("Stop all"), tr("Stop every cell on the next bar"));
	auto* addCol = plain(tr("+  Column"), tr("Add another column of cells"));
	auto* delCell = plain(tr("Delete cell"), tr("Delete the selected cell (Delete key)"));
	theme::symbolKey(stopAll, QStringLiteral("stopall"), tr("Stop all"));
	theme::symbolKey(addCol, QStringLiteral("column"), tr("Column", "key: add a column of cells"));
	theme::symbolKey(delCell, QStringLiteral("delete"), tr("Delete"));
	lt->addWidget(stopAll);
	lt->addWidget(addCol);
	lt->addSpacing(14);
	lt->addWidget(delCell);
	lt->addStretch(1);
	// no long hint here: the loop grid sits beside the editor and a long line of text would push the window wider;
	// the cells explain themselves when you hover over them
	bar->addWidget(loopTools, 1);
	loopTools->hide();
	bv->addLayout(bar);

	bottomStack = new QStackedWidget;
	timeline = new Timeline(app);
	loops = new LoopGrid(app);
	bottomStack->addWidget(timeline);
	bottomStack->addWidget(loops);
	bv->addWidget(bottomStack, 1);
	split->addWidget(bottom);
	split->setChildrenCollapsible(false);
	// the inspector beside it all
	inspector = new Inspector(app, sound, editor);
	connect(inspector, &Inspector::hideRequested, this, [this] { setInspectorShown(false); });
	hsplit = new QSplitter(Qt::Horizontal);
	hsplit->addWidget(inspector);
	hsplit->addWidget(split);
	hsplit->setStretchFactor(0, 0);
	hsplit->setStretchFactor(1, 1);
	hsplit->setCollapsible(0, false);
	hsplit->setCollapsible(1, false);
	{
		const QList<QVariant> hs = QSettings().value(QStringLiteral("layout/inspectorWidth")).toList();
		hsplit->setSizes({hs.isEmpty() ? 310 : hs.value(0).toInt(), 1200});
	}
	connect(hsplit, &QSplitter::splitterMoved, this, [this] {
		QSettings().setValue(QStringLiteral("layout/inspectorWidth"), QVariantList{hsplit->sizes().value(0)});
	});
	{ // while the inspector is put away, a slim tab on the left edge brings it back
		auto* row = new QHBoxLayout;
		row->setContentsMargins(0, 0, 0, 0);
		row->setSpacing(0);
		inspectorTab = new QToolButton;
		inspectorTab->setObjectName("inspectorTab");
		inspectorTab->setText(QStringLiteral("»"));
		inspectorTab->setToolTip(tr("Show the inspector again (Ctrl+I)"));
		inspectorTab->setFocusPolicy(Qt::NoFocus);
		inspectorTab->setFixedWidth(20);
		inspectorTab->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
		inspectorTab->hide();
		connect(inspectorTab, &QToolButton::clicked, this, [this] { setInspectorShown(true); });
		row->addWidget(inspectorTab);
		row->addWidget(hsplit, 1);
		v->addLayout(row, 1);
	}
	{ // the tabs can go into a window of their own
		auto* own = new QToolButton;
		own->setText(QStringLiteral("⧉"));
		own->setObjectName("ownWindow");
		own->setFocusPolicy(Qt::NoFocus);
		own->setToolTip(tr("Notes, Sound and Mixer in a window of their own, for a second screen. Close that window to "
		                   "bring them back."));
		connect(own, &QToolButton::clicked, this, [this] { setEditorWindow(editorWindow == nullptr); });
		auto* cw = new QWidget;
		auto* ch = new QHBoxLayout(cw);
		ch->setContentsMargins(0, 0, 6, 3);
		ch->addWidget(own);
		tabs->setCornerWidget(cw, Qt::TopRightCorner);
	}
	setCentralWidget(central);
	applyLook();
	statusBar()->showMessage(
	    tr("Press Space to play. Letter keys play the selected track, always in the song’s scale."));

	connect(scissors, &QPushButton::toggled, timeline, &Timeline::setScissors);
	connect(cutHere, &QPushButton::clicked, this, [this] {
		app->focus = App::FocusArrange;
		app->splitAtPlayhead();
	});
	connect(dup, &QPushButton::clicked, this, [this] {
		app->focus = App::FocusArrange;
		app->duplicateSelection();
	});
	connect(del, &QPushButton::clicked, this, [this] {
		app->focus = App::FocusArrange;
		app->deleteSelection();
	});
	connect(loopSel, &QPushButton::clicked, this, [this] { app->loopSelection(); });
	connect(arrangeBtn, &QPushButton::clicked, this, [this] { arrange(); });
	connect(addTrack, &QPushButton::clicked, this,
	        [this, addTrack] { timeline->addMenu(addTrack->mapToGlobal(QPoint(0, addTrack->height()))); });
	connect(zoomOut, &QPushButton::clicked, this, [this] { timeline->zoom(1 / 1.3); });
	connect(zoomIn, &QPushButton::clicked, this, [this] { timeline->zoom(1.3); });
	connect(timeline, &Timeline::clipActivated, this, [this] { tabs->setCurrentIndex(0); });
	connect(sound, &SoundPanel::openSampleLibrary, this, [this] { showSettings(2); });
	connect(loops, &LoopGrid::clipActivated, this, [this] { tabs->setCurrentIndex(0); });
	connect(songMode, &QPushButton::clicked, this, [this] { setLoopsMode(false); });
	connect(loopMode, &QPushButton::clicked, this, [this] { setLoopsMode(true); });
	connect(stopAll, &QPushButton::clicked, this, [this] { app->stopAllCells(); });
	connect(addCol, &QPushButton::clicked, this, [this] {
		if (app->song.scenes < 32) app->edit([](Song& s) { s.scenes++; });
	});
	connect(delCell, &QPushButton::clicked, this, [this] { app->deleteCell(app->editClip); });
	connect(app, &App::changed, this, &MainWindow::sync);
	connect(app, &App::selectionChanged, this, &MainWindow::sync);
	connect(app, &App::status, this, [this](const QString& m) {
		lcd->setMessage(m);
		statusBar()->showMessage(m, 6000);
	});
	// after a big change, a note at the bottom offers to take it back
	toast = new Toast(this);
	connect(toast, &Toast::actionClicked, this, [this] {
		if (!nextLesson.isEmpty()) {
			const QString id = nextLesson;
			nextLesson.clear();
			startLesson(id);
			return;
		}
		app->undo();
	});
	connect(app, &App::announced, this, [this](const QString& what) {
		if (QSettings().value(QStringLiteral("undoNotes"), true).toBool()) {
			nextLesson.clear();
			toast->showMessage(tr("Done: %1").arg(what), tr("Undo"));
		}
	});
	connect(app, &App::undone, this, [this](const QString& what, bool redo) {
		const QString t = what.isEmpty() ? (redo ? tr("Redone") : tr("Undone"))
		                                 : (redo ? tr("Redone: %1") : tr("Undone: %1")).arg(what);
		lcd->setMessage(t.toUpper());
		statusBar()->showMessage(t, 4000);
	});

	auto* timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, &MainWindow::frame);
	timer->start(33);
	qApp->installEventFilter(this);
	timelineTop = QSettings().value(QStringLiteral("layout/timelineTop"), true).toBool();
	topAct->setChecked(timelineTop);
	setInspectorShown(QSettings().value(QStringLiteral("layout/inspector"), true).toBool());
	arrangePanels();
	applyScrollBars();
	QTimer::singleShot(0, this, [this] { fitTopBar(); }); // again once shown: the style sheet sets heights then
	sync();
	// the first time the program starts, open the guide
	if (qEnvironmentVariable("QT_QPA_PLATFORM") != "offscreen" &&
	    !QSettings().value(QStringLiteral("seenGuide"), false).toBool()) {
		QSettings().setValue(QStringLiteral("seenGuide"), true);
		QTimer::singleShot(500, this, [this] { showHelp(); });
	}
	if (!app->audioError.isEmpty())
		QTimer::singleShot(300, this, [this] {
			QMessageBox::warning(
			    this, tr("No sound"),
			    tr("The sound output could not be opened, so nothing will be heard.\n\n%1").arg(app->audioError));
		});
}

// Top bar: the emblem, then three framed units of the same height, as on a piece of studio equipment.
//   Transport: the display, with the back / play / stop / record keys and the loop, click and in-time switches under
//   it. Song: tempo (with TAP), key and scale as small readouts, and swing. Output: two needle meters and the volume
//   knob.
QWidget* MainWindow::buildTopBar() {
	auto* w = new QWidget;
	w->setObjectName("topbar");
	auto* h = new QHBoxLayout(w);
	h->setContentsMargins(14, 8, 14, 10);
	h->setSpacing(12);
	// Three units of one size, each laid out on the same rows: a row of names in small engraved capitals, then two
	// rows of controls, so every name, box and key lines up across the bar. What is taller (the display, the meters,
	// the knobs, Export) spans rows.
	// (the heights come from fitTopBar(), at the end, once the look's key height is known)
	const int titleH = 18, labelH = TB_LABEL, gapTop = TB_GAP_TOP, gapRows = TB_GAP_ROWS, padV = TB_PAD;
	const int unitH = padV + labelH + gapTop + 28 + gapRows + 28 + padV; // with keys of 28 pixels: to place the mark
	auto unit = [&](const QString& title, QLayout* inner, QWidget* extra = nullptr) {
		auto* box = new QWidget;
		auto* v = new QVBoxLayout(box);
		v->setContentsMargins(0, 0, 0, 0);
		v->setSpacing(3);
		auto* row = new QHBoxLayout;
		row->setContentsMargins(12, 0, 4, 0);
		auto* t = new QLabel(title);
		t->setObjectName("unitTitle");
		t->setFixedHeight(titleH);
		row->addWidget(t);
		row->addStretch(1);
		if (extra) row->addWidget(extra);
		v->addLayout(row);
		auto* frame = new QFrame;
		frame->setObjectName("unit");
		unitFrames << frame;
		inner->setContentsMargins(17, padV, 17, padV); // clear of the screws in the corners of the well
		frame->setLayout(inner);
		v->addWidget(frame);
		box->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed); // the three share the width equally
		return box;
	};
	auto engraved = [&](const QString& name) {
		auto* l = new QLabel(name.toUpper());
		l->setObjectName("engraved");
		l->setFixedHeight(labelH);
		return l;
	};
	// a control with its name above it, in the row of names
	auto labelled = [&](const QString& name, QWidget* control, Qt::Alignment align = Qt::AlignLeft) {
		auto* v = new QVBoxLayout;
		v->setContentsMargins(0, 0, 0, 0);
		v->setSpacing(gapTop);
		v->addWidget(engraved(name), 0, align);
		v->addWidget(control, 0, align);
		return v;
	};
	// a knob spans both rows of controls, its name above it (and its value there while it is turned)
	auto knobColumn = [&](Knob* k) {
		auto* cap = engraved(k->name());
		cap->setAlignment(Qt::AlignCenter);
		cap->setMinimumWidth(56);
		k->setCaption(cap);
		auto* v = new QVBoxLayout;
		v->setContentsMargins(0, 0, 0, 0);
		v->setSpacing(gapTop);
		v->addWidget(cap, 0, Qt::AlignHCenter);
		auto* area = new QVBoxLayout; // the knob on the middle of the two rows
		area->setContentsMargins(0, 0, 0, 0);
		area->addStretch(1);
		area->addWidget(k, 0, Qt::AlignHCenter);
		area->addStretch(1);
		auto* holder = new QWidget;
		holder->setLayout(area);
		spanControls << holder;
		v->addWidget(holder);
		return v;
	};

	auto* mark = new Emblem;
	mark->setToolTip(tr("Nachtzug"));
	auto* markBox = new QVBoxLayout;
	markBox->setContentsMargins(0, titleH + 3 + (unitH - 86) / 2, 0, (unitH - 86) / 2);
	markBox->setSpacing(0);
	markBox->addWidget(mark);
	h->addLayout(markBox);
	h->setAlignment(markBox, Qt::AlignBottom);

	// ---- transport: the display and the three switches, then the keys and undo
	auto* tv = new QVBoxLayout;
	tv->setSpacing(gapRows);
	auto* upper = new QHBoxLayout;
	upper->setSpacing(8);
	lcd = new Lcd(app);
	lcd->setMinimumWidth(196); // two lines: where the song is, and what is going on
	upper->addWidget(lcd, 1);
	auto sw = [&](const QString& name, const QString& tip) {
		auto* s = new Switch(QString(), Switch::Beside);
		s->setFixedWidth(35);
		rowControls << s;
		s->setToolTip(tip);
		s->setAccessibleName(name);
		upper->addLayout(labelled(name, s, Qt::AlignHCenter));
		return s;
	};
	loopBtn = sw(tr("Loop"), tr("Repeat the stretch marked by the bar in the Loop row of the timeline (Ctrl+L)"));
	clickBtn = sw(tr("Click"), tr("Metronome: a tick on every beat, to play along to (Ctrl+K)"));
	timeBtn =
	    sw(tr("In time"), tr("Keep in time: notes you play by hand are pulled onto the beat while the song plays"));
	tv->addLayout(upper);
	auto* keys = new QHBoxLayout;
	keys->setSpacing(6);
	auto tkey = [&](const QString& kind, const QString& tip, bool checkable) {
		auto* b = new QToolButton;
		b->setIcon(theme::transportIcon(kind));
		b->setIconSize(QSize(20, 20));
		b->setCheckable(checkable);
		b->setFocusPolicy(Qt::NoFocus);
		b->setToolTip(tip);
		b->setStatusTip(tip);
		b->setProperty("transport", true);
		b->setFixedWidth(44);
		rowControls << b;
		keys->addWidget(b);
		return b;
	};
	auto* home = tkey("start", tr("Back to the start of the loop, or of the song (Home)"), false);
	playBtn = tkey("play", tr("Play (Space starts and stops)"), true);
	stopBtn = tkey("stop", tr("Stop (Space starts and stops)"), false);
	recBtn = tkey("rec",
	              tr("Record: while this is on and the song plays, the notes you play and the knobs you turn are "
	                 "written into the song (Ctrl+R)"),
	              true);
	recBtn->setObjectName("rec");
	playBtn->setProperty("lesson", "play");
	recBtn->setProperty("lesson", "rec"); // what the lessons point at
	keys->addStretch(1);
	// undo and redo: always in view, so trying things out never feels risky
	undoBtn = tkey("undo", QString(), false);
	redoBtn = tkey("redo", QString(), false);
	for (QToolButton* b : {undoBtn, redoBtn})
		b->setFixedWidth(40);
	tv->addLayout(keys);
	auto* transport = unit(tr("TRANSPORT"), tv);
	h->addWidget(transport, 1, Qt::AlignBottom);

	// ---- song: tempo over Tap, key over scale, swing beside them
	tempo = new QSpinBox;
	tempo->setObjectName("readout");
	tempo->setRange(40, 240);
	tempo->setSuffix(tr(" BPM"));
	tempo->setFixedWidth(88);
	// only when clicked: otherwise it took the keyboard at start-up, and kept showing its old number while Tap and
	// undo changed the tempo (it does not change under the hands of someone typing in it)
	tempo->setFocusPolicy(Qt::ClickFocus);
	tempo->setAlignment(
	    Qt::AlignCenter); // the arrow buttons are hidden by the style sheet; NoButtons would squash the text field
	tempo->setToolTip(tr(
	    "Tempo in beats per minute. Click and type a number, turn the mouse wheel over it, or tap the beat on TAP."));
	auto* tap = new QPushButton(tr("Tap"));
	tap->setObjectName("tap");
	tap->setFocusPolicy(Qt::NoFocus);
	tap->setFixedWidth(88);
	tap->setToolTip(tr("Tap along with a beat, four times or more, and the tempo follows"));
	// key and scale are keys with the value on them, in the program's own type; they open their list
	keyBox = new QComboBox;
	keyBox->setObjectName("selector");
	keyBox->setFocusPolicy(Qt::NoFocus);
	keyBox->setFixedWidth(72);
	for (int pc = 0; pc < 12; pc++)
		keyBox->addItem(pitchName(pc));
	scaleBox = new QComboBox;
	scaleBox->setObjectName("selector");
	scaleBox->setFocusPolicy(Qt::NoFocus);
	scaleBox->setFixedWidth(160); // Phrygian dominant, the longest
	for (int i = 0; i < NUM_SCALES; i++)
		scaleBox->addItem(ui(SCALES[i].name));
	keyBox->setToolTip(tr("The key of the song. Every note follows it, so changing it moves the whole song."));
	scaleBox->setToolTip(tr("The scale of the song: which notes are allowed. Every pattern is re-fitted to it."));
	swing = new Knob(tr("Swing"));
	swing->setDefault(0.f);
	swing->setFormatter([](float x) { return QString::number(int(std::lround(x * 100.f))) + "%"; });
	swing->setToolTip(tr("Swing: plays every second sixteenth note a little late, for a rolling feel"));
	auto* songRow = new QHBoxLayout;
	songRow->setSpacing(12);
	auto* tempoCol = new QVBoxLayout;
	tempoCol->setSpacing(gapRows);
	tempoCol->addLayout(labelled(tr("Tempo"), tempo));
	tempoCol->addWidget(tap);
	songRow->addLayout(tempoCol);
	auto* keyCol = new QVBoxLayout;
	keyCol->setSpacing(gapRows);
	keyCol->addLayout(labelled(tr("Key · Scale"), keyBox));
	keyCol->addWidget(scaleBox);
	songRow->addLayout(keyCol);
	songRow->addStretch(1);
	songRow->addLayout(knobColumn(swing));
	auto* songUnit = unit(tr("SONG"), songRow);
	h->addWidget(songUnit, 1, Qt::AlignBottom);

	// ---- output: the two meters, the volume, Export
	auto* ov = new QHBoxLayout;
	ov->setSpacing(12);
	vuL = new VuMeter(QStringLiteral("L"));
	vuR = new VuMeter(QStringLiteral("R"));
	auto* levelCol = new QVBoxLayout;
	levelCol->setSpacing(gapTop);
	levelCol->addWidget(engraved(tr("Level")));
	auto* meters = new QHBoxLayout;
	meters->setSpacing(6);
	meters->addWidget(vuL);
	meters->addWidget(vuR);
	levelCol->addLayout(meters);
	ov->addLayout(levelCol);
	volume = new Knob(tr("Volume"));
	volume->setDefault(0.8f);
	volume->setFormatter([](float x) { return QString::number(int(std::lround(x * 100.f))) + "%"; });
	volume->setToolTip(tr("Volume of the whole song"));
	ov->addLayout(knobColumn(volume));
	ov->addStretch(1);
	// the payoff: turn the song into a sound file
	exportBtn = new QToolButton;
	exportBtn->setIcon(theme::transportIcon("export"));
	exportBtn->setIconSize(QSize(20, 20));
	exportBtn->setText(tr("Export"));
	exportBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
	exportBtn->setObjectName("exportKey");
	exportBtn->setProperty("lesson", "export");
	exportBtn->setFocusPolicy(Qt::NoFocus);
	exportBtn->setFixedWidth(std::max(66, exportBtn->fontMetrics().horizontalAdvance(exportBtn->text()) + 22));
	exportBtn->setToolTip(tr("Export: save the song as a sound file (WAV) to play anywhere or share, each track as its "
	                         "own file, or the notes as MIDI (Ctrl+Shift+E)"));
	auto* exportBox = new QVBoxLayout;
	exportBox->setContentsMargins(0, labelH + gapTop, 0, 0); // on the rows of controls, as the meters
	exportBox->addWidget(exportBtn);
	ov->addLayout(exportBox);
	auto* help = new QToolButton;
	help->setText(QStringLiteral("?"));
	help->setObjectName("help");
	help->setFocusPolicy(Qt::NoFocus);
	help->setToolTip(tr("How to use Nachtzug: a short guide (F1)"));
	auto* gear = new QToolButton;
	gear->setText(QStringLiteral("⚙"));
	gear->setObjectName("help");
	gear->setFocusPolicy(Qt::NoFocus);
	gear->setToolTip(tr("Settings: sound card, MIDI, language, colours, size, samples"));
	auto* helpRow = new QWidget;
	auto* hr = new QHBoxLayout(helpRow);
	hr->setContentsMargins(0, 0, 0, 0);
	hr->setSpacing(6);
	hr->addWidget(gear);
	hr->addWidget(help);
	auto* outUnit = unit(tr("OUTPUT"), ov, helpRow);
	h->addWidget(outUnit, 1, Qt::AlignBottom);
	// the same width for all three: the widest one's smallest
	int widest = 0;
	for (QWidget* u : {transport, songUnit, outUnit})
		widest = std::max(widest, u->minimumSizeHint().width());
	for (QWidget* u : {transport, songUnit, outUnit})
		u->setMinimumWidth(widest);
	tapKey = tap;
	rowControls << tempo << tap << keyBox << scaleBox;
	spanControls << vuL << vuR << exportBtn;
	fitTopBar();
	connect(help, &QToolButton::clicked, this, [this] { showHelp(); });
	connect(gear, &QToolButton::clicked, this, [this] { showSettings(); });
	connect(exportBtn, &QToolButton::clicked, this, [this] { exportWav(); });
	connect(undoBtn, &QToolButton::clicked, this, [this] { app->undo(); });
	connect(redoBtn, &QToolButton::clicked, this, [this] { app->redo(); });

	connect(playBtn, &QToolButton::clicked, this, [this] {
		if (!app->isPlaying()) app->play();
		playBtn->setChecked(true);
	});
	connect(stopBtn, &QToolButton::clicked, this, [this] {
		if (app->isPlaying())
			app->stop();
		else
			app->seek(app->song.loopOn ? app->song.loopStart : 0);
	});
	connect(home, &QToolButton::clicked, this, [this] { app->seek(app->song.loopOn ? app->song.loopStart : 0); });
	connect(recBtn, &QToolButton::clicked, this, [this](bool on) {
		app->recording = on;
		sync();
	});
	connect(loopBtn, &QAbstractButton::clicked, this, [this](bool on) { app->edit([&](Song& s) { s.loopOn = on; }); });
	connect(clickBtn, &QAbstractButton::clicked, this, [this](bool on) {
		app->engine->metronome.store(on);
		sync();
	});
	connect(timeBtn, &QAbstractButton::clicked, this, [this](bool on) {
		app->inTime = on;
		sync();
	});
	connect(tempo, &QSpinBox::valueChanged, this, [this](int bpm) {
		if (int(std::lround(app->song.bpm)) != bpm) app->edit([&](Song& s) { s.bpm = float(bpm); });
	});
	connect(tempo, &QSpinBox::editingFinished, tempo,
	        [t = tempo] { t->clearFocus(); }); // also fires while the window closes: touch only the box
	connect(tap, &QPushButton::clicked, this, [this] {
		const qint64 now = QDateTime::currentMSecsSinceEpoch();
		if (!taps.empty() && now - taps.back() > 2000) taps.clear(); // a long pause starts a new count
		taps.push_back(now);
		if (taps.size() > 5) taps.erase(taps.begin());
		if (taps.size() >= 3) {
			const double avg = double(taps.back() - taps.front()) / double(taps.size() - 1);
			const int bpm = std::clamp(int(std::lround(60000.0 / avg)), 40, 240);
			tempo->clearFocus(); // the box shows what was tapped, even if it was clicked before
			app->edit([&](Song& s) { s.bpm = float(bpm); });
			lcd->setMessage(tr("TEMPO %1 BPM").arg(bpm));
		} else
			lcd->setMessage(tr("KEEP TAPPING THE BEAT"));
	});
	connect(keyBox, &QComboBox::activated, this, [this](int i) { app->onBar([i](Song& s) { s.root = i; }); });
	connect(scaleBox, &QComboBox::activated, this, [this](int i) { app->onBar([i](Song& s) { s.scale = i; }); });
	connect(swing, &Knob::pressed, this, [this] { app->begin(tr("Swing")); });
	connect(swing, &Knob::moved, this, [this](float x) {
		app->song.swing = x;
		app->touch();
	});
	midiMenuFor(volume, app, QStringLiteral("master"), tr("the volume of the whole song"));
	midiMenuFor(swing, app, QStringLiteral("swing"), tr("the swing"));
	connect(volume, &Knob::pressed, this, [this] { app->begin(tr("Volume")); });
	connect(volume, &Knob::moved, this, [this](float x) {
		app->song.master = x;
		app->touch();
	});
	return w;
}

void MainWindow::buildMenus() {
	QMenu* file = menuBar()->addMenu(tr("&File"));
	file->addAction(tr("New song with fresh ideas"), QKeySequence::New, this, [this] { newSong(true); });
	file->addAction(tr("New empty song"), this, [this] { newSong(false); });
	file->addAction(tr("Open…"), QKeySequence::Open, this, [this] { openSong(); });
	file->addSeparator();
	file->addAction(tr("Save"), QKeySequence::Save, this, [this] { saveSong(false); });
	file->addAction(tr("Save as…"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S), this, [this] { saveSong(true); });
	file->addAction(tr("Export: sound file, tracks or MIDI…"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E), this,
	                [this] { exportWav(); });
	file->addSeparator();
	file->addAction(tr("Settings…"), QKeySequence::Preferences, this, [this] { showSettings(); })
	    ->setMenuRole(QAction::PreferencesRole);
	file->addSeparator();
	file->addAction(tr("Quit"), QKeySequence::Quit, this, [this] { close(); })
	    ->setMenuRole(QAction::QuitRole); // on a Mac: in the Nachtzug menu

	QMenu* edit = menuBar()->addMenu(tr("&Edit"));
	undoAct = edit->addAction(tr("Undo"), QKeySequence::Undo, this, [this] { app->undo(); });
	redoAct =
	    edit->addAction(tr("Redo"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z), this, [this] { app->redo(); });
	edit->addSeparator();
	edit->addAction(tr("Cut"), QKeySequence::Cut, this, [this] { app->cut(); });
	edit->addAction(tr("Copy"), QKeySequence::Copy, this, [this] { app->copy(); });
	edit->addAction(tr("Paste"), QKeySequence::Paste, this, [this] { app->paste(); });
	edit->addAction(tr("Duplicate"), QKeySequence(Qt::CTRL | Qt::Key_D), this, [this] { app->duplicateSelection(); });
	QAction* del = edit->addAction(tr("Delete"), this, [this] { app->deleteSelection(); });
	del->setShortcuts({QKeySequence(Qt::Key_Delete), QKeySequence(Qt::Key_Backspace)});
	edit->addAction(tr("Select all"), QKeySequence::SelectAll, this, [this] { app->selectAll(); });
	edit->addSeparator();
	edit->addAction(tr("Cut clip in two at the playhead"), QKeySequence(Qt::CTRL | Qt::Key_E), this, [this] {
		app->focus = App::FocusArrange;
		app->splitAtPlayhead();
	});
	edit->addSeparator();
	edit->addAction(tr("Arrange the song for me…"), this, [this] { arrange(); });

	QMenu* play = menuBar()->addMenu(tr("&Play"));
	play->addAction(tr("Play or stop\tSpace"), this, [this] { app->togglePlay(); });
	auto* toStart = play->addAction(tr("Back to the start"), QKeySequence(Qt::Key_Home), this,
	                                [this] { app->seek(app->song.loopOn ? app->song.loopStart : 0); });
#if defined(Q_OS_MACOS)
	toStart->setShortcuts(
	    {QKeySequence(Qt::Key_Home), QKeySequence(Qt::Key_Return)}); // Mac laptops have no Home key (Fn + ← works too)
#else
	Q_UNUSED(toStart);
#endif
	play->addSeparator();
	recAct =
	    play->addAction(tr("Record notes and knob moves"), QKeySequence(Qt::CTRL | Qt::Key_R), this, [this](bool on) {
		    app->recording = on;
		    sync();
	    });
	loopAct = play->addAction(tr("Loop"), QKeySequence(Qt::CTRL | Qt::Key_L), this,
	                          [this](bool on) { app->edit([&](Song& s) { s.loopOn = on; }); });
	clickAct = play->addAction(tr("Metronome click"), QKeySequence(Qt::CTRL | Qt::Key_K), this, [this](bool on) {
		app->engine->metronome.store(on);
		sync();
	});
	timeAct = play->addAction(tr("Keep hand-played notes in time"), this, [this](bool on) {
		app->inTime = on;
		sync();
	});
	snapAct = play->addAction(tr("Snap recorded notes to the grid"), this, [this](bool on) {
		app->quantize = on;
		sync();
	});
	play->addSeparator();
	chordAct = play->addAction(tr("Letter keys change the chord"), QKeySequence(Qt::CTRL | Qt::Key_J), this,
	                           [this](bool on) { app->setChordKeys(on); });
	chordAct->setStatusTip(tr("Chords mode: the letter keys change the chord for every track that follows the chords, "
	                          "instead of playing notes"));
	for (QAction* a : {recAct, loopAct, clickAct, timeAct, snapAct, chordAct})
		a->setCheckable(true);

	QMenu* track = menuBar()->addMenu(tr("&Track"));
	const std::pair<int, QString> roles[] = {{R_BASS, tr("Add a bass line")},
	                                         {R_SEQ, tr("Add a sequence")},
	                                         {R_PAD, tr("Add a pad (chords)")},
	                                         {R_LEAD, tr("Add a lead (melody)")},
	                                         {R_DRUMS, tr("Add drums (auto drummer)")}};
	for (auto& [role, name] : roles) {
		const int r = role;
		track->addAction(name, this, [this, r] {
			app->addTrack(r);
			tabs->setCurrentIndex(0);
		});
	}
	track->addSeparator();
	track->addAction(tr("Chop a loop across the drums…"), this, [this] {
		app->releaseAllKeys();
		ChopDialog::chooseAndChop(app, this);
	});
	track->addSeparator();
	track->addAction(tr("Delete the selected track"), this, [this] {
		if (app->curTrack()) app->removeTrack(app->selTrack);
	});

	QMenu* view = menuBar()->addMenu(tr("&View"));
	padsAct = view->addAction(tr("On-screen keys under the notes"));
	padsAct->setCheckable(true);
	padsAct->setObjectName("padsAct");
	QTimer::singleShot(0, this, [this] { // the editor is made after the menus
		int mode = QSettings().value(QStringLiteral("padsMode"), -1).toInt();
		if (mode < 0)
			mode = QSettings().value(QStringLiteral("pads"), true).toBool() ? 0 : 2; // the older on/off setting
		editor->setPadsMode(mode);
		padsAct->setChecked(mode != 2);
		connect(padsAct, &QAction::toggled, this, [this](bool on) { editor->setPadsMode(on ? 1 : 2); });
		connect(editor, &EditorPanel::padsShown, padsAct, [this](bool on) {
			const QSignalBlocker b(padsAct);
			padsAct->setChecked(on);
		});
	});
	view->addSeparator();
	inspAct = view->addAction(tr("Inspector"));
	inspAct->setObjectName("inspectorAct");
	inspAct->setCheckable(true);
	inspAct->setChecked(true);
	inspAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
	inspAct->setStatusTip(tr("The column on the left with the selected track’s level, sound and settings"));
	connect(inspAct, &QAction::toggled, this, [this](bool on) { setInspectorShown(on); });
	topAct = view->addAction(tr("Timeline on top"));
	topAct->setObjectName("timelineTopAct");
	topAct->setCheckable(true);
	topAct->setChecked(true);
	topAct->setStatusTip(tr("The timeline above Notes, Sound and Mixer, as in most music programs. Untick it to have "
	                        "the timeline, or the loop grid, beside them on the left."));
	connect(topAct, &QAction::toggled, this, [this](bool on) {
		timelineTop = on;
		QSettings().setValue(QStringLiteral("layout/timelineTop"), on);
		arrangePanels();
	});
	ownWinAct = view->addAction(tr("Notes, Sound and Mixer in their own window"));
	ownWinAct->setObjectName("ownWindowAct");
	ownWinAct->setCheckable(true);
	ownWinAct->setStatusTip(
	    tr("For a second screen: the tabs in a window of their own. Close that window to bring them back."));
	connect(ownWinAct, &QAction::toggled, this, [this](bool on) { setEditorWindow(on); });
	view->addAction(tr("Reset the layout"), this, [this] { resetLayout(); })
	    ->setStatusTip(tr("Everything back where it was at the start, with every section open"));
	view->addSeparator();
	// which page the Sound tab shows; the settings have the same choice
	QAction* simpleAct = view->addAction(tr("Simple sound page"));
	simpleAct->setObjectName("simpleSound");
	simpleAct->setCheckable(true);
	simpleAct->setStatusTip(
	    tr("The Sound tab shows a few big knobs in plain words. Untick it to see every knob of the synthesiser."));
	QTimer::singleShot(0, this, [this, simpleAct] { // the Sound tab is made after the menus
		simpleAct->setChecked(sound->isSimple());
		connect(simpleAct, &QAction::toggled, this, [this](bool on) {
			sound->setSimple(on);
			tabs->setCurrentIndex(1);
		});
		connect(sound, &SoundPanel::simpleChanged, simpleAct, [simpleAct](bool on) {
			const QSignalBlocker b(simpleAct);
			simpleAct->setChecked(on);
		});
	});
	view->addSeparator();
	QMenu* langs = view->addMenu(tr("Language"));
	auto* langGroup = new QActionGroup(this);
	const QString langNow = QSettings().value(QStringLiteral("language")).toString();
	for (auto [code, name] : {std::pair{"", QT_TR_NOOP("Same as the system")}, {"en", "English"}, {"de", "Deutsch"}}) {
		const QString c = QString::fromLatin1(code);
		QAction* a = langs->addAction(c.isEmpty() ? tr(name) : QString::fromUtf8(name), this, [this, c] {
			QSettings().setValue(QStringLiteral("language"), c);
			QMessageBox::information(this, tr("Language"),
			                         tr("The new language is used the next time Nachtzug starts."));
		});
		a->setCheckable(true);
		a->setChecked(c == langNow);
		langGroup->addAction(a);
	}
	QMenu* themes = view->addMenu(tr("Colours"));
	auto* group = new QActionGroup(this);
	for (int i = 0; i < theme::count(); i++) {
		QAction* a = themes->addAction(ui(theme::name(i)), this, [this, i] { setTheme(i); });
		a->setCheckable(true);
		a->setChecked(i == theme::current());
		group->addAction(a);
	}

	QMenu* help = menuBar()->addMenu(tr("&Help"));
	help->addAction(tr("How to use Nachtzug"), QKeySequence::HelpContents, this, [this] { showHelp(); });
	help->addAction(tr("Operation manual (PDF)"), this, [this] { openManual(); })
	    ->setStatusTip(tr("The full manual, with pictures of every part, sound recipes and a table of every key"));
	lessonMenu = help->addMenu(tr("Lessons"));
	connect(lessonMenu, &QMenu::aboutToShow, this, [this] { fillLessonMenu(); });
	fillLessonMenu();
	help->addSeparator();
	QAction* about = help->addAction(tr("About Nachtzug"), this, [this] {
		QMessageBox box(this);
		box.setWindowTitle(tr("About Nachtzug"));
		box.setIconPixmap(
		    QPixmap(QStringLiteral(":/logo.png")).scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
		box.setTextFormat(Qt::RichText);
		box.setText(tr("<h2>Nachtzug %1</h2><p>A little music machine for acid and Berlin School music.</p>")
		                .arg(QStringLiteral(NACHTZUG_VERSION)));
		box.setInformativeText(tr("<p><b>The music you make with Nachtzug is yours, completely</b>, and so are the "
		                          "built-in sounds once they’re in your song.</p>"
		                          "<p>Nachtzug is free software: you may use, share and change it under the GNU "
		                          "General Public License, version 3 or later. "
		                          "It comes with no warranty. The sounds are in the public domain (CC0), the manuals "
		                          "and pictures are under CC BY-SA 4.0, "
		                          "and the fonts under the SIL Open Font License.</p>"
		                          "<p>Built with Qt and SDL. Copyright © 2026 Marvin Johanning.</p>"));
		box.exec();
	});
	about->setMenuRole(QAction::AboutRole); // in the application menu on a Mac
}

// the lessons, with a tick beside the ones finished before
void MainWindow::fillLessonMenu() {
	if (!lessonMenu) return;
	lessonMenu->clear();
	const QStringList done = QSettings().value(QStringLiteral("lessonsDone")).toStringList();
	int n = 1;
	for (const Lesson& l : makeLessons(app, this)) {
		QAction* a = lessonMenu->addAction(QStringLiteral("%1. %2").arg(n++).arg(l.title), this,
		                                   [this, id = l.id] { startLesson(id); });
		a->setCheckable(true);
		a->setChecked(done.contains(l.id));
		a->setStatusTip(l.about);
		a->setToolTip(l.about);
	}
	lessonMenu->setToolTipsVisible(true);
}

void MainWindow::startLesson(const QString& id) {
	setEditorWindow(false); // the lessons point at things in this window
	if (!lessonOverlay) {
		lessonOverlay = new LessonOverlay(this);
		connect(lessonOverlay, &LessonOverlay::finished, this, [this](const QString& done, bool completed) {
			if (!completed) return;
			QStringList list = QSettings().value(QStringLiteral("lessonsDone")).toStringList();
			if (!list.contains(done)) {
				list << done;
				QSettings().setValue(QStringLiteral("lessonsDone"), list);
			}
			// offer the next one
			const std::vector<Lesson> all = makeLessons(app, this);
			for (size_t i = 0; i + 1 < all.size(); i++)
				if (all[i].id == done) {
					toast->showMessage(tr("Lesson done. Next: %1").arg(all[i + 1].title), tr("Start it"), 8000);
					nextLesson = all[i + 1].id;
					return;
				}
			toast->showMessage(tr("That was the last lesson. Have fun!"));
		});
	}
	for (const Lesson& l : makeLessons(app, this))
		if (l.id == id) {
			lessonOverlay->start(l);
			return;
		}
}

void MainWindow::sync() {
	const Song& s = app->song;
	setWindowTitle(QStringLiteral("%1%2 — Nachtzug")
	                   .arg(app->filePath.isEmpty() ? tr("Untitled song") : QFileInfo(app->filePath).completeBaseName(),
	                        app->dirty ? QStringLiteral(" *") : QString()));
	if (!tempo->hasFocus()) {
		tempo->blockSignals(true);
		tempo->setValue(int(std::lround(s.bpm)));
		tempo->blockSignals(false);
	}
	keyBox->blockSignals(true);
	keyBox->setCurrentIndex(s.root);
	keyBox->blockSignals(false);
	scaleBox->blockSignals(true);
	scaleBox->setCurrentIndex(s.scale);
	scaleBox->blockSignals(false);
	if (!swing->isHeld()) swing->setValue(s.swing);
	if (!volume->isHeld()) volume->setValue(s.master);
	swing->setAccent(theme::amber);
	volume->setAccent(theme::amber);
	const bool click = app->engine->metronome.load();
	recBtn->setChecked(app->recording);
	recAct->setChecked(app->recording);
	loopBtn->setChecked(s.loopOn);
	loopAct->setChecked(s.loopOn);
	clickBtn->setChecked(click);
	clickAct->setChecked(click);
	timeBtn->setChecked(app->inTime);
	timeAct->setChecked(app->inTime);
	snapAct->setChecked(app->quantize);
	chordAct->setChecked(app->chordKeys());
	undoAct->setEnabled(app->canUndo());
	redoAct->setEnabled(app->canRedo());
	{ // say what Undo and Redo would take back
		const QString u = app->undoName(), r = app->redoName();
		undoAct->setText(app->canUndo() && !u.isEmpty() ? tr("Undo: %1").arg(u) : tr("Undo"));
		redoAct->setText(app->canRedo() && !r.isEmpty() ? tr("Redo: %1").arg(r) : tr("Redo"));
		undoBtn->setEnabled(app->canUndo());
		redoBtn->setEnabled(app->canRedo());
		undoBtn->setToolTip(!app->canUndo() ? tr("Undo (Ctrl+Z): nothing to undo yet")
		                    : u.isEmpty()   ? tr("Undo the last change (Ctrl+Z)")
		                                    : tr("Undo: %1 (Ctrl+Z)").arg(u));
		redoBtn->setToolTip(!app->canRedo() ? tr("Redo (Ctrl+Shift+Z): nothing to redo")
		                    : r.isEmpty()   ? tr("Redo what was undone (Ctrl+Shift+Z)")
		                                    : tr("Redo: %1 (Ctrl+Shift+Z)").arg(r));
	}
	// the bottom area follows the song's mode (undo and opening a song can change it)
	songMode->setChecked(!s.liveMode);
	loopMode->setChecked(s.liveMode);
	// narrow things first, then turn the divider: otherwise both wide toolbars briefly sit side by side
	// and the window grows to fit them
	songTools->setVisible(!s.liveMode);
	loopTools->setVisible(s.liveMode);
	editor->setCompact(s.liveMode);
	bottomStack->setCurrentIndex(s.liveMode ? 1 : 0);
	if (s.liveMode != shownLive) {
		shownLive = s.liveMode;
		arrangePanels();
	}
	loopBtn->setEnabled(!s.liveMode);
	// the first two tabs belong to the selected track and say so; the mixer shows all tracks
	const Track* t = s.track(app->selTrack);
	const QIcon dot =
	    t ? theme::roleIcon(t->role, theme::trackColor(t->role).darker(theme::isLight() ? 120 : 100)) : QIcon();
	const QString who = t ? ui(t->name).toUpper() : tr("NO TRACK");
	tabs->setTabText(0, tr("%1  ·  NOTES").arg(who));
	tabs->setTabText(1, tr("%1  ·  SOUND").arg(who));
	tabs->setTabText(2, tr("MIXER"));
	tabs->setTabIcon(0, dot);
	tabs->setTabIcon(1, dot);
	tabs->setTabIcon(2, theme::mixerIcon(theme::amber.darker(theme::isLight() ? 120 : 100)));
}

// things the style sheet cannot do
void MainWindow::applyLook() {
	// the transport symbols are drawn in the theme's colours, so draw them again
	playBtn->setIcon(theme::transportIcon("play"));
	stopBtn->setIcon(theme::transportIcon("stop"));
	recBtn->setIcon(theme::transportIcon("rec"));
	applyKeyLook();
	undoBtn->setIcon(theme::transportIcon("undo"));
	redoBtn->setIcon(theme::transportIcon("redo"));
	exportBtn->setIcon(theme::transportIcon("export"));
}

void MainWindow::setTheme(int index) {
	theme::apply(index);
	QSettings().setValue(QStringLiteral("look"), theme::name(index));
	applyLook();
	QApplication::setPalette(theme::palette());
	qApp->setStyleSheet(theme::styleSheet());
	fitTopBar();
	sound->sync();
	mixer->sync();
	sync();
	for (QWidget* w : QApplication::allWidgets())
		w->update();
}

// ---------------------------------------------------------------- the panels

// The timeline (or the loop grid) and the tabs: the timeline on top by default; without, beside the tabs on the
// left, in Song and Loops alike; or the tabs in their own window. Each arrangement remembers where its divider was.
void MainWindow::arrangePanels() {
	const QString key = editorWindow  ? QStringLiteral("alone")
	                    : timelineTop ? QStringLiteral("top")
	                                  : QStringLiteral("side");
	if (!shownArrangement.isEmpty() && split->count() > 1) splitSizes[shownArrangement] = split->sizes();
	if (editorWindow) {
		split->setOrientation(Qt::Vertical);
		split->insertWidget(0, bottomArea);
	} else {
		if (tabs->parentWidget() != split) split->addWidget(tabs);
		split->setOrientation(key == QLatin1String("side") ? Qt::Horizontal : Qt::Vertical);
		split->insertWidget(0, bottomArea);
		static const QHash<QString, QList<int>> start = {{QStringLiteral("top"), {330, 470}},
		                                                 {QStringLiteral("side"), {930, 590}}};
		split->setSizes(splitSizes.value(key, start.value(key)));
	}
	tabs->show();
	bottomArea->show();
	shownArrangement = key;
}

void MainWindow::setInspectorShown(bool on) {
	inspector->setVisible(on);
	if (inspectorTab) inspectorTab->setVisible(!on);
	inspector->adopt(on); // without the inspector, How notes are played and the drummer go back to their tabs
	QSettings().setValue(QStringLiteral("layout/inspector"), on);
	if (inspAct && inspAct->isChecked() != on) {
		const QSignalBlocker b(inspAct);
		inspAct->setChecked(on);
	}
}

void MainWindow::setEditorWindow(bool on) {
	if (on == (editorWindow != nullptr)) return;
	if (on) {
		editorWindow = new QMainWindow(nullptr);
		editorWindow->setAttribute(Qt::WA_DeleteOnClose, false);
		editorWindow->setWindowTitle(tr("Notes, Sound and Mixer — Nachtzug"));
		editorWindow->setWindowIcon(windowIcon());
		auto* holder = new QWidget;
		auto* hl = new QVBoxLayout(holder);
		hl->setContentsMargins(6, 6, 6, 6);
		hl->addWidget(tabs);
		editorWindow->setCentralWidget(holder);
		for (QAction* m : menuBar()->actions())
			if (m->menu()) editorWindow->addActions(m->menu()->actions()); // the shortcuts work there too
		editorWindow->installEventFilter(this);
		editorWindow->resize(std::max(900, tabs->width()), std::max(520, tabs->height() + 40));
		arrangePanels();
		editorWindow->show();
	} else {
		QMainWindow* w = editorWindow;
		editorWindow = nullptr;
		tabs->setParent(nullptr);
		arrangePanels();
		w->removeEventFilter(this);
		w->hide();
		w->deleteLater();
	}
	if (ownWinAct && ownWinAct->isChecked() != on) {
		const QSignalBlocker b(ownWinAct);
		ownWinAct->setChecked(on);
	}
}

void MainWindow::resetLayout() {
	setEditorWindow(false);
	timelineTop = true;
	QSettings().setValue(QStringLiteral("layout/timelineTop"), true);
	{
		const QSignalBlocker b(topAct);
		topAct->setChecked(true);
	}
	splitSizes.clear();
	shownArrangement.clear();
	setInspectorShown(true);
	hsplit->setSizes({300, std::max(600, hsplit->width() - 300)});
	QSettings().remove(QStringLiteral("layout/inspectorWidth"));
	QSettings().remove(QStringLiteral("fold"));
	editor->setPadsMode(0);
	for (QGroupBox* g : findChildren<QGroupBox*>())
		theme::setFolded(g, false);
	arrangePanels();
}

void MainWindow::setLoopsMode(bool on) {
	app->setLiveMode(on);
	sync();
}

// about 30 times a second: playhead, meters, moving knobs
void MainWindow::frame() {
	const bool playing = app->isPlaying();
	if (playBtn->isChecked() != playing) playBtn->setChecked(playing);
	vuL->feed(app->engine->peakL.load(std::memory_order_relaxed));
	vuR->feed(app->engine->peakR.load(std::memory_order_relaxed));
	lcd->update();
	if (app->song.liveMode)
		loops->viewport()->update();
	else {
		timeline->followPlayhead();
		timeline->viewport()->update();
	}
	const int tab = tabs->currentIndex();
	if (tab == 0)
		editor->repaintView();
	else if (tab == 1)
		sound->syncMoving();
	else
		mixer->tick();
}

// ---------------------------------------------------------------- computer keyboard as an instrument

bool MainWindow::mapKey(const QKeyEvent* k, int& row, int& col) {
	// the physical key, whatever the layout, so German, French or Dvorak keyboards play the same pads
#if defined(Q_OS_MACOS)
	// macOS virtual key codes (kVK_ANSI_*), left to right along each row
	static const int mac[3][10] = {{0x06, 0x07, 0x08, 0x09, 0x0B, 0x2D, 0x2E, 0x2B, 0x2F, 0x2C},
	                               {0x00, 0x01, 0x02, 0x03, 0x05, 0x04, 0x26, 0x28, 0x25, 0x29},
	                               {0x0C, 0x0D, 0x0E, 0x0F, 0x11, 0x10, 0x20, 0x22, 0x1F, 0x23}};
	if (k->spontaneous()) { // from the system (0 is a real code there: the A key)
		for (int r = 0; r < 3; r++)
			for (int c = 0; c < 10; c++)
				if (int(k->nativeVirtualKey()) == mac[r][c]) {
					row = r;
					col = c;
					return true;
				}
		return false;
	}
#else
#if defined(Q_OS_WIN)
	const int sc = int(k->nativeScanCode()); // PC scan code
#else
	const int sc = int(k->nativeScanCode()) - 8; // X11 / Wayland: evdev code + 8
#endif
	if (sc >= 16 && sc <= 25) {
		row = 2;
		col = sc - 16;
		return true;
	}
	if (sc >= 30 && sc <= 39) {
		row = 1;
		col = sc - 30;
		return true;
	}
	if (sc >= 44 && sc <= 53) {
		row = 0;
		col = sc - 44;
		return true;
	}
	if (k->nativeScanCode() != 0) return false;
#endif
	static const char* rows[3] = {"ZXCVBNM,./", "ASDFGHJKL;", "QWERTYUIOP"};
	const QString t = QKeySequence(k->key()).toString();
	if (t.size() != 1) return false;
	for (int r = 0; r < 3; r++)
		for (int c = 0; c < 10; c++)
			if (t.at(0) == QLatin1Char(rows[r][c])) {
				row = r;
				col = c;
				return true;
			}
	return false;
}

bool MainWindow::eventFilter(QObject* o, QEvent* e) {
	if (editorWindow && o == editorWindow && e->type() == QEvent::Close) {
		e->ignore();
		QTimer::singleShot(0, this, [this] { setEditorWindow(false); });
		return true;
	}
	if (e->type() == QEvent::WindowDeactivate && o == this) {
		app->releaseAllKeys();
		app->shiftHeld = false;
	}
	if (e->type() != QEvent::KeyPress && e->type() != QEvent::KeyRelease) return false;
	auto* k = static_cast<QKeyEvent*>(e);
	const bool press = e->type() == QEvent::KeyPress;
	// Shift held: the keys play the "right wrong notes", and the pads show which
	const bool shift = k->key() == Qt::Key_Shift ? press : bool(k->modifiers() & Qt::ShiftModifier);
	if (shift != app->shiftHeld && !k->isAutoRepeat()) {
		app->shiftHeld = shift;
		editor->repaintPads();
	}
	if (!press &&
	    !k->isAutoRepeat()) { // a letter let go always ends its note: even with a list open, a dialog up or Ctrl held
		int row = 0, col = 0;
		if (mapKey(k, row, col)) app->keyUp(row * 16 + col);
	}
	if (QApplication::activeModalWidget() || QApplication::activePopupWidget()) {
		if (press) app->releaseAllKeys();
		return false;
	}
	QWidget* fw = QApplication::focusWidget();
	if (fw && (qobject_cast<QLineEdit*>(fw) ||
	           qobject_cast<QAbstractSpinBox*>(fw))) { // typing a number: leave the keys alone
		if (press && k->key() == Qt::Key_Escape) fw->clearFocus();
		return false;
	}
	if (k->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) return false;
	if (k->key() == Qt::Key_Space) {
		if (press && !k->isAutoRepeat()) app->togglePlay();
		return true;
	}
	if (k->key() == Qt::Key_Escape && app->chordKeys() && app->liveChord() != Engine::NO_CHORD) {
		if (press) app->releaseChord();
		return true;
	}
	if (k->key() == Qt::Key_Escape && app->midi && !app->midi->learning().isEmpty()) {
		if (press) app->midi->cancelLearn();
		return true;
	}
	int row = 0, col = 0;
	if (!mapKey(k, row, col)) return false;
	if (k->isAutoRepeat()) return true;
	if (press)
		app->keyDown(row * 16 + col, row, col, k->modifiers() & Qt::ShiftModifier); // the release was handled above
	return true;
}

// ---------------------------------------------------------------- files

bool MainWindow::maybeSave() {
	if (!app->dirty) return true;
	const auto r = QMessageBox::question(
	    this, tr("Unsaved changes"), tr("This song has changes that are not saved. Save them first?"),
	    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
	if (r == QMessageBox::Cancel) return false;
	return r == QMessageBox::Discard || saveSong(false);
}

bool MainWindow::saveSong(bool askName) {
	QString path = app->filePath;
	if (askName || path.isEmpty()) {
		path = QFileDialog::getSaveFileName(this, tr("Save song"),
		                                    path.isEmpty() ? QDir::homePath() + "/song.nachtzug" : path,
		                                    tr("Nachtzug songs (*.nachtzug)"));
		if (path.isEmpty()) return false;
		if (!path.endsWith(".nachtzug")) path += ".nachtzug";
	}
	if (!app->save(path)) {
		QMessageBox::warning(this, tr("Save failed"), tr("The song could not be written to %1.").arg(path));
		return false;
	}
	statusBar()->showMessage(tr("Saved to %1").arg(path), 5000);
	return true;
}

void MainWindow::openSong() {
	if (!maybeSave()) return;
	const QString path =
	    QFileDialog::getOpenFileName(this, tr("Open song"), QDir::homePath(), tr("Nachtzug songs (*.nachtzug)"));
	if (path.isEmpty()) return;
	if (!app->load(path))
		QMessageBox::warning(this, tr("Open failed"),
		                     tr("%1 is not a Nachtzug song, or it could not be read.").arg(path));
}

void MainWindow::newSong(bool starter) {
	if (!maybeSave()) return;
	Song s;
	if (starter) {
		s.setChords(randomProgression(s, app->rng));
		for (int role : {R_DRUMS, R_BASS, R_SEQ, R_PAD})
			addTrackWithClip(s, role, app->rng);
		s.tracks.push_back(makeTrack(s, R_LEAD));
	}
	app->setSong(s, QString());
}

void MainWindow::exportWav() {
	app->releaseAllKeys();
	ExportDialog::run(app, this);
}

void MainWindow::arrange() {
	app->releaseAllKeys();
	QDialog dlg(this);
	dlg.setWindowTitle(tr("Arrange for me"));
	auto* v = new QVBoxLayout(&dlg);
	v->setContentsMargins(20, 18, 20, 14);
	v->setSpacing(12);
	auto* head = new QLabel(
	    tr("<b style=\"font-size:16px\">Arrange the song for me</b><br>"
		   "Your patterns become a whole song: the parts come in one by one, there is a break in the middle, "
		   "and it fades out at the end. What is in the timeline now is replaced; Undo brings it back."));
	head->setTextFormat(Qt::RichText);
	head->setWordWrap(true);
	head->setMinimumWidth(480);
	v->addWidget(head);
	auto* group = new QButtonGroup(&dlg);
	auto minutes = [this](int bars) {
		const int secs = int(std::lround(bars * 4 * 60.0 / app->song.bpm));
		return QStringLiteral("%1:%2").arg(secs / 60).arg(secs % 60, 2, 10, QLatin1Char('0'));
	};
	const char* names[3] = {QT_TR_NOOP("Short"), QT_TR_NOOP("Medium"), QT_TR_NOOP("Long")};
	auto* parts = new QLabel;
	parts->setObjectName("dim");
	parts->setWordWrap(true);
	auto describe = [parts](int len) {
		QStringList l;
		for (const ArrangePart& p : arrangePlan(len))
			l << QStringLiteral("%1 (%2)").arg(ui(p.name)).arg(p.bars);
		parts->setText(tr("Parts, with their length in bars: %1").arg(l.join(QStringLiteral("  ·  "))));
	};
	const int was = std::clamp(QSettings().value(QStringLiteral("arrangeLength"), 1).toInt(), 0, 2);
	for (int i = 0; i < 3; i++) {
		int bars = 0;
		for (const ArrangePart& p : arrangePlan(i))
			bars += p.bars;
		auto* r =
		    new QRadioButton(tr("%1: %2 bars, about %3 at this tempo").arg(tr(names[i])).arg(bars).arg(minutes(bars)));
		group->addButton(r, i);
		if (i == was) r->setChecked(true);
		v->addWidget(r);
	}
	v->addWidget(parts);
	describe(was);
	connect(group, &QButtonGroup::idClicked, &dlg, describe);
	auto* lead = new Switch(tr("If the lead track is empty, write a melody for it"), Switch::Beside);
	lead->setChecked(true);
	bool anyLead = false;
	for (const Track& t : app->song.tracks)
		anyLead |= t.role == R_LEAD;
	lead->setVisible(anyLead);
	v->addWidget(lead);
	auto* buttons = new QDialogButtonBox;
	auto* go = buttons->addButton(tr("Arrange"), QDialogButtonBox::AcceptRole);
	go->setObjectName("primary");
	go->setDefault(true);
	buttons->addButton(QDialogButtonBox::Cancel);
	connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
	v->addWidget(buttons);
	if (dlg.exec() != QDialog::Accepted) return;
	const int len = group->checkedId();
	QSettings().setValue(QStringLiteral("arrangeLength"), len);
	const bool writeLead = lead->isChecked();
	if (app->song.liveMode) setLoopsMode(false);
	app->edit([&](Song& s) { arrangeSong(s, len, writeLead, app->rng); }, tr("Arrange for me"), true);
	app->selClips.clear();
	app->editClip = -1;
	app->refreshSelection();
	app->seek(0);
	timeline->showWholeSong();
	emit app->status(tr("The song is arranged. Press Space to hear it from the start."));
}

void MainWindow::showDialog(const QString& which) {
	if (which == QLatin1String("settings"))
		showSettings(0);
	else if (which == QLatin1String("soundcard"))
		showSettings(1);
	else if (which == QLatin1String("samples"))
		showSettings(2);
	else if (which == QLatin1String("midi"))
		showSettings(3);
	else if (which == QLatin1String("export"))
		exportWav();
	else if (which == QLatin1String("arrange"))
		arrange();
	else if (which == QLatin1String("chop")) { // for pictures: two bars of the song's drums as the loop
		Song s = app->song;
		int did = -1;
		for (const Track& t : s.tracks)
			if (t.isDrums()) did = t.id;
		RenderOptions o;
		o.loopOnly = true;
		o.onlyTrack = did;
		o.maxTail = 0.0;
		s.loopStart = 0;
		s.loopEnd = 2 * BAR;
		const QString tmp = QDir::temp().filePath(QStringLiteral("nachtzug-beat.wav"));
		renderSong(s, tmp, o, {});
		auto d = std::make_shared<dsp::SampleData>();
		QString err;
		if (samples::readFile(tmp, *d, err)) {
			ChopDialog dlg(app, d, tr("Beat"), this);
			dlg.exec();
		}
		QFile::remove(tmp);
	}
}

void MainWindow::showSettings(int page) {
	SettingsDialog::Hooks h;
	h.theme = [this](int i) { setTheme(i); };
	h.simpleSound = [this](bool simple) { sound->setSimple(simple); };
	h.pads = [this](bool on) { editor->setPadsMode(on ? 0 : 2); };
	h.restart = [this] { restartProgram(); };
	h.scrollBars = [this](bool) { applyScrollBars(); };
	h.keys = [this](bool) { applyKeyLook(); };
	h.timelineTop = [this](bool on) { topAct->setChecked(on); };
	SettingsDialog dlg(app, h, this, page);
	dlg.exec();
	sync();
}

void MainWindow::fitTopBar() {
	if (!tapKey) return;
	tapKey->ensurePolished(); // Tap: a plain key of the current look
	const int key = std::max(24, tapKey->sizeHint().height());
	for (QWidget* w : rowControls) {
		w->ensurePolished(); // after the style sheet, so its heights do not override these
		w->setFixedHeight(key);
	}
	lcd->setFixedHeight(TB_LABEL + TB_GAP_TOP + key);
	for (QWidget* w : spanControls) {
		w->ensurePolished();
		w->setFixedHeight(key + TB_GAP_ROWS + key);
	}
	for (QFrame* f : unitFrames)
		f->setFixedHeight(TB_PAD + TB_LABEL + TB_GAP_TOP + key + TB_GAP_ROWS + key + TB_PAD);
}

// every key with a symbol, in every window of the program (the tabs may have one of their own), and the painted
// keys of the timeline and the loop grid
void MainWindow::applyKeyLook() {
	theme::loadKeyLook();
	for (QWidget* w : QApplication::topLevelWidgets())
		theme::dressSymbolKeys(w);
	for (QWidget* w : QApplication::allWidgets())
		if (qobject_cast<QAbstractScrollArea*>(w)) w->update();
}

// The scroll bars are hidden unless Settings shows them: the wheel and the trackpad scroll as before (Shift + wheel
// sideways), and the window looks less like a form. A scroll area that never scrolls sideways says so with the
// property "noHBar". Lists in dialogs keep their bars.
void MainWindow::applyScrollBars() {
	const bool show = QSettings().value(QStringLiteral("scrollBars"), false).toBool();
	for (QAbstractScrollArea* a : findChildren<QAbstractScrollArea*>()) {
		if (qobject_cast<QAbstractItemView*>(a) || qobject_cast<QTextEdit*>(a)) continue; // a drop-down's list, a text
		a->setVerticalScrollBarPolicy(show ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
		a->setHorizontalScrollBarPolicy(show && !a->property("noHBar").toBool() ? Qt::ScrollBarAsNeeded
		                                                                        : Qt::ScrollBarAlwaysOff);
	}
}

// closes the program and opens it again (for a new size); the song that was open comes back if it is saved
void MainWindow::restartProgram() {
	if (!maybeSave()) return;
	QStringList args;
	if (!app->filePath.isEmpty() && !app->dirty && QFileInfo::exists(app->filePath)) args << app->filePath;
	// the new run works out its own size: it gets the environment this one started with, not the scale set here
	QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	env.remove(QStringLiteral("NZ_RELAUNCHED"));
	const QString outside = qApp->property("outsideScale").toString();
	if (outside.isEmpty())
		env.remove(QStringLiteral("QT_SCALE_FACTOR"));
	else
		env.insert(QStringLiteral("QT_SCALE_FACTOR"), outside);
	QProcess again;
	again.setProgram(QCoreApplication::applicationFilePath());
	again.setArguments(args);
	again.setProcessEnvironment(env);
	if (!again.startDetached()) {
		QMessageBox::warning(
		    this, tr("Start again"),
		    tr("Nachtzug could not be started again. Close it and open it yourself to use the new size."));
		return;
	}
	app->dirty = false; // asked about already
	close();
}

void MainWindow::closeEvent(QCloseEvent* e) {
	if (maybeSave()) {
		app->stop();
		e->accept();
	} else
		e->ignore();
}

// The operation manual, on the website: in the program's language
void MainWindow::openManual() {
	const bool german = QLocale().language() == QLocale::German;
	QDesktopServices::openUrl(
	    QUrl(german ? QStringLiteral("https://mjohanning99.github.io/nachtzug/downloads/Nachtzug-Handbuch.pdf")
		            : QStringLiteral("https://mjohanning99.github.io/nachtzug/downloads/Nachtzug-Manual.pdf")));
}

// Every key and mouse move worth knowing, on the help's second tab. The key names come from Qt, so they read as on
// this computer (Ctrl here, ⌘ on a Mac) and in the program's language.
QWidget* MainWindow::keysPage(const QString& style) {
	auto key = [](const QKeySequence& k) { return k.toString(QKeySequence::NativeText).toHtmlEscaped(); };
	QString ctrl = key(QKeySequence(Qt::CTRL | Qt::Key_A));
	ctrl.chop(1);
	if (ctrl.endsWith(QLatin1Char('+'))) ctrl.chop(1);
	const QString dot = QStringLiteral(" · "), plus = QStringLiteral(" + ");
	QString h = style;
	auto group = [&](const QString& title) { h += QStringLiteral("<h3>%1</h3><table width=\"100%\">").arg(title); };
	auto row = [&](const QString& keys, const QString& what) {
		h += QStringLiteral("<tr><td width=\"34%\"><b>%1</b></td><td>%2</td></tr>").arg(keys, what);
	};
	auto end = [&] { h += QStringLiteral("</table>"); };
	group(tr("Playing"));
	row(key(Qt::Key_Space), tr("Play or stop"));
	row(key(Qt::Key_Home) + dot + key(Qt::Key_Return), tr("Back to the start of the loop, or of the song"));
	row(key(QKeySequence(Qt::CTRL | Qt::Key_R)),
	    tr("Record on or off: notes played and knobs turned are written into the song"));
	row(key(QKeySequence(Qt::CTRL | Qt::Key_L)), tr("Loop on or off"));
	row(key(QKeySequence(Qt::CTRL | Qt::Key_K)), tr("Metronome click on or off"));
	end();
	group(tr("The letter keys"));
	row(tr("Bottom row (Z, X, C …)"), tr("The lowest of three octaves, every key a note of the song’s scale"));
	row(tr("Middle row (A, S, D …)"), tr("The middle octave"));
	row(tr("Top row (Q, W, E …)"), tr("The highest octave"));
	row(key(Qt::Key_Shift) + plus + tr("letter key"),
	    tr("The “right wrong note”: a semitone outside the scale, leaning into it"));
	row(key(QKeySequence(Qt::CTRL | Qt::Key_J)),
	    tr("The letter keys change the chord instead; %1 goes back to the song’s chords").arg(key(Qt::Key_Escape)));
	end();
	group(tr("Editing"));
	row(key(QKeySequence::Undo) + dot + key(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z)), tr("Undo · redo"));
	row(key(QKeySequence::Copy) + dot + key(QKeySequence::Cut) + dot + key(QKeySequence::Paste),
	    tr("Copy · cut · paste, in the view clicked last (a thin coloured line along its top)"));
	row(key(QKeySequence(Qt::CTRL | Qt::Key_D)), tr("Duplicate: a copy right after what is selected"));
	row(key(Qt::Key_Delete) + dot + key(Qt::Key_Backspace), tr("Delete what is selected"));
	row(key(QKeySequence::SelectAll), tr("Select all"));
	row(key(QKeySequence(Qt::CTRL | Qt::Key_E)), tr("Cut the selected clips in two at the playhead"));
	end();
	group(tr("Song and window"));
	row(key(QKeySequence::New), tr("A new song with fresh ideas"));
	row(key(QKeySequence::Open) + dot + key(QKeySequence::Save) + dot +
	        key(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S)),
	    tr("Open · save · save as"));
	row(key(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E)), tr("Export: a sound file, a file per track, or MIDI"));
	if (!QKeySequence(QKeySequence::Preferences).isEmpty()) row(key(QKeySequence::Preferences), tr("Settings"));
	row(key(QKeySequence(Qt::CTRL | Qt::Key_I)), tr("Show or hide the inspector on the left"));
	row(key(QKeySequence::HelpContents), tr("This help"));
	end();
	group(tr("Mouse"));
	row(tr("Click in the note grid"), tr("A note; drag to make it longer, drag a note to move it"));
	row(tr("Right-click a note"), tr("Delete it (keep the button down to wipe several)"));
	row(key(Qt::Key_Shift) + plus + tr("drag"), tr("Select several notes, hits or clips"));
	row(tr("Double-click in the timeline"), tr("A new clip on an empty spot"));
	row(tr("Right-click"),
	    tr("More choices: on a clip (fades, filter sweeps), a track (rename), a knob (Learn MIDI control)"));
	row(ctrl + plus + tr("wheel"), tr("Zoom"));
	row(key(Qt::Key_Shift) + plus + tr("wheel"), tr("Scroll sideways"));
	row(tr("Rest the mouse"), tr("A short explanation of almost anything"));
	end();
	h += tr("<p>All of this, and much more, is in the operation manual.</p>");
	auto* b = new QTextBrowser;
	b->setOpenLinks(false);
	b->setHtml(h);
	return b;
}

void MainWindow::showHelp() {
	QDialog dlg(this);
	dlg.setWindowTitle(tr("How to use Nachtzug"));
	dlg.resize(760, 680);
	auto* v = new QVBoxLayout(&dlg);
	v->setContentsMargins(18, 16, 18, 14);
	v->setSpacing(12);
	auto* head = new QHBoxLayout;
	head->setSpacing(14);
	auto* mark = new QLabel;
	mark->setPixmap(theme::logo(128));
	mark->setScaledContents(true);
	mark->setFixedSize(56, 56);
	head->addWidget(mark);
	auto* title = new QLabel(
	    tr("<span style=\"font-size:22px; font-weight:700; letter-spacing:3px\">NACHTZUG</span><br>Make a "
		   "song in five steps. Nachtzug is built so that every note automatically lands in the right scale."));
	title->setTextFormat(Qt::RichText);
	title->setWordWrap(true);
	head->addWidget(title, 1);
	v->addLayout(head);
	auto* tabs = new QTabWidget;
	auto* text = new QTextBrowser;
	text->setOpenLinks(false);
	const QString accent = theme::amber.name();
	const QString style =
	    QStringLiteral("<style>h3 { color: %1; margin-top: 14px; margin-bottom: 2px; } p, li { line-height: 135%; } td "
		               "{ padding: 3px 14px 3px 0; vertical-align: top; }</style>")
	        .arg(accent);
	// first of all: the manual, where everything is explained in full
	const QString manualBox =
	    QStringLiteral("<table width=\"100%\" cellpadding=\"10\" bgcolor=\"%1\"><tr><td>").arg(theme::wellHi.name()) +
	    tr("<b>Everything in detail: the operation manual.</b> Nachtzug comes with a proper manual, an A5 booklet (a "
		   "PDF, "
		   "in English and German) with pictures of every part, sound recipes, patch sheets and a table of every key. "
		   "It is well worth a look: <b>Help → Operation manual</b>, or the key below.") +
	    QStringLiteral("</td></tr></table>");
	text->setHtml(
	    style + manualBox +
	    tr("<h3>1 · Listen</h3>"
		   "<p>Press <b>Space</b> (or the ▶ key in the Transport unit at the top). The song repeats the stretch under "
		   "the bar marked <b>LOOP</b> in the timeline. "
		   "Drag in the Loop row to mark another stretch, or switch <b>Loop</b> off to play the whole song.</p>"
		   "<h3>2 · Pick a track and change its pattern</h3>"
		   "<p>The timeline has one row per track. Click a clip (a coloured block) and the <b>Notes</b> "
		   "tab shows its pattern.</p>"
		   "<ul><li><b>New idea</b> writes a new pattern for you. <b>Vary</b> changes a note or two.</li>"
		   "<li>Click in the grid to add a note, drag to make it longer, drag a note to move it, <b>right-click a note "
		   "to delete it</b>.</li>"
		   "<li>While the song plays, new ideas, mutes and chord changes wait for the next bar, so they always land in "
		   "time.</li></ul>"
		   "<h3>3 · Shape the sound</h3>"
		   "<p>The <b>Sound</b> tab holds the selected track’s synthesiser, in the order the sound flows: Oscillators, "
		   "Filter, Amp envelope, Effects. "
		   "Pick a preset, press <b>New sound</b> (the die) for a fresh one, or turn knobs. The small pictures show "
		   "what "
		   "the knobs do; drag on the filter picture to sweep it. "
		   "<b>View → Simple sound page</b> switches between a few big knobs in plain words and every knob of the "
		   "synthesiser.</p>"
		   "<p>To make a sound change over time, press the <b>Lines</b> button on a track in the timeline, pick a knob "
		   "such as Cutoff, and click to draw a line. "
		   "Or switch on record (the ● key) and turn a knob while the song plays.</p>"
		   "<h3>4 · Play along</h3>"
		   "<p>The letter keys play the selected track: rows <b>Z–/</b>, <b>A–;</b> and <b>Q–P</b> are three octaves. "
		   "The pads under the note grid show which key plays which note; you can also click them. With the <b>In "
		   "time</b> switch on, what you play is pulled onto the beat. "
		   "With record on (the ● key), it is written into the clip.</p>"
		   "<h3>5 · Build the song</h3>"
		   "<p>In the timeline, drag clips to move them and drag a clip’s right edge to make it longer. "
		   "<b>Copy</b>, the scissors and <b>Delete</b> are above the timeline; "
		   "right-click a clip for fades and filter sweeps. Click a bar in the <b>Chords</b> row to change the chord "
		   "there. "
		   "<b>+ Add track</b> adds a bass line, sequence, pad, lead or drums. The <b>Mixer</b> tab balances the "
		   "tracks.</p>"
		   "<p><b>Arrange</b>, above the timeline, turns your patterns into a whole song: an intro, a build-up, "
		   "a main part, a break and an ending. "
		   "The <b>Export</b> key at the top right saves the result as a sound file.</p>"
		   "<h3>Trying things out</h3>"
		   "<p>Nothing is lost by trying: the <b>↶</b> key beside the display takes back the last change, and <b>↷</b> "
		   "brings it back again. "
		   "After a big change, such as a new idea or a deleted track, a note at the bottom of the window offers "
		   "<b>Undo</b> as well.</p>"
		   "<h3>Loops: play with patterns live</h3>"
		   "<p>Switch the bottom area from <b>Song</b> to <b>Loops</b>. Each track has a row of cells, and each cell "
		   "is a pattern that repeats. "
		   "Click a cell and it starts on the next bar, replacing what that track was playing; click it again to stop "
		   "it. "
		   "The cell you clicked last has a ring around it and is the one the <b>Notes</b> tab shows. To change a "
		   "playing cell’s notes without stopping it, Shift + click it. Click an empty cell to get a new idea there. "
		   "The numbered buttons along the bottom start a whole column at once. The <b>Chords</b> row above the cells "
		   "shows the chords, one box per bar, repeating in that order; click a box to change it.</p>"
		   "<p>Switch <b>Record</b> on while you do this and everything you start is written into the song timeline, "
		   "so a jam becomes a song. "
		   "Switch back to <b>Song</b> to see and edit it.</p>"
		   "<h3>Drums</h3>"
		   "<p>Drum clips start with the <b>auto drummer</b>: choose a style and how hard it plays, and it adds fills "
		   "by itself. Switch off <b>The drummer plays this clip</b> to place hits yourself. "
		   "Every drum can also play your own sound: on the drum track’s <b>Sound</b> tab, click <b>Built-in</b> under "
		   "a drum and pick a sound from the Nachtzug library (rest the mouse on one to hear it), or import or record "
		   "your own.</p>"
		   "<p>Rest the mouse on anything for a short explanation. Every key is on the <b>Keys and mouse</b> "
		   "tab.</p>") +
	    tr("<h3>More to try</h3><ul>"
		   "<li><b>Lessons</b> (Help menu) walk you through a song step by step, in the real program.</li>"
		   "<li><b>Chance</b>, <b>Repeat</b> and <b>Keep</b> in the Notes tab: notes that play only sometimes, quick "
		   "drum rolls, and notes Vary leaves alone. Select notes first.</li>"
		   "<li><b>Chords mode</b>: switch <b>Keys play</b> above the on-screen keys to <b>Chords</b> (Ctrl+J), and "
		   "the letter keys move every track that follows the chords to a new chord while it plays. <b>Esc</b> goes "
		   "back.</li>"
		   "<li><b>Pump</b> in the mixer makes a track duck on every kick. The <b>Input</b> strip plays a guitar or a "
		   "voice through a filter.</li>"
		   "<li>In the preset and kit lists, <b>rest the mouse</b> on a sound to hear it. <b>Track → Chop a loop "
		   "across the drums</b> cuts a drum loop onto the pads.</li>"
		   "<li>A <b>MIDI keyboard</b> plays along; right-click a knob and choose <b>Learn MIDI control</b> to turn it "
		   "from a controller. <b>Export</b> also makes a file per track, or a MIDI file.</li>"
		   "</ul>"));
	tabs->addTab(text, tr("Getting started"));
	tabs->addTab(keysPage(style), tr("Keys and mouse"));
	v->addWidget(tabs, 1);
	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
	auto* manual = buttons->addButton(tr("Open the manual"), QDialogButtonBox::ActionRole);
	manual->setToolTip(tr("The operation manual as a PDF, in the program’s language (it opens in the browser)"));
	connect(manual, &QPushButton::clicked, this, [this] { openManual(); });
	// or learn by doing: the first lesson walks through a song step by step
	auto* lesson = buttons->addButton(tr("Start the first lesson"), QDialogButtonBox::ActionRole);
	lesson->setObjectName("primary");
	lesson->setToolTip(tr("A guided tour: the program points at one thing at a time and waits until you have tried it. "
	                      "All lessons are in Help > Lessons."));
	bool startIt = false;
	connect(lesson, &QPushButton::clicked, &dlg, [&] {
		startIt = true;
		dlg.accept();
	});
	connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::accept);
	v->addWidget(buttons);
	dlg.exec();
	if (startIt) startLesson(QStringLiteral("acid"));
}
