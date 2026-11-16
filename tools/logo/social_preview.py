#!/usr/bin/env python3
"""Draws assets/logo/social-preview.png (1280 × 640), the card shown when someone shares a link to Nachtzug's GitHub
page: the cover painting above, the cover's three bands, and the enamel sign with the name on the dark band below,
as on the website's cover. GitHub takes it under Settings → Social preview (it has no way to set it from a script);
the website names its copy in site/assets/img/ as og:image.

    python3 tools/logo/social_preview.py
"""
import os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
P = lambda *a: os.path.join(ROOT, *a)

W, H, ART = 1280, 640, 410                 # the card, and how much of it is painting
BAND = (43, 38, 35)                        # .cover on the website
STRIP = 22                                 # the band under the painting: polished brass with screws, as on the website

art = Image.open(P('site/assets/img/cover-art-2400.jpg')).convert('RGB')
crop_h = int(art.width * ART / W)
top = int((art.height - crop_h) * 0.80)    # Baluschek's locomotives with their lamps lit, low in the painting
card = Image.new('RGB', (W, H), BAND)
card.paste(art.crop((0, top, art.width, top + crop_h)).resize((W, ART), Image.LANCZOS), (0, 0))

d = ImageDraw.Draw(card)
y = ART
def lerp(c1, c2, t):
    return tuple(round(a + (b - a) * t) for a, b in zip(c1, c2))
stops = [(0, (246, 220, 149)), (.45, (201, 162, 74)), (1, (125, 95, 37))]
for i in range(STRIP):                      # the brass, lit from above, a dark line at each edge
    t = i / (STRIP - 1)
    c = (58, 44, 20) if i in (0, STRIP - 1) else next(lerp(c1, c2, (t - t1) / (t2 - t1)) for (t1, c1), (t2, c2) in zip(stops, stops[1:]) if t <= t2)
    d.line([(0, y + i), (W, y + i)], fill=c)
cy, r = y + STRIP / 2, 7
for cx in range(18, W, 200):                # the screws, their slots at an angle
    d.ellipse([cx - r - .5, cy - r + .8, cx + r + .5, cy + r + 1.3], fill=(40, 30, 14))
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(214, 178, 96), outline=(92, 69, 36), width=1)
    d.ellipse([cx - r + 2, cy - r + 1.5, cx + 1, cy + 1], fill=(240, 210, 134))
    d.line([(cx - r * .62, cy - r * .29), (cx + r * .62, cy + r * .29)], fill=(74, 54, 24), width=2)
y += STRIP

sign = Image.open(P('assets/logo/enamel-sign.png')).convert('RGBA')
sw = 250
sign = sign.resize((sw, sign.height * sw // sign.width), Image.LANCZOS)
sx = 70
card.paste(sign, (sx, ART + 20 - sign.height // 2), sign)

name = ImageFont.truetype(P('assets/fonts/nachtzug/NachtzugGrotesk-SemiBold.ttf'), 92)   # as on the website and the manuals
sub = ImageFont.truetype(P('assets/fonts/nachtzug/NachtzugAntiqua-Regular.ttf'), 30)
tx = sx + sw + 50
d.text((tx, ART + 36), 'Nachtzug', font=name, fill=(243, 236, 224), features=['liga', 'dlig'])   # with the tz ligature
d.text((tx, ART + 172), 'Sequencer · Synthesiser · for acid and Berlin School', font=sub, fill=(232, 183, 122))

for out in ('assets/logo/social-preview.png', 'site/assets/img/social-preview.png'):   # GitHub's and the website's
    card.save(P(out), optimize=True)
    print('Wrote', out)
