#!/usr/bin/env python3
"""Smooths the rail of Nachtzug Antiqua's tz ligature (t_z) in the finished fonts.

build_antiqua.py joins the t's crossbar to the z's top bar with a tapering bridge; where the bridge meets the crossbar
and the bar, the rail's top and bottom edges had small jogs. This script reads both edges between the t's stem and the
z's diagonal and fills each out to a straight line between its two ends, so the rail tapers evenly from the t to the z.
It patches the finished, hinted fonts (the EB Garamond instances the Antiqua is built from are not kept in the
repository): the hinting is taken off a copy, t_z is patched and hinted again with the build's ttfautohint settings, and
only t_z goes back into the installed font, so every other glyph stays exactly as it was.

    python3 tools/fonts/fix_antiqua_tz.py            (fontTools, pyclipper, numpy; ttfautohint)
"""
import os, subprocess, sys, tempfile
import numpy as np
from fontTools.ttLib import TTFont
from fontTools.pens.recordingPen import DecomposingRecordingPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.pens.cu2quPen import Cu2QuPen
from fontTools.pens.reverseContourPen import ReverseContourPen

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from geom import contours_of, from_contours, simplify, union, minus, poly, draw_fitted

FONTS = os.path.join(HERE, '..', '..', 'assets', 'fonts', 'nachtzug')
HINT = ['ttfautohint', '--no-info', '--stem-width-mode=nss', '--windows-compatibility']   # as in build_all.sh


def glyph_path(font, name):
    rec = DecomposingRecordingPen(font.getGlyphSet())
    font.getGlyphSet()[name].draw(rec)
    return simplify(from_contours(contours_of(rec, 0.15)))


def runs(path, x):
    """the filled spans of the shape on the vertical line at x, bottom to top"""
    ys = []
    for c in contours_of(path, 0.2):
        for i in range(len(c)):
            (x0, y0), (x1, y1) = c[i], c[(i + 1) % len(c)]
            if (x0 <= x < x1) or (x1 <= x < x0):
                ys.append(y0 + (x - x0) * (y1 - y0) / (x1 - x0))
    ys.sort()
    return list(zip(ys[0::2], ys[1::2]))


def rail_at(path, x, lo, hi):
    """the span at x that lies at the rail's height (between lo and hi), or None"""
    for a, b in runs(path, x):
        if b > lo and a < hi:
            return a, b
    return None


def smooth(path, width):
    """the rail: taken in the middle of the ligature, where it is the rail alone, then followed left and right at its
    own height until it runs into the t's stem or the z's diagonal (the span there grows tall)"""
    xm = width * 0.5
    a, b = max(runs(path, xm), key=lambda r: r[1])          # the rail is the top span in the middle
    lo, hi = a - 25, b + 25
    thin = lambda r: r is not None and r[1] - r[0] < (b - a) * 1.6
    xa = xm
    while thin(rail_at(path, xa - 2, lo, hi)):
        xa -= 2
    xb = xm
    while thin(rail_at(path, xb + 2, lo, hi)):
        xb += 2
    xa, xb = xa + 6, xb - 6
    ra, rb = rail_at(path, xa, lo, hi), rail_at(path, xb, lo, hi)
    # the top edge: filled out to the straight line (the fill stays in the rail's upper half; the bottom comes next)
    quad = poly([(xa, (ra[0] + ra[1]) / 2), (xb, (rb[0] + rb[1]) / 2), (xb, rb[1]), (xa, ra[1])])
    shape = union([path, quad])
    # underneath, the end of the t's own crossbar hangs a little below the line: from just right of the stem's fillet,
    # the bottom edge becomes exactly the straight line (a thin sliver below it is trimmed)
    xc = xa + 20
    bc = rail_at(path, xc, lo, hi)[0]
    line = lambda x: bc + (rb[0] - bc) * (x - xc) / (xb - xc)
    shape = union([shape, poly([(xc, bc), (xb, rb[0]), (xb, rb[1]), (xc, rail_at(path, xc, lo, hi)[1])])])
    shape = minus(shape, poly([(xc, line(xc) - 12), (xb, line(xb) - 12), (xb, line(xb)), (xc, line(xc))]))
    return shape, (xa, xb, ra, rb)


def put(font, name, path):
    pen = TTGlyphPen(None)
    draw_fitted(path, Cu2QuPen(ReverseContourPen(pen), max_err=0.5), err=0.3, corner=40,
                rnd=lambda p: (round(p[0], 1), round(p[1], 1)))
    font['glyf'][name] = pen.glyph()


def edge_report(path, xa, xb):
    xs = np.arange(xa, xb, 4.0)
    sp = np.array([rail_at(path, x, -1e9, 1e9) and max(runs(path, x), key=lambda r: r[1]) for x in xs])
    return float(np.abs(np.diff(sp[:, 0], 2)).max()), float(np.abs(np.diff(sp[:, 1], 2)).max())


def main():
    for style in ('Regular', 'Bold'):
        src = os.path.join(FONTS, f'NachtzugAntiqua-{style}.ttf')
        with tempfile.TemporaryDirectory() as tmp:
            un, re = os.path.join(tmp, 'un.ttf'), os.path.join(tmp, 're.ttf')
            subprocess.run(['ttfautohint', '--dehint', src, un], check=True)
            f = TTFont(un)
            before = glyph_path(f, 't_z')
            after, (xa, xb, ra, rb) = smooth(before, f['hmtx']['t_z'][0])
            put(f, 't_z', after)
            f.save(un)
            subprocess.run(HINT + [un, re], check=True)
            hinted = TTFont(re)
            out = TTFont(src)
            out['glyf']['t_z'] = hinted['glyf']['t_z']
            for k in ('maxPoints', 'maxContours', 'maxStackElements', 'maxSizeOfInstructions', 'maxTwilightPoints',
                      'maxStorage', 'maxFunctionDefs', 'maxZones'):
                setattr(out['maxp'], k, max(getattr(out['maxp'], k), getattr(hinted['maxp'], k)))
            out.save(src)
            b, a = edge_report(before, xa, xb), edge_report(glyph_path(TTFont(src), 't_z'), xa, xb)
            print(f'{style}: rail {xa:.0f}–{xb:.0f}, largest kink in bottom/top edge {b[0]:.1f}/{b[1]:.1f} -> {a[0]:.1f}/{a[1]:.1f}')


main()
