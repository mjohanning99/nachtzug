"""Renders text with Qt (tools/qtrender), the way the program would: one call per font set, so that families with the
same name do not collide.

    from qtsheet import render
    img = render(fonts=[...ttf], items=[('text', x, y, family, weight, px, spacing, '#2b2623', 'Cutoff'), ...],
                 size=(w, h), bg='#ebe5d8')
"""
import os, subprocess, tempfile
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
QTR = os.path.join(HERE, 'qtrender', 'qtrender')

def render(fonts, items, size, bg='#ebe5d8'):
    lines = [f'font {os.path.abspath(f)}' for f in fonts]
    lines.append(f'canvas {size[0]} {size[1]} {bg}')
    for it in items:
        kind = it[0]
        if kind not in ('text', 'ctext', 'box'):
            lines.append(kind + ' ' + ' '.join(str(v) for v in it[1:]))
        else:
            vals = list(it[1:])
            fam_idx = 5 if kind == 'box' else 3
            vals[fam_idx - 1] = vals[fam_idx - 1].replace(' ', '_')
            lines.append(kind + ' ' + ' '.join(str(v) for v in vals))
    with tempfile.TemporaryDirectory() as td:
        spec = os.path.join(td, 'spec.txt'); out = os.path.join(td, 'out.png')
        with open(spec, 'w', encoding='utf-8') as f:
            f.write('\n'.join(lines) + '\n')
        env = dict(os.environ, QT_QPA_PLATFORM='offscreen')
        r = subprocess.run([QTR, spec, out], env=env, capture_output=True, text=True)
        if r.returncode != 0 or r.stderr.strip():
            print('qtrender:', r.returncode, r.stderr.strip()[:400])
        return Image.open(out).convert('RGB').copy()
