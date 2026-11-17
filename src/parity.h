// Parity: a record of what the song code does with fixed seeds, for proving that a tidy-up changed nothing.
// "nachtzug --parity folder" writes it; tools/parity.sh compares two of them (see AGENTS.md, "Code style").
#pragma once
#include <QString>

int writeParity(const QString& folder); // returns 0, or 1 if the folder could not be written
