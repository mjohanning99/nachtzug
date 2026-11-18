#!/usr/bin/env python3
"""Takes the pictures for the manual from the real program, at double resolution.

    python3 docs/manual/shots.py [en|de] [path/to/nachtzug]

The program reports where each part of its window is (--geometry), so the pictures stay right when the layout
changes. English pictures go to img/, German ones to img-de/.
"""
import json, os, struct, math, subprocess, sys, tempfile
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
LANG = sys.argv[1] if len(sys.argv) > 1 else 'en'
EXE = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, '..', '..', 'build', 'nachtzug')
OUT = os.path.join(HERE, 'img' if LANG == 'en' else 'img-de')
TMP = tempfile.mkdtemp(prefix='nz-shots-')
S = 2   # the pictures are taken at this scale

def run(args, name, extra_env=None):
    png, geo = os.path.join(TMP, name + '.png'), os.path.join(TMP, name + '.json')
    env = dict(os.environ, QT_SCALE_FACTOR=str(S), **(extra_env or {}))
    subprocess.run([EXE, '--screenshot', png, '--lang', LANG, '--geometry', geo] + args, env=env, check=True, timeout=120)
    g = json.load(open(geo)) if os.path.exists(geo) else {}
    return Image.open(png).convert('RGB'), g

def box(*rects, pad=4):
    x0 = min(r[0] for r in rects); y0 = min(r[1] for r in rects)
    x1 = max(r[0] + r[2] for r in rects); y1 = max(r[1] + r[3] for r in rects)
    return (x0 - pad, y0 - pad, x1 + pad, y1 + pad)

def cut(img, b, name):
    w, h = img.size
    b = (max(0, b[0] * S), max(0, b[1] * S), min(w, b[2] * S), min(h, b[3] * S))
    img.crop(b).save(os.path.join(OUT, name + '.png'))
    print(name, img.crop(b).size)

def save(img, name):
    img.save(os.path.join(OUT, name + '.png')); print(name, img.size)

def group(g, prefix):
    return [v for k, v in g.items() if k.startswith('group:' + prefix)]

def seed_samples():
    """A few sounds in the test program's sample library, so its pages are not empty in the pictures."""
    base = os.path.expanduser('~/.local/share/Nachtzug/Nachtzug-test/samples')
    os.makedirs(base, exist_ok=True)
    def wav(name, secs, fn):
        n = int(48000 * secs)
        data = b''.join(struct.pack('<h', int(max(-1, min(1, fn(i / 48000))) * 30000)) for i in range(n))
        with open(os.path.join(base, name), 'wb') as f:
            f.write(b'RIFF' + struct.pack('<I', 36 + len(data)) + b'WAVEfmt ' + struct.pack('<IHHIIHH', 16, 1, 1, 48000, 96000, 2, 16) + b'data' + struct.pack('<I', len(data)) + data)
    for old in ['Kick from the cellar.wav', 'Train door.wav', 'Snare on tape.wav']:   # names from before the library had them
        if os.path.exists(os.path.join(base, old)): os.remove(os.path.join(base, old))
    wav('Kitchen pot.wav', 0.6, lambda t: (math.sin(2 * math.pi * 612 * t) + 0.6 * math.sin(2 * math.pi * 1489 * t) + 0.3 * math.sin(2 * math.pi * 2731 * t)) * 0.4 * math.exp(-t * 7))
    wav('Hey.wav', 0.3, lambda t: math.sin(2 * math.pi * 220 * t + 3 * math.sin(2 * math.pi * 3 * t)) * math.exp(-t * 8))
    wav('Cellar door.wav', 0.9, lambda t: math.sin(2 * math.pi * (60 + 30 * math.exp(-t * 20)) * t) * math.exp(-t * 5) + 0.2 * math.sin(t * 7000.0 * t) * math.exp(-t * 25))

os.makedirs(OUT, exist_ok=True)
seed_samples()

# whole windows
TALL = ['--size', '1560x1240']   # tall enough that the on-screen keys show under the note grid
img, g = run(['--track', '1'] + TALL, 'notestall')
cut(img, box(g['tabs'], pad=0), 'notes')
cut(img, box(g['pads']), 'pads')
img, g = run(['--track', '1'], 'notes'); save(img, 'full-notes')
cut(img, box(g['topbar'], pad=0), 'topbar')
cut(img, box(g['transport']), 'transport')
cut(img, box(g['song']), 'song-unit')
cut(img, box(g['output']), 'output')

img, g = run(['--tab', '1', '--track', '1'], 'sound'); save(img, 'full-sound')
# the Sound tab wraps its sections, so they are found by name, not by row
def section(prefix):
    return [v for k, v in g.items() if k.startswith('group:') and k[6:].startswith(prefix)][0]
def side_by_side(rects, name, pad=4, gap=10):
    parts = [img.crop(((r[0] - pad) * S, (r[1] - pad) * S, (r[0] + r[2] + pad) * S, (r[1] + r[3] + pad) * S)) for r in rects]
    out = Image.new('RGB', (sum(p.width for p in parts) + gap * S * (len(parts) - 1), max(p.height for p in parts)), img.getpixel((4, img.height // 2)))
    x = 0
    for p in parts:
        out.paste(p, (x, 0)); x += p.width + gap * S
    out.save(os.path.join(OUT, name + '.png')); print(name, out.size)
cut(img, box(section('1 ')), 'oscillators')
cut(img, box(section('2 ')), 'filter')
fenv = [v for k, v in g.items() if k.startswith('group:') and not k[6:7].isdigit() and ('ENV' in k or 'HÜLLKURVE' in k)][0]
side_by_side([fenv, section('3 ')], 'envelopes')

img, g = run(['--tab', '1', '--track', '1', '--simple'], 'simple'); cut(img, box(g['tabs'], pad=0), 'simple-sound')
img, g = run(['--tab', '1', '--track', '0', '--simple', '--kit', 'Night train kit'], 'simpledrums'); cut(img, box(g['tabs'], pad=0), 'simple-drums')
img, g = run(['--tab', '1', '--track', '0', '--kit', 'Night train kit'], 'drumsound'); cut(img, box(g['tabs'], pad=0), 'drum-sound')
img, g = run(['--tab', '0', '--track', '0'] + TALL, 'drums'); cut(img, box(g['drummer']), 'drummer')
img, g = run(['--tab', '2'], 'mixer'); cut(img, box(g['tabs'], pad=0), 'mixer')
img, g = run(['--lines', '--track', '1'], 'lines')
t = g['timeline']; cut(img, (t[0], t[1] - 40, t[0] + t[2], t[1] + t[3]), 'lines')
img, g = run(['--loops'], 'loops')
l = g['loops']; cut(img, (l[0] - 4, l[1] - 40, l[0] + l[2] + 4, l[1] + l[3] + 4), 'loops-grid')
img, g = run(['--arrange', '1', '--track', '4'], 'arranged')
t = g['timeline']; cut(img, (t[0], t[1] - 40, t[0] + t[2], t[1] + t[3]), 'arranged')
img, g = run(['--theme', 'Ember', '--tab', '1', '--track', '2'], 'dark'); save(img, 'full-dark')

# round 3: notes with a chance, repeats and kept notes; the keys in Chords mode; a lesson step
example = os.path.join(HERE, 'examples', 'note-settings.nachtzug')
img, g = run(['--track', '0', example], 'chance')
t = g['tabs']; cut(img, (t[0], t[1] + 40, t[0] + t[2], t[1] + t[3] - 70), 'notes-chance')
img, g = run(['--track', '2', '--chord-keys'] + TALL, 'chordpads')
p = g['pads']; cut(img, (p[0] - 4, p[1] - 36, p[0] + p[2] + 4, p[1] + p[3] + 4), 'chord-pads')
img, g = run(['--lesson', 'acid:3', '--track', '1'], 'lesson')
t = g['tabs']; cut(img, (0, t[1], 780, t[1] + 300), 'lesson')

# windows on their own
for d in ['welcome', 'start', 'export', 'arrange', 'journey', 'soundcard', 'samples', 'settings', 'chop']:
    img, _ = run(['--dialog', d], d); save(img, 'dlg-' + d)
img, _ = run(['--guide'], 'guide'); save(img, 'guide')

# the sound library, with four sounds of one's own in a data folder of its own (pictures run as Nachtzug-test)
xdg = os.path.join(TMP, 'xdg')
for name, group, words, star, cut_ in (('Night Shift Bass', 'bass', ['dark', 'squelchy'], True, .3),
                                        ('Glass Arp', 'sequence', ['bright'], True, .7),
                                        ('Tangerine Pad', 'pad', ['soft', 'warm'], False, .5),
                                        ('Signal Box Pluck', 'sequence', [], False, .4)):
    d = os.path.join(xdg, 'Nachtzug', 'Nachtzug-test', 'sounds', group)
    os.makedirs(d, exist_ok=True)
    json.dump({'format': 1, 'kind': 'nachtzug-sound', 'name': name, 'group': group, 'words': words, 'starred': star,
               'patch': {'cutoff': cut_, 'res': .6}}, open(os.path.join(d, name + '.nzsound'), 'w'))
img, _ = run(['--dialog', 'library'], 'library', {'XDG_DATA_HOME': xdg}); save(img, 'library')

# the sample menu under a drum, with the Train submenu open (pictured by the window test). It is taken at normal
# size: the test screen is too small for the whole menu at double size, so the picture is enlarged afterwards.
shot = os.path.join(TMP, 'menu')
subprocess.run([EXE, '--guitest', '--lang', LANG], env=dict(os.environ, NZ_GUISHOT=shot), timeout=240, capture_output=True)
if os.path.exists(shot + '.menu.png') and os.path.exists(shot + '.sub.png'):
    a, b = Image.open(shot + '.menu.png').convert('RGB'), Image.open(shot + '.sub.png').convert('RGB')
    # the submenu opens beside the Train entry, as on screen
    top = 0
    px = a.load()
    rows = [y for y in range(a.size[1]) if any(px[x, y][0] < 120 for x in range(10, 60))]   # lines with dark text
    over = max(0, int(a.size[1] * 0.40))
    both = Image.new('RGB', (a.size[0] + b.size[0] - 2, max(a.size[1], over + b.size[1])), (255, 255, 255))
    both.paste(a, (0, 0)); both.paste(b, (a.size[0] - 2, over))
    both = both.resize((both.size[0] * S, both.size[1] * S), Image.LANCZOS)
    save(both, 'sample-menu')
