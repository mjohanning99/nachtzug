// The first-start window: see welcome.h.
//
// What's here: WelcomeDialog, shown by main() the first time Nachtzug starts (and for its picture); its choices are
// written to the settings when it closes.
// Changes in the song file: none.

#include "welcome.h"
#include "widgets.h"
#include <QComboBox>
#include <QEvent>
#include <QGridLayout>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

// a big choice with a heading and a line under it
static QPushButton* card() {
	auto* b = new QPushButton;
	b->setCheckable(true);
	b->setObjectName("card");
	b->setMinimumHeight(92);
	b->setFocusPolicy(Qt::StrongFocus);
	auto* l = new QVBoxLayout(b);
	l->setContentsMargins(16, 10, 16, 10);
	l->setSpacing(4);
	auto* head = new QLabel;
	head->setObjectName("cardHead");
	head->setAttribute(Qt::WA_TransparentForMouseEvents);
	auto* text = new QLabel;
	text->setObjectName("cardText");
	text->setWordWrap(true);
	text->setAttribute(Qt::WA_TransparentForMouseEvents);
	text->setAlignment(Qt::AlignLeft | Qt::AlignTop);
	l->addWidget(head);
	l->addWidget(text, 1);
	return b;
}
static void setCard(QPushButton* b, const QString& head, const QString& text) {
	b->findChild<QLabel*>(QStringLiteral("cardHead"))->setText(head);
	b->findChild<QLabel*>(QStringLiteral("cardText"))->setText(text);
	b->setAccessibleName(head);
	b->setAccessibleDescription(text);
}

WelcomeDialog::WelcomeDialog(std::function<void(const QString&)> set, QWidget* parent)
    : QDialog(parent), setLanguage(std::move(set)) {
	setFixedWidth(640);
	auto* v = new QVBoxLayout(this);
	v->setContentsMargins(26, 22, 26, 18);
	v->setSpacing(14);
	// the heading across the top, then the sign level with the words beside it
	title = new QLabel;
	title->setTextFormat(Qt::RichText);
	v->addWidget(title);
	auto* head = new QHBoxLayout;
	head->setSpacing(18);
	auto* mark = new QLabel;
	mark->setPixmap(theme::logo(192));
	mark->setScaledContents(true);
	mark->setFixedSize(84, 84);
	head->addWidget(mark, 0, Qt::AlignVCenter);
	intro = new QLabel;
	intro->setWordWrap(true);
	head->addWidget(intro, 1, Qt::AlignVCenter);
	v->addLayout(head);

	auto* lr = new QHBoxLayout;
	lr->setSpacing(10);
	langLabel = new QLabel;
	langLabel->setObjectName("cap");
	lang = new QComboBox;
	lang->addItem(QString(), QString());
	lang->addItem(QStringLiteral("English"), QStringLiteral("en"));
	lang->addItem(QStringLiteral("Deutsch"), QStringLiteral("de"));
	lang->setCurrentIndex(std::max(0, lang->findData(QSettings().value(QStringLiteral("language")).toString())));
	lr->addWidget(langLabel);
	lr->addWidget(lang);
	lr->addStretch(1);
	v->addLayout(lr);

	question = new QLabel;
	question->setObjectName("title");
	v->addWidget(question);
	auto* cards = new QHBoxLayout;
	cards->setSpacing(12);
	newBtn = card();
	proBtn = card();
	newBtn->setChecked(true);
	cards->addWidget(newBtn, 1);
	cards->addWidget(proBtn, 1);
	v->addLayout(cards);
	// how the window looks: four choices, each two joined keys
	lookTitle = new QLabel;
	lookTitle->setObjectName("title");
	v->addWidget(lookTitle);
	auto* look = new QGridLayout;
	look->setHorizontalSpacing(12);
	look->setVerticalSpacing(8);
	auto pair = [&](int row, QLabel*& label, QPushButton*& a, QPushButton*& b, bool firstOn) {
		label = new QLabel;
		label->setObjectName("cap");
		a = new QPushButton;
		b = new QPushButton;
		a->setObjectName("segL");
		b->setObjectName("segR");
		auto* both = new QHBoxLayout;
		both->setSpacing(0);
		// a group of its own for each pair: keys left to group themselves join every such key in the window, so
		// choosing in one row took the choice away from the others
		auto* only = new QButtonGroup(this);
		for (QPushButton* k : {a, b}) {
			k->setCheckable(true);
			only->addButton(k);
			k->setFocusPolicy(Qt::StrongFocus);
			k->setMinimumWidth(190);
			both->addWidget(k);
		}
		both->addStretch(1);
		(firstOn ? a : b)->setChecked(true);
		look->addWidget(label, row, 0, Qt::AlignVCenter);
		look->addLayout(both, row, 1);
	};
	QSettings st;
	pair(0, keysLabel, keysWords, keysSymbols, st.value(QStringLiteral("keys")).toString() != QLatin1String("symbols"));
	pair(1, layoutLabel, layoutTop, layoutBeside, st.value(QStringLiteral("layout/timelineTop"), true).toBool());
	pair(2, barsLabel, barsHidden, barsShown, !st.value(QStringLiteral("scrollBars"), false).toBool());
	pair(3, cursorsLabel, cursorsOwn, cursorsSystem, st.value(QStringLiteral("ownCursors"), true).toBool());
	look->setColumnStretch(1, 1);
	v->addLayout(look);
	note = new QLabel;
	note->setObjectName("dim");
	note->setWordWrap(true);
	v->addWidget(note);
	auto* buttons = new QHBoxLayout;
	buttons->addStretch(1);
	go = new QPushButton;
	go->setObjectName("primary");
	go->setDefault(true);
	go->setMinimumWidth(200);
	buttons->addWidget(go);
	v->addLayout(buttons);

	connect(newBtn, &QPushButton::clicked, this, [this] {
		newcomer = true;
		newBtn->setChecked(true);
		proBtn->setChecked(false);
	});
	connect(proBtn, &QPushButton::clicked, this, [this] {
		newcomer = false;
		proBtn->setChecked(true);
		newBtn->setChecked(false);
	});
	connect(lang, &QComboBox::activated, this, [this] {
		const QString code = lang->currentData().toString();
		QSettings().setValue(QStringLiteral("language"), code);
		if (setLanguage)
			setLanguage(code); // the translators send a LanguageChange event, and changeEvent redoes the words
	});
	connect(go, &QPushButton::clicked, this, [this] {
		QSettings st;
		st.setValue(QStringLiteral("welcomed"), true);
		st.setValue(QStringLiteral("simpleSound"), newcomer);
		st.setValue(QStringLiteral("seenGuide"), !newcomer); // newcomers get the short guide once the window is open
		st.setValue(QStringLiteral("keys"),
		            keysSymbols->isChecked() ? QStringLiteral("symbols") : QStringLiteral("words"));
		st.setValue(QStringLiteral("layout/timelineTop"), layoutTop->isChecked());
		st.setValue(QStringLiteral("scrollBars"), barsShown->isChecked());
		st.setValue(QStringLiteral("ownCursors"), cursorsOwn->isChecked());
		accept();
	});
	retranslate();
}

void WelcomeDialog::changeEvent(QEvent* e) {
	if (e->type() == QEvent::LanguageChange) retranslate();
	QDialog::changeEvent(e);
}

void WelcomeDialog::retranslate() {
	setWindowTitle(tr("Welcome to Nachtzug"));
	// the name without letter-spacing, which would switch its tz ligature off
	title->setText(tr("<span style=\"font-size:22px; font-weight:600; letter-spacing:2px\">Welcome aboard </span>"
	                  "<span style=\"font-size:22px; font-weight:600\">Nachtzug</span>"));
	intro->setText(
	    tr("Nachtzug is a music program made for beginners. It lets you create electronic "
		   "music, in the style of the Berlin School for example, without much prior knowledge. You don’t need "
		   "to know any music theory either: Nachtzug is built so that every note automatically lands in the "
		   "right scale."));
	langLabel->setText(tr("Language"));
	lang->setItemText(0, tr("Same as the system"));
	question->setText(tr("How would you like to start?"));
	setCard(newBtn, tr("I am new to this"),
	        tr("The Sound tab shows a few big knobs in plain words, and a short guide opens to show you around."));
	setCard(proBtn, tr("I know synthesisers"),
	        tr("The Sound tab shows every knob of the synthesiser. The guide waits in the Help menu."));
	lookTitle->setText(tr("How should the window look?"));
	keysLabel->setText(tr("Keys"));
	keysWords->setText(tr("A symbol and a word"));
	keysSymbols->setText(tr("Symbols only"));
	layoutLabel->setText(tr("Timeline"));
	layoutTop->setText(tr("Above the notes"));
	layoutBeside->setText(tr("Beside the notes"));
	barsLabel->setText(tr("Scroll bars"));
	barsHidden->setText(tr("Hidden"));
	barsShown->setText(tr("Shown"));
	cursorsLabel->setText(tr("Cursors"));
	cursorsOwn->setText(tr("Nachtzug’s own"));
	cursorsOwn->setToolTip(tr("Where the cursor can show what a click will do, Nachtzug draws its own: a pencil in the "
	                          "note grid, scissors, a knob’s arrows"));
	cursorsSystem->setText(tr("The system’s"));
	cursorsSystem->setToolTip(tr("The cursors of your computer, everywhere"));
	note->setText(tr("You can change all of these any time in Settings (the ⚙ key at the top right)."));
	go->setText(tr("Start making music"));
	if (layout())
		setFixedHeight(layout()->totalHeightForWidth(width())); // as tall as the words need in this language, no gaps
}
