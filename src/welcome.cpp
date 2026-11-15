#include "welcome.h"
#include "widgets.h"
#include <QComboBox>
#include <QEvent>
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
    l->setContentsMargins(16, 10, 16, 10); l->setSpacing(4);
    auto* head = new QLabel; head->setObjectName("cardHead"); head->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* text = new QLabel; text->setObjectName("cardText"); text->setWordWrap(true); text->setAttribute(Qt::WA_TransparentForMouseEvents);
    text->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    l->addWidget(head); l->addWidget(text, 1);
    return b;
}
static void setCard(QPushButton* b, const QString& head, const QString& text) {
    b->findChild<QLabel*>(QStringLiteral("cardHead"))->setText(head);
    b->findChild<QLabel*>(QStringLiteral("cardText"))->setText(text);
    b->setAccessibleName(head);
    b->setAccessibleDescription(text);
}

WelcomeDialog::WelcomeDialog(std::function<void(const QString&)> set, QWidget* parent) : QDialog(parent), setLanguage(std::move(set)) {
    setFixedWidth(640);
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(26, 22, 26, 18); v->setSpacing(14);
    auto* head = new QHBoxLayout; head->setSpacing(18);
    auto* mark = new QLabel; mark->setPixmap(theme::logo(192)); mark->setScaledContents(true); mark->setFixedSize(84, 84);
    head->addWidget(mark, 0, Qt::AlignTop);
    auto* words = new QVBoxLayout; words->setSpacing(6);
    title = new QLabel; title->setTextFormat(Qt::RichText);
    intro = new QLabel; intro->setWordWrap(true);
    words->addWidget(title); words->addWidget(intro); words->addStretch(1);
    head->addLayout(words, 1);
    v->addLayout(head);

    auto* lr = new QHBoxLayout; lr->setSpacing(10);
    langLabel = new QLabel; langLabel->setObjectName("cap");
    lang = new QComboBox;
    lang->addItem(QString(), QString());
    lang->addItem(QStringLiteral("English"), QStringLiteral("en"));
    lang->addItem(QStringLiteral("Deutsch"), QStringLiteral("de"));
    lang->setCurrentIndex(std::max(0, lang->findData(QSettings().value(QStringLiteral("language")).toString())));
    lr->addWidget(langLabel); lr->addWidget(lang); lr->addStretch(1);
    v->addLayout(lr);

    question = new QLabel; question->setObjectName("title");
    v->addWidget(question);
    auto* cards = new QHBoxLayout; cards->setSpacing(12);
    newBtn = card(); proBtn = card();
    newBtn->setChecked(true);
    cards->addWidget(newBtn, 1); cards->addWidget(proBtn, 1);
    v->addLayout(cards);
    note = new QLabel; note->setObjectName("dim"); note->setWordWrap(true);
    v->addWidget(note);
    auto* buttons = new QHBoxLayout; buttons->addStretch(1);
    go = new QPushButton; go->setObjectName("primary"); go->setDefault(true); go->setMinimumWidth(200);
    buttons->addWidget(go);
    v->addLayout(buttons);

    connect(newBtn, &QPushButton::clicked, this, [this] { newcomer = true; newBtn->setChecked(true); proBtn->setChecked(false); });
    connect(proBtn, &QPushButton::clicked, this, [this] { newcomer = false; proBtn->setChecked(true); newBtn->setChecked(false); });
    connect(lang, &QComboBox::activated, this, [this] {
        const QString code = lang->currentData().toString();
        QSettings().setValue(QStringLiteral("language"), code);
        if (setLanguage) setLanguage(code);   // the translators send a LanguageChange event, and changeEvent redoes the words
    });
    connect(go, &QPushButton::clicked, this, [this] {
        QSettings st;
        st.setValue(QStringLiteral("welcomed"), true);
        st.setValue(QStringLiteral("simpleSound"), newcomer);
        st.setValue(QStringLiteral("seenGuide"), !newcomer);   // newcomers get the short guide once the window is open
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
    title->setText(tr("<span style=\"font-size:22px; font-weight:700; letter-spacing:2px\">Welcome aboard</span>"));
    intro->setText(tr("Nachtzug makes acid and Berlin School music: bass lines, sequences, pads and drums that play by themselves. "
                      "Every note stays in the song’s key, so nothing can sound wrong."));
    langLabel->setText(tr("Language"));
    lang->setItemText(0, tr("Same as the system"));
    question->setText(tr("How would you like to start?"));
    setCard(newBtn, tr("I am new to this"),
            tr("The Sound tab shows a few big knobs in plain words, and a short guide opens to show you around."));
    setCard(proBtn, tr("I know synthesisers"),
            tr("The Sound tab shows every knob of the synthesiser. The guide waits in the Help menu."));
    note->setText(tr("You can change both any time: Settings (the ⚙ key at the top right), or View → Simple sound page."));
    go->setText(tr("Start making music"));
    if (layout()) setFixedHeight(layout()->totalHeightForWidth(width()));   // as tall as the words need in this language, no gaps
}
