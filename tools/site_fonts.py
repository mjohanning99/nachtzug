#!/usr/bin/env python3
"""Makes the website's fonts: the program's fonts cut down to the characters a web page needs, as WOFF2.

    python3 tools/site_fonts.py      -> site/assets/fonts/*.woff2

Run it again when a font changes. Needs fontTools with brotli.
"""
import os
from fontTools import subset
from fontTools.ttLib import TTFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'site', 'assets', 'fonts')
F = os.path.join(ROOT, 'assets', 'fonts')
M = os.path.join(ROOT, 'docs', 'manual', 'fonts')
TEXT = ''.join(chr(c) for c in range(0x20, 0x7f)) + 'ÄÖÜäöüßéèàçñ' + '‘’‚“”„‹›«»–—…×−·•′″°§©®™€ ' + '♯♭♮←→↑↓↗↘▲▼▸▾■●▶' + '  '
FONTS = [
    (f'{F}/Jost-400.ttf', 'jost-400'), (f'{F}/Jost-500.ttf', 'jost-500'), (f'{F}/Jost-600.ttf', 'jost-600'),
    (f'{F}/CourierPrime-Regular.ttf', 'courier-400'), (f'{F}/CourierPrime-Bold.ttf', 'courier-700'),
    (f'{F}/nachtzug/NachtzugAntiqua-Regular.ttf', 'antiqua-400'), (f'{F}/nachtzug/NachtzugAntiqua-Bold.ttf', 'antiqua-700'),
    (f'{M}/EBGaramond-Italic-440.ttf', 'antiqua-italic'),
]
WHOLE = [  # every glyph: ornaments and drop caps
    (f'{F}/nachtzug/NachtzugOrnamente-Regular.otf', 'ornamente'),
    (f'{F}/nachtzug/NachtzugInitialenRahmen-Regular.ttf', 'initialen-rahmen'),
    (f'{F}/nachtzug/NachtzugInitialenBuchstabe-Regular.ttf', 'initialen-buchstabe'),
]

def run(src, name, text=None):
    opts = subset.Options(); opts.flavor = 'woff2'; opts.layout_features = ['*']; opts.name_IDs = ['*']; opts.notdef_outline = True
    font = TTFont(src)
    s = subset.Subsetter(opts)
    if text: s.populate(text=text)
    else: s.populate(gids=list(range(len(font.getGlyphOrder()))))
    s.subset(font)
    path = os.path.join(OUT, name + '.woff2')
    font.flavor = 'woff2'; font.save(path)
    print(f'{name}.woff2  {os.path.getsize(path) // 1024} KB')

os.makedirs(OUT, exist_ok=True)
for src, name in FONTS: run(src, name, TEXT)
for src, name in WHOLE: run(src, name)
