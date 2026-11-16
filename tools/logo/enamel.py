"""The Nachtzug logo: the tz ligature of Nachtzug Antiqua Bold on a cream enamel sign.

    python3 tools/logo/enamel.py        (needs fontTools)  -> assets/logo/*.svg, then tools/logo/make_icons.sh

enamel.svg is the sign for large sizes (with screws and a double rule that curves round them), enamel-dark.svg the
same in dark, enamel-flat.svg the square app icon, enamel-tiny.svg the icon for 24 px and below."""
import os, sys, math
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
os.chdir(os.path.join(HERE, '..', '..', 'assets', 'logo'))
from glyphs import glyph
INK, RUST, CREAM = '#2b2623', '#a9441f', '#f4ead6'
W, H = 600, 420                     # the plate, 10 : 7
R = 64                              # outer corner radius; every inner curve is concentric with it
RIM = 11                            # the dark rolled rim (it was 22, which made the sign look blocky)
def rr(inset, **attr):
    """a rounded rectangle inset from the plate's edge, its corner concentric with the plate's"""
    a = ' '.join(f'{k.replace("_","-")}="{v}"' for k, v in attr.items())
    return f'<rect x="{inset}" y="{inset}" width="{W-2*inset}" height="{H-2*inset}" rx="{max(0, R-inset)}" {a}/>'
def notched(inset, around, radius, stroke, width):
    """a rule inset from the edge whose corners curve inwards around the screws at (around, around) from each corner"""
    import math
    I = inset; c = around
    k = math.sqrt(max(0.0, radius**2 - (I - c)**2))      # where the arc meets the rule's straight edges
    a = c + k
    pts = (f'M{a:.1f} {I} L{W-a:.1f} {I} A{radius} {radius} 0 0 0 {W-I} {a:.1f} L{W-I} {H-a:.1f} '
           f'A{radius} {radius} 0 0 0 {W-a:.1f} {H-I} L{a:.1f} {H-I} A{radius} {radius} 0 0 0 {I} {H-a:.1f} '
           f'L{I} {a:.1f} A{radius} {radius} 0 0 0 {a:.1f} {I} Z')
    return f'<path d="{pts}" fill="none" stroke="{stroke}" stroke-width="{width}" stroke-linejoin="round"/>'
def weight_centre():
    """where the tz's ink balances, in font units: the bold letters, not the box that includes the thin hook"""
    from fontTools.ttLib import TTFont
    from fontTools.pens.statisticsPen import StatisticsPen
    from glyphs import F
    gs = TTFont(F + 'NachtzugAntiqua-Bold.ttf').getGlyphSet()
    p = StatisticsPen(glyphset=gs); gs['t_z'].draw(p)
    return p.meanX, p.meanY
GX, GY = weight_centre()
def placed_tz(fill, cx, cy, height, top, bottom):
    """the tz with its centre of weight on (cx, cy), its ink height = height, made smaller if it would come nearer
    than top / bottom (the hook hangs well below the letters, so centring the ink box set the letters too high)"""
    d, (x0, y0, x1, y1), upm, adv = glyph('NachtzugAntiqua-Bold.ttf', 't_z')
    s = min(height / (y1 - y0), (bottom - cy) / (GY - y0), (cy - top) / (y1 - GY))
    tx = cx - GX * s; ty = cy + GY * s
    return f'<path d="{d}" fill="{fill}" transform="translate({tx:.1f},{ty:.1f}) scale({s:.4f},{-s:.4f})"/>'
def tz(fill, height):
    return placed_tz(fill, W / 2, H / 2, height, RIM + 48 + 14, H - RIM - 48 - 14)   # 14 clear of the inner rule
def screw(x, y, angle, r=9):
    a = math.radians(angle); dx, dy = math.cos(a) * r * 0.62, math.sin(a) * r * 0.62
    return (f'<circle cx="{x}" cy="{y+1.5}" r="{r+1}" fill="#000" opacity=".18"/>'
            f'<circle cx="{x}" cy="{y}" r="{r}" fill="url(#brass)" stroke="#5c4524" stroke-width="1.6"/>'
            f'<line x1="{x-dx:.1f}" y1="{y-dy:.1f}" x2="{x+dx:.1f}" y2="{y+dy:.1f}" stroke="#4a3618" stroke-width="2.6" stroke-linecap="round"/>'
            f'<line x1="{x-dx:.1f}" y1="{y-dy+1.2:.1f}" x2="{x+dx:.1f}" y2="{y+dy+1.2:.1f}" stroke="#f6dfa0" stroke-width="1" stroke-linecap="round" opacity=".6"/>')
# the rim, lit from above; the enamel gently domed, lightest towards the top left
RIMS = {INK: ('#4a413b', '#1a1614'), '#c9a24a': ('#f6dc95', '#7d5f25')}
FIELDS = {CREAM: ('#fffaf0', CREAM, '#e3d4b6'), INK: ('#3d3530', INK, '#211c19')}
def gradients(field, rim):
    r, f = RIMS[rim], FIELDS[field]
    mid = f'<stop offset=".45" stop-color="{rim}"/>' if rim != INK else ''
    return (f'<linearGradient id="rim" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="{r[0]}"/>{mid}<stop offset="1" stop-color="{r[1]}"/></linearGradient>'
            f'<radialGradient id="dome" cx=".38" cy=".3" r=".85"><stop offset="0" stop-color="{f[0]}"/><stop offset=".55" stop-color="{f[1]}"/><stop offset="1" stop-color="{f[2]}"/></radialGradient>')
def detailed(field, mark, rule, rim, name):
    defs = ('<radialGradient id="brass" cx=".35" cy=".3" r=".75"><stop offset="0" stop-color="#f3d488"/><stop offset=".55" stop-color="#c9a24a"/><stop offset="1" stop-color="#8a6a2c"/></radialGradient>'
            + gradients(field, rim))
    c = RIM + 30                                   # the screws' centres, from each corner
    # solid colours under the gradients, for renderers that drop a gradient (WeasyPrint showed the plate see-through)
    body = (rr(0, fill=rim) + rr(0, fill='url(#rim)') + rr(RIM, fill=field) + rr(RIM, fill='url(#dome)') +
            notched(RIM + 38, RIM + 22, 40, rule, 4.5) +   # a double rule whose corners curve round the screws
            notched(RIM + 48, RIM + 22, 52, rule, 1.8) +
            tz(mark, 230) +
            screw(c, c, 28) + screw(W - c, c, -18) + screw(c, H - c, 64) + screw(W - c, H - c, 8))
    open(name, 'w').write(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}"><defs>{defs}</defs>{body}</svg>')
detailed(CREAM, RUST, RUST, INK, 'enamel.svg')
detailed(INK, CREAM, '#c9a24a', '#c9a24a', 'enamel-dark.svg')
# the flat icon: square plate, a fine dark rim (a heavy one looked blocky beside other programs' icons), one rule,
# no screws, a bigger tz
Wf, Hf, Rf, RIMf = 512, 512, 112, 10
GAPf, RULEf = 24, 6    # from the rim to the rule, and the rule's width
inner = RIMf + GAPf + RULEf / 2                       # the inside of the rule
flat = (f'<defs>{gradients(CREAM, INK)}</defs><rect x="0" y="0" width="{Wf}" height="{Hf}" rx="{Rf}" fill="url(#rim)"/>'
        f'<rect x="{RIMf}" y="{RIMf}" width="{Wf-2*RIMf}" height="{Hf-2*RIMf}" rx="{Rf-RIMf}" fill="url(#dome)"/>'
        f'<rect x="{RIMf+GAPf}" y="{RIMf+GAPf}" width="{Wf-2*RIMf-2*GAPf}" height="{Hf-2*RIMf-2*GAPf}" rx="{Rf-RIMf-GAPf}" fill="none" stroke="{RUST}" stroke-width="{RULEf}"/>'
        + placed_tz(RUST, Wf / 2, Hf / 2, 348, inner + 14, Hf - inner - 14))
open('enamel-flat.svg', 'w').write(f'<svg xmlns="http://www.w3.org/2000/svg" width="{Wf}" height="{Hf}" viewBox="0 0 {Wf} {Hf}">{flat}</svg>')
print('ok')

# the tiny icon (24 px and below): no rule, a stronger rim, the tz filling the plate
Wt, Rt, RIMt = 512, 112, 56
tiny = (f'<defs>{gradients(CREAM, INK)}</defs><rect x="0" y="0" width="{Wt}" height="{Wt}" rx="{Rt}" fill="url(#rim)"/>'
        f'<rect x="{RIMt}" y="{RIMt}" width="{Wt-2*RIMt}" height="{Wt-2*RIMt}" rx="{Rt-RIMt}" fill="url(#dome)"/>'
        + placed_tz(RUST, Wt / 2, Wt / 2, 360, RIMt + 10, Wt - RIMt - 10))
open('enamel-tiny.svg', 'w').write(f'<svg xmlns="http://www.w3.org/2000/svg" width="{Wt}" height="{Wt}" viewBox="0 0 {Wt} {Wt}">{tiny}</svg>')
