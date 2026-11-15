"""4x magnified small text: python mag.py out.png PX weight "text"
Rows: Jost (Qt), Grotesk (Qt), Jost (Pillow/FreeType, its own bytecode or none), Grotesk (Pillow, ttfautohint bytecode)."""
import sys, os
from PIL import Image, ImageDraw, ImageFont
from qtsheet import render

HERE = os.path.dirname(os.path.abspath(__file__))
_REPO = os.path.normpath(os.path.join(HERE, '..', '..', '..', 'assets', 'fonts'))
J = os.environ.get('JOST_DIR', _REPO)
STY = {400: 'Regular', 500: 'Medium', 600: 'SemiBold'}
out, px, weight, text = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
W = int(px * len(text) * 0.52) + 8
H = px * 2
INK, BG = '#2b2623', '#ebe5d8'
jost = f'{J}/Jost-{weight}.ttf'
grot = os.path.join(HERE, 'out', f'NachtzugGrotesk-{STY[weight]}.ttf')
rows = [('Jost · Qt', render([jost], [('text', 3, int(px * 1.3), 'Jost', weight, px, 0, INK, text)], (W, H))),
        ('Grotesk · Qt', render([grot], [('text', 3, int(px * 1.3), 'Nachtzug Grotesk', weight, px, 0, INK, text)], (W, H)))]
for lab, f in (('Jost · FreeType', jost), ('Grotesk · FreeType bytecode', grot)):
    im = Image.new('RGB', (W, H), BG)
    ImageDraw.Draw(im).text((3, int(px * 1.3)), text, font=ImageFont.truetype(f, px), fill=INK, anchor='ls')
    rows.append((lab, im))
S = 4
big = Image.new('RGB', (W * S + 150, (H * S + 4) * len(rows)), '#a5481f')
d = ImageDraw.Draw(big)
for i, (lab, im) in enumerate(rows):
    big.paste(im.resize((W * S, H * S), Image.NEAREST), (150, i * (H * S + 4)))
    d.text((6, i * (H * S + 4) + 8), lab, fill='white')
big.save(out)
print(out, big.size)
