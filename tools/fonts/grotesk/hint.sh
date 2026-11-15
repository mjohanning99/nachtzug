#!/bin/sh
# Hints the unhinted builds with ttfautohint, tuned for small interface sizes.
#   x-height rounded up more often up to 16 ppem, strong stem snapping for the DirectWrite-like (v40) mode
#   FreeType and Qt use, ClearType-compatible windows metrics.
set -e
cd "$(dirname "$0")"
PY=${PY:-python3}
for s in "$@"; do
  ttfautohint --no-info --stem-width-mode=${STEMS:-nss} --increase-x-height=${XINC:-16} --windows-compatibility \
      --default-script=latn --fallback-script=none ${CTRL:+--control-file=$CTRL} \
      out/NachtzugGrotesk-$s-unhinted.ttf out/NachtzugGrotesk-$s.ttf
  $PY -c "import sys; from fontTools.ttLib import TTFont; f = TTFont(sys.argv[1]); f['head'].flags |= 8; f.save(sys.argv[1])" out/NachtzugGrotesk-$s.ttf
done
