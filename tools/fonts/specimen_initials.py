"""All initials in two colours, on the ivory glass and on enamel: specimen/initialen.png"""
import os
from PIL import Image, ImageDraw, ImageFont
HERE = os.path.dirname(os.path.abspath(__file__))
F = lambda n, s: ImageFont.truetype(os.path.join(HERE, 'dist', n), s)
letters = 'ABCDEFGHIJKLMNOPQRSTUVWXYZÄÖÜ0123456789'
size, pad, cols = 150, 14, 10
rows = (len(letters) + cols - 1) // cols
W = cols * (size + pad) + pad
H = rows * (size + pad) + pad + 60
img = Image.new('RGB', (W, H * 2), (235, 229, 216))
d = ImageDraw.Draw(img)
fr, le = F('NachtzugInitialenRahmen-Regular.ttf', size), F('NachtzugInitialenBuchstabe-Regular.ttf', size)
cap = F('NachtzugAntiqua-Regular.ttf', 24)
for panel, (bg, ink, rust, title) in enumerate([((240, 225, 186), (59, 42, 27), (216, 100, 42), 'Auf dem Glas: Braun #3b2a1b und Bernstein #d8642a'),
                                               ((235, 229, 216), (43, 38, 35), (165, 72, 31), 'Auf Emaille: Tinte #2b2623 und Rost #a5481f')]):
    y0 = panel * H
    d.rectangle([0, y0, W, y0 + H], fill=bg)
    d.text((pad, y0 + 16), title, font=cap, fill=(90, 80, 70))
    for i, c in enumerate(letters):
        x = pad + (i % cols) * (size + pad); y = y0 + 60 + (i // cols) * (size + pad)
        d.text((x, y), c, font=fr, fill=ink)
        d.text((x, y), c, font=le, fill=rust)
img.save(os.path.join(HERE, 'specimen', 'initialen.png'))
print(img.size)
