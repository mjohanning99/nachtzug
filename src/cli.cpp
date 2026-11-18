// The command line: what Nachtzug was asked to do when it started (see cli.h for the options), the runs that
// need no window (rendering a song), and setting up the pictures taken for the README, the website and the
// manuals.
//
// What's here: parseOptions(), renderSong(), pictureOfWelcome(), and for pictures preparePicture() (before the
// window), setUpWindow() (once it exists), scheduleViews() and scheduleScreenshot() (once it is shown); all
// called from main(), in that order.
// Changes in the song file: none (--render reads a song, the pictures change only the copy in memory).
#include "cli.h"
#include "recent.h"
#include <QDir>
#include <QAction>
#include "app.h"
#include "mainwindow.h"
#include "editor.h"
#include "soundpanel.h"
#include "welcome.h"
#include "samples.h"
#include "timeline.h"
#include "lessons.h"
#include <QApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTabWidget>
#include <QGroupBox>
#include <QTimer>
#include <QFrame>
#include <QToolButton>
#include <functional>
#include <cstdio>

Options parseOptions(int argc, char** argv) {
	Options o;
	for (int i = 1; i < argc; i++) {
		const QString a = QString::fromLocal8Bit(argv[i]);
		if (a == "--render" && i + 1 < argc)
			o.renderPath = QString::fromLocal8Bit(argv[++i]);
		else if (a == "--screenshot" && i + 1 < argc)
			o.shotPath = QString::fromLocal8Bit(argv[++i]);
		else if (a == "--theme" && i + 1 < argc)
			o.themeName = QString::fromLocal8Bit(argv[++i]);
		else if (a == "--tab" && i + 1 < argc)
			o.tab = QString::fromLocal8Bit(argv[++i]).toInt();
		else if (a == "--track" && i + 1 < argc)
			o.trackIndex = QString::fromLocal8Bit(argv[++i]).toInt();
		else if (a == "--lang" && i + 1 < argc)
			o.langName = QString::fromLocal8Bit(argv[++i]);
		else if (a == "--guide")
			o.guide = true;
		else if (a == "--simple")
			o.simpleView = true;
		else if (a == "--chord-keys")
			o.chordKeys = true;
		else if (a == "--timeline-beside")
			o.timelineBeside = true;
		else if (a == "--no-inspector")
			o.noInspector = true;
		else if (a == "--frames" && i + 1 < argc)
			o.frames = QString::fromLocal8Bit(argv[++i]).toInt();
		else if (a == "--lesson" && i + 1 < argc)
			o.lessonArg = QString::fromLocal8Bit(argv[++i]);
		else if (a == "--size" && i + 1 < argc) {
			const QStringList wh = QString::fromLocal8Bit(argv[++i]).split('x');
			if (wh.size() == 2) o.winSize = QSize(wh[0].toInt(), wh[1].toInt());
		} else if (a == "--geometry" && i + 1 < argc)
			o.geomPath = QString::fromLocal8Bit(argv[++i]);
		else if (a == "--kit" && i + 1 < argc)
			o.kitName = QString::fromLocal8Bit(argv[++i]);
		else if (a == "--arrange" && i + 1 < argc)
			o.arrangeLen = QString::fromLocal8Bit(argv[++i]).toInt();
		else if (a == "--part" && i + 1 < argc)
			o.partIndex = QString::fromLocal8Bit(argv[++i]).toInt();
		else if (a == "--journey" && i + 1 < argc) {
			const QStringList mc = QString::fromLocal8Bit(argv[++i]).split(':');
			o.journeyMin = mc[0].toInt();
			if (mc.size() > 1) o.journeyChar = std::clamp(mc[1].toInt(), 0, 2);
		} else if (a == "--dialog" && i + 1 < argc)
			o.dialogName = QString::fromLocal8Bit(argv[++i]);
		else if (a == "--loops")
			o.loopsView = true;
		else if (a == "--lines")
			o.linesView = true;
		else if (a == "--symbols")
			o.symbolsOnly = true;
		else if (a == "--check-tooltips")
			o.tipCheck = true;
		else if (a == "--selftest")
			o.selftest = true;
		else if (a == "--guitest")
			o.guiTest = true;
		else if (a == "--write-starters" && i + 1 < argc)
			o.startersDir = QString::fromLocal8Bit(argv[++i]);
		else if (a == "--parity" && i + 1 < argc)
			o.parityDir = QString::fromLocal8Bit(argv[++i]);
		else if (!a.startsWith("--"))
			o.songPath = a;
	}
	return o;
}

bool Options::windowless() const {
	return !renderPath.isEmpty() || !shotPath.isEmpty() || selftest || tipCheck || guiTest || !parityDir.isEmpty() ||
	       !startersDir.isEmpty();
}

bool Options::testRun() const {
	return !shotPath.isEmpty() || selftest || tipCheck || guiTest || !parityDir.isEmpty() || !startersDir.isEmpty();
}

int renderSong(const Options& o) {
	Song s = demoSong();
	if (!o.songPath.isEmpty()) {
		QFile f(o.songPath);
		if (!f.open(QIODevice::ReadOnly) || !Song::fromJson(QJsonDocument::fromJson(f.readAll()).object(), s)) {
			std::fprintf(stderr, "Could not read the song %s\n", qPrintable(o.songPath));
			return 1;
		}
	}
	if (o.arrangeLen >= 0) {
		std::mt19937 g(7);
		arrangeSong(s, o.arrangeLen, true, g);
	}
	if (o.journeyMin > 0) {
		std::mt19937 g(7);
		continueJourney(s, (s.endTick() + BAR - 1) / BAR, o.journeyMin, o.journeyChar, true, g);
	}
	for (const DrumKit& k : drumKits())
		if (k.name == o.kitName)
			for (Track& t : s.tracks)
				if (t.isDrums()) t.kit = chooseKit(t.kit, k);
	resolveSamples(s);
	const bool ok = renderToWav(s, o.renderPath, 48000.f, {});
	if (!ok) std::fprintf(stderr, "Could not write %s\n", qPrintable(o.renderPath));
	return ok ? 0 : 1;
}

int pictureOfWelcome(const Options& o) {
	WelcomeDialog wd({});
	wd.show();
	QTimer::singleShot(600, &wd, [&] {
		wd.grab().save(o.shotPath);
		QApplication::quit();
	});
	return QApplication::exec();
}

void preparePicture(App& app, const Options& o) {
	if (o.dialogName == QLatin1String("start")) { // for pictures: a few songs of one's own, one of them gone missing
		const QString dir = QDir::temp().filePath(QStringLiteral("Nachtzug pictures/My songs"));
		QDir().mkpath(dir);
		recent::clear();
		recent::add(QDir::temp().filePath(QStringLiteral("Nachtzug pictures/USB stick/Old idea.nachtzug")));
		for (const char* name : {"Sunday sequence", "First acid try", "Night drive"}) {
			const QString path = QDir(dir).filePath(QLatin1String(name) + QStringLiteral(".nachtzug"));
			QFile f(path);
			if (f.open(QIODevice::WriteOnly)) f.write(QJsonDocument(demoSong().toJson()).toJson());
			f.close();
			recent::add(path);
		}
	}
	if (o.trackIndex >= 0 && o.trackIndex < int(app.song.tracks.size()))
		app.selectTrack(app.song.tracks[size_t(o.trackIndex)].id);
	if (o.linesView &&
	    app.song.tracks.size() > 3) { // for test pictures: a drawn cutoff line on the bass, and a fade on the pad
		Track& bass = app.song.tracks[1];
		bass.laneOpen = true;
		bass.laneParam = dsp::P_CUTOFF;
		AutoLane l;
		l.param = dsp::P_CUTOFF;
		l.loop = 2 * BAR;
		l.pts = {{0, 0.22f}, {BAR, 0.62f}, {BAR + 192, 0.40f}, {2 * BAR - 24, 0.30f}};
		bass.lanes.push_back(l);
		if (!app.song.tracks[3].clips.empty()) {
			Clip& c = app.song.tracks[3].clips[0];
			c.inKind = 3;
			c.inLen = 2 * BAR;
			c.outKind = 1;
			c.outLen = BAR;
		}
		app.touch();
	}
	if (o.loopsView) {         // for test pictures: the loop grid with a few cells, one column playing
		app.setLiveMode(true); // fills column 1 from the clips; the window switches to the grid after it is shown, as a
		                       // person would
		app.setLiveMode(false);
		for (size_t i = 0; i < app.song.tracks.size(); i++) {
			const int id = app.song.tracks[i].id;
			app.newCell(id, 1);
			if (i % 2) app.newCell(id, 2);
		}
		for (Track& t : app.song.tracks)
			t.liveCell = t.cell(0) ? 0 : -1;
		app.touch();
	}
	if (o.arrangeLen >= 0) {
		arrangeSong(app.song, o.arrangeLen, true, app.rng);
		app.editClip = -1;
		app.touch();
		app.refreshSelection();
		if (o.partIndex >= 0) app.selectPart(o.partIndex); // --part N: that named part selected
	} // for test pictures
	if (o.journeyMin > 0) {
		std::mt19937 g(7);
		continueJourney(app.song, (app.song.endTick() + BAR - 1) / BAR, o.journeyMin, o.journeyChar, true, g);
		app.editClip = -1;
		app.touch();
		app.refreshSelection();
	}
	for (const DrumKit& k : drumKits())
		if (k.name == o.kitName) {
			for (Track& t : app.song.tracks)
				if (t.isDrums()) t.kit = chooseKit(t.kit, k);
			app.touch();
		}
}

void setUpWindow(App& app, MainWindow& w, const Options& o) {
	if (o.noInspector)
		if (auto* insp = w.findChild<QAction*>(QStringLiteral("inspectorAct"))) insp->setChecked(false);
	if (o.timelineBeside)
		if (auto* top = w.findChild<QAction*>(QStringLiteral("timelineTopAct"))) top->setChecked(false);
	if (auto* tabs = w.findChild<QTabWidget*>()) tabs->setCurrentIndex(o.tab);
	if (o.simpleView)
		if (auto* sp = w.findChild<SoundPanel*>()) sp->setSimple(true);
	if (o.chordKeys) {
		app.setChordKeys(true);
		app.playChord(app.song.q(5));
	} // for pictures: Chords mode, with the sixth playing
	if (o.winSize.isValid()) w.resize(o.winSize); // as near as the window allows
}

void scheduleViews(MainWindow& w, const Options& o) {
	if (o.arrangeLen >= 0 || o.journeyMin > 0)
		QTimer::singleShot(400, &w, [&] {
			if (auto* tl = w.findChild<Timeline*>()) tl->showWholeSong();
		});
	if (!o.lessonArg.isEmpty())
		QTimer::singleShot(200, &w, [&] {
			const QStringList parts = o.lessonArg.split(':');
			w.startLesson(parts[0]);
			for (int k = 1; k < (parts.size() > 1 ? parts[1].toInt() : 1) && w.lessons(); k++)
				w.lessons()->next();
		});
}

void scheduleScreenshot(App& app, MainWindow& w, const Options& o) {
	if (!o.shotPath.isEmpty() && o.frames > 0) { // the song playing, a picture every 83 ms: out-000.png, out-001.png …
		QTimer::singleShot(700, &w, [&app, &w, &o] {
			app.play();
			auto* tick = new QTimer(&w);
			auto* n = new int(0);
			QObject::connect(tick, &QTimer::timeout, &w, [&app, &w, &o, tick, n] {
				QString base = o.shotPath;
				if (base.endsWith(QLatin1String(".png"))) base.chop(4);
				w.grab().save(QStringLiteral("%1-%2.png").arg(base).arg(*n, 3, 10, QLatin1Char('0')));
				if (++*n >= o.frames) {
					tick->stop();
					delete n;
					app.dirty = false;
					QApplication::quit();
				}
			});
			tick->start(83);
		});
		return;
	}
	if (o.loopsView)
		QTimer::singleShot(150, &w, [&] {
			app.setLiveMode(true);
			for (Track& t : app.song.tracks)
				t.liveCell = t.cell(0) ? 0 : -1;
			app.touch();
			if (qEnvironmentVariableIsSet("NZ_DEBUG"))
				std::fprintf(stderr, "after switching to loops: %dx%d\n", w.width(), w.height());
		});
	if (!o.shotPath.isEmpty()) {
		if (!o.dialogName.isEmpty()) { // picture of one of the dialogs instead
			QTimer::singleShot(1200, &w, [&] {
				if (QWidget* d = QApplication::activeModalWidget()) {
					if (auto* t = d->findChild<QTabWidget*>(); t && o.tab > 0) // --tab 1: the Keys and mouse tab
						t->setCurrentIndex(o.tab);
					d->grab().save(o.shotPath);
					d->close();
				}
				app.dirty = false;
				QApplication::quit();
			});
			QTimer::singleShot(300, &w, [&] { w.showDialog(o.dialogName); });
		} else if (o.guide) { // picture of the how-to window instead
			QTimer::singleShot(900, &w, [&] {
				if (QWidget* d = QApplication::activeModalWidget()) {
					if (auto* t = d->findChild<QTabWidget*>(); t && o.tab > 0) // --tab 1: the Keys and mouse tab
						t->setCurrentIndex(o.tab);
					d->grab().save(o.shotPath);
					d->close();
				}
				app.dirty = false;
				QApplication::quit();
			});
			QTimer::singleShot(200, &w, [&] { w.showHelp(); });
		} else
			QTimer::singleShot(800, &w, [&] {
				if (!o.geomPath.isEmpty()) { // where each part of the window is, for cutting pictures for the manual
					QJsonObject parts;
					auto put = [&](const QString& name, QWidget* x) {
						if (!x || !x->isVisible()) return;
						const QPoint at = x->mapTo(&w, QPoint(0, 0));
						parts[name] = QJsonArray{at.x(), at.y(), x->width(), x->height()};
					};
					put(QStringLiteral("window"), &w);
					put(QStringLiteral("topbar"), w.findChild<QWidget*>(QStringLiteral("topbar")));
					QList<QFrame*> units = w.findChildren<QFrame*>(QStringLiteral("unit"));
					std::sort(units.begin(), units.end(), [&](QFrame* a, QFrame* b) {
						return a->mapTo(&w, QPoint()).x() < b->mapTo(&w, QPoint()).x();
					});
					const char* unitNames[] = {"transport", "song", "output"};
					for (int i = 0; i < units.size() && i < 3; i++)
						put(QString::fromLatin1(unitNames[i]), units[i]->parentWidget());
					put(QStringLiteral("tabs"), w.findChild<QTabWidget*>());
					put(QStringLiteral("drummer"), w.findChild<QWidget*>(QStringLiteral("drummerBox")));
					put(QStringLiteral("timeline"), w.findChild<Timeline*>());
					for (QWidget* x : w.findChildren<QWidget*>()) {
						const QString cls = QString::fromLatin1(x->metaObject()->className());
						if (cls == QLatin1String("KeyPads"))
							put(QStringLiteral("pads"), x);
						else if (cls == QLatin1String("LoopGrid"))
							put(QStringLiteral("loops"), x);
						else if (cls == QLatin1String("MixerPanel"))
							put(QStringLiteral("mixer"), x);
						else if (auto* g = qobject_cast<QGroupBox*>(x)) {
							QString t = g->title();
							if (t.startsWith(QStringLiteral("▾ ")) || t.startsWith(QStringLiteral("▸ "))) t = t.mid(2);
							put(QStringLiteral("group:") + t, g);
						}
					}
					QFile f(o.geomPath);
					if (f.open(QIODevice::WriteOnly)) f.write(QJsonDocument(parts).toJson());
				}
				if (qEnvironmentVariableIsSet("NZ_DEBUG")) {
					std::fprintf(stderr, "window %dx%d, smallest %dx%d\n", w.width(), w.height(),
					             w.minimumSizeHint().width(), w.minimumSizeHint().height());
					std::function<void(QWidget*, int)> dump = [&](QWidget* x, int depth) {
						if (depth > 7 || !x->isVisible()) return;
						const QSize m = x->minimumSizeHint().expandedTo(x->minimumSize());
						if (m.width() > 300 || m.height() > 300 || x->objectName() == QLatin1String("unit"))
							std::fprintf(stderr, "%*s%s %s min %dx%d\n", depth * 2, "", x->metaObject()->className(),
							             qPrintable(x->objectName()), m.width(), m.height());
						for (QObject* c : x->children())
							if (auto* cw = qobject_cast<QWidget*>(c)) dump(cw, depth + 1);
					};
					dump(&w, 0);
				}
				if (qEnvironmentVariableIsSet("NZ_DEBUG"))
					for (QFrame* f : w.findChildren<QFrame*>(QStringLiteral("unit")))
						std::fprintf(stderr, "unit %d wide, min %d, hint %d\n", f->width(),
						             f->minimumSizeHint().width(), f->sizeHint().width());
				if (qEnvironmentVariableIsSet("NZ_DEBUG")) // the top bar's rows: every control's top and height
					for (QWidget* x : w.findChild<QWidget*>(QStringLiteral("topbar"))->findChildren<QWidget*>())
						if (x->isVisible() && !qobject_cast<QFrame*>(x) &&
						    x->parentWidget()->objectName() == QLatin1String("unit"))
							std::fprintf(stderr, "row: %s %s y %d h %d\n", x->metaObject()->className(),
							             qPrintable(x->objectName()), x->mapTo(x->window(), QPoint()).y(), x->height());
				if (qEnvironmentVariableIsSet("NZ_DEBUG"))
					for (QToolButton* b : w.findChildren<QToolButton*>())
						std::fprintf(stderr, "%s %s %dx%d hint %dx%d\n", qPrintable(b->objectName()),
						             qPrintable(b->text()), b->width(), b->height(), b->sizeHint().width(),
						             b->sizeHint().height());
				w.grab().save(o.shotPath);
				app.dirty = false;
				QApplication::quit();
			}); // not dirty: no save question on the way out
	}
}
