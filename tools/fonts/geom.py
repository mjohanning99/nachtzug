"""Geometry for the Nachtzug fonts: robust boolean operations on polygons, calligraphic strokes and curve fitting.

Everything that is combined is first flattened to polygons (skia-pathops is far more reliable on straight segments
than on curves), and only the finished outline is turned back into smooth cubic curves by fitting, with corners kept
and points added at the extrema, as font editors expect.
"""
import math
import numpy as np
import pathops
from fontTools.pens.recordingPen import RecordingPen
from fontTools.pens.transformPen import TransformPen

# ---------------------------------------------------------------- conversion

def _sample_quad(p0, p1, p2, tol=0.25):
    d = math.hypot(p0[0] - 2 * p1[0] + p2[0], p0[1] - 2 * p1[1] + p2[1])
    n = max(2, int(math.ceil(math.sqrt(d / (4 * tol)))) + 1)
    out = []
    for i in range(1, n + 1):
        t = i / n; u = 1 - t
        out.append((u * u * p0[0] + 2 * u * t * p1[0] + t * t * p2[0], u * u * p0[1] + 2 * u * t * p1[1] + t * t * p2[1]))
    return out

def _sample_cubic(p0, p1, p2, p3, tol=0.25):
    d = max(math.hypot(p0[0] - 2 * p1[0] + p2[0], p0[1] - 2 * p1[1] + p2[1]),
            math.hypot(p1[0] - 2 * p2[0] + p3[0], p1[1] - 2 * p2[1] + p3[1]))
    n = max(2, int(math.ceil(math.sqrt(3 * d / (4 * tol)))) + 1)
    out = []
    for i in range(1, n + 1):
        t = i / n; u = 1 - t
        out.append((u**3 * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t**3 * p3[0],
                    u**3 * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t**3 * p3[1]))
    return out

def contours_of(drawable, tol=0.25):
    """Polygon contours (lists of points, no repeated end point) of anything with a draw(pen) method."""
    rec = RecordingPen()
    drawable.draw(rec)
    out, cur, start = [], None, None
    for op, args in rec.value:
        if op == 'moveTo':
            cur = [args[0]]; start = args[0]
        elif op == 'lineTo':
            cur.append(args[0])
        elif op == 'qCurveTo':
            # TrueType style: implied on-curve points between consecutive off-curve points
            pts = list(args)
            if pts[-1] is None:          # a contour of only off-curve points
                pts = pts[:-1]
                first = ((pts[-1][0] + pts[0][0]) / 2, (pts[-1][1] + pts[0][1]) / 2)
                cur = [first]; start = first
                pts = pts + [first]
            p0 = cur[-1]
            offs, on = pts[:-1], pts[-1]
            for i, c in enumerate(offs):
                nxt = on if i == len(offs) - 1 else ((c[0] + offs[i + 1][0]) / 2, (c[1] + offs[i + 1][1]) / 2)
                cur += _sample_quad(p0, c, nxt, tol); p0 = nxt
        elif op == 'curveTo':
            pts = list(args)
            p0 = cur[-1]
            # fontTools may pack several cubics; handle the common 3-point case and super-beziers by splitting
            while len(pts) > 3:
                c1, c2 = pts[0], pts[1]
                mid = ((c2[0] + pts[2][0]) / 2, (c2[1] + pts[2][1]) / 2)
                cur += _sample_cubic(p0, c1, c2, mid, tol); p0 = mid; pts = pts[2:]
            cur += _sample_cubic(p0, *pts, tol)
        elif op in ('closePath', 'endPath'):
            if cur:
                if len(cur) > 1 and math.hypot(cur[-1][0] - cur[0][0], cur[-1][1] - cur[0][1]) < 1e-6:
                    cur.pop()
                if len(cur) >= 3: out.append(cur)
            cur = None
    return out

def from_contours(cs):
    p = pathops.Path()
    for c in cs:
        if len(c) < 3: continue
        p.moveTo(*c[0])
        for q in c[1:]: p.lineTo(*q)
        p.close()
    return p

def flat(path, tol=0.25):
    return from_contours(contours_of(path, tol))

def poly(points):
    return from_contours([points])

def transformed(path, m):
    out = pathops.Path()
    path.draw(TransformPen(out.getPen(), m))
    return out

def translated(path, dx, dy): return transformed(path, (1, 0, 0, 1, dx, dy))

def signed_area(c):
    a = 0.0
    for i in range(len(c)):
        x0, y0 = c[i]; x1, y1 = c[(i + 1) % len(c)]
        a += x0 * y1 - x1 * y0
    return a / 2

# ---------------------------------------------------------------- robust booleans (Clipper, on integers)

import pyclipper
SC = 256.0          # Clipper works on integers: 1/256 unit precision

def _to_cl(path):
    return [[(int(round(x * SC)), int(round(y * SC))) for x, y in c] for c in contours_of(path)]

def _from_cl(paths):
    return from_contours([[(x / SC, y / SC) for x, y in c] for c in paths if len(c) >= 3])

def _clean(paths):
    return [c for c in pyclipper.CleanPolygons(paths, 0.05 * SC) if len(c) >= 3]

def _exec(subj, clip, kind):
    pc = pyclipper.Pyclipper()
    if subj: pc.AddPaths(subj, pyclipper.PT_SUBJECT, True)
    if clip: pc.AddPaths(clip, pyclipper.PT_CLIP, True)
    if not subj: return []
    return _clean(pc.Execute(kind, pyclipper.PFT_NONZERO, pyclipper.PFT_NONZERO))

def simplify(p):
    """The shape with self-overlaps resolved and contours oriented (outer anticlockwise, holes clockwise)."""
    return _from_cl(_exec(_to_cl(p), None, pyclipper.CT_UNION))

def union(paths):
    allp = []
    for p in paths:
        if p is None: continue
        allp += _exec(_to_cl(p), None, pyclipper.CT_UNION)   # normalise each piece first
    return _from_cl(_exec(allp, None, pyclipper.CT_UNION))

def minus(a, b): return _from_cl(_exec(_exec(_to_cl(a), None, pyclipper.CT_UNION), _exec(_to_cl(b), None, pyclipper.CT_UNION), pyclipper.CT_DIFFERENCE))
def inter(a, b): return _from_cl(_exec(_exec(_to_cl(a), None, pyclipper.CT_UNION), _exec(_to_cl(b), None, pyclipper.CT_UNION), pyclipper.CT_INTERSECTION))

def area(p):
    return sum(signed_area(c) for c in contours_of(p))

def islands(path):
    """The shape split into islands, each with its own holes."""
    pc = pyclipper.Pyclipper()
    cl = _exec(_to_cl(path), None, pyclipper.CT_UNION)
    if not cl: return []
    pc.AddPaths(cl, pyclipper.PT_SUBJECT, True)
    tree = pc.Execute2(pyclipper.CT_UNION, pyclipper.PFT_NONZERO, pyclipper.PFT_NONZERO)
    out = []
    def walk(node):
        for ch in node.Childs:       # outer contours
            out.append([ch.Contour] + [h.Contour for h in ch.Childs])
            for h in ch.Childs: walk(h)
    walk(tree)
    return [_from_cl(i) for i in out]

def drop_small(path, min_area):
    """Drops islands smaller than min_area (crumbs left by cutting)."""
    keep = [i for i in islands(path) if area(i) >= min_area]
    return union(keep) if keep else pathops.Path()

# ---------------------------------------------------------------- dilation (offsetting) of a shape

def dilate(path, r, flatten_tol=0.3):
    """The shape grown by r with round joins (negative r shrinks it)."""
    cl = _exec(_to_cl(flat(path, flatten_tol)), None, pyclipper.CT_UNION)
    off = pyclipper.PyclipperOffset(2.0, 0.2 * SC)
    off.AddPaths(cl, pyclipper.JT_ROUND, pyclipper.ET_CLOSEDPOLYGON)
    return _from_cl(_clean(off.Execute(r * SC)))

def erode(path, r):
    return dilate(path, -r)

def fill_holes(path):
    """Only the outer contours (counters filled)."""
    cl = _exec(_to_cl(path), None, pyclipper.CT_UNION)
    return _from_cl([c for c in cl if pyclipper.Orientation(c)])

def stroke_line(pts, w, cap='round'):
    """An even-width stroke along an open polyline (Clipper offset)."""
    off = pyclipper.PyclipperOffset(2.0, 0.2 * SC)
    off.AddPath([(int(round(x * SC)), int(round(y * SC))) for x, y in pts], pyclipper.JT_ROUND,
                pyclipper.ET_OPENROUND if cap == 'round' else pyclipper.ET_OPENBUTT)
    return _from_cl(_clean(off.Execute(w / 2 * SC)))

# ---------------------------------------------------------------- primitive shapes

def rect(x0, y0, x1, y1): return poly([(x0, y0), (x1, y0), (x1, y1), (x0, y1)])

def circle(cx, cy, r, n=None):
    n = n or max(24, int(2 * math.pi * r / 2.5))
    return poly([(cx + r * math.cos(2 * math.pi * i / n), cy + r * math.sin(2 * math.pi * i / n)) for i in range(n)])

def ring(cx, cy, r, w):
    return minus(circle(cx, cy, r + w / 2), circle(cx, cy, r - w / 2))

def rounded_rect(x0, y0, x1, y1, r):
    pts = []
    for cx, cy, a0 in ((x1 - r, y0 + r, -90), (x1 - r, y1 - r, 0), (x0 + r, y1 - r, 90), (x0 + r, y0 + r, 180)):
        for i in range(13):
            a = math.radians(a0 + 90 * i / 12)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return poly(pts)

def star(cx, cy, r, points=5, inner=0.42, rot=math.pi / 2):
    pts = []
    for i in range(points * 2):
        rr = r if i % 2 == 0 else r * inner
        a = rot + math.pi * i / points
        pts.append((cx + rr * math.cos(a), cy + rr * math.sin(a)))
    return poly(pts)

def sparkle(cx, cy, r, waist=0.18, rot=0.0, n=4):
    """A twinkling star: n long points joined by concave curves."""
    pts = []
    for i in range(n):
        a0 = rot + 2 * math.pi * i / n
        a1 = rot + 2 * math.pi * (i + 1) / n
        tip0 = (cx + r * math.cos(a0), cy + r * math.sin(a0))
        tip1 = (cx + r * math.cos(a1), cy + r * math.sin(a1))
        am = (a0 + a1) / 2
        ctrl = (cx + r * waist * math.cos(am), cy + r * waist * math.sin(am))
        for k in range(12):
            t = k / 12; u = 1 - t
            pts.append((u * u * tip0[0] + 2 * u * t * ctrl[0] + t * t * tip1[0], u * u * tip0[1] + 2 * u * t * ctrl[1] + t * t * tip1[1]))
    return poly(pts)

def crescent(cx, cy, r, phase=0.55, tilt=0.0):
    """A crescent moon: a disc minus an offset disc; phase 0..1 is how much is cut away. Horns point along tilt+90°."""
    off = r * (0.25 + 0.7 * phase)
    ox, oy = cx + off * math.cos(tilt), cy + off * math.sin(tilt)
    return minus(circle(cx, cy, r), circle(ox, oy, r * 0.92))

def leaf(x, y, angle, length, width, bend=0.0, rib=0.0):
    """An almond leaf from (x, y) pointing along angle; bend curls its tip; rib cuts a fine midrib."""
    top, bot = [], []
    n = 28
    for i in range(n + 1):
        t = i / n
        w = math.sin(math.pi * t) ** 0.9 * (1 - 0.25 * t)
        cy = bend * length * t * t
        top.append((t * length, cy + w * width / 2))
        bot.append((t * length, cy - w * width / 2 * 0.7))
    pts = top + bot[::-1][1:-1]
    c, s = math.cos(angle), math.sin(angle)
    shape = poly([(x + px * c - py * s, y + px * s + py * c) for px, py in pts])
    if rib > 0:
        ribpts = [(t * length, bend * length * t * t) for t in [k / 20 for k in range(3, 17)]]
        ribpts = [(x + px * c - py * s, y + px * s + py * c) for px, py in ribpts]
        shape = minus(shape, stroke(ribpts, rib, rib * 0.4))
    return shape

# ---------------------------------------------------------------- centrelines and strokes

def bezier(p0, p1, p2, p3, n=None):
    if n is None:
        L = math.hypot(p1[0] - p0[0], p1[1] - p0[1]) + math.hypot(p2[0] - p1[0], p2[1] - p1[1]) + math.hypot(p3[0] - p2[0], p3[1] - p2[1])
        n = max(8, int(L / 2))
    out = []
    for i in range(n + 1):
        t = i / n; u = 1 - t
        out.append((u**3 * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t**3 * p3[0],
                    u**3 * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t**3 * p3[1]))
    return out

def resample(pts, step=2.0):
    """Points along a polyline at equal arc-length spacing."""
    pts = np.asarray(pts, float)
    seg = np.hypot(*np.diff(pts, axis=0).T)
    keep = np.concatenate([[True], seg > 1e-9])
    pts = pts[keep]
    if len(pts) < 2: return [tuple(p) for p in pts]
    seg = np.hypot(*np.diff(pts, axis=0).T)
    s = np.concatenate([[0], np.cumsum(seg)])
    n = max(2, int(math.ceil(s[-1] / step)) + 1)
    t = np.linspace(0, s[-1], n)
    x = np.interp(t, s, pts[:, 0]); y = np.interp(t, s, pts[:, 1])
    return list(zip(x, y))

def curl(start, tangent, r0, turns, direction, r_end=None, n=None):
    """A spiral that leaves `start` along `tangent` and winds in (direction +1 = anticlockwise).
    The radius shrinks from r0 to r_end, never so tight that a stroke on it would overlap itself."""
    tx, ty = tangent
    L = math.hypot(tx, ty) or 1
    tx, ty = tx / L, ty / L
    nx, ny = -ty * direction, tx * direction           # towards the centre
    cx, cy = start[0] + nx * r0, start[1] + ny * r0
    a0 = math.atan2(start[1] - cy, start[0] - cx)
    r_end = r0 * 0.3 if r_end is None else r_end
    total = 2 * math.pi * turns
    n = n or max(30, int(total * r0 / 2))
    out = []
    for i in range(n + 1):
        t = i / n
        a = a0 + direction * total * t
        r = r_end + (r0 - r_end) * (1 - t) ** 1.25
        out.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return out

def end_tangent(pts, back=4):
    a, b = pts[-1 - back], pts[-1]
    return (b[0] - a[0], b[1] - a[1])

def stroke(pts, w0, w1=None, ease=1.0, cap='round', step=2.0, widths=None, clamp=0.8):
    """A calligraphic stroke along a polyline whose width runs from w0 to w1 (or follows `widths(t)`).
    The half-width is kept below `clamp` times the local radius of curvature so the outline never folds."""
    if w1 is None: w1 = w0
    P = np.asarray(resample(pts, step), float)
    n = len(P)
    if n < 2: return pathops.Path()
    t = np.linspace(0, 1, n)
    hw = (widths(t) if widths else (w0 + (w1 - w0) * t ** ease)) / 2
    # tangents and normals
    d = np.gradient(P, axis=0)
    dl = np.hypot(d[:, 0], d[:, 1]); dl[dl == 0] = 1
    T = d / dl[:, None]
    N = np.stack([-T[:, 1], T[:, 0]], axis=1)
    # radius of curvature from the turning of the tangent
    ang = np.unwrap(np.arctan2(T[:, 1], T[:, 0]))
    dang = np.abs(np.gradient(ang))
    R = np.where(dang > 1e-9, step / np.maximum(dang, 1e-9), 1e9)
    hw = np.minimum(hw, clamp * R)
    hw = np.maximum(hw, 0.6)
    k = 5
    if n > 2 * k:
        hw = np.convolve(np.pad(hw, k, mode='edge'), np.ones(2 * k + 1) / (2 * k + 1), mode='valid')
    L = P + N * hw[:, None]
    Rr = P - N * hw[:, None]
    out = [tuple(p) for p in L]
    def cap_pts(c, a_from, r, rev=False):
        res = []
        m = max(6, int(math.pi * r / 1.5))
        for i in range(1, m):
            a = a_from - math.pi * i / m
            res.append((c[0] + r * math.cos(a), c[1] + r * math.sin(a)))
        return res
    if cap == 'round':
        aN = math.atan2(N[-1, 1], N[-1, 0])
        out += cap_pts(P[-1], aN, hw[-1])
    out += [tuple(p) for p in Rr[::-1]]
    if cap == 'round':
        aN = math.atan2(-N[0, 1], -N[0, 0])
        out += cap_pts(P[0], aN, hw[0])
    return simplify(poly(out))

# ---------------------------------------------------------------- curve fitting (after Schneider, Graphics Gems 1990)

def _bez(ctrl, t):
    t = t[:, None]; u = 1 - t
    return u**3 * ctrl[0] + 3 * u * u * t * ctrl[1] + 3 * u * t * t * ctrl[2] + t**3 * ctrl[3]

def _bez_d(ctrl, t):
    t = t[:, None]; u = 1 - t
    return 3 * u * u * (ctrl[1] - ctrl[0]) + 6 * u * t * (ctrl[2] - ctrl[1]) + 3 * t * t * (ctrl[3] - ctrl[2])

def _bez_dd(ctrl, t):
    t = t[:, None]; u = 1 - t
    return 6 * u * (ctrl[2] - 2 * ctrl[1] + ctrl[0]) + 6 * t * (ctrl[3] - 2 * ctrl[2] + ctrl[1])

def _chord_params(P):
    d = np.concatenate([[0], np.cumsum(np.hypot(*np.diff(P, axis=0).T))])
    return d / d[-1] if d[-1] > 0 else np.linspace(0, 1, len(P))

def _gen_bezier(P, u, t1, t2):
    p0, p3 = P[0], P[-1]
    A1 = t1[None, :] * (3 * (1 - u) ** 2 * u)[:, None]
    A2 = t2[None, :] * (3 * (1 - u) * u ** 2)[:, None]
    C = np.array([[np.sum(A1 * A1), np.sum(A1 * A2)], [np.sum(A1 * A2), np.sum(A2 * A2)]])
    B = _bez(np.array([p0, p0, p3, p3]), u)
    tmp = P - B
    X = np.array([np.sum(A1 * tmp), np.sum(A2 * tmp)])
    det = C[0, 0] * C[1, 1] - C[0, 1] * C[1, 0]
    seg = np.hypot(*(p3 - p0))
    a1 = a2 = 0.0
    if abs(det) > 1e-12:
        a1 = (X[0] * C[1, 1] - X[1] * C[0, 1]) / det
        a2 = (C[0, 0] * X[1] - C[1, 0] * X[0]) / det
    eps = 1e-6 * seg
    if a1 < eps or a2 < eps or a1 > 2 * seg or a2 > 2 * seg:
        a1 = a2 = seg / 3
    return np.array([p0, p0 + t1 * a1, p3 + t2 * a2, p3])

def _reparam(ctrl, P, u):
    d = _bez(ctrl, u) - P
    d1 = _bez_d(ctrl, u); d2 = _bez_dd(ctrl, u)
    num = np.sum(d * d1, axis=1)
    den = np.sum(d1 * d1, axis=1) + np.sum(d * d2, axis=1)
    with np.errstate(divide='ignore', invalid='ignore'):
        nu = np.where(np.abs(den) > 1e-12, u - num / den, u)
    nu = np.clip(nu, 0, 1)
    nu[0], nu[-1] = 0, 1
    return np.maximum.accumulate(nu)

def _unit(v):
    L = math.hypot(v[0], v[1])
    return v / L if L > 1e-12 else v

def _fit(P, t1, t2, err, out, depth=0):
    if len(P) == 2:
        d = np.hypot(*(P[1] - P[0])) / 3
        out.append(np.array([P[0], P[0] + t1 * d, P[1] + t2 * d, P[1]])); return
    u = _chord_params(P)
    ctrl = _gen_bezier(P, u, t1, t2)
    e = np.hypot(*(_bez(ctrl, u) - P).T)
    mx = e.max()
    if mx < err:
        out.append(ctrl); return
    if mx < err * 6:
        for _ in range(12):
            u = _reparam(ctrl, P, u)
            ctrl = _gen_bezier(P, u, t1, t2)
            e = np.hypot(*(_bez(ctrl, u) - P).T)
            mx = e.max()
            if mx < err:
                out.append(ctrl); return
    split = int(np.argmax(e[1:-1])) + 1 if len(P) > 2 else len(P) // 2
    split = min(max(split, 1), len(P) - 2) if len(P) > 3 else 1
    if depth > 40 or len(P) <= 3:
        for i in range(len(P) - 1):
            out.append(np.array([P[i], P[i] + (P[i + 1] - P[i]) / 3, P[i] + 2 * (P[i + 1] - P[i]) / 3, P[i + 1]]))
        return
    tc = _unit(P[min(split + 1, len(P) - 1)] - P[max(split - 1, 0)])
    _fit(P[:split + 1], t1, -tc, err, out, depth + 1)
    _fit(P[split:], tc, t2, err, out, depth + 1)

def _corners(C, angle_deg):
    """Indices of corners in a closed polygon: where the direction turns more than angle_deg."""
    n = len(C)
    v_in = C - np.roll(C, 1, axis=0)
    v_out = np.roll(C, -1, axis=0) - C
    a_in = np.arctan2(v_in[:, 1], v_in[:, 0]); a_out = np.arctan2(v_out[:, 1], v_out[:, 0])
    turn = np.abs((a_out - a_in + np.pi) % (2 * np.pi) - np.pi)
    return [i for i in range(n) if turn[i] > math.radians(angle_deg)]

def _split_extrema(ctrl):
    """Splits a cubic at its horizontal and vertical extrema (as font editors like to see them)."""
    if np.hypot(*(ctrl[3] - ctrl[0])) < 8: return [ctrl]
    ts = []
    for k in (0, 1):
        p0, p1, p2, p3 = ctrl[:, k]
        a = -p0 + 3 * p1 - 3 * p2 + p3
        b = 2 * (p0 - 2 * p1 + p2)
        c = p1 - p0
        if abs(a) < 1e-9:
            if abs(b) > 1e-9: ts.append(-c / b)
        else:
            disc = b * b - 4 * a * c
            if disc >= 0:
                s = math.sqrt(disc)
                ts += [(-b + s) / (2 * a), (-b - s) / (2 * a)]
    ts = sorted(t for t in ts if 0.08 < t < 0.92)
    merged = []
    for t in ts:
        if not merged or t - merged[-1] > 0.08: merged.append(t)
    if not merged: return [ctrl]
    out, rest, t0 = [], ctrl, 0.0
    for t in merged:
        tt = (t - t0) / (1 - t0)
        p0, p1, p2, p3 = rest
        a = p0 + (p1 - p0) * tt; b = p1 + (p2 - p1) * tt; c = p2 + (p3 - p2) * tt
        d = a + (b - a) * tt; e = b + (c - b) * tt; f = d + (e - d) * tt
        out.append(np.array([p0, a, d, f]))
        rest = np.array([f, e, c, p3]); t0 = t
    out.append(rest)
    return out

def fit_contour(C, err=0.35, corner=38):
    """Turns a closed polygon into a list of segments: ('line', p0, p1) or ('curve', ctrl[4])."""
    C = np.asarray(C, float)
    # drop near-duplicate points
    keep = [0]
    for i in range(1, len(C)):
        if np.hypot(*(C[i] - C[keep[-1]])) > 0.7: keep.append(i)
    C = C[keep]
    if len(C) > 2 and np.hypot(*(C[-1] - C[0])) <= 0.7: C = C[:-1]
    n = len(C)
    if n < 3: return []
    cs = _corners(C, corner)
    # long straight edges are kept as lines: mark both ends of segments much longer than the sampling step
    seglen = np.hypot(*(np.roll(C, -1, axis=0) - C).T)
    long_ = set()
    for i in range(n):
        if seglen[i] > 12:
            long_.add(i); long_.add((i + 1) % n)
    cs = sorted(set(cs) | long_)
    segs = []
    if not cs:
        # smooth closed curve: break at the leftmost point (an extremum) and fit around
        i0 = int(np.argmin(C[:, 0]))
        Q = np.vstack([C[i0:], C[:i0], C[i0:i0 + 1]])
        t = _unit(Q[1] - Q[-2])
        out = []
        _fit(Q, t, -t, err, out)
        for c in out: segs += [('curve', s) for s in _split_extrema(c)]
        return segs
    for j in range(len(cs)):
        a, b = cs[j], cs[(j + 1) % len(cs)]
        idx = list(range(a, b + 1)) if b > a else list(range(a, n)) + list(range(0, b + 1))
        Q = C[idx]
        if len(Q) == 2 or (len(Q) > 2 and seglen[a] > 12 and len(Q) == 2):
            segs.append(('line', Q[0], Q[1])); continue
        # straight run?
        chord = Q[-1] - Q[0]
        L = np.hypot(*chord)
        if L > 0:
            dev = np.abs((Q[:, 0] - Q[0, 0]) * chord[1] - (Q[:, 1] - Q[0, 1]) * chord[0]) / L
            if dev.max() < err * 0.5:
                segs.append(('line', Q[0], Q[-1])); continue
        t1 = _unit(Q[1] - Q[0]) if len(Q) < 4 else _unit(Q[2] - Q[0])
        t2 = _unit(Q[-2] - Q[-1]) if len(Q) < 4 else _unit(Q[-3] - Q[-1])
        out = []
        _fit(Q, t1, t2, err, out)
        for c in out: segs += [('curve', s) for s in _split_extrema(c)]
    return segs

def _tidy(segs, tiny=1.2):
    """Removes degenerate little segments (they would collapse when coordinates are rounded)."""
    out = []
    for sg in segs:
        if sg[0] == 'line':
            a, b = np.asarray(sg[1]), np.asarray(sg[2])
        else:
            a, b = sg[1][0], sg[1][3]
        if np.hypot(*(b - a)) < tiny:
            if out:      # stretch the previous segment's end to this one's end
                p = out[-1]
                if p[0] == 'line': out[-1] = ('line', p[1], b)
                else:
                    c = p[1].copy(); c[2] = c[2] + (b - c[3]); c[3] = b; out[-1] = ('curve', c)
            continue
        if sg[0] == 'curve':
            c = sg[1]
            L = np.hypot(*(c[3] - c[0]))
            if L < 6 and max(np.hypot(*(c[1] - c[0])), np.hypot(*(c[2] - c[3]))) < 4.0:
                sg = ('line', c[0], c[3])
        out.append(sg)
    # the first segment must start where the last one ends
    if out:
        last_end = out[-1][2] if out[-1][0] == 'line' else out[-1][1][3]
        f = out[0]
        if f[0] == 'line': out[0] = ('line', last_end, f[2])
        else:
            c = f[1].copy(); c[1] = c[1] + (last_end - c[0]); c[0] = last_end; out[0] = ('curve', c)
    return out

def draw_fitted(path, pen, err=0.35, corner=38, min_area=1.0, rnd=None):
    """Draws a polygonal pathops path as fitted curves into a fontTools pen."""
    R = (lambda p: (round(p[0], 2), round(p[1], 2))) if rnd is None else rnd
    for C in contours_of(path):
        if abs(signed_area(C)) < min_area: continue
        segs = _tidy(fit_contour(C, err, corner))
        if len(segs) < 2: continue
        first = segs[0][1] if segs[0][0] == 'line' else segs[0][1][0]
        pen.moveTo(R(first))
        for s in segs:
            if s[0] == 'line':
                pen.lineTo(R(s[2]))
            else:
                c = s[1]
                pen.curveTo(R(c[1]), R(c[2]), R(c[3]))
        pen.closePath()

def fitted_path(path, err=0.35, corner=38):
    out = pathops.Path()
    draw_fitted(path, out.getPen(), err, corner)
    return out

# ---------------------------------------------------------------- TrueType output with a safety net

def draw_tt_safe(path, pen, err=0.3, corner=40, max_err=0.4):
    """Draws a polygonal path into a TrueType pen as fitted quadratic curves, contour by contour. Any contour that
    would cross itself once its points are rounded to whole units is drawn instead as a cleaned whole-unit polygon."""
    from fontTools.pens.cu2quPen import Cu2QuPen
    from shapely.geometry import LinearRing
    for C in contours_of(path):
        a = signed_area(C)
        if abs(a) < 1.0: continue
        one = from_contours([C])
        rec = RecordingPen()
        draw_fitted(one, Cu2QuPen(rec, max_err=max_err), err=err, corner=corner)
        val = []
        for op_, args in rec.value:
            val.append((op_, tuple((round(p[0]), round(p[1])) if p is not None else None for p in args)))
        rec.value = val
        ok = False
        try:
            flat_c = contours_of(rec, 0.3)
            if len(flat_c) == 1 and len(flat_c[0]) >= 3:
                ring = LinearRing(flat_c[0])
                ok = ring.is_simple and abs(signed_area(flat_c[0])) > 0.5 and (signed_area(flat_c[0]) > 0) == (a > 0)
        except Exception:
            ok = False
        if ok:
            rec.replay(pen)
            continue
        ints = [(int(round(x)), int(round(y))) for x, y in C]
        polys = pyclipper.SimplifyPolygon(ints, pyclipper.PFT_NONZERO)
        for P in polys:
            P = pyclipper.CleanPolygon(P, 0.8)
            if len(P) < 3 or abs(pyclipper.Area(P)) < 1: continue
            if (pyclipper.Area(P) > 0) != (a > 0): P = P[::-1]
            pen.moveTo(tuple(P[0]))
            for q in P[1:]: pen.lineTo(tuple(q))
            pen.closePath()
