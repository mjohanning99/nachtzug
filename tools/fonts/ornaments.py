"""The Nachtzug ornaments: moon, stars, locomotive, sleeping car, track, signal, fleurons, window and lantern.
Each function returns (shape, advance width) in 1000-unit font space, standing on the baseline."""
import math
from geom import (rect, circle, ring, poly, rounded_rect, sparkle, star, crescent, leaf, bezier, curl, end_tangent,
                  stroke, union, minus, inter, transformed, dilate)

def mirror_x(path, width):
    return union([transformed(path, (-1, 0, 0, 1, width, 0))])

def moon_waxing():
    # the lit side on the right, as the waxing moon is seen from the northern hemisphere
    return crescent(330, 300, 300, phase=0.5, tilt=math.pi), 640

def moon_waning():
    s, w = moon_waxing()
    return mirror_x(s, w), w

def twinkle():
    return union([sparkle(350, 330, 330, waist=0.19), ]), 700

def star5():
    return star(350, 300, 320, inner=0.45), 700

def asterism():
    parts = [sparkle(350, 480, 145, waist=0.2), sparkle(180, 165, 145, waist=0.2), sparkle(520, 165, 145, waist=0.2)]
    return union(parts), 700

def _wheel(cx, cy, r, spokes=8, rim=26):
    parts = [ring(cx, cy, r - rim / 2, rim), circle(cx, cy, r * 0.24)]
    for k in range(spokes):
        a = 2 * math.pi * k / spokes + 0.2
        parts.append(stroke([(cx, cy), (cx + (r - rim / 2) * math.cos(a), cy + (r - rim / 2) * math.sin(a))], 13, 13, cap='butt'))
    return union(parts)

def locomotive():
    """A steam locomotive of the Reichsbahn kind, facing right, with its cab window lit."""
    p = []
    # frame and running board
    p.append(rect(150, 175, 1265, 222))
    # boiler and smokebox
    p.append(rounded_rect(400, 222, 1150, 448, 40))
    p.append(rounded_rect(1120, 212, 1240, 470, 30))
    # chimney with a flared top, steam dome and sand dome
    p.append(rect(1090, 440, 1150, 555))
    p.append(poly([(1074, 548), (1166, 548), (1158, 590), (1082, 590)]))
    p.append(minus(circle(830, 448, 62), rect(700, 200, 960, 448)))
    p.append(rect(768, 440, 892, 470))
    p.append(minus(circle(640, 448, 46), rect(560, 200, 720, 448)))
    p.append(rect(594, 440, 686, 462))
    # smoke deflector plates (Witte type), the mark of a German express locomotive
    p.append(rect(1180, 230, 1210, 450))
    # cab with roof and lit window
    cab = rounded_rect(170, 222, 420, 575, 18)
    cab = minus(cab, rounded_rect(232, 392, 358, 520, 22))
    p.append(cab)
    p.append(rounded_rect(140, 560, 450, 602, 20))
    # buffers and headlamp
    p.append(rect(1265, 250, 1310, 300)); p.append(rect(1300, 238, 1316, 312))
    p.append(circle(1242, 520, 26))
    # wheels: trailing, three coupled drivers, leading
    wheels = [_wheel(285, 78, 78, 6, 22), _wheel(520, 108, 108), _wheel(760, 108, 108), _wheel(1000, 108, 108), _wheel(1180, 70, 70, 6, 22)]
    shape = union(p)
    shape = minus(shape, union([circle(x, y, r + 14) for x, y, r in ((285, 78, 78), (520, 108, 108), (760, 108, 108), (1000, 108, 108), (1180, 70, 70))]))
    rod = rect(520, 96, 1000, 120)
    cyl = rounded_rect(1090, 110, 1215, 190, 16)
    return union([shape, rod, cyl] + wheels), 1400

def coach():
    """A sleeping car with its windows lit."""
    p = []
    body = rounded_rect(70, 130, 1230, 560, 46)
    roof = rounded_rect(90, 520, 1210, 610, 44)
    p += [body, roof]
    shape = union(p)
    holes = []
    # windows: two narrow at the ends (doors), a row of eight compartment windows
    xs = [150 + i * 125 for i in range(9)]
    for i, x in enumerate(xs):
        holes.append(rounded_rect(x + 30, 340, x + 112, 470, 16))
    holes.append(rounded_rect(96, 230, 132, 470, 14))
    holes.append(rounded_rect(1168, 230, 1204, 470, 14))
    # the fine waist line under the windows (the coach's livery stripe)
    holes.append(rect(150, 300, 1150, 312))
    shape = minus(shape, union(holes))
    # underframe and two bogies
    under = [rect(150, 105, 1150, 132)]
    for bx in (250, 1050):
        under.append(rounded_rect(bx - 140, 70, bx + 140, 110, 18))
        under.append(_wheel(bx - 75, 60, 60, 6, 20))
        under.append(_wheel(bx + 75, 60, 60, 6, 20))
    return union([shape] + under), 1300

def track_tile():
    """A length of track that tiles seamlessly: rail on two sleepers, seen from the side. Advance 500."""
    w = 500
    parts = [minus(rect(0, 92, w, 150), rect(0, 104, w, 110))]
    for x in (125, 375):
        parts.append(rect(x - 52, 20, x + 52, 86))
    for i in range(10):
        parts.append(circle(25 + 50 * i, 0, 15))
    return union(parts), w

def signal():
    """A semaphore home signal, arm raised for a clear road."""
    p = [rect(225, 0, 275, 760)]                       # the mast
    p.append(rect(150, 0, 350, 36))                    # foot
    p.append(circle(250, 790, 34))                     # finial
    # the arm: pivoting at the mast, raised 45 degrees, with its round end disc
    a = math.radians(45)
    x0, y0 = 250, 640
    L = 300
    x1, y1 = x0 + L * math.cos(a), y0 + L * math.sin(a)
    p.append(stroke([(x0 - 40 * math.cos(a), y0 - 40 * math.sin(a)), (x1, y1)], 52, 52, cap='butt'))
    p.append(circle(x1, y1, 48))
    p.append(circle(x0, y0, 30))
    # the spectacle plate with its two lenses, and the ladder rungs
    spec = circle(x0 - 70, y0 - 70, 58)
    spec = minus(spec, union([circle(x0 - 88, y0 - 52, 20), circle(x0 - 52, y0 - 88, 20)]))
    p.append(spec)
    for k in range(6):
        p.append(rect(200, 90 + k * 95, 300, 104 + k * 95))
    shape = union(p)
    shape = minus(shape, circle(x1, y1, 18))
    return shape, 560

def fleuron():
    """A vine leaf on a curling stem, after the tendrils of the initials."""
    stem = bezier((60, 120), (160, 20), (330, 40), (420, 200))
    c = curl(stem[-1], end_tangent(stem), 70, 1.25, 1, r_end=18)
    s = stroke(stem + c[1:], 48, 16)
    s = union([s, circle(c[-1][0], c[-1][1], 20)])
    lf = leaf(400, 230, math.radians(70), 420, 290, bend=-0.08, rib=14)
    small = leaf(200, 70, math.radians(150), 170, 90, bend=0.1, rib=8)
    return union([s, lf, small]), 760

def fleuron_rev():
    s, w = fleuron()
    return mirror_x(s, w), w

def window():
    """The sleeping-car window with the moon, as in the initials."""
    w, h = 460, 600
    x0, y0, x1, y1 = 40, 0, 40 + w, h
    L = 34
    outer = rounded_rect(x0, y0, x1, y1, 90)
    inner = rounded_rect(x0 + L, y0 + L, x1 - L, y1 - L, 60)
    parts = [minus(outer, inner)]
    sy = y0 + h * 0.3
    parts.append(rect(x0 + L, sy, x1 - L, sy + 30))
    parts.append(crescent(270, 400, 105, phase=0.42, tilt=math.pi))
    parts.append(sparkle(405, 500, 36, waist=0.22))
    parts.append(sparkle(150, 300, 26, waist=0.22))
    for x in (150, 260, 380):
        parts.append(circle(x, 100 + (x % 7) * 6, 12))
    return union(parts), 540

def lantern():
    """A railwayman's hand lamp."""
    parts = []
    parts.append(minus(ring(250, 640, 85, 30), rect(100, 470, 400, 640)))   # handle
    parts.append(rect(150, 0, 350, 40))                                     # base
    body = rounded_rect(130, 40, 370, 560, 36)
    body = minus(body, circle(250, 300, 92))                                # the lens
    parts.append(body)
    parts.append(circle(250, 300, 46))                                      # the flame seen through it
    parts.append(rect(165, 560, 335, 600))
    return union(parts), 500

ORNAMENTS = [
    # name, function, PUA, other code points, letter in the dingbat font
    ('orn.moon', moon_waxing, 0xE100, [0x263D], 'a'),
    ('orn.moonwaning', moon_waning, 0xE101, [0x263E], 'b'),
    ('orn.twinkle', twinkle, 0xE102, [0x2726], 'c'),
    ('orn.star', star5, 0xE103, [0x2605], 'd'),
    ('orn.asterism', asterism, 0xE104, [0x2042], 'e'),
    ('orn.locomotive', locomotive, 0xE105, [0x1F682], 'f'),
    ('orn.sleepingcar', coach, 0xE106, [0x1F683], 'g'),
    ('orn.track', track_tile, 0xE107, [0x1F6E4], 'h'),
    ('orn.signal', signal, 0xE108, [], 'i'),
    ('orn.fleuron', fleuron, 0xE109, [], 'j'),
    ('orn.fleuronrev', fleuron_rev, 0xE10A, [], 'k'),
    ('orn.window', window, 0xE10B, [], 'l'),
    ('orn.lantern', lantern, 0xE10C, [], 'm'),
]
