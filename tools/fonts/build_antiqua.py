#!/usr/bin/env python3
"""Nachtzug Antiqua: the text face of Nachtzug, derived from EB Garamond (SIL Open Font License 1.1).

What is ours: a g whose ear is a raised semaphore arm with its disc, a tailed z, rail-joined and catenary ligatures
(tz, ct, st), swash capitals (A E J K L R Z, with EB Garamond's long Q), the narrow no-break space, and a set of
night-train ornaments (moon, stars, locomotive, sleeping car, track, signal, fleurons, window, lantern) in the
Private Use Area and on their Unicode code points. Everything else is EB Garamond, instanced a touch heavier than its
Regular (wght 440) for screens, and at 700 for the Bold.

    python build_antiqua.py        writes antiqua/NachtzugAntiqua-Regular.ttf, -Bold.ttf and NachtzugOrnamente-Regular.otf
"""
import math, os, sys, copy
import pathops
from fontTools.ttLib import TTFont, newTable
from fontTools.ttLib.tables import otTables as ot
from fontTools.ttLib.tables._g_l_y_f import Glyph, GlyphComponent
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.pens.cu2quPen import Cu2QuPen
from fontTools.pens.reverseContourPen import ReverseContourPen
from fontTools.pens.recordingPen import DecomposingRecordingPen
from fontTools.pens.t2CharStringPen import T2CharStringPen
from fontTools.otlLib import builder as otl
from fontTools.ttLib.removeOverlaps import removeOverlaps
from fontTools.fontBuilder import FontBuilder

from geom import (contours_of, from_contours, poly, rect, circle, stroke, bezier, curl, end_tangent, union, minus,
                  inter, simplify, draw_fitted, transformed)
import ornaments as ORN

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, 'antiqua', 'src')
OUT = os.path.join(HERE, 'antiqua')
VERSION = 0.3
DL = os.path.join(os.path.dirname(HERE), 'dl-fonts')

# ---------------------------------------------------------------- glyph helpers

class G:
    def __init__(self, font):
        self.font = font
        self.glyf = font['glyf']
        self.gs = font.getGlyphSet()
        self.hmtx = font['hmtx']

    def path(self, name, dx=0, dy=0):
        rec = DecomposingRecordingPen(self.gs)
        self.gs[name].draw(rec)
        p = from_contours(contours_of(rec, 0.15))
        p = simplify(p)
        return transformed(p, (1, 0, 0, 1, dx, dy)) if dx or dy else p

    def pt(self, name, i):
        g = self.glyf[name]
        c = g.getCoordinates(self.glyf)[0]
        return (float(c[i][0]), float(c[i][1]))

    def width(self, name):
        return self.hmtx[name][0]

    def put(self, name, path, width, err=0.3):
        """Writes a polygon shape as a TrueType glyph (fitted cubic curves converted to quadratics)."""
        pen = TTGlyphPen(None)
        draw_fitted(path, Cu2QuPen(ReverseContourPen(pen), max_err=0.5), err=err, corner=40,
                    rnd=lambda p: (round(p[0], 1), round(p[1], 1)))
        g = pen.glyph()
        if name not in self.glyf.glyphs:
            order = self.font.getGlyphOrder()
            order.append(name)
            self.font.setGlyphOrder(order)
        self.glyf[name] = g
        g.recalcBounds(self.glyf)
        self.hmtx[name] = (int(round(width)), int(g.xMin) if g.numberOfContours else 0)

    def empty(self, name, width):
        g = Glyph(); g.numberOfContours = 0
        if name not in self.glyf.glyphs:
            order = self.font.getGlyphOrder(); order.append(name); self.font.setGlyphOrder(order)
        self.glyf[name] = g
        self.hmtx[name] = (int(width), 0)

    def copy(self, src, dst):
        order = self.font.getGlyphOrder()
        if dst not in self.glyf.glyphs:
            order.append(dst); self.font.setGlyphOrder(order)
        self.glyf[dst] = copy.deepcopy(self.glyf[src])
        self.hmtx[dst] = self.hmtx[src]

def unit(v):
    L = math.hypot(*v) or 1
    return (v[0] / L, v[1] / L)

def tail(pts, w0, w1, ball, ease=1.0):
    """A swash stroke along pts ending in a ball terminal."""
    s = stroke(pts, w0, w1, ease=ease)
    if ball: s = union([s, circle(pts[-1][0], pts[-1][1], ball)])
    return s

# ---------------------------------------------------------------- our letters

def make_g(g):
    """g: the ear becomes a raised semaphore arm with its round disc."""
    W = g.width('g')
    p77, p92, p96, p83, p89, p85, p74 = (g.pt('g', i) for i in (77, 92, 96, 83, 89, 85, 74))
    thick = p83[1] - p89[1]
    shape = g.path('g')
    cut = poly([(p77[0] + 4, p74[1] + 60), (p92[0] + 2, p92[1] - 2), (W + 200, p92[1] - 2), (W + 200, 800), (p77[0] + 4, 800)])
    shape = minus(shape, cut)
    ang = math.radians(24)
    S = (p77[0] - 22, p92[1] + thick * 0.55)
    rd = min(thick * 0.92, 40 + (thick - 43) * 0.45)
    aw = min(thick * 1.25, 54 + (thick - 43) * 0.5)
    C = (p85[0] - rd + 10, 0)
    C = (C[0], S[1] + (C[0] - S[0]) * math.tan(ang))
    arm = stroke([S, C], aw, aw * 0.8, cap='butt')
    shape = union([shape, arm, circle(C[0], C[1], rd)])
    g.put('g', shape, W)

def make_z(g):
    """z: the spur gives way to a tail that turns down under the baseline and ends in a ball."""
    W = g.width('z')
    p57, p73, p56, p60 = (g.pt('z', i) for i in (57, 73, 56, 60))
    th = p57[1] - p73[1]
    shape = g.path('z')
    shape = minus(shape, rect(p57[0] + 10, -400, W + 300, p60[1] + 120))
    y = (p57[1] + p73[1]) / 2
    x0 = p57[0] - 30
    pts = bezier((x0, y), (x0 + 60, y), (W + 26, y - 12), (W - 14, -150))
    c = bezier(pts[-1], (W - 40, -185), (W - 90, -195), (W - 120, -170))
    shape = union([shape, tail(pts + c[1:], th * 1.02, th * 0.4, th * 0.42, ease=1.25)])
    g.put('z', shape, W)

def make_tz(g):
    """t_z: the t's crossbar runs on into the z's top bar, like a rail. The finished fonts were patched afterwards by fix_antiqua_tz.py,
    which smooths small jogs where the bridge met the crossbar and the bar; run it again after a rebuild."""
    tw = g.width('t')
    off = tw - 26
    t = g.path('t')
    z = g.path('z', dx=off)            # the new tailed z
    p29, p35 = g.pt('t', 29), g.pt('t', 35)
    z16, z27, z42 = (g.pt('z.classic', i) for i in (16, 27, 42))
    z = minus(z, rect(off - 60, z27[1] - 30, off + z16[0] - 20, z16[1] - 1))     # the beak goes
    z = minus(z, rect(off - 60, z42[1] - 2, off + z16[0] + 10, z42[1] + 60))
    top = (z42[1] + z16[1]) / 2
    bar_t = (p29[1] + p35[1]) / 2
    bridge = stroke(bezier((p29[0] - 50, bar_t), (p29[0] + 10, bar_t), (off + 40, top), (off + z16[0], top)),
                    p29[1] - p35[1], z42[1] - z16[1], cap='butt')
    g.put('t_z', union([t, z, bridge]), off + g.width('z'))

def catenary(g, first, term_idx_top, term_idx_right, name):
    """c_t / s_t: a fine overhead wire hangs from the first letter's terminal to the top of the t, its mast."""
    fw = g.width(first)
    off = fw - (34 if first == 'c' else 22)
    a = g.path(first)
    t = g.path('t', dx=off)
    top = g.pt(first, term_idx_top)          # the upper edge of the terminal
    t21 = g.pt('t', 21)
    start = (top[0] - 6, top[1] - 10)
    end = (off + t21[0] - 2, t21[1] - 12)
    span = end[0] - start[0]
    sag = 26
    mid1 = (start[0] + span * 0.33, start[1] + (end[1] - start[1]) * 0.18 - sag)
    mid2 = (start[0] + span * 0.7, start[1] + (end[1] - start[1]) * 0.55 - sag * 0.6)
    wire = stroke(bezier(start, mid1, mid2, end), 21, 17)
    hair = g.pt('t', 18)
    shape = union([a, t, wire])
    g.put(name, shape, off + g.width('t'))

def crossings(path, y):
    """x positions where the outline crosses the horizontal line at y, sorted."""
    xs = []
    for c in contours_of(path, 0.3):
        for i in range(len(c)):
            (x0, y0), (x1, y1) = c[i], c[(i + 1) % len(c)]
            if (y0 <= y < y1) or (y1 <= y < y0):
                xs.append(x0 + (y - y0) * (x1 - x0) / (y1 - y0))
    return sorted(xs)

def smooth_join(shape, box, r=10):
    """Rounds off the steps where a swash meets its letter: opening and closing, only inside box."""
    from geom import dilate
    win = rect(*box)
    sm = dilate(dilate(shape, -r), r)          # opening: knocks off small bumps
    sm = union([sm, minus(shape, dilate(win, 2 * r))])
    sm = dilate(dilate(sm, r), -r)             # closing: fills small notches
    return union([minus(shape, win), inter(sm, win)])

def leg_tail(g, name, new, y_hi=230, y_lo=150, y_top=None):
    """R K A: below y_hi the leg is replaced by a stroke that starts higher up inside the leg, follows it with the
    leg's own width, and sweeps out under the baseline into a long tail ending in a ball."""
    import numpy as np
    from geom import drop_small
    W = g.width(name)
    shape = g.path(name)
    y_top = y_top or y_hi + 50
    top, hi, lo = crossings(shape, y_top)[-2:], crossings(shape, y_hi)[-2:], crossings(shape, y_lo)[-2:]
    pt = ((top[0] + top[1]) / 2, y_top); a = ((hi[0] + hi[1]) / 2, y_hi); b = ((lo[0] + lo[1]) / 2, y_lo)
    d = unit((b[0] - pt[0], b[1] - pt[1]))
    w_top = (top[1] - top[0]) * abs(d[1]) * 0.9
    w_cut = (hi[1] - hi[0]) * abs(d[1]) * 1.04
    # where to stop cutting on the left: between the stem's foot serif and the leg's foot
    foot = crossings(shape, 15)
    x_left = (foot[-3] + foot[-2]) / 2 if len(foot) >= 4 else lo[0] - w_cut
    shape = minus(shape, poly([(hi[0] - 1, y_hi), (W + 400, y_hi), (W + 400, -300), (x_left, -300), (x_left, 40),
                               (lo[0] - 4, y_lo - 40)]))
    shape = drop_small(shape, 4000)
    L0 = math.hypot(a[0] - pt[0], a[1] - pt[1])
    p1 = (pt[0] + d[0] * (L0 + 210), pt[1] + d[1] * (L0 + 210))
    end = (W + 120, -34)
    pts = bezier(pt, p1, (end[0] - 160, -80), end)
    arc = np.concatenate([[0], np.cumsum(np.hypot(*np.diff(np.array(pts), axis=0).T))])
    t_cut = L0 / arc[-1]
    def widths(t):
        return np.where(t < t_cut, w_top + (w_cut - w_top) * t / t_cut,
                        w_cut + (16 - w_cut) * np.clip((t - t_cut) / (1 - t_cut), 0, 1) ** 0.9)
    s_ = stroke(pts, 0, widths=widths, cap="butt")
    shape = union([shape, s_, circle(end[0], end[1], 19)])
    shape = smooth_join(shape, (hi[0] - 60, y_hi - 60, hi[1] + 60, y_hi + 60), max(7, w_cut * 0.16))
    g.put(new, shape, W + 50)

def bar_tail(g, name, i_bar_top, i_bar_bot, x_cut, new):
    """E L Z: the bottom bar sweeps on to the right and down, ending in a ball."""
    W = g.width(name)
    shape = g.path(name)
    top, bot = g.pt(name, i_bar_top), g.pt(name, i_bar_bot)
    th = top[1] - bot[1]
    shape = minus(shape, rect(x_cut, -200, W + 400, 260))
    y = (top[1] + bot[1]) / 2
    x0 = x_cut - 50
    end = (W + 105, -62)
    pts = bezier((x0, y), (x0 + 140, y), (W + 30, y - 4), end)
    shape = union([shape, tail(pts, th, 15, 19, ease=1.1)])
    g.put(new, shape, W + 60)

def j_tail(g):
    """J: the descender sweeps further down and to the left, ending in a ball."""
    W = g.width('J')
    shape = g.path('J')
    st = crossings(shape, 66)[:2]
    cx = (st[0] + st[1]) / 2; sw = (st[1] - st[0])
    shape = minus(shape, rect(-400, -400, W + 100, 60))
    pts = bezier((cx, 70), (cx, -150), (cx - 120, -215), (cx - 250, -150))
    shape = union([shape, tail(pts, sw, 16, 20, ease=1.6)])
    g.put('J.swsh', shape, W)

# ---------------------------------------------------------------- OpenType tables

def lookup_subtables(lk):
    for st in lk.SubTable:
        yield (st.ExtSubTable, st.ExtensionLookupType) if lk.LookupType in (7, 9) else (st, lk.LookupType)

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
                    st.PairSet.insert(len(cov), copy.deepcopy(st.PairSet[i]))
                    cov.append(new)
                for ps in st.PairSet:
                    add = [copy.deepcopy(r) for r in ps.PairValueRecord if r.SecondGlyph == left_like]
                    for r in add:
                        r.SecondGlyph = new
                        ps.PairValueRecord.append(r)
                    ps.PairValueCount = len(ps.PairValueRecord)
                st.PairSetCount = len(st.PairSet)
            else:
                if right_like in cov and new not in cov:
                    cov.append(new)
                    c1 = st.ClassDef1.classDefs.get(right_like)
                    if c1: st.ClassDef1.classDefs[new] = c1
                c2 = st.ClassDef2.classDefs.get(left_like)
                if c2: st.ClassDef2.classDefs[new] = c2
    # coverage tables must be sorted by glyph id
    order = {n: i for i, n in enumerate(font.getGlyphOrder())}
    for lk in gpos.LookupList.Lookup:
        for st, typ in lookup_subtables(lk):
            if typ != 2: continue
            if st.Format == 1:
                pairs = sorted(zip(st.Coverage.glyphs, st.PairSet), key=lambda p: order[p[0]])
                st.Coverage.glyphs = [p[0] for p in pairs]; st.PairSet = [p[1] for p in pairs]
                for ps in st.PairSet:
                    ps.PairValueRecord.sort(key=lambda r: order[r.SecondGlyph])
            else:
                st.Coverage.glyphs.sort(key=lambda n: order[n])

def rebuild_gsub(font, swash, classic, sz):
    gsub = font['GSUB'].table
    drop = {'rlig', 'ss01', 'ss02', 'ss03', 'ss04', 'ss05', 'ss06', 'ss07'}
    recs = [r for r in gsub.FeatureList.FeatureRecord]
    keep = [r for r in recs if r.FeatureTag not in drop]
    def new_lookup(st, typ):
        lk = ot.Lookup(); lk.LookupType = typ; lk.LookupFlag = 0; lk.SubTable = [st]; lk.SubTableCount = 1
        gsub.LookupList.Lookup.append(lk); gsub.LookupList.LookupCount = len(gsub.LookupList.Lookup)
        return len(gsub.LookupList.Lookup) - 1
    L_swash = new_lookup(otl.buildSingleSubstSubtable(swash), 1)
    L_classic = new_lookup(otl.buildSingleSubstSubtable(classic), 1)
    L_tz = new_lookup(otl.buildLigatureSubstSubtable({('t', 'z'): 't_z'}), 4)
    def feat(tag, lookups):
        fr = ot.FeatureRecord(); fr.FeatureTag = tag
        fr.Feature = ot.Feature(); fr.Feature.FeatureParams = None
        fr.Feature.LookupListIndex = lookups; fr.Feature.LookupCount = len(lookups)
        return fr
    for r in keep:
        if r.FeatureTag == 'swsh':
            r.Feature.LookupListIndex = [L_swash]; r.Feature.LookupCount = 1
        if r.FeatureTag == 'dlig':
            r.Feature.LookupListIndex = list(r.Feature.LookupListIndex) + [L_tz]; r.Feature.LookupCount = len(r.Feature.LookupListIndex)
    added = [feat('ss01', [L_swash]), feat('ss02', [L_classic])]
    # stylistic set names
    names = {'ss01': 'Swash capitals', 'ss02': 'Garamond g and z'}
    for fr in added:
        p = ot.FeatureParamsStylisticSet(); p.Version = 0
        p.UINameID = font['name'].addMultilingualName({'en': names[fr.FeatureTag], 'de': {
            'ss01': 'Zierversalien', 'ss02': 'Garamond-g und -z'}[fr.FeatureTag]}, font, minNameID=256, mac=False)
        fr.Feature.FeatureParams = p
    newrecs = sorted(keep + added, key=lambda r: r.FeatureTag)
    old_index = {id(r): i for i, r in enumerate(recs)}
    remap = {}
    for ni, r in enumerate(newrecs):
        if id(r) in old_index: remap[old_index[id(r)]] = ni
    added_idx = [newrecs.index(fr) for fr in added]
    gsub.FeatureList.FeatureRecord = newrecs; gsub.FeatureList.FeatureCount = len(newrecs)
    for sr in gsub.ScriptList.ScriptRecord:
        langs = [sr.Script.DefaultLangSys] + [l.LangSys for l in sr.Script.LangSysRecord]
        for ls in langs:
            if ls is None: continue
            idx = [remap[i] for i in ls.FeatureIndex if i in remap] + added_idx
            ls.FeatureIndex = sorted(set(idx)); ls.FeatureCount = len(ls.FeatureIndex)

def set_cmap(font, extra):
    cm = font['cmap']
    best = dict(font.getBestCmap())
    best.update(extra)
    from fontTools.ttLib.tables._c_m_a_p import CmapSubtable
    tables = []
    for pid, eid, fmt in ((0, 3, 4), (3, 1, 4), (0, 4, 12), (3, 10, 12)):
        t = CmapSubtable.newSubtable(fmt); t.platformID = pid; t.platEncID = eid; t.language = 0
        t.cmap = {u: g for u, g in best.items() if fmt == 12 or u <= 0xFFFF}
        tables.append(t)
    cm.tables = tables

OFL_NOTE = ('This Font Software is licensed under the SIL Open Font License, Version 1.1. '
            'This license is available with a FAQ at: https://openfontlicense.org')
COPYRIGHT = ('Copyright 2026 The Nachtzug Project Authors. Based on EB Garamond, '
             'Copyright 2017 The EB Garamond Project Authors (https://github.com/octaviopardo/EBGaramond12).')

def set_names(font, family, style, extra_names=()):
    name = font['name']
    keep = {n.nameID: n for n in name.names if n.nameID >= 256}
    used = set()
    for t in ('GSUB',):
        for fr in font[t].table.FeatureList.FeatureRecord:
            p = fr.Feature.FeatureParams
            if p is not None and hasattr(p, 'UINameID'): used.add(p.UINameID)
    if 'STAT' in font:
        del font['STAT']
    name.names = [n for n in name.names if n.nameID in used]
    ps = family.replace(' ', '') + '-' + style
    rec = {0: COPYRIGHT, 1: family, 2: style, 3: f'{VERSION:.3f};NZUG;{ps}', 4: f'{family} {style}',
           5: f'Version {VERSION:.3f}', 6: ps, 8: 'The Nachtzug Project',
           9: 'The Nachtzug Project; EB Garamond by Georg Duffner and Octavio Pardo',
           10: 'The text face of Nachtzug: EB Garamond with a semaphore g, a tailed z, rail ligatures, swash capitals and night-train ornaments.',
           13: OFL_NOTE, 14: 'https://openfontlicense.org'}
    for k, v in rec.items():
        name.setName(v, k, 3, 1, 0x409)


def finish_metrics(font, weight, bold):
    os2 = font['OS/2']
    os2.usWeightClass = weight
    os2.achVendID = 'NZUG'
    os2.fsType = 0
    os2.fsSelection = (1 << 7) | ((1 << 5) if bold else (1 << 6))
    font['head'].macStyle = 1 if bold else 0
    font['head'].fontRevision = VERSION
    os2.recalcUnicodeRanges(font)
    os2.usFirstCharIndex = min(u for u in font.getBestCmap())
    os2.usLastCharIndex = min(0xFFFF, max(font.getBestCmap()))

# ---------------------------------------------------------------- one style

def build(style, weight):
    font = TTFont(os.path.join(SRC, f'EBG-{weight}-ovl.ttf'))
    g = G(font)
    n_orig = len(font.getGlyphOrder())
    # keep the classic forms for ss02 before changing g and z
    classic = {}
    for base in ('g', 'z'):
        g.copy(base, base + '.classic'); classic[base] = base + '.classic'
    for comp in ('gbreve', 'gcircumflex', 'gdotaccent', 'uni0123', 'gcaron', 'zacute', 'zcaron', 'zdotaccent'):
        if comp in g.glyf.glyphs and g.glyf[comp].isComposite():
            g.copy(comp, comp + '.classic')
            for c in g.glyf[comp + '.classic'].components:
                if c.glyphName in ('g', 'z'): c.glyphName += '.classic'
            classic[comp] = comp + '.classic'
    make_g(g)
    make_z(g)
    g.gs = font.getGlyphSet()
    make_tz(g)
    catenary(g, 'c', 8, 12, 'c_t')
    catenary(g, 's', 28, 31, 's_t')
    leg_tail(g, 'R', 'R.swsh')
    leg_tail(g, 'K', 'K.swsh', y_hi=205, y_lo=140)
    leg_tail(g, 'A', 'A.swsh', y_hi=185, y_lo=110, y_top=222)
    bar_tail(g, 'E', 88, 112, g.pt('E', 88)[0] + 20, 'E.swsh')
    bar_tail(g, 'L', 41, 68, g.pt('L', 41)[0] + 20, 'L.swsh')
    bar_tail(g, 'Z', 83, 5, g.pt('Z', 83)[0] + 30, 'Z.swsh')
    j_tail(g)
    # spaces: the variable source gives odd widths away from Regular, so they are set from their definitions
    cmap0 = font.getBestCmap()
    thin = 160 if weight < 600 else 172
    widths = {0x2002: 500, 0x2003: 1000, 0x2004: 333, 0x2005: 250, 0x2006: 167, 0x2009: thin, 0x200A: thin // 2,
              0x2007: g.width(cmap0[0x30]), 0x2008: g.width(cmap0[0x2E]), 0xA0: g.width('space')}
    for u, w in widths.items():
        if u in cmap0: g.hmtx[cmap0[u]] = (w, 0)
    g.empty('uni202F', thin)
    # ornaments
    extra = {0x202F: 'uni202F'}
    cap = 650
    for nm, fn, pua, others, _ in ORN.ORNAMENTS:
        shape, adv = fn()
        x0 = shape.bounds[0]
        sb = 60
        shape = transformed(shape, (1, 0, 0, 1, sb - x0, 0))
        g.put(nm, shape, shape.bounds[2] + sb)
        extra[pua] = nm
        for u in others:
            extra[u] = nm
    removeOverlaps(font)
    # GDEF classes for new glyphs
    cd = font['GDEF'].table.GlyphClassDef.classDefs
    for n in font.getGlyphOrder()[n_orig:]:
        cd[n] = 2 if n in ('t_z',) else 1
    swash = {'A': 'A.swsh', 'E': 'E.swsh', 'J': 'J.swsh', 'K': 'K.swsh', 'L': 'L.swsh', 'R': 'R.swsh', 'Z': 'Z.swsh', 'Q': 'Q.long'}
    rebuild_gsub(font, swash, classic, {'germandbls': 'germandbls.alt'})
    for base, alt in swash.items():
        if alt != 'Q.long': alias_kerning(font, alt, base, base)
    for b, c in classic.items():
        alias_kerning(font, c, b, b)
    alias_kerning(font, 't_z', 't', 'z')
    set_cmap(font, extra)
    family = 'Nachtzug Antiqua'
    set_names(font, family, style)
    finish_metrics(font, 400 if style == 'Regular' else 700, style == 'Bold')
    font['post'].formatType = 2.0
    path = os.path.join(OUT, f'NachtzugAntiqua-{style}-unhinted.ttf')
    font.save(path)
    for attempt in range(3):
        if not fix_self_intersections(path, err=0.25 if attempt == 0 else 0.12): break
    return path

def fix_self_intersections(path, err=0.25):
    """Asks FontForge which glyphs still cross themselves and redraws those from a clean union."""
    import subprocess
    out = subprocess.run(['fontforge', '-lang=py', '-script', os.path.join(HERE, 'ffselfint.py'), path],
                         capture_output=True, text=True).stdout
    bad = [l.split()[1] for l in out.splitlines() if l.startswith('SELFINT')]
    if not bad: return False
    font = TTFont(path)
    g = G(font)
    for n in bad:
        w = g.width(n)
        g.put(n, g.path(n), w, err=err)
    font.save(path)
    print('  redrawn without self-intersections:', len(bad), ' '.join(bad[:12]))
    return True

# ---------------------------------------------------------------- the dingbat font

def build_ornamente():
    fb = FontBuilder(1000, isTTF=False)
    names = [n for n, *_ in ORN.ORNAMENTS]
    fb.setupGlyphOrder(['.notdef', 'space'] + names)
    cmap = {0x20: 'space'}
    cs, mt = {}, {}
    pen = T2CharStringPen(600, None); r = rect(60, 0, 540, 650); minus(r, rect(110, 50, 490, 600)).draw(pen)
    cs['.notdef'] = pen.getCharString(); mt['.notdef'] = (600, 60)
    pen = T2CharStringPen(300, None); cs['space'] = pen.getCharString(); mt['space'] = (300, 0)
    for nm, fn, pua, others, letter in ORN.ORNAMENTS:
        shape, adv = fn()
        x0 = shape.bounds[0]
        shape = transformed(shape, (1, 0, 0, 1, 60 - x0, 0))
        adv = int(round(shape.bounds[2] + 60))
        if nm == 'orn.track':          # the track tiles without gaps
            shape = transformed(shape, (1, 0, 0, 1, -60, 0)); adv = 500
        pen = T2CharStringPen(adv, None)
        draw_fitted(shape, pen, err=0.3, corner=40, rnd=lambda p: (round(p[0]), round(p[1])))
        cs[nm] = pen.getCharString(); mt[nm] = (adv, int(shape.bounds[0]))
        for u in [pua, ord(letter), ord(letter.upper())] + others:
            cmap[u] = nm
    fb.setupCharacterMap(cmap)
    fam = 'Nachtzug Ornamente'
    fb.setupCFF('NachtzugOrnamente-Regular', {'FullName': f'{fam} Regular', 'FamilyName': fam}, cs, {})
    fb.setupHorizontalMetrics(mt)
    fb.setupHorizontalHeader(ascent=900, descent=-200, lineGap=0)
    fb.setupNameTable({'copyright': 'Copyright 2026 The Nachtzug Project Authors.', 'familyName': fam, 'styleName': 'Regular',
                       'uniqueFontIdentifier': f'{VERSION:.3f};NZUG;NachtzugOrnamente-Regular', 'fullName': f'{fam} Regular',
                       'version': f'Version {VERSION:.3f}', 'psName': 'NachtzugOrnamente-Regular',
                       'manufacturer': 'The Nachtzug Project', 'designer': 'The Nachtzug Project',
                       'description': 'Night-train ornaments: a = moon, b = waning moon, c = twinkle, d = star, e = asterism, '
                                      'f = locomotive, g = sleeping car, h = track (tiles), i = signal, j/k = fleurons, l = window, m = lantern.',
                       'licenseDescription': OFL_NOTE, 'licenseInfoURL': 'https://openfontlicense.org'})
    fb.setupOS2(sTypoAscender=900, sTypoDescender=-200, sTypoLineGap=0, usWinAscent=950, usWinDescent=250,
                achVendID='NZUG', fsType=0, usWeightClass=400, fsSelection=0x40 | 0x80, version=4, sCapHeight=650, sxHeight=400)
    fb.setupPost()
    path = os.path.join(OUT, 'NachtzugOrnamente-Regular.otf')
    fb.save(path)
    return path

if __name__ == '__main__':
    styles = sys.argv[1:] or ['Regular', 'Bold']
    for st in styles:
        print(build(st, 440 if st == 'Regular' else 700))
    print(build_ornamente())
