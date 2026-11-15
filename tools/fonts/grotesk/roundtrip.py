"""fontTools round trip: python roundtrip.py font.ttf ...  load, decompile every table, dump to TTX, read back,
compile, reload, and compare glyph order, cmap, outlines and advance widths."""
import io, sys, tempfile, os
from fontTools.ttLib import TTFont
from fontTools.pens.recordingPen import RecordingPen

ok = True
for p in sys.argv[1:]:
    f = TTFont(p)
    f.ensureDecompiled()
    with tempfile.TemporaryDirectory() as td:
        x = os.path.join(td, 'f.ttx')
        f.saveXML(x)
        g = TTFont(); g.importXML(x)
        buf = io.BytesIO(); g.save(buf); buf.seek(0)
        h = TTFont(buf); h.ensureDecompiled()
    same = h.getGlyphOrder() == f.getGlyphOrder() and h.getBestCmap() == f.getBestCmap()
    gs1, gs2 = f.getGlyphSet(), h.getGlyphSet()
    bad = []
    for n in f.getGlyphOrder():
        a, b = RecordingPen(), RecordingPen()
        gs1[n].draw(a); gs2[n].draw(b)
        if a.value != b.value or f['hmtx'][n] != h['hmtx'][n]: bad.append(n)
    same &= not bad
    ok &= same
    print(f'{os.path.basename(p):40s} {"ok" if same else "MISMATCH " + " ".join(bad[:5])}  {len(f.getGlyphOrder())} glyphs  '
          f'"{f["name"].getDebugName(4)}"  tables: {" ".join(t for t in sorted(f.keys()) if t != "GlyphOrder")}')
sys.exit(0 if ok else 1)
