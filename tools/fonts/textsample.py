"""Renders Nachtzug Antiqua with and without features (Pillow + raqm): python textsample.py out.png"""
import sys, os
from PIL import Image, ImageDraw, ImageFont
HERE = os.path.dirname(os.path.abspath(__file__))
R = os.path.join(HERE, 'antiqua/NachtzugAntiqua-Regular.ttf'); B = os.path.join(HERE, 'antiqua/NachtzugAntiqua-Bold.ttf')
lines = [
    (R, 34, [], 'Nachtzug nach Görlitz: zwölf Wagen, ganz still – „gute Nacht“.'),
    (R, 34, ['dlig'], 'dlig: Platz, Netz, jetzt; Achtung, Fenster, Pakt, Last, Rost'),
    (R, 34, [], 'off: Platz, Netz, jetzt; Achtung, Fenster, Pakt, Last, Rost'),
    (R, 34, ['swsh'], 'swsh: Abend, Erich, Jena, Köln, Leipzig, Rostock, Zürich, Quelle'),
    (R, 34, ['ss02'], 'ss02: Garamond g und z: zögernd zog der Zug'),
    (R, 34, ['onum'], 'onum: 1936 · 23:45 Uhr · Gleis 7 · 0123456789'),
    (R, 34, ['tnum', 'lnum'], 'tnum: 1936 · 23:45 Uhr · Gleis 7 · 0123456789'),
    (B, 34, [], 'Bold: Schlafwagen D 148, Abfahrt 22.14 Uhr – Gleis 4, ganz zügig'),
    (B, 34, ['swsh', 'dlig'], 'Bold swsh: Rostock, Köln, Augsburg; jetzt, Pakt'),
    (R, 34, [], '‘ ’ “ ” ‚ „ ‹ › « » – — … × − z. B. 10 %           '),
    (R, 34, [], ' '),
    (R, 18, [], 'Klein: Der Nachtzug fährt um 23.14 Uhr ab; im Schlafwagen ist es ganz still, nur das Rattern der Räder.'),
]
img = Image.new('RGB', (1500, 60 * len(lines) + 30), (240, 225, 186)); d = ImageDraw.Draw(img)
for i, (f, sz, feat, t) in enumerate(lines):
    font = ImageFont.truetype(f, sz, layout_engine=ImageFont.Layout.RAQM)
    d.text((20, 15 + i * 60), t, font=font, fill=(43, 38, 35), features=feat or None)
img.save(sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, 'look/text.png'))
