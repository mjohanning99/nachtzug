"""Quick looks: python view.py out.png SIZE "text" font1.ttf [font2.ttf ...]  (one row per font; draws baseline,
x-height and cap-height guides when SIZE >= 60)."""
import sys
from PIL import Image, ImageDraw, ImageFont
from fontTools.ttLib import TTFont

out, size, text, fonts = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4:]
rows = []
for p in fonts:
    f = ImageFont.truetype(p, size, layout_engine=ImageFont.Layout.RAQM)
    w = int(f.getlength(text)) + 40
    rows.append((p, f, w))
W = max(r[2] for r in rows)
lh = int(size * 1.55)
img = Image.new('RGB', (W, lh * len(rows)), (236, 230, 216))
d = ImageDraw.Draw(img)
for k, (p, f, w) in enumerate(rows):
    tt = TTFont(p, lazy=True)
    upm = tt['head'].unitsPerEm
    asc = tt['hhea'].ascent
    top = k * lh + int(size * 0.15)
    base = top + size * asc / upm * 0.72 + size * 0.1
    if size >= 60:
        for v, c in ((0, (150, 150, 255)), (tt['OS/2'].sxHeight, (255, 150, 150)), (tt['OS/2'].sCapHeight, (150, 200, 150))):
            y = base - v * size / upm
            d.line([(0, y), (W, y)], fill=c)
    d.text((20, base), text, font=f, fill=(43, 38, 35), anchor='ls')
img.save(out)
print(out, img.size)
