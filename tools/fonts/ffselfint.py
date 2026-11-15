"""fontforge -lang=py -script ffselfint.py font.ttf  ->  prints names of self-intersecting glyphs, one per line"""
import sys, fontforge
f = fontforge.open(sys.argv[1])
for g in f.glyphs():
    if g.validate(True) & 0x4:
        print('SELFINT', g.glyphname)
