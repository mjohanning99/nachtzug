#!/usr/bin/env python3
"""Nachtzug Grotesk: the interface face of Nachtzug, derived from Jost (SIL Open Font License 1.1).

What is ours: a taller x-height (495 instead of 460, with the horizontals kept at Jost's thickness by borrowing their
y-coordinates from the neighbouring weight), a z whose foot turns down under the baseline into a hook (the sans sister
of the Antiqua's tailed z), a g whose ear is a small raised semaphore arm with its disc, an l with a rail-like foot so
that I, l and 1 never meet, a t whose crossbar runs on into a following z as one rail (liga), the narrow no-break
space and the other typographic spaces, and the symbols the program writes (triangles, squares, discs, arrows, undo
and redo, the copy squares, check, eighth note, gear, command key) drawn in the weight of the letters.
Everything else is Jost.

    python build_grotesk.py [400 500 600]     writes out/NachtzugGrotesk-<Style>-unhinted.ttf
"""
import math, os, sys, copy, unicodedata
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(1, os.path.dirname(HERE))          # geom.py lives next to us or one folder up (tools/fonts)

from fontTools.ttLib import TTFont
from fontTools.ttLib.tables._g_l_y_f import Glyph, GlyphCoordinates
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.pens.cu2quPen import Cu2QuPen
from fontTools.pens.reverseContourPen import ReverseContourPen
from fontTools.pens.recordingPen import DecomposingRecordingPen
from fontTools.ttLib.removeOverlaps import removeOverlaps
from fontTools.feaLib.builder import addOpenTypeFeaturesFromString

from geom import (contours_of, from_contours, poly, rect, circle, stroke, bezier, union, minus, inter, simplify,
                  draw_fitted, transformed, dilate, signed_area)
import symbols as SYM

_REPO = os.path.normpath(os.path.join(HERE, '..', '..', '..', 'assets', 'fonts'))
JOST = os.environ.get('JOST_DIR', _REPO)
OUT = os.path.join(HERE, 'out')
VERSION = 0.1
STYLES = {400: 'Regular', 500: 'Medium', 600: 'SemiBold'}
PARTNER = {400: 500, 500: 400, 600: 500}          # where the thinner horizontals are borrowed from

XH0, XH = 460, 495                 # Jost's x-height and ours
ASC0, ASC = 780, 795               # ascender
S = XH / XH0
Y1, Y2 = 478, 540                  # linear up to Y1 (covers the overshoot), slope eases to A above Y2

def _slope_above():
    # f(ASC0) = ASC with f linear (slope S) to Y1, slope ramping S -> a over Y1..Y2, slope a above
    return (ASC - S * Y1 - (Y2 - Y1) * S / 2) / ((Y2 - Y1) / 2 + (ASC0 - Y2))
A = _slope_above()

def fmap(y):
    """The vertical map for lowercase: descenders unchanged, the x-height zone stretched, ascenders eased."""
    y = np.asarray(y, float)
    out = np.where(y <= 0, y, S * y)
    r = (y - Y1) / (Y2 - Y1)
    mid = S * Y1 + (y - Y1) * S + (A - S) * (y - Y1) * r / 2
    out = np.where((y > Y1) & (y <= Y2), mid, out)
    top = S * Y1 + (Y2 - Y1) * (S + A) / 2 + (y - Y2) * A
    out = np.where(y > Y2, top, out)
    return out

# ---------------------------------------------------------------- glyph helpers

class G:
    def __init__(self, font):
        self.font = font
        self.glyf = font['glyf']
        self.hmtx = font['hmtx']
        self.poly = {}                 # the polygon shapes of glyphs drawn here, before curve fitting

    def gs(self):
        return self.font.getGlyphSet()

    def path(self, name, dx=0, dy=0):
        gs = self.gs()
        rec = DecomposingRecordingPen(gs)
        gs[name].draw(rec)
        p = simplify(from_contours(contours_of(rec, 0.15)))
        return transformed(p, (1, 0, 0, 1, dx, dy)) if dx or dy else p

    def width(self, name):
        return self.hmtx[name][0]

    def _ensure(self, name):
        if name not in self.glyf.glyphs:
            order = self.font.getGlyphOrder()
            order.append(name)
            self.font.setGlyphOrder(order)

    def put(self, name, path, width, err=0.3):
        """Writes a polygon shape as a TrueType glyph (fitted cubics converted to quadratics, whole units)."""
        path = clean(path)
        self.poly[name] = path
        pen = TTGlyphPen(None)
        draw_fitted(path, Cu2QuPen(ReverseContourPen(pen), max_err=0.4), err=err, corner=40,
                    rnd=lambda p: (round(p[0]), round(p[1])))
        g = pen.glyph()
        self._ensure(name)
        self.glyf[name] = g
        g.recalcBounds(self.glyf)
        self.hmtx[name] = (int(round(width)), int(g.xMin) if g.numberOfContours else 0)

    def empty(self, name, width):
        g = Glyph(); g.numberOfContours = 0
        self._ensure(name)
        self.glyf[name] = g
        self.hmtx[name] = (int(width), 0)

    def copy(self, src, dst):
        self._ensure(dst)
        self.glyf[dst] = copy.deepcopy(self.glyf[src])
        self.hmtx[dst] = self.hmtx[src]

def clean(path, min_area=250):
    """Drops crumbs: islands and holes smaller than min_area square units."""
    import pyclipper
    from geom import _to_cl, _exec, _from_cl
    cl = _exec(_to_cl(path), None, pyclipper.CT_UNION)
    keep = [c for c in cl if abs(pyclipper.Area(c)) / (256.0 ** 2) >= min_area]
    return _from_cl(keep) if keep else path

def fix_direction(font):
    """TrueType wants outer contours clockwise; Jost's are anticlockwise. Reverses whole glyphs where needed."""
    glyf = font['glyf']
    gs = font.getGlyphSet()
    n_rev = 0
    for n in font.getGlyphOrder():
        g = glyf[n]
        if g.numberOfContours <= 0: continue
        cs = contours_of(gs[n], 1.0)
        if sum(signed_area(c) for c in cs) > 0:
            pen = TTGlyphPen(None)
            gs[n].draw(ReverseContourPen(pen))
            ng = pen.glyph(); ng.recalcBounds(glyf)
            glyf[n] = ng
            n_rev += 1
    return n_rev

def decompose_all(font):
    glyf = font['glyf']
    gs = font.getGlyphSet()
    for n in font.getGlyphOrder():
        g = glyf[n]
        if g.isComposite():
            pen = TTGlyphPen(gs)
            rec = DecomposingRecordingPen(gs); gs[n].draw(rec); rec.replay(pen)
            ng = pen.glyph(); ng.recalcBounds(glyf)
            glyf[n] = ng
    for n in font.getGlyphOrder():      # TTGlyphPen leaves no composites; refresh bounds
        glyf[n].recalcBounds(glyf)

def contour_ranges(g):
    s = 0
    for e in g.endPtsOfContours:
        yield s, e + 1
        s = e + 1

def crossings(path, y=None, x=None):
    out = []
    for c in contours_of(path, 0.2):
        for i in range(len(c)):
            (x0, y0), (x1, y1) = c[i], c[(i + 1) % len(c)]
            if y is not None and ((y0 <= y < y1) or (y1 <= y < y0)):
                out.append(x0 + (y - y0) * (x1 - x0) / (y1 - y0))
            if x is not None and ((x0 <= x < x1) or (x1 <= x < x0)):
                out.append(y0 + (x - x0) * (y1 - y0) / (x1 - x0))
    return sorted(out)

# ---------------------------------------------------------------- the taller x-height

def lowercase_names(font):
    cmap = font.getBestCmap()
    names = set()
    for u, n in cmap.items():
        cat = unicodedata.category(chr(u))
        if cat == 'Ll' and u not in (0xAA, 0xBA):
            names.add(n)
    order = font.getGlyphOrder()
    names |= {n for n in order if n.endswith('.alt')}
    names |= {'dotlessi', 'uni0237', 'napostrophe'} & set(order)
    return names

def accent_names(font):
    cmap = font.getBestCmap()
    return {n for u, n in cmap.items() if unicodedata.category(chr(u)) in ('Mn', 'Sk') and u not in (0x5E, 0x60, 0x7E)}

def raise_x_height(font, partner, H, Hp):
    """Lowercase: y-coordinates are first moved towards the partner weight so the horizontals end up as thick as
    Jost's after the stretch, then mapped by fmap. Dots and accents (contours wholly above Y2) only move up, so
    they stay round; anything below the baseline is left alone."""
    t = H * (1 / S - 1) / (Hp - H)
    glyf, pglyf = font['glyf'], partner['glyf']
    lc = lowercase_names(font)
    acc = accent_names(font)
    for n in sorted(lc | acc):
        g, pg = glyf[n], pglyf[n]
        if g.numberOfContours <= 0: continue
        c = np.array(g.coordinates, float)
        pc = np.array(pg.coordinates, float)
        for s, e in contour_ranges(g):
            ys = c[s:e, 1]
            if ys.min() >= Y2 - 10 or n in acc:
                cy = (ys.min() + ys.max()) / 2
                if ys.max() <= 0: continue
                c[s:e, 1] = ys + (fmap(cy) - cy)
                continue
            pys = pc[s:e, 1]
            blended = np.where(ys > 0, ys + t * (pys - ys), ys)
            c[s:e, 1] = fmap(blended)
        g.coordinates = GlyphCoordinates([(int(round(x)), int(round(y))) for x, y in c])
        g.recalcBounds(glyf)
    # punctuation that sits on the x-height
    lift = (XH - XH0) / 2
    cmap = font.getBestCmap()
    for u in (0x2D, 0xAD, 0x2010, 0x2011, 0x2012, 0x2013, 0x2014, 0xB7):
        n = cmap.get(u)
        if n: shift_glyph(glyf[n], 0, lift, glyf)
    for u in (0xAB, 0xBB, 0x2039, 0x203A):          # guillemets grow with the x-height
        n = cmap.get(u)
        if n:
            g = glyf[n]
            g.coordinates = GlyphCoordinates([(x, int(round(float(fmap(y))))) for x, y in g.coordinates]); g.recalcBounds(glyf)
    for u in (0x3A, 0x3B, 0x2022):                   # colon, semicolon, bullet: the upper part moves up
        n = cmap.get(u)
        if not n: continue
        g = glyf[n]
        c = np.array(g.coordinates, float)
        for s, e in contour_ranges(g):
            cy = (c[s:e, 1].min() + c[s:e, 1].max()) / 2
            if cy > 230: c[s:e, 1] += fmap(cy) - cy
        g.coordinates = GlyphCoordinates([(int(round(x)), int(round(y))) for x, y in c]); g.recalcBounds(glyf)

def shift_glyph(g, dx, dy, glyf):
    if g.numberOfContours <= 0: return
    g.coordinates = GlyphCoordinates([(int(round(x + dx)), int(round(y + dy))) for x, y in g.coordinates])
    g.recalcBounds(glyf)

# ---------------------------------------------------------------- our letters

class Metrics:
    def __init__(self, g):
        l = g.path('l')
        xs = crossings(l, y=300)
        self.V = xs[1] - xs[0]                     # vertical stem
        z = g.path('z')
        ys = crossings(z, x=g.width('z') * 0.6)
        self.H = ys[1] - ys[0]                     # horizontal bar (bottom bar of z)
        self.Ht = ys[-1] - ys[-2]                  # top bar of z

def make_z(g, m, name='z', src='z'):
    """z: the foot runs on to the right, turns down under the baseline and hooks back, like the Antiqua's."""
    W = g.width(src)
    shape = g.path(src)
    bar = crossings(shape, y=m.H / 2)
    x_right = bar[-1]
    V, H = m.V, m.H
    wv = (V + H) / 2 * (1.02 - (V - 80) * 0.001)                       # the down stroke, a touch heavier than the bar
    xr = x_right - wv / 2 + 4                        # centreline of the down stroke
    R1 = 70 + (V - 80) * 0.6                         # the corner, centreline radius
    xc = xr - R1 - 12
    shape = minus(shape, rect(xc, -400, W + 400, H + 1))
    yb = H / 2
    R2 = R1 * 0.86
    y_down = yb - R1 - 10                            # the straight part of the down stroke ends here
    bottom = -168 - (V - 80) * 0.6                   # outer edge of the hook
    yc2 = bottom + H * 0.42 + R2                     # centre of the lower turn
    pts = [(xc - 30, yb), (xr - R1, yb)]
    for i in range(1, 25):
        a = math.pi / 2 - math.pi / 2 * i / 24
        pts.append((xr - R1 + R1 * math.cos(a), yb - R1 + R1 * math.sin(a)))
    pts.append((xr, yc2))
    for i in range(1, 25):
        a = -math.pi / 2 * i / 24
        pts.append((xr - R2 + R2 * math.cos(a), yc2 + R2 * math.sin(a)))
    end_x = xr - R2 - max(40, V * 0.5)
    pts.append((end_x, yc2 - R2))
    arc = np.concatenate([[0], np.cumsum(np.hypot(*np.diff(np.array(pts), axis=0).T))])
    L = arc[-1]
    k1 = (30 + (xr - R1 - xc)) / L                     # end of the bar part
    k2 = (30 + (xr - R1 - xc) + R1 * math.pi / 2) / L  # start of the straight down part
    k3 = (30 + (xr - R1 - xc) + R1 * math.pi / 2 + (yb - R1 - yc2)) / L
    def widths(tt):
        w = np.interp(tt, [0, k1, k2, k3, 1.0], [H, H, wv, wv * 0.95, H * (0.86 - (V - 80) * 0.002)])
        return w
    tail = stroke(pts, 0, widths=widths, cap='butt', clamp=0.95)
    shape = union([shape, tail])
    g.put(name, shape, W)

def make_g(g, m, name='g', src='g'):
    """g: a small raised semaphore arm with its round disc on the top of the stem."""
    W = g.width(src)
    shape = g.path(src)
    xs = crossings(shape, y=XH - 60)
    s0, s1 = xs[-2], xs[-1]                          # the stem
    V, H = m.V, m.H
    ang = math.radians(24)
    aw = H * 0.98
    Sx, Sy = (s0 + s1) / 2, XH - aw * 0.55
    Ln = 58 + V * 0.45
    C = (Sx + Ln * math.cos(ang), Sy + Ln * math.sin(ang))
    rd = aw * 0.5 + 9 + (V - 80) * 0.08
    arm = stroke([(Sx, Sy), C], aw, aw, cap='butt')
    disc = circle(C[0], C[1], rd)
    shape = union([shape, arm, disc])
    extra = max(0, C[0] + rd + 12 - W)
    g.put(name, shape, W + extra)
    return extra

def make_l(g, m, name='l', src='l'):
    """l: the stem turns into a short rail-like foot to the right, so l is never taken for I or 1."""
    W = g.width(src)
    shape = g.path(src)
    xs = crossings(shape, y=300)
    x0, x1 = xs[0], xs[1]
    V, H = m.V, m.H
    cx = (x0 + x1) / 2
    R = max(52, V * 0.7 + 8)
    hb = H * 0.92                                   # thickness of the foot
    yb = hb / 2 - 7                                  # a little overshoot, like a round letter
    shape = minus(shape, rect(x0 - 50, -100, x1 + 400, yb + R + 6))
    pts = [(cx, yb + R + 30), (cx, yb + R)]
    for i in range(1, 25):
        a = math.pi + math.pi / 2 * i / 24
        pts.append((cx + R + R * math.cos(a), yb + R + R * math.sin(a)))
    end_x = x1 + max(54, V * 0.62)
    pts.append((end_x, yb))
    arc = np.concatenate([[0], np.cumsum(np.hypot(*np.diff(np.array(pts), axis=0).T))])
    k = (30 + R * math.pi / 2) / arc[-1]
    foot = stroke(pts, 0, widths=lambda tt: np.interp(tt, [0, 30 / arc[-1], k, 1], [V, V, hb, hb * 0.98]), cap='butt', clamp=0.95)
    shape = union([shape, foot])
    adv = W + round(end_x - x1 - V * 0.25)
    g.put(name, shape, adv)
    return adv - W

def make_tz(g, m, kern_tz=0):
    """t_z: the t's crossbar runs on into the z's top bar, one rail."""
    tw = g.width('t')
    off = tw + kern_tz
    t = g.path('t')
    z = transformed(g.poly['z'], (1, 0, 0, 1, off, 0))
    tb = crossings(t, x=tw * 0.92)                   # the crossbar at its right end
    zb = crossings(z, x=off + g.width('z') * 0.5)    # z's top bar
    # one rail at one height: the z's top bar is a little thicker than the t's crossbar in the heavier weights, so the
    # rail takes the thicker of the two along its whole length, from the crossbar's left end to the z (taking only
    # where the two overlap left a step underneath where they met)
    y0 = min(tb[0], zb[-2]); y1 = max(tb[1], zb[-1])
    tl = crossings(t, y=(tb[0] + tb[1]) / 2)[0]      # where the crossbar starts, left of the stem
    zl = crossings(z, y=(zb[-2] + zb[-1]) / 2)[0]
    bridge = rect(tl, y0, zl + 20, y1)
    g.put('t_z', union([t, z, bridge]), off + g.width('z'))

def derived(g, base, new_base_name, names, orig):
    """Accented forms: the contours that were the old base letter give way to the new one."""
    bc = [c for c in contours_of(orig[base], 0.5)]
    def key(c):
        xs = [p[0] for p in c]; ys = [p[1] for p in c]
        return (round(min(xs)), round(min(ys)), round(max(xs)), round(max(ys)))
    bkeys = [key(c) for c in bc]
    newp = g.poly.get(new_base_name) or g.path(new_base_name)
    for n in names:
        if n not in g.glyf.glyphs: continue
        cs = contours_of(orig[n], 0.5)
        keys = [key(c) for c in cs]
        # find the x offset at which every base contour appears
        found = None
        for dx in sorted(set(k[0] - bkeys[0][0] for k in keys)):
            ok = all(any(abs(k[0] - (b[0] + dx)) <= 1 and abs(k[1] - b[1]) <= 1 and abs(k[2] - (b[2] + dx)) <= 1 and abs(k[3] - b[3]) <= 1 for k in keys) for b in bkeys)
            if ok: found = dx; break
        if found is None:
            print('   ! could not find the base in', n); continue
        rest = [c for c, k in zip(cs, keys) if not any(abs(k[0] - (b[0] + found)) <= 1 and abs(k[1] - b[1]) <= 1 and abs(k[2] - (b[2] + found)) <= 1 and abs(k[3] - b[3]) <= 1 for b in bkeys)]
        shape = union([transformed(newp, (1, 0, 0, 1, found, 0))] + [simplify(from_contours([c])) for c in rest])
        g.put(n, shape, g.width(n) + (g.width(new_base_name) - g.width(base + '.jost')))

def snapshot(g, names):
    """Polygon copies of glyphs as they are now (for derived())."""
    out = {}
    gs = g.gs()
    for n in names:
        if n in g.glyf.glyphs:
            rec = DecomposingRecordingPen(gs); gs[n].draw(rec)
            out[n] = rec
    return out

# ---------------------------------------------------------------- OpenType tables

def lookup_subtables(lk):
    for st in lk.SubTable:
        yield (st.ExtSubTable, st.ExtensionLookupType) if lk.LookupType == 9 else (st, lk.LookupType)

def alias_kerning(font, new, left_like, right_like):
    """Gives a new glyph the kerning of existing ones: right_like when it comes first, left_like when second."""
    gpos = font['GPOS'].table
    for lk in gpos.LookupList.Lookup:
        for st, typ in lookup_subtables(lk):
            if typ != 2: continue
            cov = st.Coverage.glyphs
            if st.Format == 1:
                if right_like in cov and new not in cov:
                    i = cov.index(right_like)
                    st.PairSet.append(copy.deepcopy(st.PairSet[i])); cov.append(new)
                for ps in st.PairSet:
                    add = [copy.deepcopy(r) for r in ps.PairValueRecord if r.SecondGlyph == left_like]
                    for r in add:
                        r.SecondGlyph = new; ps.PairValueRecord.append(r)
                    ps.PairValueCount = len(ps.PairValueRecord)
                st.PairSetCount = len(st.PairSet)
            else:
                if right_like in cov and new not in cov:
                    cov.append(new)
                    c1 = st.ClassDef1.classDefs.get(right_like)
                    if c1: st.ClassDef1.classDefs[new] = c1
                c2 = st.ClassDef2.classDefs.get(left_like)
                if c2: st.ClassDef2.classDefs[new] = c2
    order = {n: i for i, n in enumerate(font.getGlyphOrder())}
    for lk in gpos.LookupList.Lookup:
        for st, typ in lookup_subtables(lk):
            if typ != 2: continue
            if st.Format == 1:
                pairs = sorted(zip(st.Coverage.glyphs, st.PairSet), key=lambda p: order[p[0]])
                st.Coverage.glyphs = [p[0] for p in pairs]; st.PairSet = [p[1] for p in pairs]
                for ps in st.PairSet: ps.PairValueRecord.sort(key=lambda r: order[r.SecondGlyph])
            else:
                st.Coverage.glyphs.sort(key=lambda n: order[n])

def pair_kern(font, a, b):
    gpos = font['GPOS'].table
    for lk in gpos.LookupList.Lookup:
        for st, typ in lookup_subtables(lk):
            if typ != 2 or a not in st.Coverage.glyphs: continue
            if st.Format == 1:
                ps = st.PairSet[st.Coverage.glyphs.index(a)]
                for r in ps.PairValueRecord:
                    if r.SecondGlyph == b: return getattr(r.Value1, 'XAdvance', 0) or 0
            else:
                c1 = st.ClassDef1.classDefs.get(a, 0); c2 = st.ClassDef2.classDefs.get(b, 0)
                v = st.Class1Record[c1].Class2Record[c2].Value1
                return getattr(v, 'XAdvance', 0) or 0 if v else 0
    return 0

def build_gsub(font, ss01, tnum, classic):
    def sub_block(m):
        return '\n'.join(f'    sub {a} by {b};' for a, b in m.items())
    fea = f"""
languagesystem DFLT dflt;
languagesystem latn dflt;
languagesystem cyrl dflt;
languagesystem grek dflt;

lookup TNUM {{
{sub_block(tnum)}
}} TNUM;
lookup SS01 {{
{sub_block(ss01)}
}} SS01;
lookup SS02 {{
{sub_block(classic)}
}} SS02;
lookup RAIL {{
    sub t z by t_z;
}} RAIL;

feature tnum {{ lookup TNUM; }} tnum;
feature ss01 {{
    featureNames {{ name "Double-storey a"; name 3 1 0x407 "Doppelstöckiges a"; }};
    lookup SS01;
}} ss01;
feature ss02 {{
    featureNames {{ name "Jost z, g and l"; name 3 1 0x407 "z, g und l von Jost"; }};
    lookup SS02;
}} ss02;
feature liga {{ lookup RAIL; }} liga;
feature dlig {{ lookup RAIL; }} dlig;
"""
    if 'GSUB' in font: del font['GSUB']
    addOpenTypeFeaturesFromString(font, fea, tables=['GSUB', 'name'])

def set_cmap(font, extra):
    from fontTools.ttLib.tables._c_m_a_p import CmapSubtable
    best = dict(font.getBestCmap()); best.update(extra)
    tables = []
    for pid, eid, fmt in ((0, 3, 4), (3, 1, 4)):
        t = CmapSubtable.newSubtable(fmt); t.platformID = pid; t.platEncID = eid; t.language = 0
        t.cmap = {u: g for u, g in best.items() if u <= 0xFFFF}
        tables.append(t)
    font['cmap'].tables = tables

OFL_NOTE = ('This Font Software is licensed under the SIL Open Font License, Version 1.1. '
            'This license is available with a FAQ at: https://openfontlicense.org')
COPYRIGHT = ('Copyright 2026 The Nachtzug Project Authors. Based on Jost, '
             'Copyright 2020 The Jost Project Authors (https://github.com/indestructible-type/Jost).')

def set_names(font, weight):
    name = font['name']
    used = set()
    for fr in font['GSUB'].table.FeatureList.FeatureRecord:
        p = fr.Feature.FeatureParams
        if p is not None and hasattr(p, 'UINameID'): used.add(p.UINameID)
    if 'STAT' in font: del font['STAT']
    name.names = [n for n in name.names if n.nameID in used]
    fam = 'Nachtzug Grotesk'
    style = STYLES[weight]
    ps = 'NachtzugGrotesk-' + style
    legacy_fam = fam if weight == 400 else f'{fam} {style}'
    rec = {0: COPYRIGHT, 1: legacy_fam, 2: 'Regular', 3: f'{VERSION:.3f};NZUG;{ps}', 4: f'{fam} {style}',
           5: f'Version {VERSION:.3f}', 6: ps, 8: 'The Nachtzug Project',
           9: 'The Nachtzug Project; Jost by Owen Earl (indestructible type*)',
           10: 'The interface face of Nachtzug: Jost with a taller x-height, a hooked z, a semaphore g, a railed l, '
               'a tz rail ligature and the symbols of the program.',
           13: OFL_NOTE, 14: 'https://openfontlicense.org'}
    if weight != 400:
        rec[16] = fam; rec[17] = style
    for k, v in rec.items():
        name.setName(v, k, 3, 1, 0x409)

def finish_metrics(font, weight):
    os2 = font['OS/2']
    os2.usWeightClass = weight
    os2.achVendID = 'NZUG'
    os2.fsType = 0
    os2.fsSelection = (1 << 7) | (1 << 6)
    os2.sxHeight = XH
    os2.sCapHeight = 700
    font['head'].macStyle = 0
    font['head'].fontRevision = VERSION
    os2.recalcUnicodeRanges(font)
    cm = font.getBestCmap()
    os2.usFirstCharIndex = min(cm); os2.usLastCharIndex = min(0xFFFF, max(cm))
    os2.usMaxContext = 2
    font['post'].formatType = 2.0

# ---------------------------------------------------------------- one weight

def build(weight):
    font = TTFont(os.path.join(JOST, f'Jost-{weight}.ttf'))
    partner = TTFont(os.path.join(JOST, f'Jost-{PARTNER[weight]}.ttf'))
    decompose_all(font); decompose_all(partner)
    g = G(font)
    m0 = Metrics(g)
    mp = Metrics(G(partner))
    n_orig = len(font.getGlyphOrder())
    raise_x_height(font, partner, m0.H, mp.H)
    m = Metrics(g)
    print(f'  {weight}: stem {m.V:.1f}  bar {m0.H:.1f} -> {m.H:.1f}  (slope above x-height {A:.3f})')
    # Jost's own forms stay available (ss02)
    zg_l = {'z': ['zacute', 'zdotaccent', 'zcaron'],
            'g': ['gcircumflex', 'gbreve', 'gdotaccent', 'uni0123'],
            'l': ['lacute', 'uni013C', 'lcaron', 'ldot', 'lslash']}
    classic = {}
    for base, ders in zg_l.items():
        for n in [base] + ders:
            g.copy(n, n + '.jost'); classic[n] = n + '.jost'
    orig = snapshot(g, [n for b, d in zg_l.items() for n in [b] + d])
    kern_tz = pair_kern(font, 't', 'z')
    make_z(g, m)
    make_g(g, m)
    make_l(g, m)
    for base, ders in zg_l.items():
        derived(g, base, base, ders, orig)
    make_tz(g, m, kern_tz)
    # spaces
    cmap0 = font.getBestCmap()
    thin = 140 if weight < 600 else 150
    spaces = {0x2002: 500, 0x2003: 1000, 0x2004: 333, 0x2005: 250, 0x2006: 167, 0x2007: g.width(cmap0[0x30]),
              0x2008: g.width(cmap0[0x2E]), 0x2009: thin, 0x200A: thin // 2, 0x202F: thin, 0x205F: 222}
    extra = {}
    for u, w in spaces.items():
        n = 'uni%04X' % u
        g.empty(n, w); extra[u] = n
    # symbols
    for name, uni, fn in SYM.SYMBOLS:
        shape, adv = fn(m.V, m.H, weight)
        g.put(name, shape, adv)
        for u in uni: extra[u] = name
    set_cmap(font, extra)
    removeOverlaps(font, ignoreErrors=True)
    fix_direction(font)
    # GDEF classes for new glyphs
    cd = font['GDEF'].table.GlyphClassDef.classDefs
    for n in font.getGlyphOrder()[n_orig:]:
        cd[n] = 2 if n == 't_z' else 1
    # GSUB
    ss01 = {n[:-4]: n for n in font.getGlyphOrder() if n.endswith('.alt')}
    ss01['uni0430'] = 'a.alt'
    tnum = {'zero': 'uniFF10', 'one': 'uniFF11', 'two': 'uniFF12', 'three': 'uniFF13', 'four': 'uniFF14',
            'five': 'uniFF15', 'six': 'uniFF16', 'seven': 'uniFF17', 'eight': 'uniFF18', 'nine': 'uniFF19'}
    build_gsub(font, ss01, tnum, classic)
    for b, c in classic.items():
        alias_kerning(font, c, b, b)
    alias_kerning(font, 't_z', 't', 'z')
    set_names(font, weight)
    finish_metrics(font, weight)
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, f'NachtzugGrotesk-{STYLES[weight]}-unhinted.ttf')
    font.save(path)
    for attempt in range(3):
        if not fix_self_intersections(path, err=0.25 if attempt == 0 else 0.12, polys=g.poly): break
    add_extrema(path)
    return path

def fix_self_intersections(path, err=0.25, polys=None):
    """Asks FontForge which glyphs still cross themselves and redraws those from a clean union."""
    import subprocess
    script = os.path.join(HERE, 'ffselfint.py')
    out = subprocess.run(['fontforge', '-lang=py', '-script', script, path], capture_output=True, text=True).stdout
    bad = [l.split()[1] for l in out.splitlines() if l.startswith('SELFINT')]
    if not bad: return False
    font = TTFont(path)
    g = G(font)
    for n in bad:
        src = (polys or {}).get(n)
        g.put(n, src if src is not None else g.path(n), g.width(n), err=err)
    font.save(path)
    print('  redrawn without self-intersections:', len(bad), ' '.join(bad[:12]))
    return True

def add_extrema(path):
    """Lets FontForge add the points at extrema it misses, and writes those outlines back."""
    import subprocess, json, tempfile
    from fontTools.ttLib.tables._g_l_y_f import flagOnCurve
    with tempfile.TemporaryDirectory() as td:
        js = os.path.join(td, 'x.json')
        subprocess.run(['fontforge', '-lang=py', '-script', os.path.join(HERE, 'ffextrema.py'), path, js],
                       capture_output=True, text=True)
        if not os.path.exists(js): return
        fixed = json.load(open(js))
    if not fixed: return
    font = TTFont(path)
    glyf = font['glyf']
    for n, contours in fixed.items():
        g = Glyph()
        g.numberOfContours = len(contours)
        coords, flags, ends = [], [], []
        for c in contours:
            for x, y, on in c:
                coords.append((int(round(x)), int(round(y)))); flags.append(flagOnCurve if on else 0)
            ends.append(len(coords) - 1)
        g.coordinates = GlyphCoordinates(coords)
        g.flags = bytearray(flags)
        g.endPtsOfContours = ends
        g.program = None
        from fontTools.ttLib.tables import ttProgram
        g.program = ttProgram.Program(); g.program.fromBytecode(b'')
        glyf[n] = g
        g.recalcBounds(glyf)
        adv = font['hmtx'][n][0]
        font['hmtx'][n] = (adv, g.xMin)
    font.save(path)
    print('  extrema added:', len(fixed), ' '.join(sorted(fixed)[:12]))

if __name__ == '__main__':
    ws = [int(a) for a in sys.argv[1:]] or [400, 500, 600]
    for w in ws:
        print(build(w))
