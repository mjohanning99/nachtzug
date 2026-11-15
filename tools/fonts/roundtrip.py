"""fontTools round trip for every font in dist/: load, decompile all tables, dump to TTX, read back, compile, compare."""
import os, io, sys, tempfile
from fontTools.ttLib import TTFont
HERE = os.path.dirname(os.path.abspath(__file__))
D = os.path.join(HERE, 'dist')
ok = True
for n in sorted(os.listdir(D)):
    if not n.endswith(('.ttf', '.otf')): continue
    p = os.path.join(D, n)
    f = TTFont(p)
    f.ensureDecompiled()
    tags = sorted(f.keys())
    with tempfile.TemporaryDirectory(dir=os.path.dirname(HERE)) as td:
        x = os.path.join(td, 'f.ttx')
        f.saveXML(x)
        g = TTFont(); g.importXML(x)
        buf = io.BytesIO(); g.save(buf); buf.seek(0)
        h = TTFont(buf); h.ensureDecompiled()
    same = h.getGlyphOrder() == f.getGlyphOrder() and h.getBestCmap() == f.getBestCmap()
    ok &= same
    name = f['name'].getDebugName(4)
    print(f'{n:44s} {"ok" if same else "MISMATCH"}  {len(f.getGlyphOrder()):5d} glyphs  "{name}"  tables: {" ".join(t for t in tags if t != "GlyphOrder")}')
sys.exit(0 if ok else 1)
