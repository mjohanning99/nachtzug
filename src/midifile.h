// MIDI export: the notes of the song as a standard MIDI file, for other music programs.
// The engine plays the song silently and every note that would sound is written down, so arpeggios, chords,
// quick repeats, swing and notes with a chance all come out as you hear them.
#pragma once
#include "engine.h"
#include <QString>

// General MIDI drum notes for the eight drums (kick 36, snare 38, …).
int gmDrumNote(int drum);
// Writes a type 1 MIDI file: a first track with the tempo, then one track per heard track (drums on channel 10).
// Returns the length in seconds, or a negative number if the file could not be written.
double writeMidiFile(const Song& song, const QString& path, const RenderOptions& opt);
