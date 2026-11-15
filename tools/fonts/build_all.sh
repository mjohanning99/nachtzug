#!/bin/sh
# Rebuilds the Antiqua (with hinting) and copies everything into dist/ and specimen/fonts/.
set -e
cd "$(dirname "$0")"
PY="$(cd .. && pwd)/fontenv/bin/python"
$PY build_antiqua.py
for s in Regular Bold; do
  ttfautohint --no-info --stem-width-mode=nss --windows-compatibility antiqua/NachtzugAntiqua-$s-unhinted.ttf antiqua/NachtzugAntiqua-$s.ttf
  # hinted fonts should round to integer ppem (head.flags bit 3)
  $PY -c "import sys; from fontTools.ttLib import TTFont; f = TTFont(sys.argv[1]); f['head'].flags |= 8; f.save(sys.argv[1])" antiqua/NachtzugAntiqua-$s.ttf
done
mkdir -p dist specimen/fonts
cp antiqua/NachtzugAntiqua-Regular.ttf antiqua/NachtzugAntiqua-Bold.ttf antiqua/NachtzugOrnamente-Regular.otf out/NachtzugInitialen*-Regular.ttf dist/
cp dist/*.ttf dist/*.otf dist/OFL.txt specimen/fonts/
