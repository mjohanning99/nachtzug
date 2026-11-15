"""Small-size comparison, rendered by Qt: python cmp_small.py out.png [weight] [variant ...]
variants: jost, hinted, unhinted (default jost hinted)."""
import sys, os
from PIL import Image, ImageDraw
from qtsheet import render

HERE = os.path.dirname(os.path.abspath(__file__))
_REPO = os.path.normpath(os.path.join(HERE, '..', '..', '..', 'assets', 'fonts'))
J = os.environ.get('JOST_DIR', _REPO)
out = sys.argv[1]
weight = int(sys.argv[2]) if len(sys.argv) > 2 else 400
variants = sys.argv[3:] or ['jost', 'hinted']
STY = {400: 'Regular', 500: 'Medium', 600: 'SemiBold'}
WORDS = 'Cutoff  Env amount  Roll new sound  Pump  Follow chords  Zeitleiste  Häufigkeit  Arrangieren lassen  Nachtzug  Il1 ag tz 0123456789'
SIZES = [9, 10, 11, 12, 13, 15]

def fonts_for(v):
    if v == 'jost': return [f'{J}/Jost-{w}.ttf' for w in (400, 500, 600)], 'Jost'
    suf = '' if v == 'hinted' else '-unhinted'
    d = os.path.join(HERE, 'out')
    return [os.path.join(d, f'NachtzugGrotesk-{STY[w]}{suf}.ttf') for w in (400, 500, 600)
            if os.path.exists(os.path.join(d, f'NachtzugGrotesk-{STY[w]}{suf}.ttf'))], 'Nachtzug Grotesk'

W = 980
rows = []
for v in variants:
    fonts, fam = fonts_for(v)
    items, y = [], 4
    for px in SIZES:
        y += px + 6
        items.append(('text', 6, y, 'Jost', 400, 9, 0, '#a5481f', f'{px}'))
        items.append(('text', 30, y, fam, weight, px, 0, '#2b2623', WORDS))
    rows.append((v, render(fonts, items, (W, y + 8))))
H = sum(r[1].height for r in rows)
img = Image.new('RGB', (W, H), '#ebe5d8')
y = 0
for v, im in rows:
    img.paste(im, (0, y)); y += im.height
img.save(out)
# 4x crop of the 10 and 11 px lines, all variants
crops = []
for v, im in rows:
    top = 4 + (9 + 6) + 1
    c = im.crop((26, top, 26 + 380, top + 2 * (10 + 6) + 6))
    crops.append(c.resize((c.width * 4, c.height * 4), Image.NEAREST))
mag = Image.new('RGB', (crops[0].width, sum(c.height + 6 for c in crops)), '#a5481f')
y = 0
for c in crops:
    mag.paste(c, (0, y)); y += c.height + 6
mag.save(out.replace('.png', '-4x.png'))
print(out)
