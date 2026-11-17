#!/bin/sh
# Makes the program's icons from the logo's SVG files (needs rsvg-convert, ImageMagick and Python with Pillow):
#   assets/logo.png (512, the program's own mark and the Linux icon), assets/logo-small.png (for 24 px and below),
#   assets/logo/enamel-sign.png (the sign for the README), assets/nachtzug.icns (macOS), assets/windows/nachtzug.ico,
#   assets/icons/nachtzug-*.png (the Linux icon at each size)
set -eu
cd "$(dirname "$0")/../../assets"
T=$(mktemp -d)
rsvg-convert -w 512 logo/enamel-flat.svg -o logo.png
rsvg-convert -w 64 logo/enamel-tiny.svg -o logo-small.png
rsvg-convert -w 1200 logo/enamel.svg -o logo/enamel-sign.png
cp logo/enamel-flat.svg nachtzug.svg
for s in 32 48 64 128 256; do rsvg-convert -w $s logo/enamel-flat.svg -o "$T/f$s.png"; done
for s in 16 24; do rsvg-convert -w $s logo/enamel-tiny.svg -o "$T/f$s.png"; done
magick "$T/f16.png" "$T/f24.png" "$T/f32.png" "$T/f48.png" "$T/f64.png" "$T/f128.png" "$T/f256.png" windows/nachtzug.ico
# Linux: one PNG per size the desktops ask for (CMakeLists.txt installs them next to the SVG and logo.png)
mkdir -p icons
for s in 16 24 32 48 64 128 256; do cp "$T/f$s.png" "icons/nachtzug-$s.png"; done
# macOS: the icon sits on Apple's grid, 824 of 1024, with room around it; ICNS holds PNGs of each size
python3 - "$T" <<'PY'
import struct, sys, subprocess, os
from PIL import Image
T = sys.argv[1]
def grid(px, svg):
    inner = round(px * 824 / 1024)
    src = os.path.join(T, f'g{px}.png'); subprocess.run(['rsvg-convert', '-w', str(inner), svg, '-o', src], check=True)
    im = Image.new('RGBA', (px, px)); ic = Image.open(src); im.paste(ic, ((px - inner) // 2, (px - inner) // 2), ic)
    out = os.path.join(T, f'm{px}.png'); im.save(out); return open(out, 'rb').read()
entries = [(b'icp4', 16, 'logo/enamel-tiny.svg'), (b'icp5', 32, 'logo/enamel-flat.svg'), (b'icp6', 64, 'logo/enamel-flat.svg'),
           (b'ic07', 128, 'logo/enamel-flat.svg'), (b'ic08', 256, 'logo/enamel-flat.svg'), (b'ic09', 512, 'logo/enamel-flat.svg'),
           (b'ic10', 1024, 'logo/enamel-flat.svg'), (b'ic11', 32, 'logo/enamel-tiny.svg'), (b'ic12', 64, 'logo/enamel-flat.svg'),
           (b'ic13', 256, 'logo/enamel-flat.svg'), (b'ic14', 512, 'logo/enamel-flat.svg')]
body = b''.join(t + struct.pack('>I', len(d) + 8) + d for t, px, svg in entries for d in [grid(px, svg)])
open('nachtzug.icns', 'wb').write(b'icns' + struct.pack('>I', len(body) + 8) + body)
PY
rm -rf "$T"
echo "icons made"
