// The sound library ("All sounds…" in the preset list): every ready-made sound and every sound of your own as index
// cards. Groups on the left, a search and the cards in the middle (each with a key to listen first), the chosen sound
// on the right with what can be done with it: use it on the track, star, rename, share as a file, delete. Sound files
// dropped on the window are brought in. Your own sounds are files (see mysounds.h).
#pragma once
#include "mysounds.h"
#include <QDialog>
#include <functional>

class App;
class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;

class SoundLibrary : public QDialog {
	Q_OBJECT
public:
	// use: called with the sound to put on the selected track
	SoundLibrary(App* app, std::function<void(const mysounds::Sound&)> use, QWidget* parent = nullptr);

protected:
	void dragEnterEvent(QDragEnterEvent* e) override;
	void dropEvent(QDropEvent* e) override;
	void keyPressEvent(QKeyEvent* e) override; // arrows choose, Space listens, Return uses
	void hideEvent(QHideEvent* e) override;
	bool eventFilter(QObject* o, QEvent* e) override; // the arrows, Return and Space, also while typing a search

private:
	struct Entry {
		mysounds::Sound sound;
		bool builtIn = false;
	};
	enum Shelf { Mine, Starred, GroupBass, GroupSequence, GroupPad, GroupLead, BuiltIn, ShelfCount };
	void reload();     // the sounds read again from their files
	void showCards();  // the cards of the shelf and the search
	void showChosen(); // the right-hand side
	bool onShelf(const Entry& e) const;
	void listen(const Entry& e);
	void bringIn();
	void step(int by); // the card above (-1) or below (+1) becomes the chosen one
	App* app;
	std::function<void(const mysounds::Sound&)> use;
	std::vector<Entry> entries;
	int shelf = Mine, chosen = -1; // chosen: an index into entries
	std::vector<int> shown;        // the entries on the cards now, top to bottom
	QPushButton* shelfKeys[ShelfCount];
	QLineEdit* search;
	QVBoxLayout* cards;
	QLabel *chosenGroup, *chosenName, *chosenWords, *chosenNote;
	QPushButton *useKey, *starKey, *renameKey, *shareKey, *deleteKey;
};
