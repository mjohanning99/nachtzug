"""fontforge -lang=py -script ffextrema.py font.ttf out.json
Adds the missing extrema to every glyph FontForge flags for it and writes those glyphs' new TrueType outlines
(contours of [x, y, on_curve] points) as JSON; build_grotesk.py puts them back with fontTools."""
import sys, json
import fontforge

f = fontforge.open(sys.argv[1])
out = {}
for g in f.glyphs():
    if not g.validate(True) & 0x20:
        continue
    layer = g.foreground
    layer.addExtrema('all')
    layer.round()
    g.foreground = layer
    if g.validate(True) & (0x20 | 0x4 | 0x8):
        continue           # could not be fixed cleanly: keep the original
    out[g.glyphname] = [[[p.x, p.y, bool(p.on_curve)] for p in c] for c in g.foreground]
with open(sys.argv[2], 'w') as fh:
    json.dump(out, fh)
print('extrema added to', len(out), 'glyphs')
