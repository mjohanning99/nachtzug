"""The symbols Nachtzug writes in its interface, drawn in the weight of Nachtzug Grotesk.

Every function takes (V, H, weight): the vertical stem and horizontal bar of the letters, and returns
(shape, advance) with the shape already placed on its side bearing.
"""
import math
import pyclipper
from geom import poly, rect, circle, union, minus, inter, transformed, stroke, _from_cl, _clean, SC

SB = 60                 # side bearing
AXIS = 262              # arrows and small triangles: the middle of the x-height
MID = 335               # large shapes: a little above the middle of the capitals

def place(shape, sb=SB):
    x0, y0, x1, y1 = shape.bounds
    return transformed(shape, (1, 0, 0, 1, sb - x0, 0)), round(x1 - x0 + 2 * sb)

def polyline(pts, w, closed=False, join='miter', cap='butt'):
    """A stroke of even width along a polyline with mitred (sharp) joins."""
    off = pyclipper.PyclipperOffset(3.0, 0.2 * SC)
    jt = {'miter': pyclipper.JT_MITER, 'round': pyclipper.JT_ROUND, 'square': pyclipper.JT_SQUARE}[join]
    et = pyclipper.ET_CLOSEDLINE if closed else {'butt': pyclipper.ET_OPENBUTT, 'round': pyclipper.ET_OPENROUND,
                                                 'square': pyclipper.ET_OPENSQUARE}[cap]
    off.AddPath([(int(round(x * SC)), int(round(y * SC))) for x, y in pts], jt, et)
    return _from_cl(_clean(off.Execute(w / 2 * SC)))

def rot(shape, deg, cx, cy):
    a = math.radians(deg); c, s = math.cos(a), math.sin(a)
    return transformed(shape, (c, s, -s, c, cx - c * cx + s * cy, cy - s * cx - c * cy))

def mirror_x(shape):
    x0, _, x1, _ = shape.bounds
    return transformed(shape, (-1, 0, 0, 1, x0 + x1, 0))

# ---------------------------------------------------------------- geometric shapes

def tri_right(h, cy, round_=0):
    w = h * 0.88
    return poly([(0, cy - h / 2), (w, cy), (0, cy + h / 2)])

def black_right(V, H, wt):  return place(tri_right(520, MID))
def black_left(V, H, wt):   return place(mirror_x(tri_right(520, MID)))
def small_right(V, H, wt):  return place(tri_right(330, AXIS))
def small_left(V, H, wt):   return place(mirror_x(tri_right(330, AXIS)))
def black_up(V, H, wt):     return place(_tri_v(520, MID, up=True))
def black_down(V, H, wt):   return place(_tri_v(520, MID, up=False))
def small_up(V, H, wt):     return place(_tri_v(330, AXIS, up=True))
def small_down(V, H, wt):   return place(_tri_v(330, AXIS, up=False))

def _tri_v(w, cy, up):
    h = w * 0.88
    if up: return poly([(0, cy - h / 2), (w, cy - h / 2), (w / 2, cy + h / 2)])
    return poly([(0, cy + h / 2), (w / 2, cy - h / 2), (w, cy + h / 2)])

def black_square(V, H, wt):
    s = 470
    return place(rect(0, MID - s / 2, s, MID + s / 2))

def white_square(V, H, wt):
    s = 470
    return place(minus(rect(0, MID - s / 2, s, MID + s / 2), rect(V, MID - s / 2 + H, s - V, MID + s / 2 - H)))

def small_square(V, H, wt):
    s = 300
    return place(rect(0, AXIS - s / 2, s, AXIS + s / 2))

def black_circle(V, H, wt):
    return place(circle(0, MID, 265))

def white_circle(V, H, wt):
    return place(minus(circle(0, MID, 265), circle(0, MID, 265 - (V + H) / 2)))

# ---------------------------------------------------------------- arrows

def _head(tip, direction, size, w):
    """An open arrowhead (two strokes meeting in a sharp tip) pointing along direction (degrees)."""
    a = math.radians(direction)
    pts = []
    for s in (+1, -1):
        b = a + math.pi + s * math.radians(45)
        pts.append((tip[0] + size * math.cos(b), tip[1] + size * math.sin(b)))
    return polyline([pts[0], tip, pts[1]], w)

def arrow(direction, V, H, both=False):
    L = 640
    w = H * 1.02
    head = 200 + (w - 75) * 0.6
    if direction in (0, 180):
        y = AXIS
        shaft = rect(w * 0.7 if both else 0, y - w / 2, L - w * 0.7, y + w / 2)
        tip_r = (L, y); tip_l = (0, y)
        parts = [shaft]
        if both or direction == 0: parts.append(_head(tip_r, 0, head, w))
        if both or direction == 180: parts.append(_head(tip_l, 180, head, w))
        if direction == 180 and not both: parts[0] = rect(w * 0.7, y - w / 2, L, y + w / 2)
        if direction == 0 and not both: parts[0] = rect(0, y - w / 2, L - w * 0.7, y + w / 2)
        return union(parts)
    w = V * 0.98
    x = 0
    y0, y1 = MID - L / 2 * 0.95, MID + L / 2 * 0.95
    parts = []
    if both:
        parts += [rect(x - w / 2, y0 + w * 0.7, x + w / 2, y1 - w * 0.7), _head((x, y1), 90, head, w), _head((x, y0), 270, head, w)]
    elif direction == 90:
        parts += [rect(x - w / 2, y0, x + w / 2, y1 - w * 0.7), _head((x, y1), 90, head, w)]
    elif direction == 270:
        parts += [rect(x - w / 2, y0 + w * 0.7, x + w / 2, y1), _head((x, y0), 270, head, w)]
    else:
        parts += [rect(x - w / 2, y0 + w * 0.7, x + w / 2, y1 - w * 0.7), _head((x, y1), 90, head, w), _head((x, y0), 270, head, w)]
    return union(parts)

def arrow_right(V, H, wt): return place(arrow(0, V, H))
def arrow_left(V, H, wt):  return place(arrow(180, V, H))
def arrow_up(V, H, wt):    return place(arrow(90, V, H))
def arrow_down(V, H, wt):  return place(arrow(270, V, H))
def arrow_lr(V, H, wt):    return place(arrow(0, V, H, both=True))
def arrow_ud(V, H, wt):    return place(arrow(90, V, H, both=True))

def undo(V, H, wt, clockwise=False):
    """↶: a half circle over the top with the arrowhead at its left foot, pointing down."""
    w = (V + H) / 2
    r = 235
    cx, cy = 0, 175
    pts = [(cx + r * math.cos(math.pi * i / 48), cy + r * math.sin(math.pi * i / 48)) for i in range(49)]
    arc = polyline(pts, w, cap='butt')
    head = _head((cx - r, cy - 150), 270, 175 + (w - 78) * 0.5, w)
    stem = rect(cx - r - w / 2, cy - 150 + w * 0.7, cx - r + w / 2, cy + 1)
    shape = union([arc, head, stem])
    if clockwise: shape = mirror_x(shape)
    return place(shape)

def redo(V, H, wt): return undo(V, H, wt, clockwise=True)

# ---------------------------------------------------------------- the rest

def copy_squares(V, H, wt):
    """⧉: two squares, the front one whole, the back one peeping out above and to the right."""
    s, d = 420, 170
    w = (V + H) / 2 * 0.92
    y0 = MID - (s + d) / 2
    front = rect(0, y0, s, y0 + s)
    back = rect(d, y0 + d, d + s, y0 + d + s)
    ring = lambda r: minus(r, _shrink(r, w))
    gap = _grow(front, w * 0.55)
    return place(union([ring(front), minus(ring(back), gap)]))

def _shrink(r, w):
    x0, y0, x1, y1 = r.bounds
    return rect(x0 + w, y0 + w, x1 - w, y1 - w)

def _grow(r, w):
    x0, y0, x1, y1 = r.bounds
    return rect(x0 - w, y0 - w, x1 + w, y1 + w)

def check(V, H, wt):
    w = V * 1.0
    return place(polyline([(0, 300), (190, 90), (560, 600)], w))

def cross(V, H, wt):
    w = V * 0.95
    s = 440
    return place(union([polyline([(0, MID - s / 2), (s, MID + s / 2)], w), polyline([(0, MID + s / 2), (s, MID - s / 2)], w)]))

def eighth_note(V, H, wt):
    """♪: an oval head leaning back, a stem, and a flag that swings out and down."""
    hw, hh = 165, 118
    cx, cy = 165, 110
    head = rot(_ellipse(cx, cy, hw, hh), 22, cx, cy)
    sx = cx + hw * 0.93 - V / 2
    stem = rect(sx - V / 2 + 2, cy + 20, sx + V / 2, 720)
    from geom import bezier
    fl = bezier((sx, 690), (sx + 30, 560), (sx + 230, 520), (sx + 190, 320))
    flag = stroke(fl, V * 1.25, V * 0.75, cap='round')
    return place(union([head, stem, flag]))

def beamed_notes(V, H, wt):
    """♫: two heads joined by a beam."""
    hw, hh = 150, 108
    parts = []
    for cx, top in ((150, 690), (560, 740)):
        cy = 110
        parts.append(rot(_ellipse(cx, cy, hw, hh), 22, cx, cy))
        sx = cx + hw * 0.93 - V / 2
        parts.append(rect(sx - V / 2, cy + 20, sx + V / 2, top))
    sx1 = 150 + hw * 0.93 - V / 2; sx2 = 560 + hw * 0.93 - V / 2
    bw = H * 1.7
    parts.append(poly([(sx1 - V / 2, 690 - bw), (sx2 + V / 2, 740 - bw), (sx2 + V / 2, 740), (sx1 - V / 2, 690)]))
    return place(union(parts))

def _ellipse(cx, cy, a, b, n=96):
    return poly([(cx + a * math.cos(2 * math.pi * i / n), cy + b * math.sin(2 * math.pi * i / n)) for i in range(n)])

def gear(V, H, wt):
    """⚙: eight teeth, a hub hole."""
    R, r0, hole = 305, 228, 96 - (V - 80) * 0.25
    n = 8
    body = circle(0, MID, r0)
    teeth = []
    for i in range(n):
        a = 2 * math.pi * i / n
        tw0, tw1 = 0.26, 0.18          # half-width of a tooth at its root and its tip (radians at r0 / R)
        pts = [(r0 * 0.9 * math.cos(a - tw0), r0 * 0.9 * math.sin(a - tw0)), (R * math.cos(a - tw1), R * math.sin(a - tw1)),
               (R * math.cos(a + tw1), R * math.sin(a + tw1)), (r0 * 0.9 * math.cos(a + tw0), r0 * 0.9 * math.sin(a + tw0))]
        teeth.append(poly([(x, MID + y) for x, y in pts]))
    shape = minus(union([body] + teeth), circle(0, MID, hole))
    return place(shape)

def command(V, H, wt):
    """⌘: a square whose sides run on into four loops."""
    w = H * 0.95
    a, r = 115, 105
    cy = MID
    lines = [rect(-a - w / 2, cy - a - r, -a + w / 2, cy + a + r), rect(a - w / 2, cy - a - r, a + w / 2, cy + a + r),
             rect(-a - r, cy - a - w / 2, a + r, cy - a + w / 2), rect(-a - r, cy + a - w / 2, a + r, cy + a + w / 2)]
    loops = []
    for sx in (-1, 1):
        for sy in (-1, 1):
            cx_, cy_ = sx * (a + r), cy + sy * (a + r)
            ringp = minus(circle(cx_, cy_, r + w / 2), circle(cx_, cy_, r - w / 2))
            # the quarter facing the centre is left out
            q = rect(min(cx_, cx_ - sx * 400), min(cy_, cy_ - sy * 400), max(cx_, cx_ - sx * 400), max(cy_, cy_ - sy * 400))
            loops.append(minus(ringp, q))
    return place(union(lines + loops))

# ---------------------------------------------------------------- music signs (for note names: C♯, D♭, E♮)

CAP = 700

def _beam(x0, y0, x1, y1, h):
    """A sloped beam, h thick measured upright."""
    return poly([(x0, y0 - h / 2), (x1, y1 - h / 2), (x1, y1 + h / 2), (x0, y0 + h / 2)])

def _music_weights(V):
    # uprights a little lighter than the stem, beams heavier, but never so heavy that the gap between the beams
    # closes up at 11 px
    return V * 0.74, min(V * 1.25, CAP * 0.15)

def sharp(V, H, wt):
    thin, thick = _music_weights(V)
    C = CAP
    w = C * 0.58
    lx, rx = w * 0.31, w * 0.69
    rise = C * 0.11
    parts = [rect(lx - thin / 2, -C * 0.15, lx + thin / 2, C * 0.93), rect(rx - thin / 2, -C * 0.07, rx + thin / 2, C * 1.01)]
    for yb in (C * 0.22, C * 0.64):
        parts.append(_beam(0, yb - rise / 2, w, yb + rise / 2, thick))
    return place(union(parts), 40)

def natural(V, H, wt):
    thin, thick = _music_weights(V)
    C = CAP
    w = C * 0.42
    lx, rx = thin / 2 + w * 0.08, w * 0.92 - thin / 2
    rise = C * 0.11
    parts = [rect(lx - thin / 2, C * 0.20, lx + thin / 2, C * 1.0), rect(rx - thin / 2, -C * 0.16, rx + thin / 2, C * 0.66)]
    for yb in (C * 0.22, C * 0.62):
        parts.append(_beam(lx - thin / 2, yb - rise / 2, rx + thin / 2, yb + rise / 2, thick))
    return place(union(parts), 40)

def flat(V, H, wt):
    """♭: a tall stem and a round bowl that narrows to a point at the stem's foot, like a geometric b."""
    from geom import bezier
    thin = _music_weights(V)[0]
    C = CAP
    sx = thin / 2
    stem = rect(0, -C * 0.02, thin, C * 1.03)
    h, w = C * 0.56, C * 0.47
    b0 = -C * 0.02
    pts = bezier((sx, b0 + h * 0.70), (sx + w * 0.55, b0 + h * 1.08), (sx + w * 1.12, b0 + h * 0.62), (sx + 2, b0 + thin * 0.35))
    bowl = stroke(pts, V * 1.05, V * 0.45, ease=1.5, cap='butt')
    shape = minus(union([stem, bowl]), rect(-200, -400, 1000, b0))
    return place(shape, 40)

SYMBOLS = [
    ('uni266F', [0x266F], sharp),
    ('uni266D', [0x266D], flat),
    ('uni266E', [0x266E], natural),
    ('uni25B6', [0x25B6, 0x23F5], black_right),
    ('uni25C0', [0x25C0, 0x23F4], black_left),
    ('uni25B8', [0x25B8], small_right),
    ('uni25C2', [0x25C2], small_left),
    ('uni25B2', [0x25B2, 0x23F6], black_up),
    ('uni25BC', [0x25BC, 0x23F7], black_down),
    ('uni25B4', [0x25B4], small_up),
    ('uni25BE', [0x25BE], small_down),
    ('uni25A0', [0x25A0, 0x23F9], black_square),
    ('uni25A1', [0x25A1], white_square),
    ('uni25AA', [0x25AA], small_square),
    ('uni25CF', [0x25CF, 0x23FA], black_circle),
    ('uni25CB', [0x25CB], white_circle),
    ('arrowleft', [0x2190], arrow_left),
    ('arrowup', [0x2191], arrow_up),
    ('arrowright', [0x2192], arrow_right),
    ('arrowdown', [0x2193], arrow_down),
    ('arrowboth', [0x2194], arrow_lr),
    ('arrowupdn', [0x2195], arrow_ud),
    ('uni21B6', [0x21B6], undo),
    ('uni21B7', [0x21B7], redo),
    ('uni29C9', [0x29C9], copy_squares),
    ('uni2713', [0x2713, 0x2714], check),
    ('uni2715', [0x2715, 0x2716], cross),
    ('musicalnote', [0x266A], eighth_note),
    ('musicalnotedbl', [0x266B], beamed_notes),
    ('uni2699', [0x2699], gear),
    ('uni2318', [0x2318], command),
]
