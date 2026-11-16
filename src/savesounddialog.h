// "Save sound…": the sound on the Sound tab kept under a name as one of your own sounds (see mysounds.h): its name,
// its group (the track's suggested), words to find it by, and a star to show it in the preset drop-down.
#pragma once
#include "mysounds.h"
#include <QDialog>

class QLineEdit;
class QPushButton;
class Switch;

class SaveSoundDialog : public QDialog {
	Q_OBJECT
public:
	SaveSoundDialog(const Patch& patch, int group, QWidget* parent = nullptr);
	const mysounds::Sound& saved() const { return sound; } // after it was accepted

private:
	void trySave();
	mysounds::Sound sound;
	QLineEdit *name, *words;
	QPushButton* groupKeys[mysounds::GroupCount];
	Switch* star;
};
