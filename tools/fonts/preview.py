"""Quick look at the initials: python preview.py LETTERS SIZE OUT.png [small]"""
import sys, os
from PIL import Image, ImageDraw, ImageFont
HERE = os.path.dirname(os.path.abspath(__file__))
letters = sys.argv[1] if len(sys.argv) > 1 else 'TNAD'
size = int(sys.argv[2]) if len(sys.argv) > 2 else 300
out = sys.argv[3] if len(sys.argv) > 3 else os.path.join(HERE, 'out/preview.png')
fr = ImageFont.truetype(os.path.join(HERE, 'out/NachtzugInitialenRahmen-Regular.ttf'), size)
le = ImageFont.truetype(os.path.join(HERE, 'out/NachtzugInitialenBuchstabe-Regular.ttf'), size)
cols = min(len(letters), int(sys.argv[4]) if len(sys.argv) > 4 else 8)
rows = (len(letters) + cols - 1) // cols
pad = max(8, size // 10)
img = Image.new('RGB', (cols * (size + pad) + pad, rows * (size + pad) + pad), (240, 225, 186))
d = ImageDraw.Draw(img)
for i, c in enumerate(letters):
    x = pad + (i % cols) * (size + pad); y = pad + (i // cols) * (size + pad)
    d.text((x, y), c, font=fr, fill=(43, 38, 35))
    d.text((x, y), c, font=le, fill=(165, 72, 31))
img.save(out)
print(out, img.size)
