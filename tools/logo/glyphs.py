from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.boundsPen import BoundsPen
import os
F = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'assets', 'fonts', 'nachtzug') + os.sep
def glyph(font, name):
    t = TTFont(F + font); gs = t.getGlyphSet()
    p = SVGPathPen(gs); gs[name].draw(p)
    b = BoundsPen(gs); gs[name].draw(b)
    return p.getCommands(), b.bounds, t['head'].unitsPerEm, gs[name].width
def placed(font, name, cx, cy, height, fill):
    """the glyph's ink box centred on (cx, cy), its ink height = height"""
    d, (x0, y0, x1, y1), upm, adv = glyph(font, name)
    s = height / (y1 - y0)
    tx = cx - (x0 + x1) / 2 * s; ty = cy + (y0 + y1) / 2 * s
    return f'<path d="{d}" fill="{fill}" transform="translate({tx:.1f},{ty:.1f}) scale({s:.4f},{-s:.4f})"/>', (x1 - x0) * s
