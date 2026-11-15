#!/bin/sh
# Builds Nachtzug Grotesk (400, 500, 600) from Jost, hints it, checks it, and draws the specimen.
#   PY=/path/to/python ./build_all.sh          (Python with fontTools, skia-pathops, pyclipper, numpy, Pillow)
# Needs fontforge and ttfautohint; the Qt renderer (qtrender/) is built with g++ and Qt 6 if missing.
set -e
cd "$(dirname "$0")"
PY=${PY:-python3}
$PY build_grotesk.py
PY=$PY ./hint.sh Regular Medium SemiBold
fontforge -lang=py -script ffvalidate.py out/NachtzugGrotesk-Regular.ttf out/NachtzugGrotesk-Medium.ttf out/NachtzugGrotesk-SemiBold.ttf 2>/dev/null
$PY roundtrip.py out/NachtzugGrotesk-Regular.ttf out/NachtzugGrotesk-Medium.ttf out/NachtzugGrotesk-SemiBold.ttf
[ -x qtrender/qtrender ] || g++ -std=c++17 -O1 -fPIC qtrender/qtrender.cpp -o qtrender/qtrender $(pkg-config --cflags --libs Qt6Widgets)
$PY specimen.py
# before/after of the real program (takes a few minutes: it finds every Jost label in the screenshot)
if [ -n "$NACHTZUG" ]; then
  mkdir -p shots
  QT_QPA_PLATFORM=offscreen "$NACHTZUG" --screenshot shots/program.png --size 1560x980 --tab 1 --track 1
  $PY relabel.py shots/program.png shots/after.png shots/control.png shots/found.json
  $PY beforeafter.py
fi
