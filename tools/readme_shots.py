#!/usr/bin/env python3
"""Takes the screenshots for the README from the real program.

    QT_QPA_PLATFORM=offscreen python3 tools/readme_shots.py [path/to/nachtzug]

Whole windows are 1560 × 980; a few pictures are cut to the part they show.
"""
import json, os, subprocess, sys, tempfile
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'build', 'nachtzug')
OUT = os.path.join(ROOT, 'docs', 'screenshots')
TMP = tempfile.mkdtemp(prefix='nz-readme-')
SIZE = ['--size', '1560x980']


def run(name, args):
    png, geo = os.path.join(TMP, name + '.png'), os.path.join(TMP, name + '.json')
    subprocess.run([EXE, '--screenshot', png, '--lang', 'en', '--geometry', geo] + args, check=True, timeout=120)
    g = json.load(open(geo)) if os.path.exists(geo) else {}
    return Image.open(png).convert('RGB'), g


def save(img, name, box=None):
    if box:
        img = img.crop(box)
    img.save(os.path.join(OUT, name + '.png'))
    print(name, img.size)


img, g = run('notes', ['--track', '1'] + SIZE); save(img, 'notes')
img, g = run('sound', ['--tab', '1', '--track', '1'] + SIZE); save(img, 'sound')
img, g = run('drums', ['--tab', '0', '--track', '0'] + SIZE); save(img, 'auto-drummer')
img, g = run('loops', ['--loops'] + SIZE); save(img, 'loops')
img, g = run('mixer', ['--tab', '2'] + SIZE); save(img, 'mixer')
img, g = run('dark', ['--theme', 'Ember', '--tab', '1', '--track', '2'] + SIZE); save(img, 'dark-theme')
img, g = run('lesson', ['--lesson', 'acid:3', '--track', '1'] + SIZE); save(img, 'lesson')
img, g = run('simple', ['--tab', '1', '--track', '1', '--simple'] + SIZE)
t = g['tabs']; save(img, 'simple-sound', (t[0], t[1], t[0] + t[2], t[1] + t[3]))
img, g = run('arranged', ['--arrange', '1', '--track', '4'] + SIZE)
t = g['timeline']; save(img, 'arranged', (t[0], max(0, t[1] - 40), t[0] + t[2], t[1] + t[3]))
img, _ = run('chop', ['--dialog', 'chop']); save(img, 'chop')
img, _ = run('guide', ['--guide']); save(img, 'guide')
