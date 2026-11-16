#!/usr/bin/env python3
# A gentle colour grade for the cover painting: levels, a little vibrance and a little local contrast (the old print
# had gone brown and flat). The split toning and brightness are there for trying stronger grades; the cover uses none.
#     python3 tools/cover/grade.py IN.png OUT.png
import sys
import numpy as np
from PIL import Image, ImageFilter


def grade(img, vib=1.2, clarity=0.15, split=0.0, gamma=1.0):
    a = np.asarray(img).astype(np.float32) / 255
    lum = a @ np.array([.2126, .7152, .0722], np.float32)
    # levels: stretch so the darkest and brightest half per mille touch black and white (a little short of them)
    lo, hi = np.percentile(lum, .5), np.percentile(lum, 99.7)
    a = np.clip((a - lo) / (hi - lo) * .97 + .01, 0, 1)
    # brightness
    a = a ** gamma
    # vibrance: more colour where there is little, less boost where it is already strong
    lum = a @ np.array([.2126, .7152, .0722], np.float32)
    sat = a.max(2) - a.min(2)
    k = 1 + (vib - 1) * (1 - sat)[..., None]
    a = np.clip(lum[..., None] + (a - lum[..., None]) * k, 0, 1)
    # local contrast: a large, faint unsharp mask
    if clarity:
        im = Image.fromarray((a * 255).astype(np.uint8))
        blur = np.asarray(im.filter(ImageFilter.GaussianBlur(40))).astype(np.float32) / 255
        a = np.clip(a + (a - blur) * clarity, 0, 1)
    # split toning: shadows a touch blue, highlights a touch golden
    if split:
        lum = a @ np.array([.2126, .7152, .0722], np.float32)
        sh = np.clip(1 - lum / .45, 0, 1)[..., None]
        hl = np.clip((lum - .45) / .55, 0, 1)[..., None]
        a = np.clip(a + split * (sh * np.array([-.02, .0, .05]) + hl * np.array([.06, .03, -.04])), 0, 1)
    return Image.fromarray((a * 255).round().astype(np.uint8))


grade(Image.open(sys.argv[1]).convert('RGB'), vib=1.2, clarity=.15).save(sys.argv[2])
print('graded:', sys.argv[2])
