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
RIM = 22                            # the dark rolled rim
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
def tz(fill, height, lift):
    d, (x0, y0, x1, y1), upm, adv = glyph('NachtzugAntiqua-Bold.ttf', 't_z')
    s = height / (y1 - y0)
    # optical centre: the ink box sits a little above the middle, because the long tail pulls the eye down
    cx, cy = W / 2 + 6, H / 2 - lift
    tx = cx - (x0 + x1) / 2 * s; ty = cy + (y0 + y1) / 2 * s
    return f'<path d="{d}" fill="{fill}" transform="translate({tx:.1f},{ty:.1f}) scale({s:.4f},{-s:.4f})"/>'
def screw(x, y, angle, r=11):
    a = math.radians(angle); dx, dy = math.cos(a) * r * 0.62, math.sin(a) * r * 0.62
    return (f'<circle cx="{x}" cy="{y+1.5}" r="{r+1}" fill="#000" opacity=".18"/>'
            f'<circle cx="{x}" cy="{y}" r="{r}" fill="url(#brass)" stroke="#5c4524" stroke-width="1.6"/>'
            f'<line x1="{x-dx:.1f}" y1="{y-dy:.1f}" x2="{x+dx:.1f}" y2="{y+dy:.1f}" stroke="#4a3618" stroke-width="2.6" stroke-linecap="round"/>'
            f'<line x1="{x-dx:.1f}" y1="{y-dy+1.2:.1f}" x2="{x+dx:.1f}" y2="{y+dy+1.2:.1f}" stroke="#f6dfa0" stroke-width="1" stroke-linecap="round" opacity=".6"/>')
def detailed(field, mark, rule, rim, name):
    defs = f'''<radialGradient id="brass" cx=".35" cy=".3" r=".75"><stop offset="0" stop-color="#f3d488"/><stop offset=".55" stop-color="#c9a24a"/><stop offset="1" stop-color="#8a6a2c"/></radialGradient>
<linearGradient id="rimlight" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#fff" stop-opacity=".10"/><stop offset=".08" stop-color="#fff" stop-opacity="0"/><stop offset=".88" stop-color="#000" stop-opacity="0"/><stop offset="1" stop-color="#000" stop-opacity=".25"/></linearGradient>
<linearGradient id="fieldshade" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#000" stop-opacity=".07"/><stop offset=".06" stop-color="#000" stop-opacity="0"/><stop offset=".95" stop-color="#fff" stop-opacity="0"/><stop offset="1" stop-color="#fff" stop-opacity=".18"/></linearGradient>'''
    body = (rr(0, fill=rim) + rr(0, fill='url(#rimlight)') +              # the rolled rim, lit from above
            rr(RIM, fill=field) + rr(RIM, fill='url(#fieldshade)') +       # the enamel, a touch recessed
            notched(RIM + 40, RIM + 25, 42, rule, 5) +
            notched(RIM + 50, RIM + 25, 54, rule, 2) +
            tz(mark, 226, 10) +
            # the screws sit in the cream band between the rim and the rules, set into the corners' curve
            screw(RIM + 25, RIM + 25, 28) + screw(W - RIM - 25, RIM + 25, -18) +
            screw(RIM + 25, H - RIM - 25, 64) + screw(W - RIM - 25, H - RIM - 25, 8))
    open(name, 'w').write(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}"><defs>{defs}</defs>{body}</svg>')
detailed(CREAM, RUST, RUST, INK, 'enamel.svg')
detailed(INK, CREAM, '#c9a24a', '#c9a24a', 'enamel-dark.svg')
# the flat icon: square-ish plate, thicker rim, one rule, no screws, a bigger tz
Wf, Hf, Rf, RIMf = 512, 512, 104, 40
d, (x0, y0, x1, y1), upm, adv = glyph('NachtzugAntiqua-Bold.ttf', 't_z')
s = 330 / (y1 - y0); tx = Wf/2 + 8 - (x0 + x1) / 2 * s; ty = Hf/2 - 14 + (y0 + y1) / 2 * s
flat = (f'<rect x="0" y="0" width="{Wf}" height="{Hf}" rx="{Rf}" fill="{INK}"/>'
        f'<rect x="{RIMf}" y="{RIMf}" width="{Wf-2*RIMf}" height="{Hf-2*RIMf}" rx="{Rf-RIMf}" fill="{CREAM}"/>'
        f'<rect x="{RIMf+18}" y="{RIMf+18}" width="{Wf-2*RIMf-36}" height="{Hf-2*RIMf-36}" rx="{Rf-RIMf-18}" fill="none" stroke="{RUST}" stroke-width="10"/>'
        f'<path d="{d}" fill="{RUST}" transform="translate({tx:.1f},{ty:.1f}) scale({s:.4f},{-s:.4f})"/>')
open('enamel-flat.svg', 'w').write(f'<svg xmlns="http://www.w3.org/2000/svg" width="{Wf}" height="{Hf}" viewBox="0 0 {Wf} {Hf}">{flat}</svg>')
print('ok')

# the tiny icon (24 px and below): no rule, a stronger rim, the tz filling the plate
Wt, Rt, RIMt = 512, 112, 56
s2 = 360 / (y1 - y0); tx2 = Wt/2 + 8 - (x0 + x1) / 2 * s2; ty2 = Wt/2 - 16 + (y0 + y1) / 2 * s2
tiny = (f'<rect x="0" y="0" width="{Wt}" height="{Wt}" rx="{Rt}" fill="{INK}"/>'
        f'<rect x="{RIMt}" y="{RIMt}" width="{Wt-2*RIMt}" height="{Wt-2*RIMt}" rx="{Rt-RIMt}" fill="{CREAM}"/>'
        f'<path d="{d}" fill="{RUST}" transform="translate({tx2:.1f},{ty2:.1f}) scale({s2:.4f},{-s2:.4f})"/>')
open('enamel-tiny.svg', 'w').write(f'<svg xmlns="http://www.w3.org/2000/svg" width="{Wt}" height="{Wt}" viewBox="0 0 {Wt} {Wt}">{tiny}</svg>')
