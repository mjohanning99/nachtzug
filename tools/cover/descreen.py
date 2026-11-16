#!/usr/bin/env python3
# Removes a halftone screen: the screen is a regular grid of dots, which shows as sharp peaks in the 2-D spectrum,
# away from its centre. Those peaks are notched out (soft-edged), everything else is left alone. Also writes the
# spectrum next to OUT, to see what was found. Needs numpy, scipy and Pillow.
#     python3 tools/cover/descreen.py IN.jpg OUT.png
import sys
import numpy as np
from PIL import Image
from scipy import ndimage

src, out = sys.argv[1], sys.argv[2]
img = np.asarray(Image.open(src).convert('RGB')).astype(np.float64)
H, W, _ = img.shape
lum = img.mean(2)
F = np.fft.fftshift(np.fft.fft2(lum - lum.mean()))
mag = np.log1p(np.abs(F))
yy, xx = np.mgrid[0:H, 0:W]
r = np.hypot((yy - H / 2) / H, (xx - W / 2) / W)
# a smooth background for the spectrum, so peaks stand out against what is normal at that frequency
bg = ndimage.median_filter(mag, size=31)
excess = mag - bg
cand = (r > 0.09) & (excess > 1.4) & (excess == ndimage.maximum_filter(excess, size=11))
peaks = np.argwhere(cand)
print(len(peaks), 'peaks')
mask = np.ones((H, W))
for py, px in peaks:
    d2 = (yy - py) ** 2 + (xx - px) ** 2
    mask *= 1 - np.exp(-d2 / (2 * 5.5 ** 2))
res = np.empty_like(img)
for c in range(3):
    ch = img[..., c]
    Fc = np.fft.fftshift(np.fft.fft2(ch))
    res[..., c] = np.real(np.fft.ifft2(np.fft.ifftshift(Fc * mask)))
Image.fromarray(np.clip(res, 0, 255).round().astype(np.uint8)).save(out)
Image.fromarray((np.clip(mag / mag.max(), 0, 1) * 255).astype(np.uint8)).save(out.replace('.png', '-spectrum.png'))
