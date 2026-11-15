"""Before/after board: crops of the real program (Jost) next to the same crops re-lettered in Nachtzug Grotesk
(relabel.py), the program's text styles set in both fonts by Qt, and a 4x view of 10 px text.

    python beforeafter.py      writes specimen/before-after.png, specimen/program-jost.png, specimen/program-grotesk.png
"""
import os, json, shutil
from PIL import Image, ImageDraw, ImageFont
from qtsheet import render

HERE = os.path.dirname(os.path.abspath(__file__))
SP = os.path.join(HERE, 'specimen')
SH = os.path.join(HERE, 'shots')
_REPO = os.path.normpath(os.path.join(HERE, '..', '..', '..', 'assets', 'fonts'))
J = os.environ.get('JOST_DIR', _REPO)
GROT = [os.path.join(HERE, 'out', f'NachtzugGrotesk-{s}.ttf') for s in ('Regular', 'Medium', 'SemiBold')]
JOST = [f'{J}/Jost-{w}.ttf' for w in (400, 500, 600)]
INK, DIM, RUST, AMBER, PANEL, RULE, WALNUT = '#2b2623', '#776b61', '#a5481f', '#d8642a', '#ebe5d8', '#cbbfad', '#5b3a25'

# the program's text styles: (what, px, weight, letter-spacing, colour, sample)
STYLES = [
    ('knob labels · 11 px · spaced 0.3', 11, 400, 0.3, DIM, 'Cutoff   Resonance   Env amount   Key tracking   Pulse width   Häufigkeit'),
    ('small knob labels · 8 px', 8, 400, 0.3, DIM, 'Pan   Echo   Reverb   Pump   Cutoff   Resonance   Env amount   Decay   Drive'),
    ('buttons · 13 px · 500 · spaced 0.5', 13, 500, 0.5, INK, 'Roll new sound   Mutate   Arrange for me   Loop this part   Arrangieren lassen'),
    ('lists and labels · 13 px', 13, 400, 0, INK, 'Acid Squelch   Single note   Low-pass 24 dB   Follow chords   Zeitleiste'),
    ('hints · 9 px', 9, 400, 0, DIM, 'BARS · click to move the playhead    LOOP · drag to mark what repeats    Häufigkeit'),
    ('track names · 12 px · bold', 12, 700, 0, INK, 'Drums   Bass   Sequence   Pad   Nachtzug   Zeitleiste'),
    ('note names · 12 px', 12, 400, 0, INK, 'C♯3   D♭maj   F♯m7   A♯   B♭   ▶ Play   ↶ Undo   ⧉'),
]

def label(d, x, y, text, fill=RUST, size=11, bold=False):
    f = ImageFont.truetype(GROT[1 if bold else 0], size)
    d.text((x, y), text, font=f, fill=fill)

def main():
    before = Image.open(os.path.join(SH, 'program.png')).convert('RGB')
    after = Image.open(os.path.join(SH, 'after.png')).convert('RGB')
    os.makedirs(SP, exist_ok=True)
    before.save(os.path.join(SP, 'program-jost.png')); after.save(os.path.join(SP, 'program-grotesk.png'))
    W = 1420
    left = (30, 170, 335, 740)
    tool = (345, 176, 1069, 266)
    synth = (796, 560, 1520, 810)
    rows_h = 40 + 52 * len(STYLES) + 30
    right_h = 2 * (tool[3] - tool[1] + 24) + 2 * (synth[3] - synth[1] + 24)
    H = 70 + max(left[3] - left[1], right_h) + 40 + rows_h + 260
    img = Image.new('RGB', (W, H), PANEL)
    d = ImageDraw.Draw(img)
    d.rectangle((0, 0, 11, H), fill=WALNUT); d.rectangle((W - 12, 0, W, H), fill=WALNUT)
    label(d, 30, 14, 'Nachtzug, vorher und nachher', INK, 26, True)
    label(d, 470, 24, 'links: heute mit Jost · rechts bzw. darunter: dieselbe Aufnahme, jede Jost-Beschriftung in Nachtzug Grotesk neu gesetzt (Qt, gleiche Größe, Stärke, Sperrung, Farbe)', DIM, 12)
    d.rectangle((30, 52, W - 30, 53), fill=AMBER)
    y0 = 78
    x = 30
    lw = left[2] - left[0]
    for tag, im in (('Jost', before), ('Nachtzug Grotesk', after)):
        label(d, x, y0 - 18, tag, RUST, 11, True)
        img.paste(im.crop(left), (x, y0))
        d.rectangle((x - 1, y0 - 1, x + lw, y0 + left[3] - left[1]), outline=RULE)
        x += lw + 12
    x += 14
    y = y0
    for box in (tool, synth):
        for tag, im in (('Jost', before), ('Nachtzug Grotesk', after)):
            label(d, x, y - 18, tag, RUST, 11, True)
            img.paste(im.crop(box), (x, y))
            d.rectangle((x - 1, y - 1, x + box[2] - box[0], y + box[3] - box[1]), outline=RULE)
            y += box[3] - box[1] + 24
    # the program's styles set in both fonts
    y = max(y0 + (left[3] - left[1]) + 30, y + 6)
    label(d, 30, y, 'DIE TEXTSTILE DES PROGRAMMS · OBEN JOST, DARUNTER NACHTZUG GROTESK', RUST, 10, True)
    y += 22
    items = []
    yy = 18
    for what, px, wt, sp, col, text in STYLES:
        items.append(('text', 4, yy, 'Nachtzug Grotesk', 400, 10, 0, RUST, what))
        items.append(('text', 250, yy, 'Jost', wt, px, sp, col, text))
        items.append(('text', 250, yy + 22, 'Nachtzug Grotesk', wt, px, sp, col, text))
        yy += 52
    sample = render(GROT + JOST, items, (W - 60, yy), bg=PANEL)
    img.paste(sample, (30, y))
    y += yy + 10
    # 4x view of 10 px text
    label(d, 30, y, '10 PX, VIERFACH VERGRÖSSERT · LINKS WIE IM PROGRAMM (QT, FREETYPE-AUTOHINTER) · RECHTS MIT DEN EINGEBAUTEN TTFAUTOHINT-HINTS (FREETYPE)', RUST, 10, True)
    y += 20
    text = 'Cutoff Env amount Zeitleiste Il1'
    tw, th = 170, 18
    def qt(fonts, fam, wt=400):
        return render(fonts, [('text', 2, 13, fam, wt, 10, 0, INK, text)], (tw, th))
    def ft(path):
        im = Image.new('RGB', (tw, th), PANEL)
        ImageDraw.Draw(im).text((2, 13), text, font=ImageFont.truetype(path, 10), fill=INK, anchor='ls')
        return im
    cells = [[('Jost', qt(JOST, 'Jost')), ('Grotesk', qt(GROT, 'Nachtzug Grotesk'))],
             [('Jost (ohne Hints)', ft(JOST[0])), ('Grotesk', ft(GROT[0]))]]
    for col, pair in enumerate(cells):
        cx = 30 + col * (tw * 4 + 30)
        cy = y
        for tag, im in pair:
            img.paste(im.resize((tw * 4, th * 4), Image.NEAREST), (cx, cy + 14))
            label(d, cx, cy, tag, DIM, 10)
            cy += th * 4 + 18
    img = img.crop((0, 0, W, y + 2 * (th * 4 + 18) + 10))
    img.save(os.path.join(SP, 'before-after.png'))
    print(os.path.join(SP, 'before-after.png'), img.size)

if __name__ == '__main__':
    main()
