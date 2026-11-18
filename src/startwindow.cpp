// The start window: see startwindow.h.
//
// What's here: StartWindow, made by MainWindow::showStart() (at start, from File > New from a starter song…, and by
// name for pictures); it only says what was chosen, MainWindow opens it. listen() renders a few bars of a starter on
// a thread of its own (a second or so) and plays them as the sound library plays a sample, so nothing in the song
// changes until a starter is chosen.
// Changes in the song file: none (a starter opens as a new song).

#include "startwindow.h"
#include "app.h"
#include "engine.h"
#include "recent.h"
#include "samples.h"
#include "starters.h"
#include "widgets.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSettings>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

namespace {
// a card to click: the whole of it opens what it shows; labels on it let the click through. Its height is in the
// style sheet (a style sheet's height wins over the widget's own): 56, or 44 when small
QPushButton* card(bool small) {
	auto* b = new QPushButton;
	b->setObjectName("startCard");
	b->setProperty("small", small);
	b->setFocusPolicy(Qt::StrongFocus);
	b->setAttribute(Qt::WA_Hover);
	return b;
}
// a line that ends in … when there is no room for all of it (the whole is in the card's tooltip)
class Shortened : public QLabel {
public:
	Shortened(const QString& text, Qt::TextElideMode mode) : QLabel(text), mode_(mode) {}
	QSize minimumSizeHint() const override { return {40, QLabel::minimumSizeHint().height()}; }

protected:
	void paintEvent(QPaintEvent*) override {
		QPainter p(this);
		p.setPen(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, foregroundRole()));
		p.drawText(rect(), Qt::AlignLeft | Qt::AlignVCenter, fontMetrics().elidedText(text(), mode_, width()));
	}

private:
	Qt::TextElideMode mode_;
};
QLabel* label(const QString& text, const char* object, Qt::TextElideMode shorten = Qt::ElideNone) {
	QLabel* l = shorten == Qt::ElideNone ? new QLabel(text) : new Shortened(text, shorten);
	l->setObjectName(object);
	l->setAttribute(Qt::WA_TransparentForMouseEvents);
	return l;
}
} // namespace

StartWindow::StartWindow(App* a, QWidget* parent) : QDialog(parent), app(a) {
	setWindowTitle(tr("Start"));
	const int n = int(starters().size());
	heard.resize(size_t(n));
	listenKeys.resize(size_t(n), nullptr);
	songs.resize(size_t(n));

	auto* v = new QVBoxLayout(this);
	v->setContentsMargins(22, 18, 22, 16);
	v->setSpacing(12);
	auto* head = new QHBoxLayout;
	head->setSpacing(14);
	auto* mark = new QLabel;
	mark->setPixmap(theme::logo(192));
	mark->setScaledContents(true);
	mark->setFixedSize(48, 48);
	head->addWidget(mark, 0, Qt::AlignVCenter);
	auto* words = new QVBoxLayout;
	words->setSpacing(2);
	auto* title = new QLabel(tr("What would you like to play?"));
	title->setObjectName("startTitle");
	words->addWidget(title);
	auto* sub = new QLabel(tr("Start from one of these songs and make it your own, or carry on with one of yours."));
	sub->setObjectName("dim");
	sub->setWordWrap(true);
	words->addWidget(sub);
	head->addLayout(words, 1);
	v->addLayout(head);

	auto* row = new QHBoxLayout;
	row->setSpacing(14);
	// the starters, one card each: listen, its name and a line about it, its tempo and key
	auto* left = new QGroupBox(tr("Starter songs").toUpper());
	left->setToolTip(tr("Short songs made with Nachtzug, one for each style. One opens as a new song of your own: "
	                    "the starter itself stays as it is."));
	auto* lv = new QVBoxLayout(left);
	theme::wellMargins(lv, 12, 10);
	lv->setSpacing(6);
	for (int i = 0; i < n; i++) {
		const ::Starter& st = starters()[size_t(i)];
		const bool ok = loadStarter(i, songs[size_t(i)]);
		const Song& s = songs[size_t(i)];
		QPushButton* c = card(false);
		c->setEnabled(ok);
		c->setToolTip(tr("Open “%1” as a new song").arg(ui(st.name)) + QStringLiteral("\n") + ui(st.words));
		c->setAccessibleName(ui(st.name));
		auto* h = new QHBoxLayout(c);
		h->setContentsMargins(10, 6, 14, 6);
		h->setSpacing(12);
		auto* play = new QPushButton;
		play->setFixedSize(30, 26);
		play->setCheckable(true);
		play->setFocusPolicy(Qt::StrongFocus);
		play->setToolTip(tr("Listen: a few bars from the middle of “%1”; nothing is opened").arg(ui(st.name)));
		theme::symbolKey(play, QStringLiteral("play"), tr("Listen"), true);
		connect(play, &QPushButton::clicked, this, [this, i] { listen(i); });
		listenKeys[size_t(i)] = play;
		h->addWidget(play, 0, Qt::AlignVCenter);
		auto* text = new QVBoxLayout;
		text->setSpacing(1);
		text->addStretch(1);
		auto* top = new QHBoxLayout;
		top->setSpacing(8);
		top->addWidget(label(ui(st.name), "cardName"));
		top->addStretch(1);
		const QString facts =
		    ok ? tr("%1 BPM · %2 %3").arg(int(std::lround(s.bpm))).arg(pitchName(s.root), ui(s.sc().name))
			   : tr("Missing");
		top->addWidget(label(facts.toUpper(), "engraved"), 0, Qt::AlignVCenter);
		text->addLayout(top);
		text->addWidget(label(ui(st.words), "dim", Qt::ElideRight));
		text->addStretch(1);
		h->addLayout(text, 1);
		connect(c, &QPushButton::clicked, this, [this, i] { choose(Starter, i); });
		lv->addWidget(c);
	}
	row->addWidget(left, 1);

	// the songs opened or saved lately, and Open…
	auto* right = new QGroupBox(tr("Your songs").toUpper());
	right->setToolTip(tr("The songs you opened or saved lately, newest first"));
	right->setFixedWidth(300);
	auto* rv = new QVBoxLayout(right);
	theme::wellMargins(rv, 12, 10);
	rv->setSpacing(6);
	const QStringList paths = recent::list(), names = recent::names(paths);
	for (int i = 0; i < int(paths.size()) && i < 6; i++) {
		const QString& p = paths[i];
		const bool here = recent::exists(p);
		QPushButton* c = card(true);
		c->setEnabled(here);
		c->setToolTip(here ? tr("Open “%1” from %2").arg(names[i], recent::folderOf(p))
		                   : tr("“%1” is no longer in %2").arg(names[i], recent::folderOf(p)));
		c->setAccessibleName(names[i]);
		auto* cv = new QVBoxLayout(c);
		cv->setContentsMargins(12, 4, 12, 4);
		cv->setSpacing(0);
		auto* name = label(names[i], here ? "cardName" : "cardGone", Qt::ElideRight);
		cv->addWidget(name);
		const QString where = here ? recent::folderOf(p) : tr("Not found in %1").arg(recent::folderOf(p));
		auto* folder = label(where, "dim", Qt::ElideMiddle);
		cv->addWidget(folder);
		connect(c, &QPushButton::clicked, this, [this, p] { choose(Recent, -1, p); });
		rv->addWidget(c);
	}
	if (paths.isEmpty()) {
		auto* none = new QLabel(tr("The songs you open or save will be listed here."));
		none->setObjectName("dim");
		none->setWordWrap(true);
		rv->addWidget(none);
	}
	rv->addStretch(1);
	auto* open = new QPushButton(tr("Open…"));
	open->setToolTip(tr("Choose a song file (.nachtzug) to open"));
	connect(open, &QPushButton::clicked, this, [this] { choose(OpenFile); });
	rv->addWidget(open);
	row->addWidget(right);
	v->addLayout(row, 1);

	note = new QLabel(tr("Click a song to open it, or ▶ to listen first."));
	note->setObjectName("dim");
	v->addWidget(note);

	auto* foot = new QHBoxLayout;
	foot->setSpacing(8);
	auto* atStart = new Switch(tr("Show this window when Nachtzug starts"), Switch::Beside);
	atStart->setToolTip(tr("Also in Settings → General. File → New from a starter song… opens it any time."));
	atStart->setChecked(QSettings().value(QStringLiteral("startWindow"), true).toBool());
	connect(atStart, &QAbstractButton::toggled, this,
	        [](bool on) { QSettings().setValue(QStringLiteral("startWindow"), on); });
	foot->addWidget(atStart);
	foot->addStretch(1);
	auto* empty = new QPushButton(tr("Empty song"));
	empty->setToolTip(tr("A new song with nothing in it yet"));
	connect(empty, &QPushButton::clicked, this, [this] { choose(Empty); });
	foot->addWidget(empty);
	auto* close = new QPushButton(tr("Close"));
	close->setToolTip(tr("Carry on with the song that is open"));
	connect(close, &QPushButton::clicked, this, &QDialog::reject);
	foot->addWidget(close);
	v->addLayout(foot);
	theme::dressSymbolKeys(this);
	close->setFocus();

	endTimer = new QTimer(this);
	endTimer->setSingleShot(true);
	connect(endTimer, &QTimer::timeout, this, [this] { stopListening(); });
	resize(900, sizeHint().height());
}

StartWindow::~StartWindow() {
	if (worker) { // a render still running: let it end, then tidy up after it
		worker->wait();
		delete worker;
		QFile::remove(renderFile);
	}
}

void StartWindow::hideEvent(QHideEvent* e) {
	stopListening();
	QDialog::hideEvent(e);
}

void StartWindow::choose(Choice c, int starter, const QString& path) {
	stopListening();
	choice_ = c;
	starter_ = starter;
	path_ = path;
	accept();
}

void StartWindow::stopListening() {
	waiting = -1;
	if (playing >= 0) app->previewData(nullptr);
	playing = -1;
	endTimer->stop();
	for (QPushButton* k : listenKeys)
		if (k) k->setChecked(false);
	note->setText(tr("Click a song to open it, or ▶ to listen first."));
}

void StartWindow::listen(int i) {
	const bool again = playing == i || waiting == i;
	stopListening();
	if (again) return;
	if (!app->audioRunning()) { // nothing would be heard: say so, rather than seem to play in silence
		note->setText(tr("No sound output is open, so nothing can be heard. Settings → Audio picks one."));
		return;
	}
	app->stop(); // the song that is open would play over it
	const QString name = ui(starters()[size_t(i)].name);
	listenKeys[size_t(i)]->setChecked(true);
	if (const auto& d = heard[size_t(i)]) {
		app->previewData(d);
		playing = i;
		note->setText(tr("Listening to “%1”. Click it to open it.").arg(name));
		endTimer->start(int(1000.0 * double(d->frames()) / double(d->rate)) + 100);
		return;
	}
	waiting = i;
	note->setText(tr("Getting “%1” ready to listen to…").arg(name));
	if (!worker) render(i); // otherwise once the one being rendered is ready
}

// a few bars from the starter's first main part, faded out at the end, written on a thread of its own
void StartWindow::render(int i) {
	Song s = songs[size_t(i)];
	s.loopOn = true;
	s.loopEnd = s.loopStart + 8 * BAR;
	renderFile = QDir::temp().filePath(QStringLiteral("nachtzug-starter-%1-%2.wav")
	                                       .arg(QLatin1String(starters()[size_t(i)].id))
	                                       .arg(QCoreApplication::applicationPid()));
	const QString file = renderFile;
	worker = QThread::create([s, file] {
		RenderOptions o;
		o.loopOnly = true;
		o.fadeOut = true;
		o.maxTail = 0.0;
		renderSong(s, file, o, {});
	});
	connect(worker, &QThread::finished, this, [this, i, file] {
		worker->deleteLater();
		worker = nullptr;
		renderFile.clear();
		rendered(i, file);
	});
	worker->start(QThread::LowPriority);
}

void StartWindow::rendered(int i, const QString& file) {
	auto d = std::make_shared<dsp::SampleData>();
	QString err;
	if (samples::readFile(file, *d, err)) heard[size_t(i)] = d;
	QFile::remove(file);
	if (waiting < 0) return;
	if (waiting == i) {
		waiting = -1;
		if (heard[size_t(i)]) {
			listenKeys[size_t(i)]->setChecked(false);
			listen(i);
		} else
			stopListening();
	} else if (!heard[size_t(waiting)])
		render(waiting); // another was asked for meanwhile
	else
		listen(std::exchange(waiting, -1));
}
