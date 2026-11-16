#!/usr/bin/env python3
"""Makes the cover pictures from assets/art/grossstadtbahnhof.jpg (Hans Baluschek, "Großstadtbahnhof", 1904,
public domain): the website's opening and footer (site/assets/img/cover-art-1200.jpg and -2400.jpg, the whole
painting, placed by the CSS) and the manuals' wraparound cover (docs/manual/img/cover-wrap.jpg, a band across the
painting: the locomotives with their lamps on the front, the signal box on the back). Then run
tools/logo/social_preview.py, docs/manual/build.py (en and de) and tools/site_covers.sh.

How the master was made from the only copy on Wikimedia Commons (1600 × 1125, a scan of a printed reproduction):
    1. tools/cover/descreen.py takes the printer's halftone screen out (the dots, as peaks in the spectrum)
    2. upscaled 4× with 4xRealWebPhoto_v4_dat2 (Phhofm, CC BY 4.0, https://github.com/Phhofm/models), run as
       ComfyUI runs upscale models (spandrel), in tiles; then brought down to 3200 px wide
    3. tools/cover/grade.py: levels, a little vibrance and local contrast, as the print had gone brown and flat

    python3 tools/cover/make.py
"""
import os
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
P = lambda *a: os.path.join(ROOT, *a)
WRAP_TOP, WRAP = 0.40, (2962, 1152)        # the band for the manuals' cover: from 40 % down, at the wrap's 2.57 : 1

art = Image.open(P('assets/art/grossstadtbahnhof.jpg')).convert('RGB')
for w in (1200, 2400):
    art.resize((w, round(art.height * w / art.width)), Image.LANCZOS).save(
        P(f'site/assets/img/cover-art-{w}.jpg'), quality=84, optimize=True, progressive=True)
band = round(art.width * WRAP[1] / WRAP[0])
top = round(art.height * WRAP_TOP)
art.crop((0, top, art.width, top + band)).resize(WRAP, Image.LANCZOS).save(
    P('docs/manual/img/cover-wrap.jpg'), quality=90, optimize=True)
print('cover pictures made')
