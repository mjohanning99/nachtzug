#!/usr/bin/env python3
"""Nachtzug Initialen: ornamented drop caps for a night train.

Each initial is a square plate: a heavy riveted frame with star-pierced corner blocks and a fine inner rule, a railway
track along the bottom, Jugendstil tendrils that grow from the track and curl into volutes, a medallion with the
crescent moon in the emptier upper corner, little twinkling stars in the gaps, and an Antiqua capital in the middle,
set off by a clear halo and a fine contour line into which the ornament runs.

Everything is drawn here from geometry; the capitals come from EB Garamond (SIL Open Font License 1.1).

    python initialen.py            all initials, all four fonts and preview pictures
    python initialen.py A N D      only these (quick look)
"""
import math, random, sys, os
import numpy as np
import pathops
from PIL import Image, ImageDraw
from fontTools.ttLib import TTFont
from fontTools.pens.t2CharStringPen import T2CharStringPen
from fontTools.pens.transformPen import TransformPen
from fontTools.pens.boundsPen import BoundsPen
from fontTools.pens.recordingPen import RecordingPen
from fontTools.fontBuilder import FontBuilder

from geom import (poly, rect, circle, ring, rounded_rect, star, sparkle, crescent, leaf, bezier, curl, end_tangent, stroke,
                  union, minus, inter, simplify, dilate, fill_holes, drop_small, flat, contours_of, signed_area,
                  transformed, draw_fitted, from_contours)

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, 'out')
EM = 1000
VERSION = '0.3'
LETTERS = [chr(c) for c in range(ord('A'), ord('Z') + 1)] + ['Ä', 'Ö', 'Ü'] + [str(d) for d in range(10)]

# ---------------------------------------------------------------- the frame system (shared by all initials)

M = 20                 # margin around the plate
BAND = 36              # the heavy riveted band
GAP = 12
RULE = 8               # the fine inner rule
CORNER = 92            # the corner blocks
F0 = M + BAND + GAP + RULE         # the field inside the rule
F1 = EM - F0
TRACK_TOP = F0 + 66                # the rail head's upper edge
LINE = 8                           # weight of all fine rules (inner rule, contour, medallion)

def frame():
    parts = [minus(rect(M, M, EM - M, EM - M), rect(M + BAND, M + BAND, EM - M - BAND, EM - M - BAND))]
    r0 = M + BAND + GAP
    parts.append(minus(rect(r0, r0, EM - r0, EM - r0), rect(r0 + RULE, r0 + RULE, EM - r0 - RULE, EM - r0 - RULE)))
    for x in (M, EM - M - CORNER):
        for y in (M, EM - M - CORNER):
            parts.append(rect(x, y, x + CORNER, y + CORNER))
    shape = union(parts)
    holes = []
    # corner blocks pierced by a four-pointed star inside a fine ring
    for x in (M, EM - M - CORNER):
        for y in (M, EM - M - CORNER):
            cx, cy = x + CORNER / 2, y + CORNER / 2
            holes.append(sparkle(cx, cy, 33, waist=0.2, rot=math.pi / 4))
            holes.append(ring(cx, cy, 36, 3.5))
    # rivets along the band, like the panel of a sleeping car
    mid = M + BAND / 2
    span0, span1 = M + CORNER + 18, EM - M - CORNER - 18
    n = 9
    for i in range(n):
        t = span0 + (span1 - span0) * i / (n - 1)
        for (x, y) in ((t, mid), (t, EM - mid), (mid, t), (EM - mid, t)):
            holes.append(circle(x, y, 6.8))
    return minus(shape, union(holes))

def track(rng):
    """Seen from the side: a bed of ballast, the sleeper ends, and the rail (head and foot)."""
    parts = []
    x0, x1 = F0, F1
    rail0 = TRACK_TOP - 22
    parts.append(minus(rect(x0, rail0, x1, TRACK_TOP), rect(x0, rail0 + 5, x1, rail0 + 7.5)))
    n = 11
    for i in range(n):
        x = x0 + (x1 - x0) * (i + 0.5) / n
        parts.append(rect(x - 18, F0 + 22, x + 18, rail0 - 2.5))
    # ballast: two staggered rows of round stones
    for row, yy in enumerate((F0 + 7.5, F0 + 16)):
        m = 31
        for i in range(m - row):
            xx = x0 + 9 + (x1 - x0 - 18) * (i + 0.5 * row) / (m - 1)
            parts.append(circle(xx, yy, 4.6 if row == 0 else 4.2))
    return union(parts)

# ---------------------------------------------------------------- the capitals

SRC = TTFont(os.path.join(OUT, 'EBG-800.ttf'))
GLYPHS = SRC.getGlyphSet()
CMAP = SRC.getBestCmap()
GLYPH_FOR = {'Q': 'Q.short', 'J': 'J'}
DIGIT = {str(d): n + '.lf' for d, n in enumerate(['zero', 'one', 'two', 'three', 'four', 'five', 'six', 'seven', 'eight', 'nine'])}

BOX = (150, TRACK_TOP + 36, 850, F1 - 64)    # where the letter (and its halo) may go
CAP = 528

def letter_path(ch, shrink=1.0):
    name = GLYPH_FOR.get(ch) or DIGIT.get(ch) or CMAP[ord(ch)]
    if name not in GLYPHS: name = CMAP[ord(ch)]
    raw = pathops.Path()
    GLYPHS[name].draw(raw.getPen(glyphSet=GLYPHS))
    try:
        raw = pathops.simplify(raw, fix_winding=True)          # resolves the overlaps of the variable outlines
    except pathops.PathOpsError:
        raw = simplify(flat(raw))
    x0, y0, x1, y1 = raw.bounds
    s = CAP / 650.0
    if ch.isdigit(): s = CAP * 0.98 / 640
    bw = BOX[2] - BOX[0]
    top = max(y1, 650)
    s = min(s, bw / (x1 - x0), (BOX[3] - BOX[1]) / top) * shrink
    # optical centre: between the box centre and the centre of the ink
    cs = contours_of(raw, 1.0)
    A = sum(signed_area(c) for c in cs)
    cx = sum(signed_area(c) * sum(p[0] for p in c) / len(c) for c in cs) / A if A else (x0 + x1) / 2
    ox = 0.7 * (x0 + x1) / 2 + 0.3 * cx
    if ch == 'Q': ox = 0.5 * (x0 + 760)        # centre the bowl, let the tail run out
    # all letters share the baseline logic: centred on the cap height; a descender may run down over the rails
    while True:
        ty = (BOX[1] + BOX[3]) / 2 - top / 2 * s
        if ty + y0 * s >= F0 + 18: break
        s *= 0.97
    tx = EM / 2 - ox * s
    if tx + x1 * s > F1 - 40: tx = F1 - 40 - x1 * s
    return transformed(raw, (s, 0, 0, s, tx, ty))

# ---------------------------------------------------------------- ornament: tendrils, leaves, buds

def bud(rng, p, kind=None, size=1.0):
    kind = kind or rng.choice(['star', 'ball', 'berry'])
    x, y = p
    if kind == 'star':
        return sparkle(x, y, 14 * size, waist=0.2, rot=rng.uniform(-0.2, 0.2))
    if kind == 'berry':
        return union([circle(x, y, 7 * size), circle(x + 8 * size, y + 7 * size, 5 * size)])
    return circle(x, y, 7.5 * size)

def tendril(pts, w0, w1, curl_r=None, turns=1.2, direction=1, r_end=None, ease=1.0, ball=7.0):
    """A stem along pts that ends in a volute with a ball; returns (shape, centreline, stem length in points)."""
    line = list(pts)
    if curl_r:
        c = curl(line[-1], end_tangent(line), curl_r, turns, direction, r_end=r_end or max(9, curl_r * 0.24))
        line = line + c[1:]
    shape = stroke(line, w0, w1, ease=ease)
    if ball: shape = union([shape, circle(line[-1][0], line[-1][1], ball)])
    return shape, line

def leaves_on(rng, line, at, size=1.0, side=None):
    out = []
    n = len(line)
    for k, f in enumerate(at):
        i = max(3, min(n - 4, int(f * n)))
        a, b = line[i - 3], line[i + 3]
        sd = side if side is not None else (1 if k % 2 == 0 else -1)
        ang = math.atan2(b[1] - a[1], b[0] - a[0]) + sd * rng.uniform(0.7, 0.9)
        out.append(leaf(line[i][0], line[i][1], ang, rng.uniform(60, 70) * size, rng.uniform(25, 29) * size,
                        bend=-sd * 0.14, rib=2.6 * size))
    return out

def sprig(rng, line, f, length, side, kind=None, width=8):
    """A short side shoot off a stem, ending in a bud."""
    n = len(line)
    i = max(3, min(n - 4, int(f * n)))
    a, b = line[i - 3], line[i + 3]
    ang = math.atan2(b[1] - a[1], b[0] - a[0]) + side * rng.uniform(0.55, 0.8)
    x, y = line[i]
    tip = (x + length * math.cos(ang), y + length * math.sin(ang))
    c1 = (x + 0.45 * length * math.cos(ang - side * 0.45), y + 0.45 * length * math.sin(ang - side * 0.45))
    pts = bezier((x, y), c1, c1, tip)
    return union([stroke(pts, width, width * 0.55), bud(rng, tip, kind)])

def ranken(rng, variant):
    """The left half of the tendril ground (mirrored later); variant picks one of four growth plans."""
    j = lambda a: rng.uniform(-a, a)
    parts = []
    v = variant % 4
    base = (F0 + 30 + j(4), TRACK_TOP - 4)
    # the main stem: up the side into a big volute
    if v == 0:
        sp = bezier(base, (F0 + 2, 330 + j(20)), (F0 + 190 + j(15), 440 + j(20)), (F0 + 96 + j(8), 600 + j(15)))
        main, sl = tendril(sp, 21, 7, curl_r=60 + j(5), turns=1.3, direction=1, ball=9)
    elif v == 1:
        sp = bezier(base, (F0 + 170 + j(15), 290 + j(20)), (F0 - 5, 450 + j(20)), (F0 + 70 + j(8), 640 + j(15)))
        main, sl = tendril(sp, 21, 7, curl_r=56 + j(5), turns=1.3, direction=1, ball=9)
    elif v == 2:
        sp = bezier(base, (F0 + 10, 380 + j(20)), (F0 + 150 + j(15), 520 + j(20)), (F0 + 80 + j(8), 680 + j(15)))
        main, sl = tendril(sp, 21, 7, curl_r=54 + j(5), turns=1.25, direction=1, ball=9)
    else:
        sp = bezier(base, (F0 + 120 + j(15), 260 + j(20)), (F0 + 10, 400 + j(20)), (F0 + 60 + j(8), 560 + j(15)))
        main, sl = tendril(sp, 21, 7, curl_r=58 + j(5), turns=1.35, direction=1, ball=9)
    parts.append(main)
    ns = len(sp)
    parts += leaves_on(rng, sl[:ns], [0.2, 0.55] if v % 2 else [0.3, 0.62], size=1.0)
    # a branch from the stem sweeping towards the middle and up, curling into a volute
    i = int(ns * (0.4 + j(0.04)))
    p = sp[i]
    if v in (0, 3):
        br = bezier(p, (p[0] + 150, p[1] + 10 + j(15)), (p[0] + 200 + j(15), p[1] + 190), (p[0] + 135 + j(10), p[1] + 280))
        b, bl = tendril(br, 13, 5.5, curl_r=40 + j(4), turns=1.2, direction=1)
    else:
        br = bezier(p, (p[0] + 100, p[1] - 50 + j(15)), (p[0] + 240 + j(15), p[1] - 20), (p[0] + 240 + j(10), p[1] + 100))
        b, bl = tendril(br, 13, 5.5, curl_r=36 + j(4), turns=1.2, direction=1)
    parts.append(b)
    parts += leaves_on(rng, bl[:len(br)], [0.5], size=0.8, side=-1)
    parts.append(sprig(rng, bl, 0.3, 46 + j(6), 1, kind='star'))
    # the crest: out of the top rule beside the corner block, towards the middle, curling down
    t0 = (F0 + CORNER - 4, F1 + 2)
    if v in (0, 1):
        cr = bezier(t0, (F0 + 170, F1 - 4), (F0 + 300 + j(15), F1 - 6), (EM / 2 - 60 + j(8), F1 - 70))
        c, cl = tendril(cr, 12, 5, curl_r=32 + j(3), turns=1.2, direction=-1)
    else:
        cr = bezier(t0, (F0 + 180, F1 - 10), (F0 + 220 + j(15), F1 - 70), (EM / 2 - 90 + j(8), F1 - 60))
        c, cl = tendril(cr, 12, 5, curl_r=28 + j(3), turns=1.25, direction=1)
    parts.append(c)
    parts += leaves_on(rng, cl[:len(cr)], [0.45], size=0.7, side=-1)
    # along the bottom: from the stem foot towards the middle, curling up
    bt = bezier(base, (F0 + 150, TRACK_TOP + 2), (F0 + 270 + j(15), TRACK_TOP + 4), (EM / 2 - 85 + j(10), TRACK_TOP + 62 + j(8)))
    d, dl = tendril(bt, 14, 5, curl_r=30 + j(3), turns=1.2, direction=1)
    parts.append(d)
    parts += leaves_on(rng, dl[:len(bt)], [0.42], size=0.72, side=1)
    # shoots with buds
    parts.append(sprig(rng, sl, 0.48 + j(0.04), 50 + j(6), -1, kind='star' if v % 2 else 'berry'))
    parts.append(sprig(rng, cl, 0.62, 38, 1, kind='ball'))
    return parts

def mirror(path):
    return transformed(path, (-1, 0, 0, 1, EM, 0))

# ---------------------------------------------------------------- the sky: hatching, the window with the moon, stars

HATCH = 14          # pitch of the night-sky hatching
HATCH_W = 5         # its line weight (whole units, so nothing shifts when coordinates are rounded)
CLEAR = 6.0        # the clear rim around anything set into the hatching

def hatching(region):
    x0, y0, x1, y1 = region.bounds
    lines = []
    # whole-unit positions with a slight, fixed irregularity in the spacing, as in an engraving: a perfectly
    # even pitch beats against the pixel grid at middle sizes and shows as broad bands
    jit = [0, 1, -1, 1, 0, -1, 1, 0, -1, 0, 1, -1]
    y, k = F0 + 2, 0
    while y < y1:
        if y + HATCH_W > y0: lines.append(rect(x0 - 2, y, x1 + 2, y + HATCH_W))
        y += HATCH + jit[k % len(jit)]; k += 1
    return inter(union(lines), region)

def window(rng, cx, cy, w, h, waxing):
    """A sleeping-car window with a drop sash, the crescent moon and two stars in it.
    Returns (ink, opening) where opening is the clear area inside the frame."""
    r = min(w, h) * 0.2
    x0, y0, x1, y1 = cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2
    outer = rounded_rect(x0, y0, x1, y1, r)
    inner = rounded_rect(x0 + LINE, y0 + LINE, x1 - LINE, y1 - LINE, r - LINE * 0.7)
    fine0 = rounded_rect(x0 + LINE + 4, y0 + LINE + 4, x1 - LINE - 4, y1 - LINE - 4, max(4, r - LINE - 2))
    fine1 = rounded_rect(x0 + LINE + 8, y0 + LINE + 8, x1 - LINE - 8, y1 - LINE - 8, max(3, r - LINE - 6))
    ink = [minus(outer, inner), minus(fine0, fine1)]
    # the drop sash: the lower pane's top rail, low in the window
    sy = y0 + h * 0.3
    ink.append(rect(x0 + LINE, sy, x1 - LINE, sy + 7))
    ink.append(rect(x0 + LINE, sy + 10, x1 - LINE, sy + 12.5))
    # the moon and stars in the upper pane, sized to the pane
    ix0, ix1 = x0 + LINE + 9, x1 - LINE - 9
    iy0, iy1 = sy + 14, y1 - LINE - 9
    iw, ih = ix1 - ix0, iy1 - iy0
    sgn = -1 if waxing else 1
    mr = min(iw * 0.3, ih * 0.34)
    mx = (ix0 + ix1) / 2 + sgn * iw * 0.1
    my = (iy0 + iy1) / 2 + ih * 0.02
    tilt = math.radians(0 if waxing else 180) + rng.uniform(-0.2, 0.2)
    ink.append(crescent(mx, my, mr, phase=0.42, tilt=tilt))
    sx = (ix0 + ix1) / 2 - sgn * iw * 0.3
    ink.append(sparkle(sx, my + ih * 0.22, iw * 0.11, waist=0.22))
    ink.append(sparkle(sx + sgn * iw * 0.06, my - ih * 0.26, iw * 0.075, waist=0.22, rot=0.25))
    # the lower pane: a few lights of a distant town
    if h > 120:
        for k in range(3):
            lx = ix0 + 6 + (iw - 12) * (k + 0.5 + rng.uniform(-0.2, 0.2)) / 3
            ink.append(circle(lx, y0 + LINE + 10 + rng.uniform(0, h * 0.3 - LINE - 22), 2.8))
    return union(ink), outer

def stars_in(rng, free, count=16, rmin=7, rmax=19, spacing=44):
    """Stars reversed out of the hatching: Poisson-disc spots in `free` (a region well inside the hatching)."""
    x0, y0, x1, y1 = free.bounds if free.bounds else (0, 0, 0, 0)
    pts = []
    tries = 0
    out = []
    while len(pts) < count and tries < 4000:
        tries += 1
        x, y = rng.uniform(x0, x1), rng.uniform(y0, y1)
        if not free.contains((x, y)): continue
        r = rng.uniform(rmin, rmax) if rng.random() < 0.35 else rng.uniform(rmin, (rmin + rmax) / 2)
        if any(math.hypot(x - a, y - b) < spacing + r + rb for a, b, rb in pts): continue
        # the whole star must sit in the free area
        if not all(free.contains((x + r * math.cos(t), y + r * math.sin(t))) for t in (0, 1.57, 3.14, 4.71)): continue
        pts.append((x, y, r))
        if r > 10:
            out.append(sparkle(x, y, r + 3, waist=0.22, rot=rng.uniform(-0.12, 0.12)))
        elif r > 7.5:
            out.append(star(x, y, r, inner=0.45, rot=math.pi / 2 + rng.uniform(-0.3, 0.3)))
        else:
            out.append(circle(x, y, r * 0.55))
    return out

# ---------------------------------------------------------------- one initial

HALO = 22   # clear space between letter and contour line

WIN_W, WIN_H = 136, 164

def place_window(lfill, variant):
    """The largest spot for the window: an upper corner if possible, else a lower one or the top middle."""
    near = dilate(lfill, HALO + LINE + 14)
    best = None
    spots = []
    for name in ('ul', 'ur', 'll', 'lr', 'tc'):
        pref = {'ul': 0, 'ur': 0, 'tc': 0.08, 'll': 0.12, 'lr': 0.12}[name]
        if name in ('ul', 'ur'):
            pref += 0.01 if (name == 'ul') == (variant % 2 == 0) else 0
        spots.append((name, pref))
    for name, pref in spots:
        for k in range(14):
            s = 1.0 - 0.03 * k
            w, h = WIN_W * s, WIN_H * s
            cx = {'ul': F0 + 26 + w / 2, 'll': F0 + 26 + w / 2, 'ur': F1 - 26 - w / 2, 'lr': F1 - 26 - w / 2, 'tc': EM / 2}[name]
            cy = (F1 - 26 - h / 2) if name in ('ul', 'ur', 'tc') else (TRACK_TOP + 26 + h / 2)
            if inter(rounded_rect(cx - w / 2 - 12, cy - h / 2 - 12, cx + w / 2 + 12, cy + h / 2 + 12, 30), near).area < 1:
                score = s - pref
                if best is None or score > best[0]: best = (score, s, cx, cy, w, h, name)
                break
    return best

def initial(ch):
    rng = random.Random(ord(ch) * 7919 + 17)
    variant = LETTERS.index(ch) if ch in LETTERS else ord(ch)
    shrink = 1.0
    tries = []
    for attempt in range(5):
        let = letter_path(ch, shrink)
        lfill = fill_holes(let)
        best = place_window(lfill, variant)
        tries.append((best[1] + 0.6 * shrink if best else 0, shrink, let, lfill, best))
        if best and best[1] >= 0.82: break
        shrink *= 0.965
    _, shrink, let, lfill, best = max(tries, key=lambda t: (t[4] is not None and t[4][1] >= 0.82, t[0]))
    score, s, cx, cy, w, h, spot = best
    left = spot in ('ul', 'll') or (spot == 'tc' and variant % 2 == 0)
    corners = union([rect(M, EM - M - CORNER, M + CORNER, EM - M), rect(EM - M - CORNER, EM - M - CORNER, EM - M, EM - M)])
    field = minus(rect(F0, TRACK_TOP, F1, F1), corners)
    half = union(ranken(rng, variant))
    vines = union([half, mirror(half)])
    win_ink, win_open = window(rng, cx, cy, w, h, waxing=not left)
    win_clear = dilate(win_open, CLEAR + 2)
    # the letter: a clear halo, and a fine contour line into which the vines run
    outer = dilate(lfill, HALO + LINE)
    contour = minus(dilate(let, HALO + LINE), dilate(let, HALO))
    vines = minus(vines, union([outer, dilate(win_open, LINE / 2)]))
    vines = inter(vines, field)
    vines = drop_small(vines, 260)
    # the night sky: hatching, kept clear around vines, letter and window, with stars reversed out of it
    sky = hatching(field)
    keep_clear = union([dilate(vines, CLEAR), dilate(lfill, HALO + LINE + CLEAR), win_clear,
                        rect(F0, TRACK_TOP - 10, F1, TRACK_TOP + CLEAR)])
    sky = minus(sky, keep_clear)
    free = minus(inter(field, rect(F0 + 14, TRACK_TOP + 20, F1 - 14, F1 - 14)), dilate(keep_clear, 8))
    stars = stars_in(rng, free)
    sky = minus(sky, union([dilate(s, 3.5) for s in stars]))
    sky = drop_small(sky, 30)
    tr = minus(track(rng), dilate(lfill, HALO + LINE + 2))
    orn = union([frame(), tr, sky, vines, contour, win_ink])
    return orn, let

# ---------------------------------------------------------------- fonts

def glyph_name(c):
    return {'Ä': 'Adieresis', 'Ö': 'Odieresis', 'Ü': 'Udieresis'}.get(c) or \
        ({str(d): n for d, n in enumerate(['zero', 'one', 'two', 'three', 'four', 'five', 'six', 'seven', 'eight', 'nine'])}.get(c)) or c

def tt_glyph(path, fitted=True):
    """A TrueType glyph: fitted cubic curves (or the letter's own curves) as quadratics, outer contours clockwise."""
    from fontTools.pens.ttGlyphPen import TTGlyphPen
    from fontTools.pens.cu2quPen import Cu2QuPen
    from fontTools.pens.reverseContourPen import ReverseContourPen
    pen = TTGlyphPen(None)
    if fitted:
        from geom import draw_tt_safe
        draw_tt_safe(path, ReverseContourPen(pen))
    elif path.bounds and path.bounds[2] > path.bounds[0]:
        from geom import draw_tt_safe, flat
        draw_tt_safe(flat(path, 0.1), ReverseContourPen(pen), err=0.18)
    return pen.glyph()

OFL_NOTE = ('This Font Software is licensed under the SIL Open Font License, Version 1.1. '
            'This license is available with a FAQ at: https://openfontlicense.org')
COPYRIGHT = ('Copyright 2026 The Nachtzug Project Authors. Capitals derived from EB Garamond, '
             'Copyright 2017 The EB Garamond Project Authors (https://github.com/octaviopardo/EBGaramond12).')

def build(name, glyphs, layers=None, palettes=None):
    """glyphs: char -> TT glyph ; layers: char -> [(TT glyph, palette index), ...] for COLR."""
    fam = f'Nachtzug {name}'
    ps = fam.replace(' ', '') + '-Regular'
    fb = FontBuilder(EM, isTTF=True)
    names = {c: glyph_name(c) for c in glyphs}
    order = ['.notdef', 'space', 'nbspace'] + [names[c] for c in glyphs]
    if layers:
        order += [f'{names[c]}.l{i}' for c in glyphs for i in range(len(layers[c]))]
    fb.setupGlyphOrder(order)
    cmap = {0x20: 'space', 0xA0: 'nbspace'}
    for c in glyphs:
        cmap[ord(c)] = names[c]
        if c.lower() != c: cmap[ord(c.lower())] = names[c]   # lower case gives the same initial
    fb.setupCharacterMap(cmap)
    gl = {}
    nd = minus(rect(M, M, EM - M, EM - M), rect(M + 40, M + 40, EM - M - 40, EM - M - 40))
    gl['.notdef'] = tt_glyph(nd, False)
    for sp in ('space', 'nbspace'):
        gl[sp] = tt_glyph(pathops.Path(), False)
    for c, g in glyphs.items():
        gl[names[c]] = g
        if layers:
            for i, (lg, _) in enumerate(layers[c]):
                gl[f'{names[c]}.l{i}'] = lg
    fb.setupGlyf(gl)
    glyf = fb.font['glyf']
    mt = {}
    for n in order:
        g = glyf[n]; g.recalcBounds(glyf)
        mt[n] = (EM // 4 if n in ('space', 'nbspace') else EM, g.xMin if g.numberOfContours else 0)
    fb.setupHorizontalMetrics(mt)
    fb.setupHorizontalHeader(ascent=EM, descent=0, lineGap=0)
    fb.setupNameTable({
        'copyright': COPYRIGHT, 'familyName': fam, 'styleName': 'Regular',
        'uniqueFontIdentifier': f'{VERSION};NZUG;{ps}', 'fullName': f'{fam} Regular',
        'version': f'Version {VERSION}', 'psName': ps,
        'manufacturer': 'The Nachtzug Project', 'designer': 'The Nachtzug Project (ornament); EB Garamond Project (capitals)',
        'description': 'Ornamented night-train initials: riveted frame, rails, moon in a sleeping-car window, stars and tendrils around an Antiqua capital.',
        'licenseDescription': OFL_NOTE, 'licenseInfoURL': 'https://openfontlicense.org',
        'sampleText': 'Nachtzug'})
    fb.setupOS2(sTypoAscender=EM, sTypoDescender=0, sTypoLineGap=0, usWinAscent=EM, usWinDescent=0,
                sCapHeight=EM - M, sxHeight=EM - M, achVendID='NZUG', fsType=0, version=4,
                usWeightClass=400, fsSelection=0x40 | 0x80, ulUnicodeRange1=1 | 2)
    fb.setupPost(isFixedPitch=0)
    fb.setupDummyDSIG = None
    from fontTools.ttLib import newTable
    gasp = newTable('gasp'); gasp.version = 1; gasp.gaspRange = {0xFFFF: 0x000A}   # unhinted: smooth at all sizes
    fb.font['gasp'] = gasp
    if layers:
        fb.setupCPAL(palettes)
        fb.setupCOLR({names[c]: [(f'{names[c]}.l{i}', pi) for i, (_, pi) in enumerate(layers[c])] for c in glyphs})
    path = os.path.join(OUT, f'{ps}.ttf')
    fb.save(path)
    return path

def hexcol(h, a=1.0):
    h = h.lstrip('#')
    return (int(h[0:2], 16) / 255, int(h[2:4], 16) / 255, int(h[4:6], 16) / 255, a)

PALETTES = [
    [hexcol('#2b2623'), hexcol('#a5481f')],     # ink and deep rust, for paper and enamel
    [hexcol('#3b2a1b'), hexcol('#d8642a')],     # display brown and amber, for the ivory glass
    [hexcol('#ebe5d8'), hexcol('#d8642a')],     # enamel and amber, for dark grounds
]

def make_all(which):
    frames, letters, both = {}, {}, {}
    for c in which:
        print('initial', c, flush=True)
        o, l = initial(c)
        frames[c] = tt_glyph(o, True)
        letters[c] = tt_glyph(l, False)
        both[c] = tt_glyph(union([o, l]), True)
    print(build('Initialen', both))
    print(build('Initialen Rahmen', frames))
    print(build('Initialen Buchstabe', letters))
    print(build('Initialen Farbe', both, layers={c: [(frames[c], 0), (letters[c], 1)] for c in which}, palettes=PALETTES))

if __name__ == '__main__':
    which = sys.argv[1:] or LETTERS
    make_all(which)
