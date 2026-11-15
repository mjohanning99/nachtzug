"""Run with: fontforge -lang=py -script ffvalidate.py font1 font2 ...
Prints, per font, the number of glyphs with each validation problem and a few example glyph names."""
import sys
import fontforge

FLAGS = [
    (0x2, 'open contour'), (0x4, 'self-intersecting'), (0x8, 'wrong direction'),
    (0x10, 'flipped references'), (0x20, 'missing extrema'), (0x40, 'missing anchor'),
    (0x80, 'too many points'), (0x100, 'too many hints'), (0x200, 'bad glyph name'),
    (0x400, 'too many points (maxp)'), (0x800, 'too many component levels'),
    (0x1000, 'too many components'), (0x2000, 'bad glyph name'),
    (0x40000, 'points too far apart'), (0x80000, 'non-integral coordinates'),
    (0x100000, 'missing anchor'), (0x200000, 'duplicate glyph name'), (0x400000, 'duplicate unicode'),
    (0x800000, 'overlapping hints'),
]
only = None
for path in sys.argv[1:]:
    f = fontforge.open(path)
    counts = {}
    for g in f.glyphs():
        v = g.validate(True)
        for bit, name in FLAGS:
            if v & bit:
                counts.setdefault(name, []).append(g.glyphname)
    print(path.rsplit('/', 1)[-1], 'glyphs:', len(list(f.glyphs())))
    for name, gl in sorted(counts.items(), key=lambda x: -len(x[1])):
        print('   %-26s %5d   e.g. %s' % (name, len(gl), ' '.join(gl[:8])))
    f.close()
