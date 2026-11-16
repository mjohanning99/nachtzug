// The first time Nachtzug starts: pick the language, say whether you are new to this (the simple sound page and the
// guide) or know your way around synthesisers (every knob), and how the window looks: the keys with their words or
// symbols only, the timeline above or beside the notes, scroll bars or none. All can be changed later in Settings.
#pragma once
#include <QDialog>
#include <functional>

class QComboBox;
class QLabel;
class QPushButton;

class WelcomeDialog : public QDialog {
	Q_OBJECT
public:
	// setLanguage loads the translation for a language code ("" = the system's) so the window can switch at once
	WelcomeDialog(std::function<void(const QString&)> setLanguage, QWidget* parent = nullptr);
	bool beginner() const { return newcomer; }

protected:
	void changeEvent(QEvent* e) override;

private:
	void retranslate();
	std::function<void(const QString&)> setLanguage;
	QComboBox* lang;
	QLabel *title, *intro, *question, *note, *langLabel;
	QPushButton *newBtn, *proBtn, *go;
	// the window's look: each a pair of joined keys, the chosen one lit
	QLabel *lookTitle, *keysLabel, *layoutLabel, *barsLabel;
	QPushButton *keysWords, *keysSymbols, *layoutTop, *layoutBeside, *barsHidden, *barsShown;
	bool newcomer = true;
};
