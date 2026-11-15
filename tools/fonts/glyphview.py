"""Draws glyphs big with a 50-unit grid: python glyphview.py font.ttf out.png g z c_t ..."""
import sys
from PIL import Image, ImageDraw
from fontTools.ttLib import TTFont
from fontTools.pens.recordingPen import DecomposingRecordingPen
sys.path.insert(0, __file__.rsplit('/', 1)[0])
from geom import contours_of
font = TTFont(sys.argv[1]); out = sys.argv[2]; names = sys.argv[3:]
gs = font.getGlyphSet()
S = 0.42
cell = int(1100 * S)
img = Image.new('RGB', (cell * len(names), int(1400 * S)), 'white')
d = ImageDraw.Draw(img)
for k, n in enumerate(names):
    ox = k * cell + 60; base = int(1050 * S)
    g = gs[n]
    for x in range(-100, 1100, 50):
        d.line([(ox + x * S, 0), (ox + x * S, img.height)], fill=(235, 235, 235) if x % 100 else (205, 205, 205))
    for y in range(-350, 1050, 50):
        d.line([(k * cell, base - y * S), ((k + 1) * cell, base - y * S)], fill=(235, 235, 235) if y % 100 else (205, 205, 205))
    d.line([(k * cell, base), ((k + 1) * cell, base)], fill=(150, 150, 255))
    rec = DecomposingRecordingPen(gs); g.draw(rec)
    cs = contours_of(rec, 0.5)
    from geom import signed_area
    for c in sorted(cs, key=lambda c: -abs(signed_area(c))):
        d.polygon([(ox + x * S, base - y * S) for x, y in c], fill=(60, 60, 60) if signed_area(c) < 0 else 'white', outline=(0, 0, 0))
    d.line([(ox + g.width * S, 0), (ox + g.width * S, img.height)], fill=(255, 120, 120))
    d.text((k * cell + 5, 5), f'{n} w={g.width}', fill='red')
img.save(out)
