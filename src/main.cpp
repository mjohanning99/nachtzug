// Nachtzug: a scale-locked synth workstation for acid and Berlin School music. This file starts the program:
// the language, the look, the first-start window, then the main window, or one of the runs the command line
// asks for (see cli.h).
//
// What's here: main(), and two small helpers it installs: MacKeyNames (⌘ instead of Ctrl in the texts on a Mac)
// and FileOpener (a song double-clicked in the Finder).
// Changes in the song file: none (it opens the song named on the command line).
#include "cli.h"
#include "guitest.h"
#include "parity.h"
#include "selftest.h"
#include "app.h"
#include "mainwindow.h"
#include "widgets.h"
#include "welcome.h"
#include "autosave.h"
#include "midi.h"
#include <QApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>
#include <QFileOpenEvent>
#include <QRegularExpression>
#include <QFile>
#include <QSettings>
#include <QStyleFactory>
#include <QTimer>
#include <QDialog>
#include <QProcess>
#include <QScreen>
#include <cmath>
#include <cstdio>
#if defined(_WIN32)
#include <windows.h>
#endif

#if defined(Q_OS_MACOS)
// On a Mac, Qt puts what Ctrl does elsewhere on the Command key, so the texts should say ⌘ too.
// Sits in front of the real translation and only touches texts that mention Ctrl (Strg in German).
class MacKeyNames : public QTranslator {
public:
	explicit MacKeyNames(const QTranslator* inner) : inner_(inner) {}
	bool isEmpty() const override { return false; }
	QString translate(const char* ctx, const char* src, const char* dis, int n) const override {
		const QString t = inner_ && !inner_->isEmpty() ? inner_->translate(ctx, src, dis, n) : QString();
		QString s = t.isEmpty() ? QString::fromUtf8(src) : t;
		if (!s.contains(QLatin1String("Ctrl")) && !s.contains(QLatin1String("Strg"))) return t;
		static const QRegularExpression word(QStringLiteral("(Ctrl|Strg)\\+(?=\\w\\w)")),
		    plus(QStringLiteral("(Ctrl|Strg)\\+")), bare(QStringLiteral("\\b(Ctrl|Strg)\\b"));
		s.replace(word, QStringLiteral("⌘-")).replace(plus, QStringLiteral("⌘")).replace(bare, QStringLiteral("⌘"));
		return s;
	}

private:
	const QTranslator* inner_;
};
#endif

// macOS hands over a song double-clicked in the Finder as an event, not on the command line
class FileOpener : public QObject {
public:
	FileOpener(App* app, MainWindow* w) : app_(app), w_(w) {}
	bool eventFilter(QObject* o, QEvent* e) override {
		if (e->type() != QEvent::FileOpen) return QObject::eventFilter(o, e);
		const QString path = static_cast<QFileOpenEvent*>(e)->file();
		if (!path.isEmpty() && path != app_->filePath && w_->maybeSave() && !app_->load(path))
			std::fprintf(stderr, "Could not read the song %s\n", qPrintable(path));
		return true;
	}

private:
	App* app_;
	MainWindow* w_;
};

int main(int argc, char** argv) {
	const Options o = parseOptions(argc, argv);
	QString langName = o.langName, themeName = o.themeName; // both filled in below when not given
	if (o.windowless() && !qEnvironmentVariable("QT_QPA_PLATFORM").startsWith(QLatin1String("offscreen")))
		qputenv("QT_QPA_PLATFORM",
		        "offscreen"); // (an offscreen setting given from outside, such as a screen size, is kept)
	// a finished Mac program has no window-less platform packed in: its build script tests it on the real one; and
	// pictures can be taken on the real one too (to see how a system draws, on GitHub's Mac machines)
	if (qEnvironmentVariableIsSet("NZ_TEST_PLATFORM") &&
	    (o.selftest || !o.renderPath.isEmpty() || !o.shotPath.isEmpty()))
		qputenv("QT_QPA_PLATFORM", qgetenv("NZ_TEST_PLATFORM"));

	// test runs and pictures keep their own settings and sample folder, so they neither depend on nor change yours
	const bool testRun = o.testRun();
	// the size of everything (Settings > General). It has to be known before the program starts drawing.
	// Automatic uses a size worked out from the screen at an earlier start (see fitToScreen below).
	double uiScale = 1.0;
	bool autoScale = true;
	const QByteArray outsideScale = qgetenv("QT_SCALE_FACTOR"); // one set from outside is kept, and multiplied
	if (!testRun && o.renderPath.isEmpty()) {
		QSettings pre(QStringLiteral("Nachtzug"),
		              QStringLiteral("Nachtzug")); // the same place QSettings() uses once the program has its name
		const double chosen = pre.value(QStringLiteral("uiScale"), 0.0).toDouble();
		autoScale = chosen <= 0.0;
		uiScale = std::clamp(autoScale ? pre.value(QStringLiteral("autoScale"), 1.0).toDouble() : chosen, 0.5, 3.0);
		if (std::fabs(uiScale - 1.0) > 0.001) {
			const double base =
			    qEnvironmentVariableIsSet("QT_SCALE_FACTOR") ? qEnvironmentVariable("QT_SCALE_FACTOR").toDouble() : 1.0;
			qputenv("QT_SCALE_FACTOR", QByteArray::number(std::max(0.25, base) * uiScale, 'f', 3));
		}
	}

#if defined(Q_OS_WIN)
	// a window program on Windows has no console: the tests and --render print into the one they were started from
	if ((o.selftest || o.guiTest || o.tipCheck || !o.renderPath.isEmpty()) && AttachConsole(ATTACH_PARENT_PROCESS)) {
		std::freopen("CONOUT$", "w", stdout);
		std::freopen("CONOUT$", "w", stderr);
	}
#endif
	QApplication qa(argc, argv);
	qa.setProperty("uiScale", uiScale); // for the settings window: the size in use now
	qa.setProperty("outsideScale",
	               QString::fromLocal8Bit(outsideScale)); // for starting again: the scale set from outside, if any
	QApplication::setApplicationName(testRun ? QStringLiteral("Nachtzug-test") : QStringLiteral("Nachtzug"));
	QApplication::setOrganizationName(QStringLiteral("Nachtzug"));
	if (testRun) {
		QSettings().clear();
		QSettings().setValue(QStringLiteral("welcomed"), true);
		if (o.symbolsOnly) QSettings().setValue(QStringLiteral("keys"), QStringLiteral("symbols"));
	}
	QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
#if !defined(Q_OS_MACOS)
	// (on a Mac this would replace the app's icon in the Dock with one that fills the whole square, larger than the
	// other programs' icons; the bundle's icon, on Apple's grid, is the right one there)
	QApplication::setWindowIcon(theme::appIcon());
#endif
	theme::loadFonts();
	// the language: chosen in View > Language, or the system's; English is built in
	if (langName.isEmpty() && (o.selftest || o.guiTest || o.tipCheck || !o.parityDir.isEmpty()))
		langName = QStringLiteral("en"); // the tests look for English words
	if (langName.isEmpty()) langName = QSettings().value(QStringLiteral("language")).toString();
	if (langName.isEmpty()) langName = QLocale::system().name().left(2);
	static QTranslator own, qtbase;
	// numbers follow the language of the program (5,3 ms in German), whatever the system is set to
	QLocale::setDefault(own.load(QStringLiteral(":/i18n/nachtzug_%1.qm").arg(langName))
	                        ? QLocale(langName)
							: QLocale(QLocale::English, QLocale::UnitedKingdom));
	if (!own.isEmpty()) {
		QApplication::installTranslator(&own);
		const QString qtName = QStringLiteral("qtbase_%1").arg(langName);
		if (qtbase.load(qtName, QLibraryInfo::path(QLibraryInfo::TranslationsPath)) ||
		    qtbase.load(qtName, QCoreApplication::applicationDirPath() +
		                            QStringLiteral("/../Resources/translations"))) // inside Nachtzug.app
			QApplication::installTranslator(&qtbase);
	}
#if defined(Q_OS_MACOS)
	static MacKeyNames macKeys(&own); // asks own first when it holds a language
	QApplication::installTranslator(&macKeys);
#endif
	if (themeName.isEmpty())
		themeName = QSettings()
		                .value(QStringLiteral("look"), QStringLiteral("Bakelite"))
		                .toString(); // the instrument look by default
	int themeIndex = 0;
	for (int i = 0; i < theme::count(); i++)
		if (theme::name(i).compare(themeName, Qt::CaseInsensitive) == 0) themeIndex = i;
	theme::apply(themeIndex);
	theme::installWells();
	QApplication::setPalette(theme::palette());
	qa.setStyleSheet(theme::styleSheet());

	// the very first start: ask for the language and how to begin
	if (!testRun && o.renderPath.isEmpty() && !QSettings().value(QStringLiteral("welcomed"), false).toBool()) {
		WelcomeDialog welcome([&](const QString& code) {
			QApplication::removeTranslator(&own);
			QApplication::removeTranslator(&qtbase);
			const QString l = code.isEmpty() ? QLocale::system().name().left(2) : code;
			if (own.load(QStringLiteral(":/i18n/nachtzug_%1.qm").arg(l))) {
				QLocale::setDefault(QLocale(l));
				QApplication::installTranslator(&own);
				if (qtbase.load(QStringLiteral("qtbase_%1").arg(l),
				                QLibraryInfo::path(QLibraryInfo::TranslationsPath)) ||
				    qtbase.load(QStringLiteral("qtbase_%1").arg(l),
				                QCoreApplication::applicationDirPath() + QStringLiteral("/../Resources/translations")))
					QApplication::installTranslator(&qtbase);
			} else {
				QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedKingdom));
				// English: an empty translator still sends the change, so the window goes back to English
				QEvent ev(QEvent::LanguageChange);
				for (QWidget* w : QApplication::topLevelWidgets())
					QApplication::sendEvent(w, &ev);
			}
		});
		if (welcome.exec() != QDialog::Accepted)
			QSettings().setValue(QStringLiteral("welcomed"), true); // closed: do not ask again
	}

	if (o.selftest) return selfTest();
	if (!o.parityDir.isEmpty()) return writeParity(o.parityDir);
	if (!o.renderPath.isEmpty()) return renderSong(o);

	if (o.dialogName == QLatin1String("welcome") && !o.shotPath.isEmpty()) return pictureOfWelcome(o);
	App app(o.shotPath.isEmpty() || o.frames > 0); // pictures need no sound card, unless they show the song playing
	if (!o.shotPath.isEmpty()) app.rng.seed(1); // pictures come out the same every time (tools/parity.sh relies on it)
	{ // keyboards, controllers and clock (test runs do not keep their choices)
		app.midi = new Midi(&app, !testRun);
		QObject::connect(app.midi, &Midi::mappingsChanged, &app, [] {
			for (QWidget* x : QApplication::allWidgets())
				if (x->property("midiTarget").isValid()) x->update();
		});
	}
	Knob::midiLookup = [&app](const QString& target) { return app.midi ? app.midi->controllerFor(target) : -1; };
	if (!o.songPath.isEmpty() && !app.load(o.songPath))
		std::fprintf(stderr, "Could not read the song %s\n", qPrintable(o.songPath));
	preparePicture(app, o);
	MainWindow w(&app);
	FileOpener opener(&app, &w);
	qa.installEventFilter(&opener);
	setUpWindow(app, w, o);
	if (!testRun) {
		// does the window fit the screen? With the size on Automatic, a size that fits is worked out and the program
		// starts again at it (once, before anything is shown). Otherwise it opens as large as the screen allows.
		const QRect avail = w.screen() ? w.screen()->availableGeometry() : QRect(0, 0, 1560, 980);
		const QSize need = w.minimumSizeHint() + QSize(0, 34); // and the window's title bar
		const double fit =
		    std::min(double(avail.width()) / need.width(), double(avail.height()) / need.height()) * uiScale;
		if (autoScale && !qEnvironmentVariableIsSet("NZ_RELAUNCHED")) {
			// on a laptop screen everything a little smaller, so the window is not cramped: the size at which a
			// window of 1500 × 920 would fit, if that is smaller than the size the window merely needs
			const double comfort = std::min(double(avail.width()) / 1500.0, double(avail.height()) / 920.0) * uiScale;
			double best = 0.8; // the smallest size offered; below it the window is maximised and scrolls
			for (double c : {0.8, 0.85, 0.9, 1.0})
				if (c <= std::min(fit, std::max(0.85, comfort)) + 0.001) best = c;
			if (std::fabs(best - uiScale) > 0.001) {
				QSettings().setValue(QStringLiteral("autoScale"), best);
				QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
				env.insert(QStringLiteral("NZ_RELAUNCHED"), QStringLiteral("1"));
				if (outsideScale.isEmpty())
					env.remove(QStringLiteral("QT_SCALE_FACTOR"));
				else
					env.insert(QStringLiteral("QT_SCALE_FACTOR"), QString::fromLocal8Bit(outsideScale));
				QProcess again;
				again.setProgram(QCoreApplication::applicationFilePath());
				again.setArguments(QCoreApplication::arguments().mid(1));
				again.setProcessEnvironment(env);
				if (again.startDetached()) return 0;
			}
		}
		const QSize want(std::min(1560, avail.width()), std::min(980, avail.height() - 34));
		w.resize(want.expandedTo(w.minimumSizeHint()));
		if (w.width() > avail.width() || w.height() + 34 > avail.height()) w.setWindowState(Qt::WindowMaximized);
	}
	w.show();
	if (!testRun) { // a spare copy of unsaved work, and the one a crash left behind
		auto* autosave = new Autosave(&app);
		QObject::connect(&qa, &QApplication::aboutToQuit, autosave, &Autosave::finish);
		QTimer::singleShot(300, &w, [autosave, &w] { autosave->offerRecovery(&w); });
	}
	scheduleViews(w, o);
	if (o.guiTest) scheduleGuiTest(app, w);
	if (o.tipCheck) scheduleTooltipCheck(app, w);
	scheduleScreenshot(app, w, o);
	return qa.exec();
}
