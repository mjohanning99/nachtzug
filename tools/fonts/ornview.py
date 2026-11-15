import sys
from PIL import Image, ImageDraw
from geom import contours_of, signed_area
import ornaments as O
S = 0.22
items = [(n, f()) for n, f, *_ in O.ORNAMENTS]
W = int(sum(w for _, (s, w) in items) * S + 40 * len(items))
img = Image.new('RGB', (min(W, 3000), 260), (240, 225, 186)); d = ImageDraw.Draw(img)
x = 20
for n, (s, w) in items:
    for c in sorted(contours_of(s, 0.5), key=lambda c: -abs(signed_area(c))):
        d.polygon([(x + px * S, 220 - py * S) for px, py in c], fill=(43, 38, 35) if signed_area(c) > 0 else (240, 225, 186))
    d.line([(x, 222), (x + w * S, 222)], fill=(200, 80, 40))
    x += w * S + 40
img.save('look/orn.png'); print(img.size)
