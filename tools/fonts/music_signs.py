#!/usr/bin/env python3
"""Adds the sharp, flat and natural signs (♯ ♭ ♮) to the fonts Nachtzug ships.

    python3 tools/fonts/music_signs.py assets/fonts/Jost-400.ttf …

None of Jost, Courier Prime or the Nachtzug Antiqua has them, and a fallback
font's sharp sits badly next to the note names. The signs are drawn from each
font's own measurements: the cap height sets their size and the stem of the
"l" sets how heavy they are, so they match every weight. Fonts that already
have a sign keep their own. Needs fontTools and skia-pathops.
"""
import math, sys
from fontTools.ttLib import TTFont
from fontTools.pens.boundsPen import BoundsPen
from fontTools.pens.recordingPen import RecordingPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.pens.cu2quPen import Cu2QuPen
import pathops

SIGNS = {0x266F: 'uni266F', 0x266D: 'uni266D', 0x266E: 'uni266E'}


def stem_of(font, ch='l'):
    """Width of the letter's stem halfway up, measured on the outline."""
    gs = font.getGlyphSet()
    name = font.getBestCmap()[ord(ch)]
    path = pathops.Path()
    gs[name].draw(path.getPen())
    bp = BoundsPen(gs); gs[name].draw(bp)
    x0, y0, x1, y1 = bp.bounds
    y = (y0 + y1) / 2
    band = pathops.Path(); p = band.getPen()
    p.moveTo((x0 - 10, y - 1)); p.lineTo((x1 + 10, y - 1)); p.lineTo((x1 + 10, y + 1)); p.lineTo((x0 - 10, y + 1)); p.closePath()
    cut = pathops.op(path, band, pathops.PathOp.INTERSECTION)
    b = cut.bounds
    return b[2] - b[0]


def quad(pen, a, b, c, d):
    pen.moveTo(a); pen.lineTo(b); pen.lineTo(c); pen.lineTo(d); pen.closePath()


def bar(pen, x0, y0, x1, y1, h):
    """A sloped beam from (x0, y0) to (x1, y1), h thick (measured upright)."""
    quad(pen, (x0, y0 - h / 2), (x1, y1 - h / 2), (x1, y1 + h / 2), (x0, y0 + h / 2))


def stroke(pen, x, y0, y1, w):
    quad(pen, (x - w / 2, y0), (x + w / 2, y0), (x + w / 2, y1), (x - w / 2, y1))


def weights(C, s):
    """Upright strokes a little lighter than the font's stem, beams heavier, but
    never so heavy that the gap between the two beams closes up when small."""
    return s * 0.72, min(s * 1.3, C * 0.15)


def sharp(pen, C, s):
    thin, thick = weights(C, s)
    w = C * 0.60
    lx, rx = w * 0.32, w * 0.68
    rise = C * 0.12
    stroke(pen, lx, -C * 0.16, C * 0.92, thin)
    stroke(pen, rx, -C * 0.08, C * 1.0, thin)
    for yb in (C * 0.21, C * 0.63):
        bar(pen, 0, yb - rise / 2, w, yb + rise / 2, thick)
    return w


def natural(pen, C, s):
    thin, thick = weights(C, s)
    w = C * 0.42
    lx, rx = w * 0.12 + thin / 2, w * 0.88 - thin / 2
    rise = C * 0.12
    stroke(pen, lx, C * 0.20, C * 1.0, thin)
    stroke(pen, rx, -C * 0.16, C * 0.66, thin)
    for yb in (C * 0.22, C * 0.62):
        bar(pen, lx - thin / 2, yb - rise / 2, rx + thin / 2, yb + rise / 2, thick)
    return w


def flat(pen, C, s):
    thin = weights(C, s)[0]
    sx = thin / 2
    top, bot = C * 1.04, -C * 0.02
    stroke(pen, sx, bot, top, thin)
    # The bowl: a teardrop on the right of the stem, its point at the bottom of the stem.
    h, w = C * 0.56, C * 0.46
    pen.moveTo((sx, bot))
    pen.curveTo((sx + w * 0.55, bot + h * 0.25), (sx + w, bot + h * 0.55), (sx + w * 0.92, bot + h * 0.82))
    pen.curveTo((sx + w * 0.86, bot + h * 1.02), (sx + w * 0.55, bot + h * 1.06), (sx, bot + h * 0.80))
    pen.lineTo((sx, bot + h * 0.62))
    pen.curveTo((sx + w * 0.35, bot + h * 0.86), (sx + w * 0.58, bot + h * 0.82), (sx + w * 0.58, bot + h * 0.62))
    pen.curveTo((sx + w * 0.58, bot + h * 0.40), (sx + w * 0.30, bot + h * 0.20), (sx, bot + h * 0.12))
    pen.closePath()
    return sx + w


def build(font, fn):
    C = font['OS/2'].sCapHeight or font['head'].unitsPerEm * 0.7
    s = stem_of(font)
    path = pathops.Path()
    width = fn(path.getPen(), C, s)
    path = pathops.simplify(path, fix_winding=True, clockwise=True)
    rec = RecordingPen(); path.draw(rec)
    ttp = TTGlyphPen(None)
    rec.replay(Cu2QuPen(ttp, max_err=1.0, reverse_direction=False))
    side = round(C * 0.09)
    glyph = ttp.glyph()
    return glyph, width, side


def add(path):
    font = TTFont(path)
    cmap = font.getBestCmap()
    order = font.getGlyphOrder()
    added = []
    for cp, name in SIGNS.items():
        if cp in cmap:
            continue
        fn = {0x266F: sharp, 0x266D: flat, 0x266E: natural}[cp]
        glyph, width, side = build(font, fn)
        # Shift the outline right by the side bearing.
        glyph.coordinates.translate((side, 0))
        glyph.recalcBounds(font['glyf'])
        font['glyf'][name] = glyph
        adv = round(width + 2 * side)
        if font['post'].isFixedPitch:
            adv = font['hmtx'][cmap[ord('0')]][0]
            dx = (adv - (glyph.xMax - glyph.xMin)) / 2 - glyph.xMin
            glyph.coordinates.translate((round(dx), 0)); glyph.recalcBounds(font['glyf'])
        font['hmtx'][name] = (adv, glyph.xMin)
        if name not in order:
            order.append(name)
        for t in font['cmap'].tables:
            if t.isUnicode():
                t.cmap[cp] = name
        added.append(chr(cp))
    if added:
        font.setGlyphOrder(order)
        if 'post' in font and font['post'].formatType == 2.0:
            font['post'].extraNames = []
            font['post'].mapping = {}
        font['maxp'].numGlyphs = len(order)
        font.save(path)
    print(path, 'added', ' '.join(added) or 'nothing')


if __name__ == '__main__':
    for p in sys.argv[1:]:
        add(p)
