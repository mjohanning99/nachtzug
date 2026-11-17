#!/bin/sh
# Renders the demo song for the website's player: the song Nachtzug opens with, arranged by "Arrange for me" at medium
# length (intro, build-up, break, ending), exactly as the program plays it. Writes:
#
#   site/assets/audio/demo.opus   for most browsers (Opus, 96 kbit/s)
#   site/assets/audio/demo.mp3    for the rest, and to download (MP3, VBR about 130 kbit/s)
#   site/_data/demo_song.json     its length and its loudness in 100 steps, which the player (_includes/player.html)
#                                 draws as the groove
#
#   tools/site_audio.sh [path/to/nachtzug]      (default: build/nachtzug)
#
# Run it again whenever the sound or the demo song changes, as with the screenshots. Run it on Linux: the idea
# generators make other notes from the same seed on a Mac (AGENTS.md, "Known traps"). Needs ffmpeg and python3.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
NZ=${1:-$ROOT/build/nachtzug}
OUT="$ROOT/site/assets/audio"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
mkdir -p "$OUT"

QT_QPA_PLATFORM=offscreen SDL_AUDIODRIVER=dummy "$NZ" --render "$TMP/demo.wav" --arrange 1 2>/dev/null
ffmpeg -v error -y -i "$TMP/demo.wav" -c:a libopus -b:a 96k -map_metadata -1 \
    -metadata title="Nachtzug demo song" -metadata license="CC0-1.0" "$OUT/demo.opus"
ffmpeg -v error -y -i "$TMP/demo.wav" -c:a libmp3lame -q:a 4 -map_metadata -1 \
    -metadata title="Nachtzug demo song" -metadata copyright="CC0-1.0" "$OUT/demo.mp3"

# the song's loudness in 100 steps (each step's average level, RMS, 0 to 1 against the loudest), for the player's
# picture of the song: the loudest sample would be near the top almost everywhere, the average shows its shape
python3 - "$TMP/demo.wav" "$ROOT/site/_data/demo_song.json" <<'PY'
import array, json, sys, wave
w = wave.open(sys.argv[1])
assert w.getsampwidth() == 2, "16-bit WAV expected"
frames = array.array("h", w.readframes(w.getnframes()))
n, steps = len(frames), 100
size = n // steps
peaks = [(sum(v * v for v in frames[i * size:(i + 1) * size]) / size) ** .5 for i in range(steps)]
top = max(peaks) or 1
json.dump({"seconds": round(w.getnframes() / w.getframerate(), 2), "peaks": [round(p / top, 3) for p in peaks]},
          open(sys.argv[2], "w"), separators=(",", ":"))
PY
for f in demo.opus demo.mp3; do echo "Wrote site/assets/audio/$f ($(du -h "$OUT/$f" | cut -f1))"; done
echo "Wrote site/_data/demo_song.json"
