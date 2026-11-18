// The start window: what to play when Nachtzug opens. A starter song for each style (each with a key to listen
// first), the songs opened or saved lately, an empty song, and Open…. A starter opens as a new, untitled song, so
// Save asks for a name and the starter itself is never changed. Shown at start (unless switched off, or the
// first-start window or a song to bring back came first) and from File > New from a starter song….
#pragma once
#include "model.h"
#include <QDialog>
#include <QString>
#include <memory>
#include <vector>

class App;
class QLabel;
class QPushButton;
class QThread;
class QTimer;
namespace dsp {
struct SampleData;
}

class StartWindow : public QDialog {
	Q_OBJECT
public:
	StartWindow(App* app, QWidget* parent = nullptr);
	~StartWindow() override;
	enum Choice { Nothing, Starter, Empty, Recent, OpenFile };
	Choice choice() const { return choice_; }
	int starter() const { return starter_; }     // with Starter: which one (an index into starters())
	QString recentPath() const { return path_; } // with Recent: the song

protected:
	void hideEvent(QHideEvent* e) override; // stops listening

private:
	void choose(Choice c, int starter = -1, const QString& path = QString());
	void listen(int i); // plays a few bars of the starter's main part; again: stops
	void stopListening();
	void render(int i);                        // starts rendering those bars on a thread of its own
	void rendered(int i, const QString& file); // the bars are ready (on the GUI thread)
	App* app;
	Choice choice_ = Nothing;
	int starter_ = -1;
	QString path_;
	std::vector<Song> songs; // the starters, as they come with the program
	std::vector<QPushButton*> listenKeys;
	std::vector<std::shared_ptr<const dsp::SampleData>> heard; // the bars of each starter, once rendered
	int playing = -1, waiting = -1;                            // the starter heard now, the one being rendered
	QThread* worker = nullptr;
	QString renderFile; // what the worker writes
	QTimer* endTimer;   // the bars heard to their end
	QLabel* note;
};
