#!/bin/sh
# Draws the website's pictures of the two manuals (site/assets/img/cover-en.jpg and cover-de.jpg) from the front page
# of each PDF in docs/, at twice the size they're shown so they stay sharp on fine screens. Run it after rebuilding
# the manuals (docs/manual/build.py). Needs pdftoppm (poppler) and ImageMagick.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
for pair in en:Nachtzug-Manual de:Nachtzug-Handbuch; do
    lang=${pair%%:*}; pdf=${pair#*:}
    pdftoppm -f 1 -l 1 -png -scale-to-x 1282 -scale-to-y -1 "$ROOT/docs/$pdf.pdf" "$TMP/$lang"
    magick "$TMP/$lang"-*1.png -resize 1282x1820! -quality 86 -sampling-factor 4:2:0 -strip "$ROOT/site/assets/img/cover-$lang.jpg"
    echo "Wrote site/assets/img/cover-$lang.jpg"
done
