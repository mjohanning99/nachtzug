#!/usr/bin/env python3
# A page to compare: where each string sits, the English, the current German (which says "du"), and Kolibri's.
# Usage: review.py batch.json out.json page.html [notes.json]
import html, json, sys
sys.path.insert(0, __file__.rsplit('/', 1)[0])
from translate import where  # noqa: E402

items = json.load(open(sys.argv[1]))
out = {int(k): v for k, v in json.load(open(sys.argv[2])).items()}
notes = {}
if len(sys.argv) > 4:
    notes = {int(k): v for k, v in json.load(open(sys.argv[4])).items()}
e = html.escape
rows = []
last = None
for x in items:
    w = where(x)
    if w != last:
        rows.append(f'<tr class="sec"><td colspan="4">{e(w)}</td></tr>')
        last = w
    k = out.get(x['id'], {})
    probs = ''.join(f'<span class="bad">{e(p)}</span>' for p in k.get('problems', []))
    note = notes.get(x['id'])
    noteh = f'<div class="note"><b>Mein Vorschlag:</b> {e(note)}</div>' if note else ''
    rows.append(f'<tr><td class="kind">{e(x.get("kind", ""))}</td><td>{e(x["en"])}</td><td class="now">{e(x.get("de_now", ""))}</td>'
                f'<td class="new">{e(k.get("de", "—"))}{probs}{noteh}</td></tr>')
page = f'''<!doctype html><html lang="de"><meta charset="utf-8"><title>Kolibri-Pilot</title>
<style>
:root {{ --paper:#f4efe5; --ink:#2b2623; --soft:#6e6054; --rule:#cbbfac; --sec:#e1d9c9; --new:#fbf3dc; --bad:#c4361c; }}
@media (prefers-color-scheme: dark) {{ :root:not([data-theme="light"]) {{ --paper:#221d1a; --ink:#f3ece0; --soft:#c2b3a1; --rule:#4a3f37; --sec:#2b2623; --new:#2e2620; --bad:#ff7a5c; }} }}
body {{ margin:0; padding:20px 16px 40px; background:var(--paper); color:var(--ink); font:15px/1.45 system-ui, sans-serif; }}
h1 {{ font-size:22px; margin:0 0 4px; }} p.sub {{ color:var(--soft); margin:0 0 16px; max-width:70em; }}
table {{ border-collapse:collapse; width:100%; table-layout:fixed; }}
th {{ text-align:left; font-size:12px; letter-spacing:.08em; text-transform:uppercase; color:var(--soft); padding:6px 8px; border-bottom:2px solid var(--rule); }}
td {{ vertical-align:top; padding:7px 8px; border-bottom:1px solid var(--rule); overflow-wrap:anywhere; }}
tr.sec td {{ background:var(--sec); font-weight:600; padding:8px; }}
td.kind {{ font-size:12px; color:var(--soft); width:14%; }} td.now {{ color:var(--soft); }} td.new {{ background:var(--new); }}
.bad {{ display:block; color:var(--bad); font-size:12px; font-weight:600; margin-top:4px; }}
.note {{ font-size:13px; margin-top:6px; padding-top:5px; border-top:1px dashed var(--rule); }}
col.k {{ width:14%; }} col.t {{ width:28.6%; }}
@media (max-width: 700px) {{ table, tbody, tr, td {{ display:block; width:auto; }} th {{ display:none; }} td.kind {{ width:auto; }} }}
</style>
<h1>Kolibri-Pilot: Begrüßungsfenster und Einstellungen</h1>
<p class="sub">{len(items)} Texte. Links das Englische, in der Mitte die bisherige deutsche Fassung (sie duzt), rechts Kolibris unpersönliche Fassung. Rot: was die automatische Prüfung bemängelt hat.</p>
<table><colgroup><col class="k"><col class="t"><col class="t"><col class="t"></colgroup>
<tr><th>Art</th><th>Englisch</th><th>Bisher</th><th>Kolibri</th></tr>
{''.join(rows)}
</table></html>'''
open(sys.argv[3], 'w').write(page)
print('written', len(items), 'rows')
