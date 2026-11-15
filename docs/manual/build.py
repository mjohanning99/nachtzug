#!/usr/bin/env python3
"""Build the Nachtzug manual with WeasyPrint.

    python3 docs/manual/build.py        English: manual.html    -> docs/Nachtzug-Manual.pdf
    python3 docs/manual/build.py de     German:  manual.de.html -> docs/Nachtzug-Handbuch.pdf

The HTML holds knob placeholders, which are drawn here:
    <i class="knob" data-v="0.30">Cutoff</i>        a knob set to 30 % (data-v="-1": an empty knob to fill in by hand)
    <i class="sel" data-opts="A|B|C" data-on="1">Label</i>   a selector with option 1 lit (data-on="-1": none)
Chapter thumb tabs are generated (one named page per chapter), and the patch sheet between <!--SHEET-->
and <!--/SHEET--> is repeated so that the booklet has a multiple of four pages, as a folded print would.
"""
import html, math, pathlib, re, sys
from weasyprint import HTML

here = pathlib.Path(__file__).resolve().parent
lang = sys.argv[1] if len(sys.argv) > 1 else "en"
src_file = here / ("manual.html" if lang == "en" else f"manual.{lang}.html")
out = here.parent / ("Nachtzug-Manual.pdf" if lang == "en" else "Nachtzug-Handbuch.pdf")

# ---- thumb tabs on the outer edge, stepping down the page one slot per chapter
chapters = ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "A"]
css = []
for i, c in enumerate(chapters):
    top = 4 + i * 13.2   # mm below the top margin edge
    label = "" if c == "0" else c
    for side, box in (("right", "right-top"), ("left", "left-top")):
        css.append(f"@page ch{c}:{side} {{ @{box} {{ content: '{label}'; margin-top: {top}mm; height: 11mm; width: 7mm;"
                   f" background: {'#2b2623' if c != '0' else '#b4512a'}; color: #f3ece0; font: 600 {'7' if len(c) > 1 else '8'}pt 'Jost';"
                   f" text-align: center; vertical-align: middle; margin-{'left' if side == 'right' else 'right'}: auto;"
                   f" border-radius: {'1.2mm 0 0 1.2mm' if side == 'right' else '0 1.2mm 1.2mm 0'}; }} }}")
(here / "tabs.css").write_text("\n".join(css) + "\n")

# ---- knobs and selectors
INK, RUST, PAPER = "#2b2623", "#b4512a", "#f7f2e8"

def knob_svg(v):
    a0, a1 = -135.0, 135.0
    def pt(a, r):
        t = math.radians(a - 90)
        return 20 + r * math.cos(t), 20 + r * math.sin(t)
    parts = []
    for i in range(11):   # scale marks
        a = a0 + (a1 - a0) * i / 10
        x1, y1 = pt(a, 16.2); x2, y2 = pt(a, 18.6 if i % 5 == 0 else 17.6)
        parts.append(f'<line x1="{x1:.2f}" y1="{y1:.2f}" x2="{x2:.2f}" y2="{y2:.2f}" stroke="{INK}" stroke-width="{0.9 if i % 5 == 0 else 0.6}"/>')
    if v >= 0:   # the travelled arc, the cap and its pointer
        a = a0 + (a1 - a0) * v
        if v > 0.004:
            xs, ys = pt(a0, 14.4); xe, ye = pt(a, 14.4)
            parts.append(f'<path d="M{xs:.2f},{ys:.2f} A14.4,14.4 0 {1 if a - a0 > 180 else 0} 1 {xe:.2f},{ye:.2f}" fill="none" stroke="{RUST}" stroke-width="2"/>')
        parts.append(f'<circle cx="20" cy="20" r="11.5" fill="{INK}"/><circle cx="20" cy="20" r="8.6" fill="#3d3530"/>')
        xp, yp = pt(a, 10.6); xq, yq = pt(a, 4.5)
        parts.append(f'<line x1="{xq:.2f}" y1="{yq:.2f}" x2="{xp:.2f}" y2="{yp:.2f}" stroke="#f1c27a" stroke-width="1.8" stroke-linecap="round"/>')
    else:        # an empty knob to draw the pointer in by hand
        parts.append(f'<circle cx="20" cy="20" r="11.5" fill="{PAPER}" stroke="{INK}" stroke-width="0.9"/><circle cx="20" cy="20" r="1" fill="{INK}"/>')
    return f'<svg viewBox="0 0 40 40" xmlns="http://www.w3.org/2000/svg">{"".join(parts)}</svg>'

def knob(m):
    return f'<span class="kn">{knob_svg(float(m.group(1)))}<span class="kl">{m.group(2)}</span></span>'

def sel(m):
    opts = html.unescape(m.group(1)).split("|"); on = int(m.group(2))
    cells = "".join(f'<span class="o{" on" if i == on else ""}">{html.escape(o)}</span>' for i, o in enumerate(opts))
    return f'<span class="sl"><span class="sv">{cells}</span><span class="kl">{m.group(3)}</span></span>'

src = src_file.read_text()
# figures are numbered in the order they come, so new ones can go anywhere ("FIG 0" / "ABB. 0" in the source)
fig = iter(range(1, 1000))
src = re.sub(r'<figcaption><b>(FIG|ABB\.?) [0-9]+</b>', lambda mm: f'<figcaption><b>{mm.group(1)} {next(fig)}</b>', src)
src = re.sub(r'<i class="knob" data-v="([-0-9.]+)">(.*?)</i>', knob, src)
src = re.sub(r'<i class="sel" data-opts="([^"]*)" data-on="([-0-9]+)">(.*?)</i>', sel, src)

# the flourish under each chapter title: a tapered calligraphic swash either side of a four-pointed star, each side
# rolling up into a volute with a small leaf at its end
def flourish_svg():
    W, H, cy, mid = 300.0, 30.0, 15.0, 150.0
    def stroke(pts):
        # a filled outline around a centre line whose points carry their own half-width: a pen stroke that swells and thins
        left, right = [], []
        for i in range(len(pts)):
            x, y, w = pts[i]
            xa, ya, _ = pts[max(0, i - 1)]; xb, yb, _ = pts[min(len(pts) - 1, i + 1)]
            dx, dy = xb - xa, yb - ya; L = math.hypot(dx, dy) or 1
            nx, ny = -dy / L, dx / L
            left.append((x + nx * w, y + ny * w)); right.append((x - nx * w, y - ny * w))
        poly = left + right[::-1]
        return '<path d="M' + ' L'.join(f'{x:.2f} {y:.2f}' for x, y in poly) + ' Z"/>'
    def side(s):
        pts = []
        n = 160
        for i in range(n + 1):   # an S: down from the star, up through the middle, over into the volute
            t = i / n
            x = mid + s * (13 + t * 112)
            y = cy + 3.4 * math.sin(math.pi * 1.6 * t) * (1 - 0.25 * t)
            w = 0.3 + 1.2 * math.sin(math.pi * min(1.0, 0.5 + t * 0.9) if t > 0.55 else math.pi * (0.15 + t * 0.64)) ** 0.8   # thin at the star, full in the middle, still firm where it rolls up
            pts.append((x, y, w))
        # the volute: the end of the stroke rolls up and back over itself, thinning to a hair
        x0, y0, _ = pts[-1]
        r0 = 7.5
        w0 = pts[-1][2]
        cx, cyy = x0, y0 - r0
        for i in range(1, 141):
            u = i / 140
            a = math.pi / 2 + u * math.pi * 1.95
            r = r0 * (1 - 0.72 * u)
            pts.append((cx + s * r * math.cos(a - math.pi), cyy + r * math.sin(a), 0.22 + (w0 - 0.22) * (1 - u) ** 1.4))
        out = stroke(pts)
        ex, ey, _ = pts[-1]
        out += f'<circle cx="{ex:.2f}" cy="{ey:.2f}" r="1.25"/>'   # the ball at the end of the curl
        # a leaf on the rising part of the S, and a smaller one near the star
        for (lx, ly, ln, ang) in ((mid + s * 92, cy - 1.0, 13.0, -38), (mid + s * 34, cy + 2.6, 8.0, 32)):
            a = math.radians(ang)
            tx, ty = s * ln * math.cos(a), ln * math.sin(a)
            nx, ny = -ty / ln * ln * 0.32, tx / ln * ln * 0.32
            out += (f'<path d="M{lx:.2f} {ly:.2f} Q{lx + tx / 2 + nx:.2f} {ly + ty / 2 + ny:.2f} {lx + tx:.2f} {ly + ty:.2f} '
                    f'Q{lx + tx / 2 - nx:.2f} {ly + ty / 2 - ny:.2f} {lx:.2f} {ly:.2f} Z"/>')
        return out
    star = f"M{mid} 5 Q{mid + 1.7} 13.3 {mid + 9.5} {cy} Q{mid + 1.7} 16.7 {mid} 25 Q{mid - 1.7} 16.7 {mid - 9.5} {cy} Q{mid - 1.7} 13.3 {mid} 5 Z"
    return (f'<svg class="flourish" viewBox="0 0 {W:.0f} {H:.0f}" xmlns="http://www.w3.org/2000/svg">'
            f'<g fill="#2b2623">{side(-1)}{side(1)}</g><path d="{star}" fill="#b8501f"/></svg>')
src = src.replace('<div class="stripes"></div>', flourish_svg())

# drop caps: the first paragraph of each chapter opens with a Nachtzug initial (the frame and the letter are two
# fonts laid over each other, in two colours); an initial already marked in the source gets the same treatment
INITIAL = "ABCDEFGHIJKLMNOPQRSTUVWXYZÄÖÜ"
def initial(letter):
    return f'<span class="dc"><span class="r">{letter}</span><span class="b">{letter}</span></span>'
src = re.sub(r'<span class="dc">([A-ZÄÖÜ])</span>', lambda mm: initial(mm.group(1)), src)
def chapter_initial(mm):
    sec = mm.group(0)
    if 'class="dc"' in sec or any(k in mm.group(1) for k in ("cont", "panelch", "index", "notes")):
        return sec
    # where the chapter's head (number, kicker, title, flourish) ends: its <div> closed with all the ones inside it
    open_at = sec.find('<div class="chap-head"')
    if open_at < 0:
        return sec
    level, head = 0, -1
    for t in re.finditer(r'<(/?)div\b[^>]*>', sec[open_at:]):
        level += -1 if t.group(1) else 1
        if level == 0:
            head = open_at + t.start()
            break
    if head < 0:
        return sec
    # the initial opens the chapter: its first paragraph of body text, looking only at how the chapter begins. The
    # italic introduction (lede), pictures and the first heading are passed over; anything else that comes first (a
    # list, a table, a box, a second heading) means this chapter opens without an initial
    depth, headings, base = 0, 0, head + 6
    for t in re.finditer(r'<(/?)(div|figure|ul|ol|table|li|blockquote|p|h2|h3)\b([^>]*)>', sec[base:]):
        closing, name, attrs = t.group(1), t.group(2), t.group(3)
        if closing:
            if name not in ("p", "h2", "h3"):
                depth -= 1
            continue
        if depth:
            if name not in ("p", "h2", "h3"):
                depth += 1
            continue
        if name == "p":
            if 'class="lede"' in attrs:
                continue
            if 'class=' in attrs:
                return sec
            start = base + t.end()
            if not re.match(r'[A-ZÄÖÜ]', sec[start:]):
                return sec
            return sec[:base + t.start()] + '<p class="dropcap"' + attrs + '>' + initial(sec[start]) + sec[start + 1:]
        if name == "h2" and headings == 0:
            headings += 1
            continue
        if name == "figure":
            depth += 1
            continue
        return sec
    return sec
src = re.sub(r'<section class="chapter([^"]*)".*?</section>', chapter_initial, src, flags=re.S)
m = re.search(r"<!--SHEET-->(.*?)<!--/SHEET-->", src, re.S)

def render(n):
    page = src[:m.start()] + m.group(1) * n + src[m.end():] if m else src
    return HTML(string=page, base_url=str(here)).render()

for n in range(1, 6):   # the smallest number of patch sheets that makes the page count a multiple of four
    doc = render(n)
    if len(doc.pages) % 4 == 0:
        break
doc.metadata.title = "Nachtzug · Operation Manual" if lang == "en" else "Nachtzug · Bedienungsanleitung"
doc.write_pdf(out)
print(out, len(doc.pages), "pages")
