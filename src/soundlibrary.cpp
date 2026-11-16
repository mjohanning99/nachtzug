// The sound library: see soundlibrary.h.
#include "soundlibrary.h"
#include "app.h"
#include "widgets.h"
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QKeyEvent>
#include <QFileInfo>
#include <QFrame>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

namespace {
// a card in the middle: clicking anywhere on it chooses the sound
class Card : public QFrame {
public:
	explicit Card(std::function<void()> chosen) : chosen_(std::move(chosen)) { setObjectName("soundCard"); }

protected:
	void mousePressEvent(QMouseEvent*) override { chosen_(); }
	void mouseDoubleClickEvent(QMouseEvent*) override { chosen_(); }

private:
	std::function<void()> chosen_;
};
} // namespace

SoundLibrary::SoundLibrary(App* a, std::function<void(const mysounds::Sound&)> u, QWidget* parent)
    : QDialog(parent), app(a), use(std::move(u)) {
	setWindowTitle(tr("Sound library"));
	setAcceptDrops(true);
	resize(900, 520);
	auto* row = new QHBoxLayout(this);
	row->setContentsMargins(16, 14, 16, 14);
	row->setSpacing(14);

	// the shelves: keys down the left side
	auto* left = new QVBoxLayout;
	left->setSpacing(6);
	auto* only = new QButtonGroup(this); // one shelf at a time
	const QString names[ShelfCount] = {tr("My sounds"),
	                                   tr("Starred"),
	                                   ui(mysounds::groupName(0)),
	                                   ui(mysounds::groupName(1)),
	                                   ui(mysounds::groupName(2)),
	                                   ui(mysounds::groupName(3)),
	                                   tr("Ready-made")};
	for (int i = 0; i < ShelfCount; i++) {
		auto* k = new QPushButton(names[i]);
		k->setCheckable(true);
		only->addButton(k);
		k->setChecked(i == shelf);
		k->setMinimumWidth(150);
		connect(k, &QPushButton::clicked, this, [this, i] {
			shelf = i;
			showCards();
		});
		shelfKeys[i] = k;
		left->addWidget(k);
		if (i == Starred || i == GroupLead) left->addSpacing(8);
	}
	left->addStretch(1);
	auto* in = new QPushButton(tr("Bring in a file…"));
	in->setToolTip(tr("A sound someone shared with you (a .nzsound file); files can also be dropped on this window"));
	connect(in, &QPushButton::clicked, this, [this] { bringIn(); });
	left->addWidget(in);
	row->addLayout(left);

	// the cards
	auto* mid = new QVBoxLayout;
	mid->setSpacing(8);
	search = new QLineEdit;
	search->setPlaceholderText(tr("Search by name or word"));
	search->setClearButtonEnabled(true);
	search->installEventFilter(this);
	connect(search, &QLineEdit::textChanged, this, [this] { showCards(); });
	mid->addWidget(search);
	auto* scroll = new QScrollArea;
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	auto* holder = new QWidget;
	cards = new QVBoxLayout(holder);
	cards->setContentsMargins(0, 0, 6, 0);
	cards->setSpacing(6);
	scroll->setWidget(holder);
	mid->addWidget(scroll, 1);
	row->addLayout(mid, 1);

	// the chosen sound
	auto* right = new QFrame;
	right->setObjectName("soundChosen");
	right->setFixedWidth(250);
	auto* rv = new QVBoxLayout(right);
	rv->setContentsMargins(14, 12, 14, 12);
	rv->setSpacing(6);
	chosenGroup = new QLabel;
	chosenGroup->setObjectName("cap");
	chosenName = new QLabel;
	chosenName->setObjectName("chosenName");
	chosenName->setWordWrap(true);
	chosenWords = new QLabel;
	chosenWords->setObjectName("dim");
	chosenWords->setWordWrap(true);
	chosenNote = new QLabel;
	chosenNote->setObjectName("dim");
	chosenNote->setWordWrap(true);
	for (QWidget* w :
	     {static_cast<QWidget*>(chosenGroup), static_cast<QWidget*>(chosenName), static_cast<QWidget*>(chosenWords)})
		rv->addWidget(w);
	rv->addStretch(1);
	useKey = new QPushButton(tr("Use on this track"));
	useKey->setObjectName("primary");
	starKey = new QPushButton;
	starKey->setCheckable(true);
	renameKey = new QPushButton(tr("Rename…"));
	shareKey = new QPushButton(tr("Share as a file…"));
	deleteKey = new QPushButton(tr("Delete"));
	for (QPushButton* k : {useKey, starKey, renameKey, shareKey, deleteKey})
		rv->addWidget(k);
	rv->addWidget(chosenNote);
	row->addWidget(right);

	connect(useKey, &QPushButton::clicked, this, [this] {
		if (chosen < 0) return;
		use(entries[size_t(chosen)].sound);
		if (!entries[size_t(chosen)].builtIn) mysounds::noteUsed(entries[size_t(chosen)].sound);
		accept();
	});
	connect(starKey, &QPushButton::clicked, this, [this](bool on) {
		if (chosen < 0 || entries[size_t(chosen)].builtIn) return;
		mysounds::setStarred(entries[size_t(chosen)].sound, on);
		showCards();
	});
	connect(renameKey, &QPushButton::clicked, this, [this] {
		if (chosen < 0 || entries[size_t(chosen)].builtIn) return;
		mysounds::Sound& s = entries[size_t(chosen)].sound;
		bool ok = false;
		const QString name = QInputDialog::getText(this, tr("Rename"), tr("New name"), QLineEdit::Normal, s.name, &ok);
		QString error;
		if (ok && !mysounds::rename(s, name, &error) && !error.isEmpty())
			QMessageBox::warning(this, windowTitle(), error);
		showCards();
	});
	connect(shareKey, &QPushButton::clicked, this, [this] {
		if (chosen < 0) return;
		mysounds::Sound s = entries[size_t(chosen)].sound;
		const QString to = QFileDialog::getSaveFileName(
		    this, tr("Share as a file"), s.name + QStringLiteral(".nzsound"), tr("Nachtzug sounds (*.nzsound)"));
		if (to.isEmpty()) return;
		QString error; // written where asked, as one file
		if (!mysounds::writeTo(s, to, &error)) QMessageBox::warning(this, windowTitle(), error);
	});
	connect(deleteKey, &QPushButton::clicked, this, [this] {
		if (chosen < 0 || entries[size_t(chosen)].builtIn) return;
		const mysounds::Sound& s = entries[size_t(chosen)].sound;
		if (QMessageBox::question(this, windowTitle(),
		                          tr("Delete “%1”? Songs that use it keep their copy.").arg(s.name)) !=
		    QMessageBox::Yes)
			return;
		mysounds::remove(s);
		chosen = -1;
		reload();
	});
	reload();
	if (std::none_of(entries.begin(), entries.end(), [](const Entry& e) { return !e.builtIn; })) {
		shelf = BuiltIn; // nothing of your own yet: the ready-made ones
		shelfKeys[BuiltIn]->setChecked(true);
		showCards();
	}
}

void SoundLibrary::reload() {
	const QString was =
	    chosen >= 0 ? entries[size_t(chosen)].sound.file + entries[size_t(chosen)].sound.name : QString();
	entries.clear();
	for (const mysounds::Sound& s : mysounds::all())
		entries.push_back({s, false});
	for (const Patch& p : presets()) {
		Entry e;
		e.builtIn = true;
		e.sound.name = ui(p.name);
		e.sound.patch = p;
		const QString g = presetGroup(p.name);
		e.sound.group = g == QLatin1String("Bass")   ? mysounds::Bass
		                : g == QLatin1String("Pad")  ? mysounds::Pad
		                : g == QLatin1String("Lead") ? mysounds::Lead
		                                             : mysounds::Sequence;
		entries.push_back(e);
	}
	chosen = -1;
	for (size_t i = 0; i < entries.size(); i++)
		if (!was.isEmpty() && entries[i].sound.file + entries[i].sound.name == was) chosen = int(i);
	showCards();
}

bool SoundLibrary::onShelf(const Entry& e) const {
	switch (shelf) {
	case Mine:
		return !e.builtIn;
	case Starred:
		return !e.builtIn && e.sound.starred;
	case BuiltIn:
		return e.builtIn;
	default:
		return e.sound.group == shelf - GroupBass;
	}
}

void SoundLibrary::showCards() {
	while (QLayoutItem* it = cards->takeAt(0)) {
		delete it->widget();
		delete it;
	}
	const QString q = search->text().trimmed();
	shown.clear();
	for (size_t i = 0; i < entries.size(); i++) {
		const Entry& e = entries[i];
		if (!onShelf(e)) continue;
		if (!q.isEmpty() && !e.sound.name.contains(q, Qt::CaseInsensitive) &&
		    !e.sound.words.join(' ').contains(q, Qt::CaseInsensitive))
			continue;
		auto* card = new Card([this, i] {
			chosen = int(i);
			showCards();
		});
		card->setProperty("on", int(i) == chosen);
		auto* h = new QHBoxLayout(card);
		h->setContentsMargins(8, 6, 10, 6);
		h->setSpacing(10);
		auto* play = new QPushButton;
		play->setFixedSize(30, 26);
		play->setToolTip(tr("Listen: a short phrase with this sound on the selected track; nothing is changed"));
		theme::symbolKey(play, QStringLiteral("play"), tr("Listen"), true);
		connect(play, &QPushButton::clicked, this, [this, i] { listen(entries[i]); });
		h->addWidget(play);
		auto* name = new QLabel(e.sound.name + (e.sound.starred ? QStringLiteral("  ★") : QString()));
		name->setObjectName("cardName");
		h->addWidget(name, 1);
		auto* words = new QLabel(e.builtIn ? tr("ready-made") : e.sound.words.join(QStringLiteral(", ")));
		words->setObjectName("dim");
		h->addWidget(words);
		cards->addWidget(card);
		shown.push_back(int(i));
	}
	if (shown.empty()) {
		auto* none = new QLabel(shelf == Mine || shelf == Starred
		                            ? tr("No sounds of your own here yet. “Save sound…” on the Sound tab keeps one.")
		                            : tr("Nothing found."));
		none->setObjectName("dim");
		none->setWordWrap(true);
		cards->addWidget(none);
	}
	cards->addStretch(1);
	theme::dressSymbolKeys(this);
	showChosen();
}

void SoundLibrary::showChosen() {
	const bool any = chosen >= 0;
	const Entry* e = any ? &entries[size_t(chosen)] : nullptr;
	chosenGroup->setText(any ? ui(mysounds::groupName(e->sound.group)).toUpper() +
	                               (e->builtIn ? QStringLiteral(" · ") + tr("ready-made") : QString())
	                         : QString());
	chosenName->setText(any ? e->sound.name : tr("Choose a sound"));
	chosenWords->setText(any && !e->builtIn ? e->sound.words.join(QStringLiteral(", ")) : QString());
	chosenNote->setText(any && e->builtIn ? tr("Ready-made sounds can be used and shared; save a changed one with "
	                                           "“Save sound…” to keep it as your own.")
	                                      : QString());
	useKey->setEnabled(any);
	shareKey->setEnabled(any);
	for (QPushButton* k : {starKey, renameKey, deleteKey})
		k->setEnabled(any && !e->builtIn);
	starKey->setChecked(any && e->sound.starred);
	starKey->setText(any && e->sound.starred ? tr("★ Starred") : tr("☆ Star it"));
}

void SoundLibrary::listen(const Entry& e) {
	app->previewSound(&e.sound.patch, nullptr);
	QTimer::singleShot(2600, this, [this] { app->previewSound(nullptr, nullptr); });
}

void SoundLibrary::bringIn() {
	const QStringList files =
	    QFileDialog::getOpenFileNames(this, tr("Bring in a sound"), QString(), tr("Nachtzug sounds (*.nzsound)"));
	QString errors;
	for (const QString& f : files) {
		QString error;
		if (!mysounds::bringIn(f, nullptr, &error)) errors += error + '\n';
	}
	if (!errors.isEmpty()) QMessageBox::warning(this, windowTitle(), errors.trimmed());
	shelf = Mine;
	shelfKeys[Mine]->setChecked(true);
	reload();
}

void SoundLibrary::dragEnterEvent(QDragEnterEvent* e) {
	if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void SoundLibrary::dropEvent(QDropEvent* e) {
	QString errors;
	for (const QUrl& u : e->mimeData()->urls()) {
		QString error;
		if (u.isLocalFile() && !mysounds::bringIn(u.toLocalFile(), nullptr, &error)) errors += error + '\n';
	}
	if (!errors.isEmpty()) QMessageBox::warning(this, windowTitle(), errors.trimmed());
	shelf = Mine;
	shelfKeys[Mine]->setChecked(true);
	reload();
}

void SoundLibrary::step(int by) {
	if (shown.empty()) return;
	const auto at = std::find(shown.begin(), shown.end(), chosen);
	const int pos = at == shown.end() ? (by > 0 ? 0 : int(shown.size()) - 1)
	                                  : std::clamp(int(at - shown.begin()) + by, 0, int(shown.size()) - 1);
	chosen = shown[size_t(pos)];
	showCards();
}

bool SoundLibrary::eventFilter(QObject* o, QEvent* e) {
	if (o == search && e->type() == QEvent::KeyPress) {
		const int k = static_cast<QKeyEvent*>(e)->key();
		if (k == Qt::Key_Down || k == Qt::Key_Up) {
			step(k == Qt::Key_Down ? 1 : -1);
			return true;
		}
		if ((k == Qt::Key_Return || k == Qt::Key_Enter) && chosen >= 0) {
			useKey->click();
			return true;
		}
	}
	return QDialog::eventFilter(o, e);
}

void SoundLibrary::keyPressEvent(QKeyEvent* e) {
	switch (e->key()) {
	case Qt::Key_Down:
		step(1);
		return;
	case Qt::Key_Up:
		step(-1);
		return;
	case Qt::Key_Space:
		if (chosen >= 0) listen(entries[size_t(chosen)]);
		return;
	case Qt::Key_Return:
	case Qt::Key_Enter:
		if (chosen >= 0) useKey->click();
		return;
	default:
		QDialog::keyPressEvent(e);
	}
}

void SoundLibrary::hideEvent(QHideEvent* e) {
	app->previewSound(nullptr, nullptr);
	QDialog::hideEvent(e);
}
