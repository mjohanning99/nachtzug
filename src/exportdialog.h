// Export: a small dialog for the kind of file (one sound file, a sound file per track, or a MIDI file), what to
// write (whole song or the loop), how it ends, quality and where; then the progress, and a last window that says
// the song is ready, with buttons to play it or show it in its folder.
#pragma once
#include <QDialog>

class App;
class QButtonGroup;
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
struct RenderOptions;

class ExportDialog : public QDialog {
	Q_OBJECT
public:
	explicit ExportDialog(App* app, QWidget* parent);
	static void run(App* app, QWidget* parent); // the whole export, from asking to the finished window
private:
	enum Kind { Wav, Stems, Midi };
	RenderOptions options() const;
	Kind kind() const;
	void updateInfo();
	void choosePath();
	void kindChanged();
	QString baseName() const;
	App* app;
	QButtonGroup *format, *what, *ending;
	QWidget *endingBox, *qualityBox;
	QLabel *endingCap, *qualityCap, *head;
	QString lastDir;
	QSpinBox* repeats;
	QComboBox* quality;
	QLineEdit* path;
	QLabel *info, *wholeText, *loopText;
};
