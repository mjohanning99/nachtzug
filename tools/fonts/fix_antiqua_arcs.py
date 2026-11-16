#!/usr/bin/env python3
"""Gives Nachtzug Antiqua's ct and st ligatures (c_t, s_t) back their historical arcs, from EB Garamond.

Earlier builds replaced EB Garamond's c_t and s_t with a fine "overhead wire" from the letter to the t; the owner chose
the old form instead, as in books of the 18th century: a bow rising from the head of the c or s and coming down onto
the t's lengthened stem. build_antiqua.py now keeps EB Garamond's own glyphs; this script puts them into the finished,
hinted fonts the same way fix_antiqua_tz.py patches the tz: the outlines are taken from the EB Garamond instances the
build starts from (overlaps removed), hinted with the build's ttfautohint settings, and only c_t and s_t go into the
installed fonts, so every other glyph stays exactly as it was. It also makes tz, ct and st standard ligatures (liga),
as build_antiqua.py now does: browsers reset font-feature-settings with the font shorthand, so the arcs only show
reliably in running text when the font applies them by itself.

    python3 tools/fonts/fix_antiqua_arcs.py [folder with EBG-440-ovl.ttf and EBG-700-ovl.ttf]
    (default: tools/fonts/antiqua/src, where build_antiqua.py reads them; fontTools, skia-pathops; ttfautohint)

Afterwards copy the two fonts to docs/manual/fonts and run tools/site_fonts.py for the website.
"""
import os, subprocess, sys, tempfile
from fontTools.ttLib import TTFont
from fontTools.pens.recordingPen import DecomposingRecordingPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.pens.cu2quPen import Cu2QuPen
from fontTools.pens.reverseContourPen import ReverseContourPen

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from geom import contours_of, from_contours, simplify, draw_fitted

FONTS = os.path.join(HERE, '..', '..', 'assets', 'fonts', 'nachtzug')
SRC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, 'antiqua', 'src')
HINT = ['ttfautohint', '--no-info', '--stem-width-mode=nss', '--windows-compatibility']   # as in build_all.sh
NAMES = ('c_t', 's_t')


def glyph_path(font, name):
    """the glyph's outline as one shape, overlaps removed"""
    rec = DecomposingRecordingPen(font.getGlyphSet())
    font.getGlyphSet()[name].draw(rec)
    return simplify(from_contours(contours_of(rec, 0.15)))


def put(font, name, path):
    pen = TTGlyphPen(None)
    draw_fitted(path, Cu2QuPen(ReverseContourPen(pen), max_err=0.5), err=0.3, corner=40,
                rnd=lambda p: (round(p[0], 1), round(p[1], 1)))
    font['glyf'][name] = pen.glyph()


def into_liga(font):
    """a lookup with tz, ct and st, added to every liga feature (once)"""
    from fontTools.otlLib import builder as otl
    from fontTools.ttLib.tables import otTables as ot
    gsub = font['GSUB'].table
    for i, lk in enumerate(gsub.LookupList.Lookup):       # already there from an earlier run?
        st = lk.SubTable[0]
        if lk.LookupType == 4 and getattr(st, 'ligatures', None) and any(
                L.LigGlyph == 's_t' for ligs in st.ligatures.values() for L in ligs) and any(
                i in r.Feature.LookupListIndex for r in gsub.FeatureList.FeatureRecord if r.FeatureTag == 'liga'):
            return
    lk = ot.Lookup(); lk.LookupType = 4; lk.LookupFlag = 0
    lk.SubTable = [otl.buildLigatureSubstSubtable({('t', 'z'): 't_z', ('c', 't'): 'c_t', ('s', 't'): 's_t'})]
    lk.SubTableCount = 1
    gsub.LookupList.Lookup.append(lk); gsub.LookupList.LookupCount = len(gsub.LookupList.Lookup)
    idx = len(gsub.LookupList.Lookup) - 1
    for r in gsub.FeatureList.FeatureRecord:
        if r.FeatureTag == 'liga':
            r.Feature.LookupListIndex = list(r.Feature.LookupListIndex) + [idx]
            r.Feature.LookupCount = len(r.Feature.LookupListIndex)


def main():
    for style, weight in (('Regular', 440), ('Bold', 700)):
        dst = os.path.join(FONTS, f'NachtzugAntiqua-{style}.ttf')
        ebg = TTFont(os.path.join(SRC, f'EBG-{weight}-ovl.ttf'))
        with tempfile.TemporaryDirectory() as tmp:
            un, re = os.path.join(tmp, 'un.ttf'), os.path.join(tmp, 're.ttf')
            subprocess.run(['ttfautohint', '--dehint', dst, un], check=True)
            f = TTFont(un)
            for n in NAMES:
                put(f, n, glyph_path(ebg, n))
                f['hmtx'][n] = ebg['hmtx'][n]
            f.save(un)
            subprocess.run(HINT + [un, re], check=True)
            hinted = TTFont(re)
            out = TTFont(dst)
            for n in NAMES:
                out['glyf'][n] = hinted['glyf'][n]
                out['hmtx'][n] = hinted['hmtx'][n]
            for k in ('maxPoints', 'maxContours', 'maxStackElements', 'maxSizeOfInstructions', 'maxTwilightPoints',
                      'maxStorage', 'maxFunctionDefs', 'maxZones'):
                setattr(out['maxp'], k, max(getattr(out['maxp'], k), getattr(hinted['maxp'], k)))
            into_liga(out)
            out.save(dst)
            print(f'{style}: c_t and s_t with their arcs, widths {out["hmtx"]["c_t"][0]} and {out["hmtx"]["s_t"][0]}')


main()
