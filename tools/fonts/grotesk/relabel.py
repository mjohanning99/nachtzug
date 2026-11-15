"""Re-letters a screenshot of Nachtzug: finds every label set in Jost (by matching Qt renders of the known strings at
the program's sizes, weights and letter-spacings), paints the background back in, and sets the same text in
Nachtzug Grotesk at the same size, weight, spacing, colour and centre. A control run sets Jost again the same way,
to show how faithful the method is.

    python relabel.py shots/program.png shots/after.png shots/control.png shots/found.json
"""
import sys, os, json, itertools
import numpy as np
from PIL import Image
from qtsheet import render

HERE = os.path.dirname(os.path.abspath(__file__))
_REPO = os.path.normpath(os.path.join(HERE, '..', '..', '..', 'assets', 'fonts'))
J = os.environ.get('JOST_DIR', _REPO)
JOST = [f'{J}/Jost-{w}.ttf' for w in (400, 500, 600)]
GROT = [os.path.join(HERE, 'out', f'NachtzugGrotesk-{s}.ttf') for s in ('Regular', 'Medium', 'SemiBold')]

STRINGS = """Loop|Click|In time|Tempo|Key|Scale|Tap|Swing|Volume|Export|Pan|Echo|Reverb|Pump|Mute|Solo|Acid Squelch|Mutate|
Roll new sound|Cutoff|Resonance|Env amount|Decay|Drive|Octave|Each note plays|Single note|Arpeggio|Off|1/16|1 octave|
Follow chords|Song|Loops|Add track|Duplicate|Loop this part|Cut at playhead|Scissors|Delete|Arrange for me|Zoom|
BARS · click to move the playhead|LOOP · drag to mark what repeats|CHORDS · click a bar to change it|Drums|Bass|Sequence|Pad|
Lines|Auto drummer · Four on the floor|Acid line|Berlin sequence|Held chord|Wave 1|Wave 2|Saw|Osc 2|Unison|Osc mix|Detune|
FM|Pulse width|Sub|Noise|Voices|1 (off)|Play|Mono|Glide|Type|Low-pass 24 dB|Key|tracking|Accent|detune|Stereo|spread|
Attack|Sustain|Release|To pitch|LOOP|Am|Press Space to play. Letter keys play the selected track, always in the song’s scale.""".replace('\n', '').split('|')
STRINGS = list(dict.fromkeys(s for s in STRINGS if s))
SIZES = [8, 9, 10, 11, 12, 13]
WEIGHTS = [400, 500, 600, 700]       # 700: setBold(true), which Qt fakes from the 600
SPACINGS = [0, 0.3, 0.5, 1.0]
ROW = 34

PHASES = [0, 0.25, 0.5, 0.75]       # Qt places glyphs at quarter pixels
SCALED = [(11, w, 0.3, sc) for w in (400, 500, 600) for sc in (0.8, 0.76)]   # the small knobs draw 11 px under a painter scale

def templates(fonts, family, px, wt, sp, sc=1.0, ph=0.0):
    """Black-on-white renders of every string, trimmed to the ink, with the ink box relative to the pen origin
    (10, baseline) in device pixels; the pen itself sits at 10 + ph."""
    W = 900
    items = [('scale', sc)]
    items += [('text', (10 + ph) / sc, (ROW * i + 24) / sc, family, wt, px, sp, '#000000', s) for i, s in enumerate(STRINGS)]
    im = np.asarray(render(fonts, items, (W, ROW * len(STRINGS)), bg='#ffffff').convert('L'), float)
    out = {}
    for i, s in enumerate(STRINGS):
        band = im[ROW * i: ROW * (i + 1)]
        ys, xs = np.nonzero(band < 250)
        if len(xs) == 0: continue
        y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
        out[s] = (band[y0:y1, x0:x1], (x0 - 10, y0 - 24))
    return out

def ncc_map(img, t):
    """Normalised cross-correlation of template t over img (valid positions), via FFT."""
    th, tw = t.shape
    tz = t - t.mean()
    tn = np.sqrt((tz ** 2).sum()) or 1
    H, W = img.shape
    fh, fw = H + th, W + tw
    F = np.fft.rfft2(img, (fh, fw)) * np.conj(np.fft.rfft2(tz, (fh, fw)))
    num = np.fft.irfft2(F, (fh, fw))[:H - th + 1, :W - tw + 1]
    c1 = np.pad(img, ((1, 0), (1, 0))).cumsum(0).cumsum(1)
    c2 = np.pad(img ** 2, ((1, 0), (1, 0))).cumsum(0).cumsum(1)
    def box(c):
        return c[th:, tw:] - c[:-th, tw:] - c[th:, :-tw] + c[:-th, :-tw]
    s1, s2 = box(c1), box(c2)
    var = np.maximum(s2 - s1 ** 2 / (th * tw), 1e-6)
    return num / (np.sqrt(var) * tn)

def local_ncc(img, t, x, y, r=3):
    th, tw = t.shape
    best = (-2, x, y)
    tz = t - t.mean(); tn = np.sqrt((tz ** 2).sum()) or 1
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            X, Y = x + dx, y + dy
            if X < 0 or Y < 0 or Y + th > img.shape[0] or X + tw > img.shape[1]: continue
            p = img[Y:Y + th, X:X + tw]
            pz = p - p.mean(); pn = np.sqrt((pz ** 2).sum()) or 1
            v = abs((pz * tz).sum() / (pn * tn))
            if v > best[0]: best = (v, X, Y)
    return best

def main(src, out_after, out_control, out_json):
    shot = Image.open(src).convert('RGB')
    S = np.asarray(shot, float)
    G = S.mean(axis=2)
    base = [c + (1.0,) for c in itertools.product(SIZES, WEIGHTS, SPACINGS)] + SCALED
    combos = [b + (ph,) for b in base for ph in PHASES]
    TJ = {c: templates(JOST, 'Jost', *c) for c in combos}
    TG = {c: templates(GROT, 'Nachtzug Grotesk', *c) for c in combos}
    found = []
    coarse = [(10, 400, 0, 1.0, 0), (9, 400, 0, 1.0, 0), (11, 400, 0, 1.0, 0), (8, 400, 0.3, 1.0, 0), (10, 400, 0.3, 1.0, 0), (11, 400, 0.3, 1.0, 0), (13, 400, 0, 1.0, 0), (13, 500, 0.5, 1.0, 0),
              (12, 600, 0, 1.0, 0), (13, 600, 0, 1.0, 0), (11, 600, 0, 1.0, 0), (9, 600, 1.0, 1.0, 0), (11, 400, 0.3, 0.76, 0)]
    for s in STRINGS:
        cands = []
        for c in coarse:
            if s not in TJ[c]: continue
            t, _ = TJ[c][s]
            m = np.abs(ncc_map(G, t))
            ys, xs = np.nonzero(m > 0.6)
            for y, x in sorted(zip(ys, xs), key=lambda p: -m[p[0], p[1]]):
                if all(abs(x - a) > 8 or abs(y - b) > 6 for a, b in cands): cands.append((x, y))
        for (x, y) in cands:
            scored = []
            for b in base:
                c = b + (0,)
                if s not in TJ[c]: continue
                v, X, Y = local_ncc(G, TJ[c][s][0], x, y, r=4)
                scored.append((v, c, X, Y))
            scored.sort(key=lambda r: -r[0])
            best = scored[0] if scored else None
            for v0, c0, X0, Y0 in scored[:4]:
                for ph in PHASES[1:]:
                    c = c0[:4] + (ph,)
                    if s not in TJ[c]: continue
                    v, X, Y = local_ncc(G, TJ[c][s][0], X0, Y0, r=1)
                    if v > best[0]: best = (v, c, X, Y)
            if best and best[0] >= 0.92:
                v, c, X, Y = best
                t, off = TJ[c][s]
                found.append(dict(s=s, ncc=float(v), c=list(c), x=int(X), y=int(Y), w=int(t.shape[1]),
                                  h=int(t.shape[0]), ox=int(X - off[0]), oy=int(Y - off[1])))
    # a doubtful match (a knob mark or tick touching the label) falls back on a style the program uses often,
    # unless another style is clearly better
    from collections import Counter
    common = [c for c, n in Counter(tuple(f['c'][:4]) for f in found if f['ncc'] >= 0.97).items() if n >= 3]
    for f in found:
        if f['ncc'] >= 0.97: continue
        best = None
        for b in common:
            for ph in PHASES:
                c = b + (ph,)
                if f['s'] not in TJ[c]: continue
                v, X, Y = local_ncc(G, TJ[c][f['s']][0], f['x'], f['y'], r=2)
                if best is None or v > best[0]: best = (v, c, X, Y)
        if best and best[0] >= f['ncc'] - 0.03 and tuple(f['c']) != best[1]:
            v, c, X, Y = best
            t, off = TJ[c][f['s']]
            f.update(ncc=float(v), c=list(c), x=int(X), y=int(Y), w=int(t.shape[1]), h=int(t.shape[0]),
                     ox=int(X - off[0]), oy=int(Y - off[1]))
    # one label per place: keep the best match where boxes overlap
    found.sort(key=lambda f: (-len(f['s']), -f['ncc']))     # a whole label wins over a word inside it
    keep = []
    for f in found:
        if all(f['x'] + f['w'] <= k['x'] or k['x'] + k['w'] <= f['x'] or f['y'] + f['h'] <= k['y'] or k['y'] + k['h'] <= f['y'] for k in keep):
            keep.append(f)
    # erase, estimate colours
    E = S.copy()
    for f in keep:
        t, off = TJ[tuple(f['c'])][f['s']]
        x0, y0, x1, y1 = f['x'], f['y'], f['x'] + f['w'], f['y'] + f['h']
        X0, Y0, X1, Y1 = x0 - 1, y0 - 1, x1 + 1, y1 + 1
        top, bot = S[Y0 - 1, X0:X1], S[Y1, X0:X1]
        patch = S[y0:y1, x0:x1]
        cov = (1 - t / 255.0)
        rows = np.linspace(0, 1, y1 - y0 + 2)[1:-1]
        bg = top[None, 1:-1] * (1 - rows[:, None, None]) + bot[None, 1:-1] * rows[:, None, None]
        sel = cov > 0.6
        est = (patch[sel] - (1 - cov[sel])[:, None] * bg[sel]) / cov[sel][:, None]
        ink = np.clip(np.median(est, axis=0), 0, 255).round().astype(int)
        f['ink'] = '#%02x%02x%02x' % tuple(ink)
        for k, yy in enumerate(range(Y0, Y1)):
            a = (k + 1) / (Y1 - Y0 + 1)
            E[yy, X0:X1] = top * (1 - a) + bot * a
    erased = os.path.join(os.path.dirname(os.path.abspath(out_after)), 'erased.png')
    Image.fromarray(E.round().astype(np.uint8)).save(erased)
    W, H = shot.size
    for fonts, fam, T, out in ((GROT, 'Nachtzug Grotesk', TG, out_after), (JOST, 'Jost', TJ, out_control)):
        items = [('image', 0, 0, erased)]
        for f in keep:
            c = tuple(f['c'])
            px, wt, sp, sc, ph = c
            tj, oj = TJ[c][f['s']]
            tg, og = T[c][f['s']]
            cj = oj[0] + tj.shape[1] / 2
            cg = og[0] + tg.shape[1] / 2
            ox = f['ox'] + ph + (cj - cg)
            items.append(('scale', sc))
            items.append(('text', round(ox / sc, 4), round(f['oy'] / sc, 4), fam, wt, px, sp, f['ink'], f['s']))
            items.append(('scale', 1.0))
        render(fonts, items, (W, H)).save(out)
    json.dump(keep, open(out_json, 'w'), indent=1, ensure_ascii=False)
    ctrl = np.asarray(Image.open(out_control).convert('RGB'), float)
    diff = np.abs(ctrl - S).mean()
    print(f'{len(keep)} labels re-lettered; control differs from the screenshot by {diff:.3f} levels on average')
    missing = [s for s in STRINGS if not any(k['s'] == s for k in keep)]
    print('not found:', missing)

if __name__ == '__main__':
    main(*sys.argv[1:5])
