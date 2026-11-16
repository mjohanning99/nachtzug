// The inspector: the selected track at a glance, in a column beside the timeline. Its sections fold shut when their
// names are clicked: the track's level and sends, its sound (with the knobs turned most), how its notes are played,
// and for drum clips the auto drummer.
#pragma once
#include <QScrollArea>

class App;
class EditorPanel;
class Knob;
class QLabel;
class QPushButton;
class QSlider;
class QVBoxLayout;
class SoundPanel;

class Inspector : public QScrollArea {
	Q_OBJECT
public:
	Inspector(App* app, SoundPanel* sound, EditorPanel* editor, QWidget* parent = nullptr);
	void adopt(bool on); // take the How notes and drummer sections in (on), or give them back to their tabs

private:
	void alignForms(); // the label columns of the sections above each other, one width, so their boxes line up
	QSize sizeHint() const override { return {300, 600}; }
signals:
	void hideRequested();
public slots:
	void sync();

private:
	App* app;
	SoundPanel* sound;
	EditorPanel* editor;
	QVBoxLayout* col;
	QLabel* head;
	QSlider* volume;
	Knob *pan, *echo, *reverb, *pump;
	QPushButton *mute, *solo;
	bool adopted = false;
};
