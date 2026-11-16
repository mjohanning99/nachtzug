#include "settingsdialog.h"
#include "chopdialog.h"
#include "midi.h"
#include <QScrollArea>
#include <QApplication>
#include <QScreen>
#include <QStandardItemModel>
#include "app.h"
#include "samples.h"
#include "widgets.h"
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QTreeWidget>
#include <QHeaderView>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <cmath>

// A horizontal level meter for the input: green, then amber, red at the very top.
class LevelBar : public QWidget {
public:
	explicit LevelBar(QWidget* parent = nullptr) : QWidget(parent) {
		setMinimumHeight(16);
		setMinimumWidth(200);
	}
	void setLevel(float peak) {
		const float db = peak > 1e-6f ? 20.f * std::log10(peak) : -120.f;
		const float x = std::clamp((db + 60.f) / 60.f, 0.f, 1.f); // -60 dB .. 0 dB
		shown = std::max(x, shown - 0.04f);
		if (x >= 0.999f) clipUntil = 20;
		if (clipUntil > 0) clipUntil--;
		update();
	}

protected:
	void paintEvent(QPaintEvent*) override {
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
		p.setPen(QPen(theme::rule, 1));
		p.setBrush(theme::slot());
		p.drawRoundedRect(r, 3, 3);
		const QRectF fill(r.left() + 2, r.top() + 2, (r.width() - 4) * shown, r.height() - 4);
		QLinearGradient g(r.topLeft(), r.topRight());
		g.setColorAt(0.0, QColor(0x5f, 0x9a, 0x4a));
		g.setColorAt(0.75, QColor(0x8a, 0xa8, 0x3c));
		g.setColorAt(0.9, theme::amber);
		g.setColorAt(1.0, QColor(0xc4, 0x36, 0x1c));
		p.setPen(Qt::NoPen);
		p.setBrush(g);
		p.drawRoundedRect(fill, 2, 2);
		if (clipUntil > 0) {
			p.setBrush(QColor(0xc4, 0x36, 0x1c));
			p.drawRoundedRect(QRectF(r.right() - 10, r.top() + 2, 8, r.height() - 4), 2, 2);
		}
	}

private:
	float shown = 0.f;
	int clipUntil = 0;
};

static QLabel* hintLabel(const QString& text) {
	auto* l = new QLabel(text);
	l->setObjectName("dim");
	l->setWordWrap(true);
	return l;
}

SettingsDialog::SettingsDialog(App* a, const Hooks& h, QWidget* parent, int page) : QDialog(parent), app(a), hooks(h) {
	setWindowTitle(tr("Settings"));
	resize(660, 640);
	auto* v = new QVBoxLayout(this);
	v->setContentsMargins(14, 14, 14, 12);
	v->setSpacing(10);
	tabs = new QTabWidget;
	tabs->addTab(generalPage(), tr("General"));
	tabs->addTab(audioPage(), tr("Sound card"));
	tabs->addTab(samplesPage(), tr("Samples"));
	{ // the MIDI page has the most to say: it scrolls in a small window
		auto* sc = new QScrollArea;
		sc->setWidget(midiPage());
		sc->setWidgetResizable(true);
		sc->setFrameShape(QFrame::NoFrame);
		tabs->addTab(sc, tr("MIDI"));
	}
	tabs->setCurrentIndex(page);
	v->addWidget(tabs, 1);
	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
	v->addWidget(buttons);
	connect(this, &QDialog::finished, this, [this] { app->previewSample(QString()); });
}

// ---------------------------------------------------------------- general

QWidget* SettingsDialog::generalPage() {
	auto* w = new QWidget;
	auto* v = new QVBoxLayout(w);
	v->setContentsMargins(16, 16, 16, 16);
	v->setSpacing(14);
	auto* form = new QFormLayout;
	theme::tidyForm(form);
	form->setHorizontalSpacing(14);
	form->setVerticalSpacing(10);

	auto* lang = new QComboBox;
	const QString now = QSettings().value(QStringLiteral("language")).toString();
	lang->addItem(tr("Same as the system"), QString());
	lang->addItem(QStringLiteral("English"), QStringLiteral("en"));
	lang->addItem(QStringLiteral("Deutsch"), QStringLiteral("de"));
	lang->setCurrentIndex(std::max(0, lang->findData(now)));
	lang->setToolTip(tr("The language of the program. It changes the next time Nachtzug starts."));
	auto* langNote = hintLabel(tr("The new language is used the next time Nachtzug starts."));
	langNote->hide();
	connect(lang, &QComboBox::activated, this, [lang, langNote] {
		QSettings().setValue(QStringLiteral("language"), lang->currentData().toString());
		langNote->show();
	});
	auto* langCol = new QVBoxLayout;
	langCol->setSpacing(4);
	langCol->addWidget(lang);
	langCol->addWidget(langNote); // the note takes no room until it shows
	form->addRow(tr("Language"), langCol);

	auto* look = new QComboBox;
	for (int i = 0; i < theme::count(); i++)
		look->addItem(ui(theme::name(i)));
	look->setCurrentIndex(theme::current());
	look->setToolTip(tr("The colours of the program. Bakelite looks like an old instrument; the others are flat."));
	connect(look, &QComboBox::activated, this, [this](int i) {
		if (hooks.theme) hooks.theme(i);
	});
	form->addRow(tr("Colours"), look);

	// the size of everything: Automatic fits the window to the screen; larger sizes suit big or far-away screens
	auto* size = new QComboBox;
	const double nowScale = qApp->property("uiScale").isValid() ? qApp->property("uiScale").toDouble() : 1.0;
	const double chosen = QSettings().value(QStringLiteral("uiScale"), 0.0).toDouble();
	size->addItem(tr("Automatic: normal size, smaller if the screen needs it"), 0.0);
	// how large the window may get on this screen, from its smallest size now
	double fit = 4.0;
	if (QWidget* top = parentWidget() ? parentWidget()->window() : nullptr; top && top->screen()) {
		const QRect avail = top->screen()->availableGeometry();
		const QSize need = top->minimumSizeHint() + QSize(0, 34);
		fit = std::min(double(avail.width()) / need.width(), double(avail.height()) / need.height()) * nowScale;
	}
	for (int pct : {80, 90, 100, 115, 130, 150, 175, 200}) {
		const double sc = pct / 100.0;
		size->addItem(pct == 100 ? tr("100% (normal)") : tr("%1%").arg(pct), sc);
		auto* m = qobject_cast<QStandardItemModel*>(size->model());
		if (sc > fit + 0.001 && m) { // would not fit: shown, but not offered
			QStandardItem* it = m->item(size->count() - 1);
			it->setFlags(it->flags() & ~Qt::ItemIsEnabled);
			it->setText(tr("%1% (too large for this screen)").arg(pct));
		}
	}
	size->setCurrentIndex(std::max(0, size->findData(chosen <= 0 ? 0.0 : chosen)));
	size->setToolTip(
	    tr("How large the whole window and everything in it is drawn. Automatic picks a size that fits your screen."));
	auto* sizeNote = new QWidget;
	{
		auto* sn = new QHBoxLayout(sizeNote);
		sn->setContentsMargins(0, 0, 0, 0);
		sn->setSpacing(8);
		sn->addWidget(hintLabel(tr("The new size is used when Nachtzug starts again.")), 1);
		auto* again = new QPushButton(tr("Start again now"));
		again->setToolTip(
		    tr("Close Nachtzug and open it again at the new size. Unsaved changes are asked about first."));
		connect(again, &QPushButton::clicked, this, [this] {
			if (hooks.restart) {
				accept();
				hooks.restart();
			}
		});
		sn->addWidget(again);
	}
	sizeNote->hide();
	connect(size, &QComboBox::activated, this, [size, sizeNote] {
		QSettings().setValue(QStringLiteral("uiScale"), size->currentData().toDouble());
		sizeNote->show();
	});
	auto* sizeCol = new QVBoxLayout;
	sizeCol->setSpacing(4);
	sizeCol->addWidget(size);
	sizeCol->addWidget(sizeNote);
	form->addRow(tr("Size"), sizeCol);

	auto* page = new QComboBox;
	page->addItem(tr("Simple: a few big knobs in plain words"), true);
	page->addItem(tr("Full: every knob of the synthesiser"), false);
	page->setCurrentIndex(QSettings().value(QStringLiteral("simpleSound"), false).toBool() ? 0 : 1);
	page->setToolTip(tr("What the Sound tab shows. You can also switch at the top of the Sound tab."));
	connect(page, &QComboBox::activated, this, [this, page] {
		const bool simple = page->currentData().toBool();
		QSettings().setValue(QStringLiteral("simpleSound"), simple);
		if (hooks.simpleSound) hooks.simpleSound(simple);
	});
	form->addRow(tr("Sound tab"), page);

	// how the window is laid out and its keys drawn: also asked in the welcome window, the first time
	auto* keys = new QComboBox;
	keys->addItem(tr("A symbol and a word"), false);
	keys->addItem(tr("Symbols only (the word in the tooltip)"), true);
	keys->setCurrentIndex(theme::symbolsOnly ? 1 : 0);
	keys->setToolTip(tr("What the keys with a symbol show. Mute, Solo and Stop in the track rows always show the "
	                    "symbol alone."));
	connect(keys, &QComboBox::activated, this, [this, keys] {
		const bool only = keys->currentData().toBool();
		QSettings().setValue(QStringLiteral("keys"), only ? QStringLiteral("symbols") : QStringLiteral("words"));
		if (hooks.keys) hooks.keys(only);
	});
	form->addRow(tr("Keys"), keys);
	auto* layout = new QComboBox;
	layout->addItem(tr("The timeline above the notes"), true);
	layout->addItem(tr("The timeline beside the notes"), false);
	layout->setCurrentIndex(QSettings().value(QStringLiteral("layout/timelineTop"), true).toBool() ? 0 : 1);
	layout->setToolTip(tr("Where the timeline, or the loop grid, sits: above Notes, Sound and Mixer, as in most "
	                      "music programs, or beside them on the left. Also in the View menu."));
	connect(layout, &QComboBox::activated, this, [this, layout] {
		const bool top = layout->currentData().toBool();
		QSettings().setValue(QStringLiteral("layout/timelineTop"), top);
		if (hooks.timelineTop) hooks.timelineTop(top);
	});
	form->addRow(tr("Window"), layout);
	v->addLayout(form);

	auto* pads = new Switch(tr("Show the on-screen keys under the notes"), Switch::Beside);
	pads->setChecked(
	    QSettings()
	        .value(QStringLiteral("padsMode"), QSettings().value(QStringLiteral("pads"), true).toBool() ? 0 : 2)
	        .toInt() != 2);
	connect(pads, &QAbstractButton::toggled, this, [this](bool on) {
		QSettings().setValue(QStringLiteral("padsMode"), on ? 0 : 2);
		if (hooks.pads) hooks.pads(on);
	});
	v->addWidget(pads);
	auto* toasts = new Switch(tr("After a big change, show a note with an Undo button"), Switch::Beside);
	toasts->setChecked(QSettings().value(QStringLiteral("undoNotes"), true).toBool());
	connect(toasts, &QAbstractButton::toggled, this,
	        [](bool on) { QSettings().setValue(QStringLiteral("undoNotes"), on); });
	v->addWidget(toasts);
	auto* bars = new Switch(tr("Show scroll bars (the wheel and the trackpad scroll either way)"), Switch::Beside);
	bars->setToolTip(tr("Scroll bars beside the notes, the timeline, the loops and the panels. Without them, scroll "
	                    "with the mouse wheel or the trackpad; Shift + wheel scrolls sideways."));
	bars->setChecked(QSettings().value(QStringLiteral("scrollBars"), false).toBool());
	connect(bars, &QAbstractButton::toggled, this, [this](bool on) {
		QSettings().setValue(QStringLiteral("scrollBars"), on);
		if (hooks.scrollBars) hooks.scrollBars(on);
	});
	v->addWidget(bars);
	auto* welcome = new QPushButton(tr("Show the welcome window next time"));
	welcome->setToolTip(tr("The window that asks how you would like to start, shown the first time Nachtzug opens"));
	connect(welcome, &QPushButton::clicked, this, [welcome] {
		QSettings().setValue(QStringLiteral("welcomed"), false);
		welcome->setEnabled(false);
		welcome->setText(tr("The welcome window opens next time"));
	});
	auto* wr = new QHBoxLayout;
	wr->addWidget(welcome);
	wr->addStretch(1);
	v->addLayout(wr);
	v->addStretch(1);
	return w;
}

// ---------------------------------------------------------------- sound card

QWidget* SettingsDialog::audioPage() {
	auto* w = new QWidget;
	auto* v = new QVBoxLayout(w);
	v->setContentsMargins(16, 16, 16, 16);
	v->setSpacing(12);
	auto* out = new QGroupBox(tr("OUTPUT"));
	auto* form = new QFormLayout(out);
	theme::tidyForm(form);
	form->setHorizontalSpacing(14);
	form->setVerticalSpacing(10);
	driver = new QComboBox;
	output = new QComboBox;
	rate = new QComboBox;
	buffer = new QComboBox;
	driver->setToolTip(
	    tr("The sound system of your computer that Nachtzug plays through. Automatic picks the best one."));
	output->setToolTip(tr("Which speakers or headphones the sound comes out of"));
	rate->setToolTip(
	    tr("How many samples per second. 48 kHz suits almost everything; the sound card may pick its own."));
	buffer->setToolTip(
	    tr("How much sound is prepared ahead. Smaller reacts faster to the keys; larger is safer against crackles."));
	for (int r : {44100, 48000, 88200, 96000})
		rate->addItem(QStringLiteral("%1 kHz").arg(QLocale().toString(r / 1000.0, 'g', 3)), r);
	for (int b : {64, 128, 256, 512, 1024, 2048})
		buffer->addItem(QString(), b);
	form->addRow(tr("Sound system"), driver);
	form->addRow(tr("Play through"), output);
	form->addRow(tr("Sample rate"), rate);
	form->addRow(tr("Buffer"), buffer);
	audioState = hintLabel(QString());
	form->addRow(QString(), audioState);
	auto* testRow = new QHBoxLayout;
	auto* test = new QPushButton(tr("Play a test tone"));
	test->setToolTip(tr("A short beep, to check that sound comes out"));
	testRow->addWidget(test);
	testRow->addStretch(1);
	form->addRow(QString(), testRow);
	v->addWidget(out);

	auto* in = new QGroupBox(tr("INPUT"));
	auto* inForm = new QFormLayout(in);
	theme::tidyForm(inForm);
	inForm->setHorizontalSpacing(14);
	inForm->setVerticalSpacing(10);
	input = new QComboBox;
	input->setToolTip(tr("The microphone or line input: for recording your own samples, and for playing live through "
	                     "the Input strip of the mixer"));
	inForm->addRow(tr("Record from"), input);
	inForm->addRow(QString(),
	               hintLabel(tr("Nachtzug only listens while you record a sample (Samples page → Record…).")));
	v->addWidget(in);
	v->addStretch(1);

	fillAudio();
	connect(driver, &QComboBox::activated, this, [this] { applyAudio(); });
	connect(output, &QComboBox::activated, this, [this] { applyAudio(); });
	connect(rate, &QComboBox::activated, this, [this] { applyAudio(); });
	connect(buffer, &QComboBox::activated, this, [this] { applyAudio(); });
	connect(input, &QComboBox::activated, this,
	        [this] { QSettings().setValue(QStringLiteral("audio/input"), input->currentData().toString()); });
	connect(test, &QPushButton::clicked, this, [this] { app->testTone(); });
	return w;
}

void SettingsDialog::fillAudio() {
	filling = true;
	const App::AudioSetup s = app->audioSetup();
	driver->clear();
	driver->addItem(app->audioRunning() && s.driver.isEmpty() ? tr("Automatic (%1)").arg(app->audioDriver())
	                                                          : tr("Automatic"),
	                QString());
	for (const QString& d : App::audioDrivers())
		driver->addItem(d, d);
	driver->setCurrentIndex(std::max(0, driver->findData(s.driver)));
	output->clear();
	output->addItem(tr("The system’s default"), QString());
	for (const QString& d : app->audioDevices(false))
		output->addItem(d, d);
	if (!s.device.isEmpty() && output->findData(s.device) < 0)
		output->addItem(tr("%1 (not found)").arg(s.device), s.device);
	output->setCurrentIndex(std::max(0, output->findData(s.device)));
	rate->setCurrentIndex(std::max(0, rate->findData(s.rate)));
	const int sr = app->audioRunning() ? app->audioRate() : s.rate;
	for (int i = 0; i < buffer->count(); i++) {
		const int b = buffer->itemData(i).toInt();
		buffer->setItemText(i, tr("%1 samples (%2 ms)").arg(b).arg(QLocale().toString(1000.0 * b / sr, 'f', 1)));
	}
	buffer->setCurrentIndex(std::max(0, buffer->findData(s.buffer)));
	const QString wantIn = QSettings().value(QStringLiteral("audio/input")).toString();
	input->clear();
	input->addItem(tr("The system’s default"), QString());
	for (const QString& d : app->audioDevices(true))
		input->addItem(d, d);
	input->setCurrentIndex(std::max(0, input->findData(wantIn)));
	if (app->audioRunning())
		audioState->setText(tr("Playing through %1 at %2 Hz, %3 samples ahead (%4 ms).")
		                        .arg(app->audioDriver())
		                        .arg(app->audioRate())
		                        .arg(app->audioBuffer())
		                        .arg(QLocale().toString(1000.0 * app->audioBuffer() / app->audioRate(), 'f', 1)));
	else
		audioState->setText(
		    tr("No sound: %1").arg(app->audioError.isEmpty() ? tr("the sound output is not open.") : app->audioError));
	filling = false;
}

void SettingsDialog::applyAudio() {
	if (filling) return;
	App::AudioSetup s;
	s.driver = driver->currentData().toString();
	s.device = output->currentData().toString();
	if (s.driver != app->audioSetup().driver) s.device.clear(); // device names belong to one sound system
	s.rate = rate->currentData().toInt();
	s.buffer = buffer->currentData().toInt();
	setCursor(Qt::WaitCursor);
	const bool ok = app->openAudio(s);
	unsetCursor();
	if (ok) {
		QSettings st;
		st.setValue(QStringLiteral("audio/driver"), s.driver);
		st.setValue(QStringLiteral("audio/device"), s.device);
		st.setValue(QStringLiteral("audio/rate"), s.rate);
		st.setValue(QStringLiteral("audio/buffer"), s.buffer);
	} else {
		const QString err = app->audioError;
		QMessageBox::warning(
		    this, tr("Sound card"),
		    tr("That setting did not work:\n%1\n\nNachtzug goes back to the default output.").arg(err));
		app->openAudio(App::AudioSetup{});
	}
	fillAudio();
}

// ---------------------------------------------------------------- samples

// ---------------------------------------------------------------- MIDI

QWidget* SettingsDialog::midiPage() {
	auto* w = new QWidget;
	auto* v = new QVBoxLayout(w);
	v->setContentsMargins(16, 16, 16, 16);
	v->setSpacing(12);
	Midi* m = app->midi;
	if (!m) {
		v->addWidget(hintLabel(tr("MIDI is not used in this run of Nachtzug.")));
		v->addStretch(1);
		return w;
	}
	const bool usable = m->io().ok();
	if (!usable) { // say why, above the settings, which stay in view but cannot be used
		const QString why = m->io().error() == QLatin1String("access")
		                        ? tr("Nachtzug is not allowed to use this computer’s MIDI service. On Linux, your user "
		                             "has to be in the “audio” group: in a terminal, type  sudo usermod -aG audio "
		                             "$USER  then log out and back in, and start Nachtzug again.")
		                        : tr("This computer has no MIDI service that Nachtzug can use.");
		auto* l = new QLabel(why);
		l->setWordWrap(true);
		l->setTextInteractionFlags(Qt::TextSelectableByMouse);
		l->setStyleSheet(QStringLiteral("QLabel { border: 1px solid %1; border-radius: 6px; padding: 8px 10px; }")
		                     .arg(theme::amber.name()));
		v->addWidget(l);
	}
	auto* in = new QGroupBox(tr("KEYBOARDS AND CONTROLLERS"));
	auto* inForm = new QFormLayout(in);
	theme::tidyForm(inForm);
	inForm->setHorizontalSpacing(14);
	inForm->setVerticalSpacing(10);
	auto* inBox = new QComboBox;
	inBox->setToolTip(tr("Which MIDI devices Nachtzug listens to"));
	auto* heard = hintLabel(m->lastHeard().isEmpty() ? tr("Nothing heard yet: play a key or turn a knob.")
	                                                 : tr("Heard: %1").arg(m->lastHeard()));
	auto* follow = new Switch(tr("Follow its clock: its tempo, and when it starts and stops"), Switch::Beside);
	follow->setToolTip(
	    tr("For a drum machine or another program that leads: Nachtzug takes its tempo and starts and stops with it"));
	follow->setChecked(m->followClock());
	inForm->addRow(tr("Listen to"), inBox);
	inForm->addRow(QString(), heard);
	inForm->addRow(QString(), follow);
	auto* inCol = new QVBoxLayout;
	inCol->setSpacing(6);
	inCol->addWidget(in);
	inCol->addWidget(hintLabel(
	    tr("A keyboard plays the selected track, always in the song’s scale; notes played hard are accented. "
		   "In Chords mode it changes the chord. On a drum track the General MIDI drum notes play the drums.")));
	v->addLayout(inCol);

	auto* out = new QGroupBox(tr("CLOCK OUT"));
	auto* outForm = new QFormLayout(out);
	theme::tidyForm(outForm);
	outForm->setHorizontalSpacing(14);
	outForm->setVerticalSpacing(10);
	auto* outBox = new QComboBox;
	outBox->setToolTip(tr("A drum machine, synthesiser or program that should play in time with Nachtzug"));
	auto* send = new Switch(tr("Send the clock, start and stop to it"), Switch::Beside);
	send->setToolTip(tr("Nachtzug leads: the other device follows its tempo and starts and stops with it"));
	send->setChecked(m->sendClock());
	outForm->addRow(tr("Send to"), outBox);
	outForm->addRow(QString(), send);
	v->addWidget(out);

	auto* knobs = new QGroupBox(tr("CONTROLLER KNOBS"));
	auto* kv = new QVBoxLayout(knobs);
	kv->setSpacing(8);
	auto* count = new QLabel;
	auto* forget = new QPushButton(tr("Forget them all"));
	forget->setToolTip(tr("Untie every controller knob from the knobs on screen"));
	auto* kr = new QHBoxLayout;
	kr->addWidget(count, 1);
	kr->addWidget(forget);
	kv->addLayout(kr);
	kv->addWidget(hintLabel(tr("To tie a knob or fader of your controller to one on screen: right-click the knob on "
	                           "screen, choose Learn MIDI control, then turn the one on your controller. "
	                           "A knob of the Sound tab then works on whichever track is selected.")));
	v->addWidget(knobs);
	auto* again = new QPushButton(tr("Look for devices again"));
	again->setToolTip(tr("After plugging a MIDI device in"));
	auto* ar = new QHBoxLayout;
	ar->addWidget(again);
	ar->addStretch(1);
	v->addLayout(ar);
	v->addStretch(1);
	for (QWidget* x : {static_cast<QWidget*>(in), static_cast<QWidget*>(out), static_cast<QWidget*>(knobs),
	                   static_cast<QWidget*>(again)})
		x->setEnabled(usable);

	auto fill = [m, inBox, outBox, count, forget] {
		inBox->blockSignals(true);
		outBox->blockSignals(true);
		inBox->clear();
		outBox->clear();
		inBox->addItem(tr("Nothing"), QString());
		inBox->addItem(tr("Every device"), QStringLiteral("*"));
		for (const QString& n : m->io().inputs())
			inBox->addItem(n, n);
		outBox->addItem(tr("Nothing"), QString());
		for (const QString& n : m->io().outputs())
			outBox->addItem(n, n);
		// a device chosen before but not plugged in now stays chosen, and says so
		if (inBox->findData(m->input()) < 0) inBox->addItem(tr("%1 (not connected)").arg(m->input()), m->input());
		if (outBox->findData(m->output()) < 0) outBox->addItem(tr("%1 (not connected)").arg(m->output()), m->output());
		inBox->setCurrentIndex(std::max(0, inBox->findData(m->input())));
		outBox->setCurrentIndex(std::max(0, outBox->findData(m->output())));
		inBox->blockSignals(false);
		outBox->blockSignals(false);
		count->setText(tr("Controller knobs learned: %1").arg(m->learnedCount()));
		forget->setEnabled(m->learnedCount() > 0);
	};
	fill();
	connect(again, &QPushButton::clicked, this,
	        [m, fill] { // connect again: devices plugged in since are heard, and the chosen output found again
		        m->setInput(m->input());
		        m->setOutput(m->output());
		        fill();
	        });
	connect(inBox, &QComboBox::activated, this, [m, inBox] { m->setInput(inBox->currentData().toString()); });
	connect(outBox, &QComboBox::activated, this, [m, outBox] { m->setOutput(outBox->currentData().toString()); });
	connect(follow, &QAbstractButton::toggled, this, [m](bool on) { m->setFollowClock(on); });
	connect(send, &QAbstractButton::toggled, this, [m](bool on) { m->setSendClock(on); });
	connect(forget, &QPushButton::clicked, this, [m] { m->forgetAll(); });
	connect(m, &Midi::mappingsChanged, this, fill);
	connect(m, &Midi::heard, heard, [heard](const QString& what) { heard->setText(tr("Heard: %1").arg(what)); });
	return w;
}

QWidget* SettingsDialog::samplesPage() {
	auto* w = new QWidget;
	auto* v = new QVBoxLayout(w);
	v->setContentsMargins(16, 16, 16, 16);
	v->setSpacing(10);
	v->addWidget(hintLabel(tr("Sounds for the drums: the Nachtzug library, made for this program, and your own. Import "
	                          "WAV or AIFF files or record one, "
	                          "then open a drum track’s Sound tab and choose a sample for any of its eight drums. "
	                          "Imported files are copied here, so songs keep finding them.")));
	auto* h = new QHBoxLayout;
	h->setSpacing(10);
	sampleList = new QTreeWidget;
	sampleList->setColumnCount(2);
	sampleList->setHeaderHidden(true);
	sampleList->setRootIsDecorated(true);
	sampleList->header()->setStretchLastSection(false);
	sampleList->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	sampleList->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	sampleList->setToolTip(tr("Double-click a sample to hear it"));
	h->addWidget(sampleList, 1);
	auto* side = new QVBoxLayout;
	side->setSpacing(6);
	auto* importBtn = new QPushButton(tr("Import…"));
	auto* recordBtn = new QPushButton(tr("Record…"));
	playBtn = new QPushButton(tr("Listen"));
	renameBtn = new QPushButton(tr("Rename…"));
	removeBtn = new QPushButton(tr("Remove"));
	chopBtn = new QPushButton(tr("Chop…"));
	chopBtn->setToolTip(
	    tr("Cut the selected sound into pieces at its hits and put one on each drum pad: for drum loops"));
	auto* folderBtn = new QPushButton(tr("Open folder"));
	importBtn->setObjectName("primary");
	importBtn->setToolTip(tr("Copy WAV or AIFF files into the sample library"));
	recordBtn->setToolTip(tr("Record a sound from the microphone or line input"));
	playBtn->setToolTip(tr("Hear the selected sample"));
	renameBtn->setToolTip(tr("Give the selected sample another name. Songs that use it then need it picked again. The "
	                         "Nachtzug library cannot be renamed."));
	removeBtn->setToolTip(tr("Delete the selected sample from your samples. The Nachtzug library cannot be removed."));
	folderBtn->setToolTip(tr("Open the sample folder in your file manager"));
	for (auto* b : {importBtn, recordBtn})
		side->addWidget(b);
	side->addSpacing(12);
	for (auto* b : {playBtn, renameBtn, removeBtn, chopBtn})
		side->addWidget(b);
	side->addStretch(1);
	side->addWidget(folderBtn);
	h->addLayout(side);
	v->addLayout(h, 1);
	fillSamples();

	connect(sampleList, &QTreeWidget::currentItemChanged, this, [this] {
		const QString cur = currentSample();
		playBtn->setEnabled(!cur.isEmpty());
		chopBtn->setEnabled(!cur.isEmpty());
		renameBtn->setEnabled(!cur.isEmpty() && !samples::isLibrary(cur));
		removeBtn->setEnabled(!cur.isEmpty() && !samples::isLibrary(cur));
	});
	connect(sampleList, &QTreeWidget::itemDoubleClicked, this, [this] { app->previewSample(currentSample()); });
	connect(playBtn, &QPushButton::clicked, this, [this] { app->previewSample(currentSample()); });
	connect(chopBtn, &QPushButton::clicked, this, [this] {
		const QString cur = currentSample();
		if (cur.isEmpty()) return;
		app->previewSample(QString());
		if (ChopDialog::chop(app, cur, this)) {
			fillSamples();
			accept();
		} // done: back to the program, where the drums now play the pieces
	});
	connect(importBtn, &QPushButton::clicked, this, [this] {
		QSettings st;
		QString dir = st.value(QStringLiteral("samples/importDir")).toString();
		if (dir.isEmpty()) dir = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
		const QStringList files = QFileDialog::getOpenFileNames(
		    this, tr("Import samples"), dir,
		    tr("Sound files (*.wav *.aif *.aiff *.aifc *.WAV *.AIF *.AIFF);;All files (*)"));
		if (files.isEmpty()) return;
		st.setValue(QStringLiteral("samples/importDir"), QFileInfo(files[0]).absolutePath());
		QStringList failed;
		QString last;
		for (const QString& f : files) {
			QString err;
			const QString name = samples::import(f, err);
			if (name.isEmpty())
				failed << QStringLiteral("%1: %2").arg(QFileInfo(f).fileName(), err);
			else
				last = name;
		}
		fillSamples(last);
		if (!failed.isEmpty())
			QMessageBox::warning(this, tr("Import samples"),
			                     tr("Some files could not be imported:\n\n%1").arg(failed.join(QStringLiteral("\n"))));
	});
	connect(recordBtn, &QPushButton::clicked, this, [this] {
		RecordDialog dlg(app, this);
		dlg.exec();
		fillSamples(dlg.savedName());
		fillAudio(); // recording may have reopened things
	});
	connect(renameBtn, &QPushButton::clicked, this, [this] {
		const QString from = currentSample();
		if (from.isEmpty()) return;
		bool ok = false;
		const QString to = QInputDialog::getText(this, tr("Rename sample"), tr("New name"), QLineEdit::Normal,
		                                         QFileInfo(from).completeBaseName(), &ok);
		if (!ok || to.trimmed().isEmpty() || to.trimmed() == QFileInfo(from).completeBaseName()) return;
		QString err;
		if (!samples::rename(from, to, err)) {
			QMessageBox::warning(this, tr("Rename sample"), err);
			return;
		}
		fillSamples(to.trimmed() + QLatin1Char('.') + QFileInfo(from).suffix());
	});
	connect(removeBtn, &QPushButton::clicked, this, [this] {
		const QString name = currentSample();
		if (name.isEmpty()) return;
		if (QMessageBox::question(
		        this, tr("Remove sample"),
		        tr("Delete %1 from the sample library? Songs that use it play the built-in drum instead.").arg(name),
		        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
			return;
		app->previewSample(QString());
		samples::remove(name);
		fillSamples();
		app->touch(); // drums that used it fall back to their own sound
	});
	connect(folderBtn, &QPushButton::clicked, this,
	        [] { QDesktopServices::openUrl(QUrl::fromLocalFile(samples::folder())); });
	return w;
}

void SettingsDialog::fillSamples(const QString& select) {
	// which parts were open, so filling again does not fold everything up
	QSet<QString> open;
	for (int i = 0; i < sampleList->topLevelItemCount(); i++) {
		QTreeWidgetItem* top = sampleList->topLevelItem(i);
		if (top->isExpanded()) open << top->text(0);
		for (int k = 0; k < top->childCount(); k++)
			if (top->child(k)->isExpanded()) open << top->child(k)->text(0);
	}
	const bool first = sampleList->topLevelItemCount() == 0;
	sampleList->clear();
	auto secs = [](double v) {
		return v > 0 ? QStringLiteral("%1 s").arg(QLocale().toString(v, 'f', v < 10 ? 2 : 1)) : tr("cannot be read");
	};
	QTreeWidgetItem* selectItem = nullptr;
	auto bold = [](QTreeWidgetItem* it) {
		QFont f = it->font(0);
		f.setWeight(QFont::DemiBold); // the Grotesk’s heaviest, 600
		it->setFont(0, f);
	};
	// the library: categories, each folded until opened
	auto* lib = new QTreeWidgetItem(sampleList,
	                                {tr("Nachtzug library"), tr("%n sounds", nullptr, int(samples::library().size()))});
	bold(lib);
	lib->setFlags(Qt::ItemIsEnabled);
	for (const QString& cat : samples::libraryCategories()) {
		auto* c = new QTreeWidgetItem(lib, {cat});
		c->setFlags(Qt::ItemIsEnabled);
		for (const samples::LibrarySound& ls : samples::library()) {
			if (ls.category != cat) continue;
			auto* it = new QTreeWidgetItem(c, {ls.name, secs(samples::seconds(ls.id))});
			it->setData(0, Qt::UserRole, ls.id);
			it->setToolTip(0, ls.about);
			if (ls.id == select) selectItem = it;
		}
		c->setExpanded(open.contains(cat) || (selectItem && selectItem->parent() == c));
	}
	lib->setExpanded(first || open.contains(lib->text(0)) || (selectItem != nullptr));
	// your own
	const QStringList names = samples::list();
	auto* mine = new QTreeWidgetItem(sampleList,
	                                 {tr("Your samples"), names.isEmpty() ? QString() : QString::number(names.size())});
	bold(mine);
	mine->setFlags(Qt::ItemIsEnabled);
	for (const QString& n : names) {
		auto* it = new QTreeWidgetItem(mine, {n, secs(samples::seconds(n))});
		it->setData(0, Qt::UserRole, n);
		if (n == select) selectItem = it;
	}
	if (names.isEmpty()) {
		auto* none = new QTreeWidgetItem(mine, {tr("No samples yet. Import a sound file or record one.")});
		none->setFlags(Qt::NoItemFlags);
	}
	mine->setExpanded(true);
	if (selectItem) {
		sampleList->setCurrentItem(selectItem);
		sampleList->scrollToItem(selectItem);
	}
	const QString cur = currentSample();
	playBtn->setEnabled(!cur.isEmpty());
	chopBtn->setEnabled(!cur.isEmpty());
	renameBtn->setEnabled(!cur.isEmpty() && !samples::isLibrary(cur));
	removeBtn->setEnabled(!cur.isEmpty() && !samples::isLibrary(cur));
}

QString SettingsDialog::currentSample() const {
	const QTreeWidgetItem* it = sampleList->currentItem();
	return it ? it->data(0, Qt::UserRole).toString() : QString();
}

// ---------------------------------------------------------------- recording

RecordDialog::RecordDialog(App* a, QWidget* parent) : QDialog(parent), app(a) {
	setWindowTitle(tr("Record a sample"));
	resize(480, 300);
	auto* v = new QVBoxLayout(this);
	v->setContentsMargins(18, 16, 18, 12);
	v->setSpacing(12);
	auto* form = new QFormLayout;
	theme::tidyForm(form);
	form->setHorizontalSpacing(14);
	form->setVerticalSpacing(10);
	input = new QComboBox;
	input->addItem(tr("The system’s default"), QString());
	for (const QString& d : app->audioDevices(true))
		input->addItem(d, d);
	input->setCurrentIndex(std::max(0, input->findData(QSettings().value(QStringLiteral("audio/input")).toString())));
	form->addRow(tr("Record from"), input);
	level = new LevelBar;
	form->addRow(tr("Level"), level);
	timeLabel = new QLabel(QStringLiteral("0.0 s"));
	timeLabel->setFont(theme::lcdFont(18, true));
	form->addRow(tr("Length"), timeLabel);
	name = new QLineEdit(tr("Recording"));
	form->addRow(tr("Name"), name);
	trim = new Switch(tr("Cut off the silence at the start and end"), Switch::Beside);
	trim->setChecked(true);
	form->addRow(QString(), trim);
	v->addLayout(form);
	hint = hintLabel(tr("Press Record, make the sound, then press Stop. Up to two minutes."));
	v->addWidget(hint);
	v->addStretch(1);
	auto* buttons = new QDialogButtonBox;
	recBtn = buttons->addButton(tr("● Record"), QDialogButtonBox::ActionRole);
	saveBtn = buttons->addButton(tr("Save"), QDialogButtonBox::AcceptRole);
	buttons->addButton(QDialogButtonBox::Cancel);
	saveBtn->setEnabled(false);
	saveBtn->setObjectName("primary");
	v->addWidget(buttons);
	auto* timer = new QTimer(this);
	connect(timer, &QTimer::timeout, this, [this] {
		if (!recording) return;
		level->setLevel(app->captureLevel());
		const double secs = app->captureSeconds();
		timeLabel->setText(QStringLiteral("%1 s").arg(QLocale().toString(secs, 'f', 1)));
		if (secs >= 119.5) stop();
	});
	timer->start(40);
	connect(recBtn, &QPushButton::clicked, this, [this] {
		if (recording)
			stop();
		else
			start();
	});
	connect(buttons, &QDialogButtonBox::accepted, this, &RecordDialog::save);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(this, &QDialog::finished, this, [this] {
		if (recording) {
			int r = 0;
			app->stopCapture(r);
			recording = false;
		}
	});
}

void RecordDialog::start() {
	QString err;
	if (!app->startCapture(input->currentData().toString(), err)) {
		QMessageBox::warning(this, tr("Record a sample"),
		                     app->audioDevices(true).isEmpty()
		                         ? tr("No microphone or line input was found. Plug one in, or check your system’s "
								      "sound settings, then try again.")
								 : tr("The input could not be opened:\n%1").arg(err));
		return;
	}
	QSettings().setValue(QStringLiteral("audio/input"), input->currentData().toString());
	recording = true;
	take.clear();
	recBtn->setText(tr("■ Stop"));
	saveBtn->setEnabled(false);
	input->setEnabled(false);
	hint->setText(tr("Recording… press Stop when you are done."));
}

void RecordDialog::stop() {
	take = app->stopCapture(takeRate);
	recording = false;
	level->setLevel(0.f);
	recBtn->setText(tr("● Record again"));
	input->setEnabled(true);
	float peak = 0.f;
	for (float x : take)
		peak = std::max(peak, std::fabs(x));
	saveBtn->setEnabled(!take.empty());
	if (take.empty())
		hint->setText(tr("Nothing came in. Check the input in the list above, or your system’s sound settings."));
	else if (peak < 0.003f)
		hint->setText(tr(
		    "That was almost silent. Is the right input chosen, and is it turned up in your system’s sound settings?"));
	else if (peak > 0.99f)
		hint->setText(tr("Recorded %1 s. It was so loud it clipped; turn the input down for a cleaner take.")
		                  .arg(QLocale().toString(double(take.size()) / takeRate, 'f', 1)));
	else
		hint->setText(tr("Recorded %1 s. Save it, or record again.")
		                  .arg(QLocale().toString(double(take.size()) / takeRate, 'f', 1)));
}

void RecordDialog::save() {
	std::vector<float> s = take;
	if (trim->isChecked() && !s.empty()) {
		float peak = 0.f;
		for (float x : s)
			peak = std::max(peak, std::fabs(x));
		const float gate = std::max(peak * 0.02f, 0.0008f); // -34 dB under the loudest part counts as silence
		size_t a = 0, b = s.size();
		while (a < b && std::fabs(s[a]) < gate)
			a++;
		while (b > a && std::fabs(s[b - 1]) < gate)
			b--;
		const size_t pre = size_t(takeRate) / 200, post = size_t(takeRate) / 20; // keep 5 ms before and 50 ms after
		a = a > pre ? a - pre : 0;
		b = std::min(s.size(), b + post);
		s = std::vector<float>(s.begin() + long(a), s.begin() + long(b));
		const size_t fade = std::min<size_t>(s.size() / 2, size_t(takeRate) / 100); // soft edges, no clicks
		for (size_t i = 0; i < fade; i++) {
			const float g = float(i) / float(fade);
			s[i] *= g;
			s[s.size() - 1 - i] *= g;
		}
	}
	QString err;
	saved = samples::saveRecording(name->text(), s, takeRate, err);
	if (saved.isEmpty()) {
		QMessageBox::warning(this, tr("Record a sample"), err);
		return;
	}
	accept();
}
