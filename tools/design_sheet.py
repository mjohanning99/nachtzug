#!/usr/bin/env python3
"""Draws docs/design-language.png, the one-page companion to docs/design-language.md: the principles, every colour of
the six themes (read from src/theme.cpp), the typefaces in their own files, real parts cut from the program at double
size, the radii and the brand. Run it after the look changes.

    python3 tools/design_sheet.py          (needs the program built in build/, Pillow and Firefox)
"""
import json, os, re, subprocess, tempfile
from PIL import Image
import numpy as np

R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
S = tempfile.mkdtemp(prefix='nz-sheet-')
env = dict(os.environ, QT_QPA_PLATFORM='offscreen', SDL_AUDIODRIVER='dummy', QT_SCALE_FACTOR='2')
subprocess.run([f'{R}/build/nachtzug', '--screenshot', f'{S}/main.png', '--geometry', f'{S}/main.json', '--size',
                '1560x980', '--lang', 'en', '--track', '2'], env=env, check=True, timeout=180,
               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
# ---- the themes, from the table in src/theme.cpp
src = open(f'{R}/src/theme.cpp').read()
block = src[src.index('static const Def DEFS[] = {'):]
block = block[:block.index('};')]
TOK = ['panel', 'well', 'wellHi', 'rule', 'ink', 'dim', 'canvas', 'lane', 'laneSel', 'grid', 'gridBar', 'accent',
       'accentInk', 'second', 'cap', 'capEdge', 'capInk', 'key', 'keyInk', 'hover', 'lcdBg', 'lcd']
themes = []
for m in re.finditer(r'\{QT_TRANSLATE_NOOP\("Model", "([^"]+)"\),(.*?)\{([^}]*)\}', block, re.S):
    vals = re.findall(r'0x([0-9a-fA-F]{6})', m.group(2))
    tracks = re.findall(r'0x([0-9a-fA-F]{6})', m.group(3))
    themes.append((m.group(1), dict(zip(TOK, ['#' + v.lower() for v in vals])), ['#' + t.lower() for t in tracks]))

# ---- real parts, cut from the double-size screenshots
g = json.load(open(f'{S}/main.json'))
main = Image.open(f'{S}/main.png')
def cut(img, rect, name, pad=4):
    x, y, w, h = rect
    img.crop(((x - pad) * 2, (y - pad) * 2, (x + w + pad) * 2, (y + h + pad) * 2)).save(f'{S}/p-{name}.png')
for k, n in (('transport', 'transport'), ('song', 'song'), ('output', 'output'), ('group:TRACK', 'track'),
             ('group:SOUND', 'sound'), ('group:HOW NOTES ARE PLAYED', 'notes')):
    cut(main, g[k], n)
t = g['tabs']
cut(main, (t[0], t[1], t[2], 30), 'tabs', pad=2)
cut(main, (322, 154, 1236, 36), 'toolbar', pad=2)
tl = g['timeline']
cut(main, (tl[0], tl[1], min(tl[2], 760), 150), 'timeline', pad=2)

def swatches(d):
    return ''.join(f'<div class="sw"><span style="background:{v}"></span><b>{k}</b><code>{v}</code></div>' for k, v in d.items())

theme_rows = ''
for name, d, tr in themes:
    keyset = {k: d[k] for k in ('panel', 'well', 'wellHi', 'rule', 'ink', 'dim', 'accent', 'key', 'lcdBg', 'lcd')}
    strip = ''.join(f'<i style="background:{c}"></i>' for c in tr)
    theme_rows += (f'<div class="theme"><div class="tname">{name}</div><div class="mini">' +
                   ''.join(f'<span title="{k}" style="background:{v}"></span>' for k, v in keyset.items()) +
                   f'</div><div class="tracks">{strip}</div></div>')
bak = themes[0][1]
radii = [(2, 'chord boxes, cutoff tag'), (3, 'painted key face, fader, flat clips'), (4, 'keys, fields, drop-downs, display'),
         (5, 'clips, loop cells, name plates'), (6, 'menus, lists, sound cards'), (8, 'screwed wells, top-bar units, cards'),
         (9, 'card-index tab (top)')]
radius_html = ''.join(f'<div class="rad"><div class="box" style="border-radius:{r * 2}px"></div><b>{r} px</b><span>{u}</span></div>' for r, u in radii)

html = f'''<!doctype html><meta charset="utf-8"><style>
@font-face{{font-family:G;font-weight:400;src:url({R}/site/assets/fonts/grotesk-400.woff2)}}
@font-face{{font-family:G;font-weight:500;src:url({R}/site/assets/fonts/grotesk-500.woff2)}}
@font-face{{font-family:G;font-weight:600;src:url({R}/site/assets/fonts/grotesk-600.woff2)}}
@font-face{{font-family:A;font-weight:400;src:url({R}/site/assets/fonts/antiqua-400.woff2)}}
@font-face{{font-family:A;font-weight:700;src:url({R}/site/assets/fonts/antiqua-700.woff2)}}
@font-face{{font-family:D;src:url({R}/site/assets/fonts/din-condensed-700.woff2)}}
@font-face{{font-family:DR;src:url({R}/site/assets/fonts/din-400.woff2)}}
@font-face{{font-family:C;src:url({R}/assets/fonts/CourierPrime-Regular.ttf)}}
body{{margin:0;background:{bak['panel']};color:{bak['ink']};font-family:G;width:1600px}}
.head{{background:#2b2623;color:#f3ece0;padding:30px 48px 26px;display:flex;gap:28px;align-items:center}}
.head img{{width:150px}} .head h1{{font:600 64px/1 G;margin:0;font-feature-settings:"liga","dlig"}} .head p{{margin:8px 0 0;font-size:17px;color:#d9c7ad;max-width:60em}}
.brass{{height:22px;background:url({R}/docs/manual/img/brass-band.svg) 0 0/100% 100%}}
section{{padding:26px 48px 8px}} h2{{font:700 30px A;margin:0 0 4px}} .sub{{color:{bak['dim']};margin:0 0 16px;font-size:15px}}
.cap{{font:600 12px G;letter-spacing:.1em;text-transform:uppercase;color:{bak['dim']};margin:14px 0 8px}}
.grid{{display:flex;flex-wrap:wrap;gap:10px}} .sw{{width:176px;display:flex;align-items:center;gap:8px;font-size:13px}}
.sw span{{width:34px;height:34px;border-radius:4px;border:1px solid rgba(0,0,0,.25);flex:none}} .sw b{{font-weight:600;width:66px}} .sw code{{font:12px C;color:{bak['dim']}}}
.themes{{display:grid;grid-template-columns:repeat(3,1fr);gap:14px}} .theme{{background:{bak['wellHi']};border:1px solid {bak['rule']};border-radius:8px;padding:10px 12px}}
.tname{{font-weight:600;margin-bottom:6px}} .mini{{display:flex;gap:3px}} .mini span{{flex:1;height:28px;border-radius:3px;border:1px solid rgba(0,0,0,.2)}}
.tracks{{display:flex;gap:3px;margin-top:4px}} .tracks i{{flex:1;height:10px;border-radius:2px}}
.brand{{display:flex;gap:14px;flex-wrap:wrap}} .brand .c{{display:flex;gap:10px;align-items:center;font-size:14px}} .brand .c span{{width:46px;height:46px;border-radius:6px;border:1px solid rgba(0,0,0,.25)}}
.types{{display:grid;grid-template-columns:1fr 1fr;gap:16px}} .type{{background:{bak['wellHi']};border:1px solid {bak['rule']};border-radius:8px;padding:14px 18px}}
.type .n{{font-size:13px;color:{bak['dim']};margin-top:6px}}
.parts{{display:grid;grid-template-columns:repeat(3,auto);gap:18px;align-items:start;justify-content:start}} .parts figure{{margin:0}} .parts img{{display:block;border-radius:6px}}
.parts figcaption{{font-size:13px;color:{bak['dim']};margin-top:5px}}
.radii{{display:flex;gap:22px;flex-wrap:wrap}} .rad{{width:150px;font-size:13px}} .rad .box{{width:120px;height:60px;background:{bak['field'] if 'field' in bak else '#fbf8f1'};border:2px solid {bak['rule']}}} .rad b{{display:block;margin-top:6px}} .rad span{{color:{bak['dim']}}}
.rules{{columns:2;column-gap:40px;font-size:15px;line-height:1.5}} .rules li{{margin-bottom:6px;break-inside:avoid}}
.foot{{padding:18px 48px 26px;color:{bak['dim']};font-size:13px}}
</style>
<div class="head"><img src="{R}/assets/logo/enamel-sign.png"><div><h1>Nachtzug</h1><p>Design language: a tactile instrument, a 1970s/80s synthesiser or studio desk crossed with an old German night train. Every part below is cut from the program itself; the colours are read from <code>src/theme.cpp</code>. The full text is <b>docs/design-language.md</b>.</p></div></div>
<div class="brass"></div>

<section><h2>Principles</h2><ul class="rules">
<li><b>Tactile.</b> Keys have depth, a lip below; a pressed or lit key loses its lip and sits lower.</li>
<li><b>Concentric.</b> A rounded thing inside another takes the outer radius minus the inset.</li>
<li><b>The program’s own parts.</b> Bakelite keys, shaded fields, joined keys, switches, captions, screwed panels, the display.</li>
<li><b>No pills, no round-only keys.</b> Words are soft text or engraved captions; a listen key is a key with ▶.</li>
<li><b>A symbol and a word</b>, or the symbol alone; symbols on an 18-unit grid, 1.6 line, the accent for what is added.</li>
<li><b>The name</b> in Nachtzug Grotesk with its tz ligature, never letter-spaced (spacing switches ligatures off).</li>
<li><b>British English</b> and proper typography (“ ” ‘ ’ – × …); German impersonal, never du or Sie.</li>
<li><b>Mockups first.</b> Any change of look is drawn in a few versions for the owner to choose from.</li>
</ul></section>

<section><h2>Colour</h2><p class="sub">Bakelite, the default and the reference, token by token; then all six themes (panel, well, wellHi, rule, ink, dim, accent, key, display) with their five track colours.</p>
<div class="grid">{swatches(bak)}</div>
<div class="cap">The six themes</div><div class="themes">{theme_rows}</div>
<div class="cap">Brand</div><div class="brand">
<div class="c"><span style="background:#2b2623"></span><div><b>Ink</b><br><code>#2b2623</code></div></div>
<div class="c"><span style="background:#a9441f"></span><div><b>Rust</b> (logo)<br><code>#a9441f</code></div></div>
<div class="c"><span style="background:#f4ead6"></span><div><b>Cream</b><br><code>#f4ead6</code></div></div>
<div class="c"><span style="background:linear-gradient(#f6dc95,#c9a24a 45%,#7d5f25)"></span><div><b>Brass</b><br><code>#f6dc95 → #c9a24a → #7d5f25</code></div></div>
</div></section>

<section><h2>Type</h2><div class="types">
<div class="type"><div style="font:600 46px/1 G;font-feature-settings:'liga','dlig'">Nachtzug</div><div style="font:400 20px G;margin-top:8px">Nachtzug Grotesk 400 · <span style="font-weight:500">500</span> · <span style="font-weight:600">600</span> — every label in the program</div><div class="n">Built on Jost: taller x-height, hooked z, semaphore g, railed l, the tz rail ligature (liga, dlig). The name is never letter-spaced.</div></div>
<div class="type"><div style="font:700 46px/1 A">tz · Anleitung</div><div style="font:400 20px A;margin-top:8px">Nachtzug Antiqua 400 and 700, from EB Garamond</div><div class="n">The logo’s tz, book-like headings, the manual’s text.</div></div>
<div class="type"><div style="font:400 44px/1 D;letter-spacing:.01em">GLEIS 3 · ABFAHRT 22 58</div><div style="font:400 20px DR;margin-top:8px">D-DIN and D-DIN Condensed</div><div class="n">The website’s railway signage: the departure board, signs, the timetable.</div></div>
<div class="type"><div style="font:400 40px/1 C">BAR 1 · BEAT 1</div><div style="font:400 20px C;margin-top:8px">Courier Prime</div><div class="n">Display-like text: the LCD, typed values.</div></div>
</div></section>

<section><h2>Parts</h2><p class="sub">Cut from the program at double size, Bakelite theme.</p><div class="parts">
<figure><img src="{S}/p-transport.png" style="width:470px"><figcaption>Transport: the display, keys with a lip, switches (on: lit)</figcaption></figure>
<figure><img src="{S}/p-song.png" style="width:360px"><figcaption>Song: number display, selectors, a knob, Tap</figcaption></figure>
<figure><img src="{S}/p-output.png" style="width:340px"><figcaption>Output: VU meters, volume knob, Export</figcaption></figure>
<figure><img src="{S}/p-track.png" style="width:310px"><figcaption>A screwed panel: fader, knobs, Mute and Solo</figcaption></figure>
<figure><img src="{S}/p-sound.png" style="width:310px"><figcaption>Preset list, symbol keys, the primary (dark) key</figcaption></figure>
<figure><img src="{S}/p-notes.png" style="width:310px"><figcaption>Fields, drop-downs and a switch</figcaption></figure>
</div><div class="parts" style="grid-template-columns:auto;margin-top:14px">
<figure><img src="{S}/p-toolbar.png" style="width:1240px"><figcaption>Joined keys (Song lit), the primary dark key, symbol keys with words</figcaption></figure>
<figure><img src="{S}/p-tabs.png" style="width:1240px"><figcaption>Card-index tabs: the chosen tab runs into its page</figcaption></figure>
<figure><img src="{S}/p-timeline.png" style="width:760px"><figcaption>Timeline: track headers and clips in the track colours</figcaption></figure>
</div></section>

<section><h2>Shape</h2><p class="sub">The family of radii (drawn here twice as large), from the most used: 4 for keys and fields, 6 for lists and cards, 8 for screwed wells.</p><div class="radii">{radius_html}</div></section>

<section><h2>Brand</h2><div class="parts" style="grid-template-columns:repeat(4,auto);align-items:center">
<figure><img src="{R}/assets/logo/enamel-sign.png" style="width:260px"><figcaption>The enamel sign</figcaption></figure>
<figure><img src="{R}/assets/logo.png" style="width:120px"><figcaption>App icon</figcaption></figure>
<figure><img src="{R}/site/assets/img/cover-art-1200.jpg" style="width:420px"><figcaption>Cover: Hans Baluschek, “Großstadtbahnhof”, 1904</figcaption></figure>
<figure><img src="{R}/assets/logo/social-preview.png" style="width:420px"><figcaption>Social preview, with the brass strip</figcaption></figure>
</div></section>
<div class="foot">Made by tools/design_sheet.py from the program, the theme table and the font files.</div>
'''
open(f'{S}/sheet.html', 'w').write(html)
profile = tempfile.mkdtemp(prefix='nz-ff-')
subprocess.run(['firefox', '--headless', '--no-remote', '--profile', profile, '--window-size=1600,5200', '--screenshot',
                f'{S}/sheet.png', f'file://{S}/sheet.html'], timeout=240, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
im = Image.open(f'{S}/sheet.png').convert('RGB')
a = np.asarray(im)
bg = a[-5, 800]
rows = np.where((np.abs(a.astype(int) - bg.astype(int)).sum(2) > 30).sum(1) > 3)[0]
im.crop((0, 0, 1600, rows.max() + 30)).save(f'{R}/docs/design-language.png', optimize=True)
print('docs/design-language.png written')
