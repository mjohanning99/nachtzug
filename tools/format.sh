#!/bin/sh
# Lays out the C++ as .clang-format says (AGENTS.md, "Code style"): tabs, 120 characters, one statement per line.
#
#     tools/format.sh            checks: lists the files that aren't laid out yet, and fails if there are any
#     tools/format.sh --write    lays them out
#
# It changes only spaces and line breaks; tools/parity.sh proves the program is the same. A table that reads better
# as written sits between "// clang-format off" and "// clang-format on". Needs clang-format 18 or newer.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
CF=${CLANG_FORMAT:-clang-format}
command -v "$CF" >/dev/null || { echo "clang-format is needed (pacman -S clang, apt install clang-format, brew install clang-format)"; exit 1; }
cd "$ROOT"
FILES=$(ls src/*.cpp src/*.h tools/samplegen/*.cpp)
if [ "${1:-}" = "--write" ]; then
	"$CF" -i $FILES
	echo "Laid out $(echo "$FILES" | wc -l) files."
else
	bad=0
	for f in $FILES; do
		"$CF" --dry-run --Werror "$f" >/dev/null 2>&1 || { echo "not laid out: $f"; bad=1; }
	done
	[ $bad = 0 ] && echo "All laid out." || { echo "Run tools/format.sh --write"; exit 1; }
fi
