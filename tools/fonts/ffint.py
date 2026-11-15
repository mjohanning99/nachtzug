import sys, fontforge
f = fontforge.open(sys.argv[1])
g = f[sys.argv[2]]
l = g.layers[1]
bad = []
for i, c in enumerate(l):
    if c.selfIntersects(): bad.append((i, len(c), c.boundingBox()))
print('contours', len(l), 'self-int contours', len(bad), bad[:10])
