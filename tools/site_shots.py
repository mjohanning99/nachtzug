#!/usr/bin/env python3
"""Takes the website's pictures from the real program: the whole window once, then each part cut out.

    QT_QPA_PLATFORM=offscreen python3 tools/site_shots.py [path/to/nachtzug]   -> site/assets/img/shots/*.png
"""
import json, os, subprocess, sys, tempfile
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'build', 'nachtzug')
OUT = os.path.join(ROOT, 'site', 'assets', 'img', 'shots')
TMP = tempfile.mkdtemp(prefix='nz-site-')
SIZE = ['--size', '1440x900']


def run(name, args):
    png, geo = os.path.join(TMP, name + '.png'), os.path.join(TMP, name + '.json')
    subprocess.run([EXE, '--screenshot', png, '--lang', 'en', '--geometry', geo] + args, check=True, timeout=120,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return Image.open(png).convert('RGB'), (json.load(open(geo)) if os.path.exists(geo) else {})


def box(*rects, pad=6):
    rects = [r for r in rects if r]
    x0 = min(r[0] for r in rects) - pad; y0 = min(r[1] for r in rects) - pad
    x1 = max(r[0] + r[2] for r in rects) + pad; y1 = max(r[1] + r[3] for r in rects) + pad
    return (max(0, x0), max(0, y0), x1, y1)


def save(img, name, crop=None, width=1100):
    if crop:   # kept inside the window: a part reaching past its edge would leave a black band
        x0, y0, x1, y1 = crop
        img = img.crop((max(0, x0), max(0, y0), min(img.width, x1), min(img.height, y1)))
    if img.width > width: img = img.resize((width, round(img.height * width / img.width)), Image.LANCZOS)
    path = os.path.join(OUT, name + '.png')
    img.save(path, optimize=True)
    print(f'{name}: {img.size[0]}×{img.size[1]}, {os.path.getsize(path) // 1024} KB')


os.makedirs(OUT, exist_ok=True)
img, g = run('notes', ['--track', '1'] + SIZE); save(img, 'notes', width=1200)
img, g = run('drums', ['--tab', '0', '--track', '0'] + SIZE); save(img, 'auto-drummer', box(g.get('drummer'), g.get('tabs')))
img, g = run('sound', ['--tab', '1', '--track', '1'] + SIZE); save(img, 'sound', box(g['tabs']))
img, g = run('simple', ['--tab', '1', '--track', '1', '--simple'] + SIZE); save(img, 'simple-sound', box(g['tabs']))
img, g = run('loops', ['--loops'] + SIZE); save(img, 'loops', box(g.get('loops') or g['timeline']))
img, g = run('arranged', ['--arrange', '1', '--track', '4'] + SIZE); save(img, 'arranged', box(g['timeline']))
img, g = run('mixer', ['--tab', '2'] + SIZE); save(img, 'mixer', box(g['tabs']))
img, g = run('lesson', ['--lesson', 'acid:3', '--track', '1'] + SIZE); save(img, 'lesson')
img, g = run('dark', ['--theme', 'Ember', '--tab', '1', '--track', '2'] + SIZE); save(img, 'dark-theme')
img, g = run('chop', ['--dialog', 'chop']); save(img, 'chop')
