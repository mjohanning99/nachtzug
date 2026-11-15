"""The specimen board of Nachtzug Grotesk: specimen/specimen.png (rendered by Qt, as the program would) and
specimen/index.html (the same board for a browser, fonts next to it)."""
import os, shutil, html
from qtsheet import render

HERE = os.path.dirname(os.path.abspath(__file__))
SP = os.path.join(HERE, 'specimen')
FD = os.path.join(SP, 'fonts')
_REPO = os.path.normpath(os.path.join(HERE, '..', '..', '..', 'assets', 'fonts'))
J = os.environ.get('JOST_DIR', _REPO)
GROT = [os.path.join(HERE, 'out', f'NachtzugGrotesk-{s}.ttf') for s in ('Regular', 'Medium', 'SemiBold')]
JOST = [f'{J}/Jost-{w}.ttf' for w in (400, 500, 600)]
G, JF = 'Nachtzug Grotesk', 'Jost'
INK, DIM, RUST, AMBER, PANEL, IVORY, RULE, WALNUT = '#2b2623', '#776b61', '#a5481f', '#d8642a', '#ebe5d8', '#f6f1e4', '#cbbfad', '#5b3a25'
W, H = 1280, 860
NNBSP = ' '

UC = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ ÄÖÜẞ'
LC = 'abcdefghijklmnopqrstuvwxyz äöüß'
SIG = 'z g tz Il1'
WORDS = 'Cutoff · Env amount · Roll new sound · Pump · Follow chords · Zeitleiste · Häufigkeit · Arrangieren lassen · Nachtzug'
SIZES = [9, 10, 11, 13, 15]
MUSIC = 'C♯3 D♭maj F♯m7 A♯ B♭'
PUNCT = ['‘ ’ “ ” ‚ „ ‹ › « »', '– — … × − · • & § @', f'„Zeitleiste“ – 3 × 4 … ‹gut› 120{NNBSP}BPM']
SYMS = '▶ ■ ● ○ ▾ ▸ ▴ ◂ ↶ ↷ ⧉ ♪ ♫ ✓ ✕ ⚙ ⌘ ← → ↑ ↓ ↔ ♯ ♭ ♮'
INLINE = '▶ Play   ■ Stop   ● Rec   ↶ Undo   ↷ Redo   ⧉ Copy   ▾ More   ▸ Open   ♪ Notes   → Next'
NUMS = ['1111', '4080', '1717', '9898']
FOOT = ('Nachtzug Grotesk 0.1 · Regular 400 · Medium 500 · SemiBold 600 · derived from Jost by Owen Earl '
        '(indestructible type*) · SIL Open Font License 1.1')

def heading(items, x, y, text):
    items.append(('text', x, y, G, 500, 10, 2.5, RUST, text.upper()))

def screw(items, x, y):
    items.append(('ellipse', x, y, 4, 4, '#b7ad9d'))
    items.append(('rect', x - 3, y, 6, 1, '#6f665b'))

def panel(items, x, y, w, h, fill=PANEL):
    items.append(('rrect', x, y, w, h, 7, fill, RULE, 1))
    for sx, sy in ((x + 8, y + 8), (x + w - 8, y + 8), (x + 8, y + h - 8), (x + w - 8, y + h - 8)):
        screw(items, sx, sy)

def board():
    it = []
    it += [('rect', 0, 0, 16, H, WALNUT), ('rect', W - 16, 0, 16, H, WALNUT), ('rect', 15, 0, 1, H, '#3a2416'),
           ('rect', W - 16, 0, 1, H, '#3a2416')]
    it.append(('text', 40, 66, G, 600, 46, 0, INK, 'Nachtzug Grotesk'))
    it.append(('text', 470, 66, G, 400, 15, 0, DIM, 'die Schrift der Bedienelemente · nach Jost, mit hakigem z, Signal-g und Schienen-tz'))
    it.append(('rect', 40, 84, W - 80, 2, AMBER))
    # alphabet
    panel(it, 30, 100, 640, 228)
    heading(it, 48, 124, 'Alphabet')
    y = 156
    for wt in (400, 500, 600):
        it.append(('text', 48, y, G, wt, 22, 0, INK, UC))
        it.append(('text', 48, y + 28, G, wt, 22, 0, INK, LC))
        it.append(('text', 610, y + 28, G, 400, 10, 0, DIM, str(wt)))
        y += 64
    # signature letters against Jost, on the ivory glass
    panel(it, 684, 100, 566, 228, IVORY)
    heading(it, 702, 124, 'Eigenheiten · neben Jost')
    it.append(('text', 702, 150, G, 400, 10, 0, DIM, 'Jost'))
    it.append(('text', 702, 210, JF, 400, 60, 0, DIM, SIG))
    it.append(('text', 702, 236, G, 400, 10, 0, RUST, 'Nachtzug Grotesk'))
    it.append(('text', 702, 296, G, 400, 60, 0, INK, SIG))
    it.append(('text', 1030, 210, JF, 500, 30, 0, DIM, 'Nachtzug'))
    it.append(('text', 1030, 296, G, 500, 30, 0, INK, 'Nachtzug'))
    # interface sizes
    panel(it, 30, 340, 1220, 252)
    heading(it, 48, 364, 'In Bediengrößen · 400')
    y = 386
    for px in SIZES:
        it.append(('text', 48, y, G, 400, 9, 0, RUST, f'{px} px'))
        it.append(('text', 92, y, G, 400, px, 0, INK, WORDS))
        y += px + 12
    # a strip of controls: buttons (13 px, 500, spaced 0.5) and knob labels (11 px, spaced 0.3)
    y0 = 500
    x = 48
    for label, primary in (('Roll new sound', True), ('Arrangieren lassen', False), ('Follow chords', False), ('Mutate', False)):
        wbtn = 22 + len(label) * 7.4
        if primary:
            it.append(('rrect', x, y0, wbtn, 26, 4, '#3a3430', '#000000', 1))
            it.append(('ctext', x + wbtn / 2, y0 + 17, G, 500, 13, 0.5, '#f4eee2', label))
        else:
            it.append(('rrect', x, y0, wbtn, 26, 4, '#efe9dd', RULE, 1))
            it.append(('rect', x + 2, y0 + 24, wbtn - 4, 2, '#b3a794'))
            it.append(('ctext', x + wbtn / 2, y0 + 17, G, 500, 13, 0.5, INK, label))
        x += wbtn + 10
    kx = 48
    for label in ('Cutoff', 'Resonance', 'Env amount', 'Häufigkeit', 'Pump', 'Zeitleiste'):
        cx = kx + 34
        it.append(('ellipse', cx, y0 + 52, 11, 11, '#2a2522'))
        it.append(('rect', int(cx), y0 + 43, 2, 6, AMBER))
        it.append(('ctext', cx, y0 + 80, G, 400, 11, 0.3, DIM, label))
        kx += 76
    it.append(('text', 560, y0 + 82, G, 400, 9, 0, DIM, 'BARS · click to move the playhead      Häufigkeit · 9 px hint text'))
    # music
    mx = 900
    it.append(('text', mx, 500, G, 500, 10, 2.5, RUST, 'NOTENNAMEN'))
    yy = 520
    for px in (11, 12, 13, 14):
        it.append(('text', mx, yy, G, 400, 9, 0, RUST, f'{px}'))
        it.append(('text', mx + 22, yy, G, 400, px, 0, INK, MUSIC))
        it.append(('text', mx + 175, yy, G, 600, px, 0, INK, MUSIC))
        yy += px + 5
    # punctuation, figures, symbols
    panel(it, 30, 604, 420, 214)
    heading(it, 48, 628, 'Satzzeichen')
    y = 662
    for line in PUNCT:
        it.append(('text', 48, y, G, 400, 22, 0, INK, line))
        y += 36
    it.append(('text', 48, y - 6, G, 400, 11, 0, DIM, f'schmales Leerzeichen U+202F: z.{NNBSP}B. · 120{NNBSP}BPM · 4{NNBSP}×{NNBSP}4'))
    panel(it, 462, 604, 330, 214)
    heading(it, 480, 628, 'Ziffern')
    it.append(('text', 480, 664, G, 400, 26, 0, INK, '0123456789'))
    it.append(('text', 480, 690, G, 400, 10, 0, DIM, 'proportional'))
    it.append(('text', 640, 690, G, 400, 10, 0, DIM, 'tnum (tabellarisch)'))
    y = 712
    for n in NUMS:
        it.append(('text', 480, y, G, 400, 15, 0, INK, n))
        it.append(('feat', 'tnum', 1))
        it.append(('text', 640, y, G, 400, 15, 0, INK, n))
        it.append(('nofeat',))
        y += 22
    panel(it, 804, 604, 446, 214)
    heading(it, 822, 628, 'Zeichen der Oberfläche')
    it.append(('text', 822, 664, G, 400, 22, 0, INK, '▶ ■ ● ○ ▾ ▸ ▴ ◂ ↶ ↷ ⧉ ♪ ♫ ✓ ✕ ⚙'))
    it.append(('text', 822, 698, G, 400, 22, 0, INK, '⌘ ← → ↑ ↓ ↔ ♯ ♭ ♮ · •'))
    it.append(('text', 822, 734, G, 500, 13, 0.5, INK, '▶ Play   ■ Stop   ● Rec   ↶ Undo   ↷ Redo'))
    it.append(('text', 822, 756, G, 500, 13, 0.5, INK, '⧉ Copy   ▾ More   ▸ Open   ♪ Notes   → Next'))
    it.append(('text', 822, 782, G, 600, 22, 0, INK, '▶ ■ ● ↶ ↷ ⧉ ♪ ✓ ⚙ ⌘ →'))
    it.append(('text', 40, 846, G, 400, 10, 0, DIM, FOOT))
    return it

def png():
    os.makedirs(SP, exist_ok=True)
    img = render(GROT + JOST, board(), (W, H), bg=PANEL)
    img.save(os.path.join(SP, 'specimen.png'))

def page():
    os.makedirs(FD, exist_ok=True)
    for f in GROT: shutil.copy(f, FD)
    for f in JOST: shutil.copy(f, FD)
    shutil.copy(os.path.join(HERE, 'OFL.txt'), FD)
    shutil.copy(f'{J}/OFL-Jost.txt', FD)
    e = html.escape
    face = ''.join(f"@font-face{{font-family:'{fam}';src:url('fonts/{os.path.basename(p)}') format('truetype');font-weight:{w};}}\n"
                   for fam, ps in ((G, GROT), (JF, JOST)) for p, w in zip(ps, (400, 500, 600)))
    rows = ''.join(f'<div class="sz"><span class="lab">{px} px</span><span style="font-size:{px}px">{e(WORDS)}</span></div>' for px in SIZES)
    alpha = ''.join(f'<div class="al" style="font-weight:{w}"><div>{e(UC)}</div><div>{e(LC)}</div><span class="wl">{w}</span></div>' for w in (400, 500, 600))
    music = ''.join(f'<div class="mu"><span class="lab">{px}</span><span style="font-size:{px}px">{e(MUSIC)}</span>'
                    f'<span style="font-size:{px}px;font-weight:600">{e(MUSIC)}</span></div>' for px in (11, 12, 13, 14))
    nums = ''.join(f'<div><span>{n}</span><span class="tn">{n}</span></div>' for n in NUMS)
    btns = ''.join(f'<button class="{c}">{e(t)}</button>' for t, c in (('Roll new sound', 'pri'), ('Arrangieren lassen', ''), ('Follow chords', ''), ('Mutate', '')))
    knobs = ''.join(f'<div class="knob"><i></i><span>{e(t)}</span></div>' for t in ('Cutoff', 'Resonance', 'Env amount', 'Häufigkeit', 'Pump', 'Zeitleiste'))
    doc = f"""<!doctype html>
<html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Nachtzug Grotesk</title>
<style>
{face}
:root{{--ink:{INK};--dim:{DIM};--rust:{RUST};--amber:{AMBER};--panel:{PANEL};--ivory:{IVORY};--rule:{RULE};--walnut:{WALNUT};}}
*{{box-sizing:border-box}}
body{{margin:0;background:var(--walnut);color:var(--ink);font-family:'{G}',sans-serif;-webkit-font-smoothing:antialiased}}
main{{max-width:1248px;margin:0 auto;background:var(--panel);padding:18px 24px 14px;border-left:1px solid #3a2416;border-right:1px solid #3a2416}}
header{{display:flex;flex-wrap:wrap;align-items:baseline;gap:6px 24px;border-bottom:2px solid var(--amber);padding-bottom:10px;margin-bottom:14px}}
h1{{font-weight:600;font-size:46px;margin:0;line-height:1.1}}
header p{{margin:0;font-size:15px;color:var(--dim)}}
h2{{font-weight:500;font-size:10px;letter-spacing:2.5px;text-transform:uppercase;color:var(--rust);margin:0 0 10px}}
.grid{{display:grid;grid-template-columns:repeat(12,1fr);gap:12px}}
.p{{position:relative;background:var(--panel);border:1px solid var(--rule);border-radius:7px;padding:16px 18px}}
.p::before,.p::after{{content:'';position:absolute;top:5px;width:7px;height:7px;border-radius:50%;background:#b7ad9d;box-shadow:inset 0 3px 0 -2px #6f665b}}
.p::before{{left:5px}}.p::after{{right:5px}}
.glass{{background:var(--ivory)}}
.c7{{grid-column:span 7}}.c5{{grid-column:span 5}}.c12{{grid-column:span 12}}.c4{{grid-column:span 4}}.c3{{grid-column:span 3}}.c5b{{grid-column:span 5}}
.al{{position:relative;font-size:22px;line-height:28px;margin-bottom:8px}}.wl{{position:absolute;right:0;bottom:4px;font-size:10px;color:var(--dim);font-weight:400}}
.sig .tag{{font-size:10px;color:var(--dim)}}.sig .big{{font-size:60px;line-height:1.15;white-space:nowrap}}.sig .j{{font-family:'{JF}';color:var(--dim)}}
.sig .row{{display:flex;gap:30px;align-items:baseline;flex-wrap:wrap}}.sig .mid{{font-size:30px;font-weight:500}}
.sz{{white-space:nowrap;overflow:hidden;line-height:1.25;margin:3px 0}}.lab{{display:inline-block;width:44px;font-size:9px;color:var(--rust)}}
.ctl{{display:flex;flex-wrap:wrap;gap:24px;margin-top:14px;align-items:flex-start}}
button{{font:500 13px/24px '{G}';letter-spacing:.5px;color:var(--ink);background:linear-gradient(#f3eee4,#e2dacb);border:1px solid var(--rule);border-bottom:3px solid #a99c88;border-radius:4px;padding:0 12px;margin-right:8px}}
button.pri{{background:linear-gradient(#4a433e,#2b2623);color:#f4eee2;border-color:#000;border-bottom-color:#120f0d}}
.knobs{{display:flex;gap:10px}}.knob{{width:70px;text-align:center}}.knob i{{display:block;margin:0 auto 6px;width:22px;height:22px;border-radius:50%;background:#2a2522;border-top:3px solid var(--amber)}}
.knob span{{font-size:11px;letter-spacing:.3px;color:var(--dim)}}
.mu{{white-space:nowrap;display:flex;gap:14px;align-items:baseline;margin:2px 0}}.mu .lab{{width:18px}}
.big22{{font-size:22px;line-height:36px}}.small{{font-size:11px;color:var(--dim)}}
.nums{{display:grid;grid-template-columns:1fr;font-size:15px;line-height:22px}}.nums div{{display:flex;gap:40px}}.tn{{font-feature-settings:'tnum' 1}}
.inl{{font-weight:500;font-size:13px;letter-spacing:.5px;line-height:22px}}
footer{{font-size:10px;color:var(--dim);margin-top:12px}}
@media (max-width:900px){{.c7,.c5,.c4,.c3,.c5b{{grid-column:span 12}}h1{{font-size:34px}}.sig .big{{font-size:44px}}}}
</style></head><body><main>
<header><h1>Nachtzug Grotesk</h1><p>die Schrift der Bedienelemente · nach Jost, mit hakigem z, Signal-g und Schienen-tz</p></header>
<div class="grid">
<section class="p c7"><h2>Alphabet</h2>{alpha}</section>
<section class="p glass c5 sig"><h2>Eigenheiten · neben Jost</h2>
<div class="tag">Jost</div><div class="row"><span class="big j">{e(SIG)}</span><span class="mid j">Nachtzug</span></div>
<div class="tag" style="color:var(--rust)">Nachtzug Grotesk</div><div class="row"><span class="big">{e(SIG)}</span><span class="mid">Nachtzug</span></div></section>
<section class="p c12"><h2>In Bediengrößen · 400</h2>{rows}
<div class="ctl"><div>{btns}</div><div class="knobs">{knobs}</div>
<div><h2>Notennamen</h2>{music}</div></div>
<div class="small" style="font-size:9px;margin-top:8px;word-spacing:0">BARS · click to move the playhead<span style="margin-left:3em">Häufigkeit · 9 px hint text</span></div></section>
<section class="p c4"><h2>Satzzeichen</h2>{''.join(f'<div class="big22">{e(l)}</div>' for l in PUNCT)}
<div class="small">schmales Leerzeichen U+202F: z.{NNBSP}B. · 120{NNBSP}BPM · 4{NNBSP}×{NNBSP}4</div></section>
<section class="p c3"><h2>Ziffern</h2><div style="font-size:26px">0123456789</div>
<div class="small" style="display:flex;gap:40px;margin:6px 0"><span>proportional</span><span>tnum</span></div><div class="nums">{nums}</div></section>
<section class="p c5b"><h2>Zeichen der Oberfläche</h2><div class="big22">{e(SYMS)}</div>
<div class="inl">{e(INLINE)}</div><div class="big22" style="font-weight:600">▶ ■ ● ↶ ↷ ⧉ ♪ ✓ ⚙ ⌘ →</div></section>
</div>
<footer>{e(FOOT)}</footer>
</main></body></html>
"""
    with open(os.path.join(SP, 'index.html'), 'w', encoding='utf-8') as f:
        f.write(doc)

if __name__ == '__main__':
    png(); page()
    print(os.path.join(SP, 'specimen.png'))
