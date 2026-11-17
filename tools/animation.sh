#!/bin/sh
# Makes the short animation of Nachtzug playing its demo song: docs/screenshots/playing.gif for the README and
# site/assets/img/playing.mp4 (with playing.jpg, its first picture) for the website. Retake it when the look changes.
# Needs the program built in build/, ffmpeg and gifsicle.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
EXE=${NACHTZUG:-$ROOT/build/nachtzug}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
# eight seconds of the song playing, the sequence track selected; the sound goes nowhere. 1440×900 like the website's
# other pictures: at 1280 wide the keys above the timeline take two rows, and fewer tracks show
QT_QPA_PLATFORM=offscreen SDL_AUDIODRIVER=dummy "$EXE" --screenshot "$T/f.png" --frames 96 --size 1440x900 --lang en --track 2 >/dev/null 2>&1
W=$(magick identify -format %w "$T/f-000.png")
CROP="crop=$W:450:0:0" # the top: the display and the meters, the keys and the timeline with the playhead
ffmpeg -loglevel error -y -framerate 12 -i "$T/f-%03d.png" \
	-vf "$CROP,scale=960:-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=160:stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=4:diff_mode=rectangle" \
	"$T/playing.gif"
gifsicle -O3 --lossy=40 "$T/playing.gif" -o "$ROOT/docs/screenshots/playing.gif"
ffmpeg -loglevel error -y -framerate 12 -i "$T/f-%03d.png" -vf "$CROP,scale=1280:-2" -c:v libx264 -pix_fmt yuv420p -crf 26 \
	-preset slow -movflags +faststart "$ROOT/site/assets/img/playing.mp4"
magick "$T/f-000.png" -crop "${W}x450+0+0" -resize 1280x -quality 86 "$ROOT/site/assets/img/playing.jpg"
ls -l "$ROOT/docs/screenshots/playing.gif" "$ROOT/site/assets/img/playing.mp4" "$ROOT/site/assets/img/playing.jpg"
