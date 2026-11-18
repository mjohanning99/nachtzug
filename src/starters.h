// Starter songs: a short, finished song for each style Nachtzug plays (acid, Berlin School, motorik, electro, a slow
// half-time one), to open from the start window and make one's own. They come with the program as files
// (:/starters/<id>.nachtzug, CC0) so they sound the same on every system; makeStarter() is how those files were made,
// from the program's own generators with fixed seeds (nachtzug --write-starters DIR writes them again). No window.
#pragma once
#include "model.h"
#include <vector>

struct Starter {
	const char* id;    // the file's name in the resources
	const char* name;  // shown in the start window (translated with ui())
	const char* words; // a line about it (translated with ui())
};
const std::vector<Starter>& starters();
// the starter as it comes with the program; false if it is missing from the resources or cannot be read
bool loadStarter(int i, Song& out);
// the starter made afresh from the generators (other notes on a Mac: see AGENTS.md); for --write-starters
Song makeStarter(int i);
// --write-starters: every starter made afresh into folder as <id>.nachtzug; the number written
int writeStarters(const QString& folder);
