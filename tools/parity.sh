#!/bin/sh
# Proves that a tidy-up of the code changed nothing Nachtzug does (AGENTS.md, "Code style").
#
#     tools/parity.sh baseline     before the change: records what the program does now
#     tools/parity.sh check        after it: records again and compares, file by file
#
# What it records, in build-parity/ (not in git):
#   model.txt   every idea generator, Vary, Clean up, the drummer, sound rolling and Arrange, with fixed seeds
#               (nachtzug --parity, src/parity.cpp)
#   *.wav       the demo song, arranged three ways, and the example songs, rendered
#   *.png       every tab, all six colour themes, the views, the dialogs and a lesson, in English and German
# For anything but changes to the sound, all of it must stay identical to the byte. The program is the one in
# build/ unless NACHTZUG says otherwise.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
EXE=${NACHTZUG:-$ROOT/build/nachtzug}
MODE=${1:?baseline or check}
case "$MODE" in baseline) OUT=$ROOT/build-parity/baseline ;; check) OUT=$ROOT/build-parity/now ;; *) echo "baseline or check"; exit 1 ;; esac
[ -x "$EXE" ] || { echo "No program at $EXE: build it first."; exit 1; }
export QT_QPA_PLATFORM=offscreen SDL_AUDIODRIVER=dummy
rm -rf "$OUT"
mkdir -p "$OUT"

echo "The song code"
"$EXE" --parity "$OUT"

echo "Sound"
"$EXE" --render "$OUT/demo.wav" >/dev/null
for a in 0 1 2; do
	"$EXE" --render "$OUT/demo-arranged-$a.wav" --arrange "$a" >/dev/null
done
for song in "$ROOT"/docs/manual/examples/*.nachtzug; do
	"$EXE" --render "$OUT/example-$(basename "$song" .nachtzug).wav" "$song" >/dev/null
done

echo "Pictures"
shot() {
	name=$1
	shift
	"$EXE" --screenshot "$OUT/$name.png" --size 1560x980 "$@" >/dev/null 2>&1
}
for lang in en de; do
	for t in 0 1 2; do
		shot "$lang-tab$t" --lang "$lang" --tab "$t"
	done
	shot "$lang-loops" --lang "$lang" --loops
	shot "$lang-lines" --lang "$lang" --lines
	shot "$lang-simple" --lang "$lang" --tab 1 --simple
	shot "$lang-guide" --lang "$lang" --guide
	shot "$lang-arranged" --lang "$lang" --arrange 1
	shot "$lang-chordkeys" --lang "$lang" --chord-keys
	shot "$lang-lesson" --lang "$lang" --lesson acid:2
	for d in welcome settings soundcard samples midi export arrange chop; do
		shot "$lang-dialog-$d" --lang "$lang" --dialog "$d"
	done
done
for theme in Bakelite Ember Parchment Wine Charcoal "Blue panel"; do
	file=$(printf '%s' "$theme" | tr 'A-Z ' 'a-z-')
	shot "theme-$file-tab0" --lang en --theme "$theme" --tab 0
	shot "theme-$file-tab1" --lang en --theme "$theme" --tab 1
	shot "theme-$file-mixer" --lang en --theme "$theme" --tab 2
done
echo "Recorded $(ls "$OUT" | wc -l) files in ${OUT#$ROOT/}"

[ "$MODE" = check ] || exit 0
BASE=$ROOT/build-parity/baseline
[ -d "$BASE" ] || { echo "No baseline yet: run tools/parity.sh baseline before the change."; exit 1; }
differ=0
for f in "$BASE"/*; do
	n=$(basename "$f")
	if [ ! -f "$OUT/$n" ]; then
		echo "missing now: $n"
		differ=1
	elif ! cmp -s "$f" "$OUT/$n"; then
		echo "different: $n"
		differ=1
	fi
done
for f in "$OUT"/*; do
	[ -f "$BASE/$(basename "$f")" ] || { echo "new: $(basename "$f")"; differ=1; }
done
if [ $differ = 0 ]; then
	echo "Parity: everything identical."
else
	echo "Parity: something changed (above). For pictures, compare them side by side; for model.txt, diff it."
	exit 1
fi
