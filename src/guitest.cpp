// The window tests: the GUI test drives the window's own controls the way a person would ("--guitest"), and the
// tooltip check lists every control that does not explain itself when the mouse rests on it ("--check-tooltips").
//
// What's here: scheduleGuiTest() and scheduleTooltipCheck(), called from main() once the window is shown. Each
// starts a moment later, from the event loop, and ends the program with its result.
// Changes in the song file: none.
#include "guitest.h"
#include "app.h"
#include "mainwindow.h"
#include "inspector.h"
#include "editor.h"
#include "widgets.h"
#include "soundpanel.h"
#include "samples.h"
#include "lessons.h"
#include "chopdialog.h"
#include <QApplication>
#include <QSettings>
#include <QTabWidget>
#include <QAbstractButton>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QGroupBox>
#include <QMouseEvent>
#include <QTabBar>
#include <QTimer>
#include <QMenu>
#include <QElapsedTimer>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QKeyEvent>
#include <QDialog>
#include <QLineEdit>
#include <QRadioButton>
#include <QThread>
#include <cmath>
#include <functional>
#include <cstdio>

void scheduleGuiTest(App& app, MainWindow& w) {
	QTimer::singleShot(150000, &w, [] { // a test that hangs is a failed test
		std::fprintf(stderr, "FAILED: the window test did not finish in time\n");
		for (QWidget* p = QApplication::activePopupWidget(); p; p = QApplication::activePopupWidget())
			p->close();
		std::_Exit(2);
	});
	QTimer::singleShot(300, &w, [&] { // drive the window's own controls, the way a person would
		int problems = 0;
		auto check = [&](bool ok, const char* what) {
			if (!ok) {
				problems++;
				std::fprintf(stderr, "FAILED: %s\n", what);
			}
		};
		auto* sp = w.findChild<SoundPanel*>();
		auto* tabs = w.findChild<QTabWidget*>();
		tabs->setCurrentIndex(1);
		Track* bass = nullptr;
		for (Track& t : app.song.tracks)
			if (t.role == R_BASS) bass = &t;
		app.selectTrack(bass->id);
		// the note grid opens on the clip's notes, also when the track was chosen while the grid's tab was hidden
		if (auto* ed = w.findChild<NoteEditor*>()) {
			tabs->setCurrentIndex(0);
			QApplication::processEvents();
			check(ed->notesInView(), "the note grid shows the bass line's notes, not empty rows");
			check(ed->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff, "the scroll bars are hidden at first");
			QSettings().setValue(QStringLiteral("scrollBars"), true);
			w.applyScrollBars();
			check(ed->verticalScrollBarPolicy() == Qt::ScrollBarAsNeeded, "the setting shows the scroll bars");
			QSettings().setValue(QStringLiteral("scrollBars"), false);
			w.applyScrollBars();
			// the keys: a symbol and a word, or the symbol alone with the word in the tooltip
			QAbstractButton* varyKey = nullptr;
			for (QAbstractButton* b : w.findChildren<QAbstractButton*>())
				if (b->property("symbol").toString() == QLatin1String("vary")) varyKey = b;
			check(varyKey && varyKey->text() == QStringLiteral("Vary") && !varyKey->icon().isNull(),
			      "Vary shows its symbol and its word");
			QSettings().setValue(QStringLiteral("keys"), QStringLiteral("symbols"));
			w.applyKeyLook();
			check(varyKey && varyKey->text().isEmpty() && varyKey->toolTip().startsWith(QStringLiteral("Vary")),
			      "with symbols only, Vary shows its symbol, and its word is in the tooltip");
			QSettings().setValue(QStringLiteral("keys"), QStringLiteral("words"));
			w.applyKeyLook();
			tabs->setCurrentIndex(1);
			QApplication::processEvents();
		}
		if (qEnvironmentVariableIsSet("NZ_DEBUG")) std::fprintf(stderr, "step: tap tempo\n");
		// tapping the beat shows the new tempo in the tempo box, wherever the keyboard was
		if (auto* tap = w.findChild<QPushButton*>(QStringLiteral("tap"))) {
			auto* box = w.findChild<QSpinBox*>(QStringLiteral("readout"));
			check(box && QApplication::focusWidget() != box, "the tempo box does not take the keyboard at start-up");
			if (box) box->setFocus();
			for (int i = 0; i < 4; i++) {
				tap->click();
				QElapsedTimer t;
				t.start();
				while (t.elapsed() < 400)
					QApplication::processEvents();
			}
			check(box && std::abs(box->value() - 150) <= 3 && std::abs(app.song.bpm - 150.f) <= 3.f,
			      "four taps 0.4 s apart set the tempo to about 150, and the tempo box shows it");
		}
		if (qEnvironmentVariableIsSet("NZ_DEBUG")) std::fprintf(stderr, "step: folding the inspector\n");
		// folding a section of the inspector leaves it as wide as it was: nothing runs off its right edge
		if (auto* insp = w.findChild<Inspector*>()) {
			for (QGroupBox* g : insp->widget()->findChildren<QGroupBox*>()) {
				if (!g->isVisible() || !g->title().startsWith(QStringLiteral("▾ "))) continue;
				theme::setFolded(g, true);
				QApplication::processEvents();
				const bool fits = insp->widget()->width() <= insp->viewport()->width();
				if (!fits)
					std::fprintf(stderr, "  folding \"%s\" makes the inspector %d wide in %d\n", qPrintable(g->title()),
					             insp->widget()->width(), insp->viewport()->width());
				check(fits, "folding a section of the inspector keeps everything inside it");
				theme::setFolded(g, false);
				QApplication::processEvents();
			}
		}
		if (qEnvironmentVariableIsSet("NZ_DEBUG")) std::fprintf(stderr, "step: simple switch\n");
		// View > Simple sound page
		QAction* simpleAct = w.findChild<QAction*>(QStringLiteral("simpleSound"));
		check(simpleAct != nullptr && simpleAct->isCheckable() && !simpleAct->isChecked(),
		      "the View menu has Simple sound page, not ticked on the full page");
		if (simpleAct) simpleAct->trigger();
		QApplication::processEvents();
		check(sp->isSimple() && QSettings().value(QStringLiteral("simpleSound")).toBool(),
		      "Simple shows the simple page and is remembered");
		if (qEnvironmentVariableIsSet("NZ_DEBUG")) std::fprintf(stderr, "step: big knobs\n");
		// every big knob moves its settings, and one undo step takes each move back
		QList<Knob*> big;
		for (Knob* k : sp->findChildren<Knob*>())
			if (k->isVisible() && k->width() > 70) big << k;
		check(big.size() == 8, "the simple synth page has eight big knobs");
		for (Knob* k : big) {
			const Song before = app.song;
			const float v0 = k->value();
			emit k->pressed();
			emit k->moved(std::clamp(v0 + 0.3f, 0.f, 1.f));
			emit k->released();
			const Track* t = app.song.track(bass->id);
			bool changed = false;
			for (int i = 0; i < dsp::P_COUNT; i++)
				changed |= t->patch.p[i] != before.track(bass->id)->patch.p[i];
			if (v0 < 0.69f) check(changed, "a big knob changes the sound");
			check(app.undoName() == k->name(), "the knob's move is an undo step named after it");
			app.undo();
			bool same = true;
			for (int i = 0; i < dsp::P_COUNT; i++)
				same &= app.song.track(bass->id)->patch.p[i] == before.track(bass->id)->patch.p[i];
			check(same, "undo puts every setting back");
		}
		if (qEnvironmentVariableIsSet("NZ_DEBUG")) std::fprintf(stderr, "step: drum page\n");
		// drums simple page: eight drums, each with its sample button
		Track* dr = nullptr;
		for (Track& t : app.song.tracks)
			if (t.isDrums()) dr = &t;
		app.selectTrack(dr->id);
		QApplication::processEvents();
		int sampleButtons = 0;
		for (QPushButton* b : sp->findChildren<QPushButton*>(QStringLiteral("sample")))
			sampleButtons += b->isVisible();
		check(sampleButtons == dsp::D_COUNT, "each drum on the simple drum page can take a sample");
		if (qEnvironmentVariableIsSet("NZ_DEBUG")) std::fprintf(stderr, "step: sample menu\n");
		// the sample menu under a drum: the library in submenus, and resting on a sound plays it
		{
			QPushButton* sb = nullptr;
			for (QPushButton* b : sp->findChildren<QPushButton*>(QStringLiteral("sample")))
				if (b->isVisible()) {
					sb = b;
					break;
				}
			bool sawLibrary = false, previewed = false;
			int tries = 0;
			std::function<void()> inspect = [&] {
				QMenu* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
				if (!menu)
					for (QWidget* top : QApplication::topLevelWidgets())
						if (auto* m = qobject_cast<QMenu*>(top);
						    m && m->isVisible() && !m->actions().isEmpty() && m->actions().first()->isCheckable())
							menu = m;
				if (!menu) { // not open yet: look again in a moment, but never wait for ever
					if (++tries < 40)
						QTimer::singleShot(50, &w, inspect);
					else
						for (QWidget* top : QApplication::topLevelWidgets())
							if (auto* m = qobject_cast<QMenu*>(top); m && m->isVisible()) m->close();
					return;
				}
				QAction* train = nullptr;
				for (QAction* a : menu->actions()) // the submenu that holds the rail joint, in any language
					if (a->menu())
						for (QAction* x : a->menu()->actions())
							if (x->data().toString() == QLatin1String("nz:Train/Rail joint")) train = a;
				sawLibrary = train != nullptr;
				if (qEnvironmentVariableIsSet("NZ_GUISHOT"))
					menu->grab().save(qEnvironmentVariable("NZ_GUISHOT") + QStringLiteral(".menu.png"));
				if (train) {
					menu->setActiveAction(train);
					QTimer::singleShot(250, &w, [&, menu, train] {
						QMenu* sub = train->menu();
						if (sub && !sub->actions().isEmpty()) {
							app.engine->previewReq.store(nullptr);
							emit sub->hovered(sub->actions().first()); // as when the mouse rests on it
							previewed = app.engine->previewReq.load() != nullptr;
							if (qEnvironmentVariableIsSet("NZ_GUISHOT"))
								sub->grab().save(qEnvironmentVariable("NZ_GUISHOT") + QStringLiteral(".sub.png"));
						}
						menu->close();
					});
				} else
					menu->close();
			};
			QTimer::singleShot(150, &w, inspect);
			if (sb) sb->click();
			check(sb && sawLibrary, "the sample menu offers the Nachtzug library");
			check(previewed, "resting on a library sound plays it");
		}
		if (qEnvironmentVariableIsSet("NZ_DEBUG")) std::fprintf(stderr, "step: back to full\n");
		// back to Full
		if (simpleAct) simpleAct->trigger();
		QApplication::processEvents();
		check(!sp->isSimple(), "Full shows every knob again");
		sp->setSimple(true);
		QApplication::processEvents(); // the settings' choice ticks the menu item too
		check(simpleAct && simpleAct->isChecked(), "the menu item follows the settings");
		sp->setSimple(false);
		QApplication::processEvents();
		if (qEnvironmentVariableIsSet("NZ_DEBUG")) std::fprintf(stderr, "step: panels\n");
		// the panels: a section folds shut when its name is clicked; the inspector goes away and comes back with
		// its sections; the tabs go into a window of their own and come back
		{
			for (const Track& tk : app.song.tracks)
				if (!tk.isDrums()) {
					app.selectTrack(tk.id);
					break;
				}
			sp->setSimple(false);
			w.findChild<QTabWidget*>()->setCurrentIndex(1);
			QApplication::processEvents();
			QGroupBox* osc = nullptr;
			for (QGroupBox* g : sp->findChildren<QGroupBox*>())
				if (g->isVisible() && g->title().contains(QStringLiteral("OSCILLATORS"))) osc = g;
			check(osc != nullptr, "the Sound tab has its Oscillators section");
			auto clickAt = [](QWidget* wd, QPoint at) {
				QMouseEvent press(QEvent::MouseButtonPress, QPointF(at), QPointF(wd->mapToGlobal(at)), Qt::LeftButton,
				                  Qt::LeftButton, Qt::NoModifier);
				QApplication::sendEvent(wd, &press);
				QMouseEvent rel(QEvent::MouseButtonRelease, QPointF(at), QPointF(wd->mapToGlobal(at)), Qt::LeftButton,
				                Qt::NoButton, Qt::NoModifier);
				QApplication::sendEvent(wd, &rel);
			};
			if (osc) {
				const int tall = osc->height();
				clickAt(osc, QPoint(30, 6));
				QApplication::processEvents();
				check(osc->property("folded").toBool() && osc->height() < tall / 2,
				      "clicking a section's name folds it shut");
				clickAt(osc, QPoint(30, 6));
				QApplication::processEvents();
				check(!osc->property("folded").toBool() && osc->height() >= tall - 2, "and clicking it again opens it");
			}
			auto* insp = w.findChild<QWidget*>(QStringLiteral("inspector"));
			QAction* inspAct = w.findChild<QAction*>(QStringLiteral("inspectorAct"));
			QGroupBox* how = nullptr;
			for (QGroupBox* g : w.findChildren<QGroupBox*>())
				if (g->title().contains(QStringLiteral("HOW NOTES"))) how = g;
			check(insp && insp->isVisible() && how && insp->isAncestorOf(how),
			      "the inspector holds How notes are played");
			if (inspAct && how) {
				inspAct->trigger();
				QApplication::processEvents();
				check(!insp->isVisible() && sp->isAncestorOf(how),
				      "without the inspector, How notes are played is back on the Sound tab");
				auto* tab = w.findChild<QToolButton*>(QStringLiteral("inspectorTab"));
				check(tab && tab->isVisible(), "a tab on the left edge offers the inspector back");
				if (tab) tab->click();
				QApplication::processEvents();
				check(insp->isVisible() && insp->isAncestorOf(how) && !tab->isVisible(),
				      "the tab brings the inspector back, with How notes are played in it");
			}
			QAction* own = w.findChild<QAction*>(QStringLiteral("ownWindowAct"));
			auto* tabsW = w.findChild<QTabWidget*>();
			if (own) {
				own->trigger();
				QApplication::processEvents();
				QTabWidget* t2 = nullptr;
				for (QWidget* top : QApplication::topLevelWidgets())
					if (top != &w && top->isVisible())
						if (auto* t = top->findChild<QTabWidget*>()) t2 = t;
				check(t2 != nullptr && !w.isAncestorOf(t2), "the tabs go into a window of their own");
				own->trigger();
				QApplication::processEvents();
				check(w.findChild<QTabWidget*>() != nullptr && w.findChild<QTabWidget*>()->isVisible(),
				      "and come back when it closes");
			}
			(void)tabsW;
		}
		{ // the on-screen keys open and close with their button, whatever room the editor has
			w.findChild<QTabWidget*>()->setCurrentIndex(0);
			QApplication::processEvents();
			auto* toggle = w.findChild<QPushButton*>(QStringLiteral("padsToggle"));
			auto* keys = w.findChild<KeyPads*>();
			check(toggle && keys && toggle->isVisible(), "the keys row has a button for the on-screen keys");
			if (toggle && keys) {
				const bool before = keys->isVisible();
				toggle->click();
				QApplication::processEvents();
				check(keys->isVisible() != before, "the button shows or hides the on-screen keys");
				toggle->click();
				QApplication::processEvents();
				check(keys->isVisible() == before, "and a second press undoes it");
			}
		}
		if (qEnvironmentVariableIsSet("NZ_DEBUG")) std::fprintf(stderr, "step: undo key\n");
		// the undo key in the top bar, and the note that offers Undo after a big change
		app.selectTrack(bass->id);
		const int clips = int(app.song.track(bass->id)->clips.size());
		app.removeTrack(bass->id);
		QApplication::processEvents();
		auto* toast = w.findChild<Toast*>();
		check(toast && toast->isVisible(), "deleting a track shows a note offering Undo");
		if (qEnvironmentVariableIsSet("NZ_GUISHOT"))
			w.grab().save(qEnvironmentVariable("NZ_GUISHOT")); // a picture with the note showing
		QToolButton* undoKey = nullptr;
		for (QToolButton* b : w.findChildren<QToolButton*>())
			if (b->toolTip().startsWith(QStringLiteral("Undo: Delete track"))) undoKey = b;
		check(undoKey && undoKey->isEnabled(), "the undo key says what it will undo");
		if (undoKey) undoKey->click();
		QApplication::processEvents();
		check(app.song.track(bass->id) && int(app.song.track(bass->id)->clips.size()) == clips,
		      "the undo key brings the track back");
		// the export key is there and says what it does
		bool exportKey = false;
		for (QToolButton* b : w.findChildren<QToolButton*>(QStringLiteral("exportKey")))
			exportKey = b->isVisible() && b->toolTip().contains(QStringLiteral("WAV"));
		check(exportKey, "the export key is in the top bar");
		// ---- round 3: the new controls, used the way a person would
		auto dbg = [](const char* step) {
			if (qEnvironmentVariableIsSet("NZ_DEBUG")) std::fprintf(stderr, "step: %s\n", step);
		};
		auto settle = [](int ms) {
			QElapsedTimer t;
			t.start();
			while (t.elapsed() < ms) {
				QApplication::processEvents();
				QThread::msleep(5);
			}
		};
		// runs f on the next window that opens over the main one, then makes sure it is gone
		auto onModal = [&](std::function<void(QWidget*)> f) {
			auto tries = std::make_shared<int>(0);
			auto poll = std::make_shared<std::function<void()>>();
			*poll = [&w, f, tries, poll] {
				QWidget* d = QApplication::activeModalWidget();
				if (!d) {
					if (++*tries < 100) QTimer::singleShot(30, &w, *poll);
					return;
				}
				f(d);
				if (d->isVisible()) d->close();
			};
			QTimer::singleShot(30, &w, *poll);
		};
		auto byLessonKey = [&](const char* key) -> QWidget* {
			for (QWidget* x : w.findChildren<QWidget*>())
				if (x->isVisible() && x->property("lesson").toString() == QLatin1String(key)) return x;
			return nullptr;
		};
		tabs->setCurrentIndex(0);
		app.selectTrack(bass->id);
		if (Track* bt = app.song.track(bass->id); bt && !bt->clips.empty()) app.openClip(bt->clips[0].id);
		settle(50);
		dbg("chance menu");
		{
			Clip* c = app.curClip();
			for (Note& n : c->notes)
				n.sel = n.start < BAR / 2;
			auto* chanceKey = qobject_cast<QToolButton*>(byLessonKey("chance"));
			check(chanceKey && chanceKey->menu() && chanceKey->menu()->actions().size() == 4,
			      "the Chance key offers four choices");
			if (chanceKey && chanceKey->menu()) chanceKey->menu()->actions()[2]->trigger();
			bool half = true, any = false;
			for (const Note& n : app.curClip()->notes)
				if (n.start < BAR / 2) {
					any = true;
					half &= n.chance == 50;
				}
			check(any && half, "Half the time sets the chance of the selected notes");
			dbg("keep and vary");
			QPushButton* keepKey = nullptr;
			for (QPushButton* b : w.findChildren<QPushButton*>())
				if (b->isVisible() && b->property("word").toString() == QStringLiteral("Keep")) keepKey = b;
			if (keepKey) keepKey->click();
			const std::vector<Note> was = app.curClip()->notes;
			QPushButton* varyKey = qobject_cast<QPushButton*>(byLessonKey("vary"));
			for (int i = 0; i < 25 && varyKey; i++) {
				varyKey->click();
				settle(5);
			}
			bool kept = true;
			for (const Note& n0 : was)
				if (n0.sel)
					kept &= std::any_of(app.curClip()->notes.begin(), app.curClip()->notes.end(), [&](const Note& n) {
						return n.start == n0.start && n.deg == n0.deg && n.lock;
					});
			check(keepKey && kept, "Keep protects notes from Vary");
			app.undo();
		}
		dbg("chords switch");
		{
			QWidget* ck = byLessonKey("chordKeys");
			if (auto* b = qobject_cast<QPushButton*>(ck)) b->click();
			check(app.chordKeys(), "the Chords switch under the notes turns Chords mode on");
			app.keyDown(1 * 16 + 4, 1, 4);
			app.keyUp(1 * 16 + 4);
			check(app.liveChord() == 4, "a letter key in Chords mode sets the chord");
			QKeyEvent esc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
			QApplication::sendEvent(&w, &esc);
			check(app.liveChord() == Engine::NO_CHORD, "Esc goes back to the song's chords");
			if (auto* b = qobject_cast<QPushButton*>(byLessonKey("noteKeys"))) b->click();
			check(!app.chordKeys(), "the Notes switch turns Chords mode off");
		}
		dbg("preset preview");
		{
			tabs->setCurrentIndex(1);
			settle(30);
			QComboBox* presetBox = nullptr;
			for (QComboBox* c : sp->findChildren<QComboBox*>())
				if (c->isVisible() && c->toolTip().contains(QStringLiteral("rest the mouse on a sound"))) presetBox = c;
			check(presetBox != nullptr, "the preset list can be heard");
			if (presetBox) {
				presetBox->showPopup();
				settle(50);
				app.engine->phraseReq.store(nullptr);
				emit presetBox->highlighted(3);
				settle(300);
				check(app.engine->phraseReq.load() != nullptr || app.engine->stopPhrase.load() == false,
				      "resting on a preset plays a phrase with it");
				presetBox->hidePopup();
				settle(30);
			}
			tabs->setCurrentIndex(0);
		}
		dbg("lesson");
		{
			app.stop();
			settle(100);
			w.startLesson(QStringLiteral("acid"));
			check(w.lessons() && w.lessons()->running() && w.lessons()->step() == 0,
			      "a lesson starts at its first step");
			if (app.audioRunning()) {
				app.play();
				settle(1600);
				check(w.lessons()->step() == 1, "a lesson step moves on by itself once it is done");
				app.stop();
			}
			QPushButton* end = nullptr;
			for (QPushButton* b : w.findChildren<QPushButton*>())
				if (b->isVisible() && b->text() == QStringLiteral("End lesson")) end = b;
			if (end) end->click();
			check(end && !w.lessons()->running(), "End lesson ends it");
		}
		dbg("export kinds");
		{
			bool stemsPath = false, midiOff = false;
			onModal([&](QWidget* d) {
				QList<QRadioButton*> radios = d->findChildren<QRadioButton*>();
				for (QRadioButton* r : radios)
					if (r->text().contains(QStringLiteral("each track"))) r->click();
				for (QLineEdit* e : d->findChildren<QLineEdit*>())
					stemsPath = e->text().endsWith(QStringLiteral(" (tracks)"));
				for (QRadioButton* r : radios)
					if (r->text().contains(QStringLiteral("MIDI"))) r->click();
				for (QComboBox* c : d->findChildren<QComboBox*>())
					midiOff = !c->isEnabled();
				for (QRadioButton* r : radios)
					if (r->text().contains(QStringLiteral("One sound file"))) r->click();
				static_cast<QDialog*>(d)->reject();
			});
			w.showDialog(QStringLiteral("export"));
			check(stemsPath, "choosing stems makes the export a folder of track files");
			check(midiOff, "a MIDI export has no sound quality to choose");
		}
		dbg("chop");
		{
			int pieces = 0;
			onModal([&](QWidget* d) {
				if (auto* v = d->findChild<ChopView*>()) pieces = int(v->cuts().size());
				for (QLineEdit* e : d->findChildren<QLineEdit*>())
					e->setText(QStringLiteral("guitest beat"));
				for (QPushButton* b : d->findChildren<QPushButton*>())
					if (b->text() == QStringLiteral("Put on the drums")) {
						b->click();
						break;
					}
			});
			w.showDialog(QStringLiteral("chop"));
			const Track* dt = nullptr;
			for (const Track& t : app.song.tracks)
				if (t.isDrums()) dt = &t;
			check(pieces >= 4 && dt && dt->kit.sample[0].startsWith(QStringLiteral("guitest beat")),
			      "a beat chopped in the dialog lands on the drums");
			app.undo();
			for (const QString& n : samples::list())
				if (n.startsWith(QStringLiteral("guitest beat"))) samples::remove(n);
		}
		dbg("settings size and midi pages");
		{
			int sizes = 0, pages = 0;
			onModal([&](QWidget* d) {
				for (QComboBox* c : d->findChildren<QComboBox*>())
					if (c->itemText(0).startsWith(QStringLiteral("Automatic"))) sizes = c->count();
				if (auto* t = d->findChild<QTabWidget*>()) pages = t->count();
			});
			w.showSettings(0);
			check(sizes == 9 && pages == 4, "the settings have a size choice and a MIDI page");
		}
		dbg("mixer");
		{
			tabs->setCurrentIndex(2);
			settle(50);
			int pumps = 0;
			for (Knob* k : w.findChildren<Knob*>())
				pumps += k->isVisible() && k->name() == QStringLiteral("Pump");
			int synths = 0;
			for (const Track& t : app.song.tracks)
				synths += !t.isDrums();
			if (qEnvironmentVariableIsSet("NZ_DEBUG"))
				std::fprintf(stderr, "pumps %d, synth tracks %d\n", pumps, synths);
			check(pumps == synths + 1, "every synth track and the input have a Pump knob");
			QPushButton* inKey = nullptr;
			for (QPushButton* b : w.findChildren<QPushButton*>(QStringLiteral("stripName")))
				if (b->isVisible() && b->text() == QStringLiteral("Input")) inKey = b;
			check(inKey != nullptr, "the mixer has an Input strip");
			if (inKey) {
				onModal([](QWidget* d) { d->close(); }); // no input here: the warning is closed
				inKey->click();
				settle(100);
				check(inKey->isChecked() == app.liveInput(), "the Input key shows whether the input is open");
				if (app.liveInput()) {
					inKey->click();
					settle(50);
				}
			}
			tabs->setCurrentIndex(0);
		}

		// the sound card, if there is one: switching rate, buffer and sound system, and a moment of input
		if (app.audioRunning()) {
			const App::AudioSetup was = app.audioSetup();
			std::printf("sound: %s at %d Hz, %d samples; systems: %s; outputs: %d, inputs: %d\n",
			            qPrintable(app.audioDriver()), app.audioRate(), app.audioBuffer(),
			            qPrintable(App::audioDrivers().join(QStringLiteral(", "))), int(app.audioDevices(false).size()),
			            int(app.audioDevices(true).size()));
			app.seek(5 * BAR);
			App::AudioSetup s = was;
			s.rate = 44100;
			s.buffer = 512;
			check(app.openAudio(s), "the sound card opens again at 44.1 kHz with a larger buffer");
			check(std::fabs(app.pos() - 5 * BAR) < 1.0,
			      "the playhead stays where it was when the sound card is switched");
			std::printf("  now %d Hz, %d samples\n", app.audioRate(), app.audioBuffer());
			for (const QString& d : App::audioDrivers()) {
				if (d == QLatin1String("jack")) continue; // starting a JACK server is not something a test should do
				App::AudioSetup t = was;
				t.driver = d;
				const bool ok = app.openAudio(t);
				std::printf("  sound system %-10s %s%s\n", qPrintable(d),
				            ok ? "works" : "does not open: ", ok ? "" : qPrintable(app.audioError));
			}
			check(app.openAudio(was) && app.audioRunning(), "back to the sound card it started with");
			QString err;
			if (app.startCapture(QString(), err)) {
				QElapsedTimer t;
				t.start();
				while (t.elapsed() < 300)
					QApplication::processEvents();
				int rate = 0;
				const std::vector<float> got = app.stopCapture(rate);
				std::printf("  input: %d samples at %d Hz in 0.3 s (thrown away)\n", int(got.size()), rate);
				check(!got.empty(), "the input delivers sound");
			} else
				std::printf("  no input: %s\n", qPrintable(err));
		} else
			std::printf("no sound card here: %s\n", qPrintable(app.audioError));
		std::printf(problems ? "gui test: %d problem(s)\n" : "gui test passed\n", problems);
		app.dirty = false;
		QCoreApplication::exit(problems ? 1 : 0);
	});
}

void scheduleTooltipCheck(App& app, MainWindow& w) {
	QTimer::singleShot(300, &w, [&] {
		QStringList missing;
		auto sweep = [&](const QString& where) {
			QApplication::processEvents();
			for (QWidget* x : w.findChildren<QWidget*>()) {
				if (!x->isVisible()) continue;
				if (QLatin1String(x->metaObject()->className()) == QLatin1String("QScrollBar"))
					continue; // scroll bars explain themselves
				const bool control = qobject_cast<QAbstractButton*>(x) || qobject_cast<QComboBox*>(x) ||
				                     qobject_cast<QAbstractSlider*>(x) || qobject_cast<QAbstractSpinBox*>(x) ||
				                     qobject_cast<Knob*>(x);
				if (!control || qobject_cast<QTabBar*>(x->parentWidget())) continue;
				if (!x->toolTip().isEmpty()) continue;
				QString name = x->metaObject()->className();
				if (auto* b = qobject_cast<QAbstractButton*>(x)) name += " \"" + b->text() + "\"";
				if (auto* c = qobject_cast<QComboBox*>(x)) name += " [" + c->currentText() + "]";
				QString path;
				for (QWidget* p = x->parentWidget(); p && p != &w; p = p->parentWidget())
					if (auto* g = qobject_cast<QGroupBox*>(p)) {
						path = g->title();
						break;
					}
				missing << where + ": " + name + (path.isEmpty() ? QString() : "  in " + path);
			}
		};
		auto* tabs = w.findChild<QTabWidget*>();
		for (int tr = 0; tr < 2; tr++) {
			app.selectTrack(app.song.tracks[size_t(tr == 0 ? 1 : 0)].id);
			for (int t = 0; t < 3; t++) {
				tabs->setCurrentIndex(t);
				sweep(QStringLiteral("%1 / tab %2").arg(tr == 0 ? "synth" : "drums").arg(t));
			}
		}
		if (auto* sp = w.findChild<SoundPanel*>()) { // the simple sound pages too
			sp->setSimple(true);
			tabs->setCurrentIndex(1);
			for (int tr = 0; tr < 2; tr++) {
				app.selectTrack(app.song.tracks[size_t(tr == 0 ? 1 : 0)].id);
				sweep(QStringLiteral("simple %1").arg(tr == 0 ? "synth" : "drums"));
			}
			sp->setSimple(false);
		}
		app.setLiveMode(true);
		sweep("loops");
		missing.removeDuplicates();
		for (const QString& m : missing)
			std::fprintf(stderr, "%s\n", qPrintable(m));
		std::fprintf(stderr, "%d controls without a tooltip\n", int(missing.size()));
		app.dirty = false;
		QApplication::quit();
	});
}
