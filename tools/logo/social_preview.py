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
STRIPES = ['#b4512a', '#e08a2c', '#c9a24a']

art = Image.open(P('site/assets/img/cover-art-2400.jpg')).convert('RGB')
crop_h = int(art.width * ART / W)
top = int((art.height - crop_h) * 0.45)    # the train and the moon, not the sky's empty top
card = Image.new('RGB', (W, H), BAND)
card.paste(art.crop((0, top, art.width, top + crop_h)).resize((W, ART), Image.LANCZOS), (0, 0))

d = ImageDraw.Draw(card)
y = ART
for c in STRIPES:
    d.rectangle([0, y, W, y + 9], fill=c)
    y += 13

sign = Image.open(P('assets/logo/enamel-sign.png')).convert('RGBA')
sw = 250
sign = sign.resize((sw, sign.height * sw // sign.width), Image.LANCZOS)
sx = 70
card.paste(sign, (sx, ART + 20 - sign.height // 2), sign)

name = ImageFont.truetype(P('assets/fonts/nachtzug/NachtzugAntiqua-Bold.ttf'), 88)
sub = ImageFont.truetype(P('assets/fonts/nachtzug/NachtzugAntiqua-Regular.ttf'), 30)
tx = sx + sw + 50
d.text((tx, ART + 48), 'Nachtzug', font=name, fill=(243, 236, 224))
d.text((tx, ART + 158), 'Sequencer · Synthesiser · for acid and Berlin School', font=sub, fill=(232, 183, 122))

for out in ('assets/logo/social-preview.png', 'site/assets/img/social-preview.png'):   # GitHub's and the website's
    card.save(P(out), optimize=True)
    print('Wrote', out)
