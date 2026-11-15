# The Nachtzug type family

These scripts build the fonts in `assets/fonts/nachtzug/`:

- **Nachtzug Antiqua** (Regular and Bold), derived from EB Garamond, with Nachtzug’s own *g* and *z*, rail ligatures (tz, ct, st), swash capitals and the full set of typographic punctuation: `build_antiqua.py`.
- **Nachtzug Initialen**, the ornamented drop caps (one colour, two stackable layers, and a COLR colour font): `initialen.py`, with shared geometry in `geom.py`.
- **Nachtzug Ornamente**, the moons, stars, train and track: `ornaments.py`.

`music_signs.py` adds the sharp, flat and natural signs (♯ ♭ ♮) to Jost, Courier Prime and the Antiqua, drawn from each font’s own cap height and stem weight; run it again on any font that is rebuilt.

`build_all.sh` rebuilds the Antiqua, hints it with ttfautohint and copies everything into place. The checks are `ffvalidate.py` (FontForge), `roundtrip.py` (fontTools) and the preview scripts.

They need Python with fontTools, skia-pathops, pyclipper and Pillow, plus FontForge and ttfautohint, and the EB Garamond variable fonts (SIL Open Font License) from <https://github.com/octaviopardo/EBGaramond12>. Some scripts still point at the folders they were first written in; set the paths at the top of each script before running them.

All of these fonts are released under the SIL Open Font License (see `assets/fonts/nachtzug/OFL.txt`).
