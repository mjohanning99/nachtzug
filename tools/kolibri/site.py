#!/usr/bin/env python3
"""Has Kolibri rework the German website (site/_data/de.yml): every German text with its English beside it and
where on the page it sits, rewritten as natural, impersonal German. Writes a JSON of suggestions and a page to compare;
nothing is changed in the site.

    python3 tools/kolibri/site.py OUT_DIR
"""
import html, json, os, re, sys, time, urllib.request
import yaml

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
URL = 'http://127.0.0.1:8080/v1/chat/completions'
BATCH = 12
# not to be reworded: the owner's own pitch; names, files, links and codes
SKIP = re.compile(r'(^|\.)(lang|code|url|img|icon|file|id|colour|class|link|href|page|repo|time|number|other\.name|'
                  r'hero\.lead|description|alt|ticket_colours|ticket_class|manual_en|manual_de|label)$|^board\.|^footer\.sub$|'
                  r'^footer\.line$|^title$')
WHERE = [('nav', 'Navigation oben (kurze Tasten)'), ('ticker', 'Laufschrift im Display oben, Großbuchstaben'),
         ('hero', 'Eröffnung der Seite, über dem Gemälde'), ('board', 'Zugzielanzeiger in der Eröffnung'),
         ('timetable', 'gelber Abfahrtsplan wie im Bahnhof (Kopf und Spalten)'), ('stations', 'Zeilen des Abfahrtsplans: was das Programm kann'),
         ('gallery', 'Bilder des Programms mit Bildunterschrift'), ('more', 'kleine Karten: weitere Eigenschaften'),
         ('manual', 'Abschnitt über die gedruckte Bedienungsanleitung'), ('download', 'Abschnitt mit den Fahrkarten zum Herunterladen'),
         ('ticket', 'aufgedruckt auf die Edmondson-Fahrkarten oder daneben'), ('tickets', 'Fahrkarten: Systeme und Anleitungen zum Installieren'),
         ('build', 'Anleitung: das Programm selbst bauen'), ('licence', 'Abschnitt über die Lizenzen („Beförderungsbedingungen“)'),
         ('faq', 'häufige Fragen und Antworten'), ('footer', 'Fußzeile der Seite'), ('your_ticket', 'Schild über der Fahrkarte für diesen Computer'),
         ('stations_', 'Überschrift des Abfahrtsplans (Emailleschild)'), ('more_title', 'Überschrift der kleinen Karten')]

SYSTEM = '''Du bist ein erfahrener deutscher Texter und Lektor für Websites. Du überarbeitest die deutsche Website eines Musikprogramms.

DAS PROGRAMM
Nachtzug ist ein für Anfänger gedachtes Musikprogramm, das es ermöglicht, elektronische Musik, z. B. im Stil der Berliner Schule, ohne große Vorkenntnisse zu erstellen. Musiktheorie ist nicht nötig, denn alle Noten landen automatisch in der korrekten Tonleiter. Kostenlos, freie Software, für Linux, macOS und Windows.

DIE WEBSITE
Sie ist wie ein Bahnhof gestaltet: ein Zugzielanzeiger, ein gelber Abfahrtsplan, Emailleschilder als Überschriften, Edmondson-Fahrkarten zum Herunterladen. Bahn-Anspielungen sind erwünscht, müssen aber natürlich klingen, wie es ein Deutscher sagen würde (z. B. „Einsteigen, bitte!“, „Freifahrt“, „Gute Fahrt!“). Wortspiele, die im Deutschen schief klingen (z. B. „Frei mitfahren“), durch etwas Besseres ersetzen.

STIL
- Natürliches, idiomatisches Deutsch, wie ein deutscher Muttersprachler es schreiben würde; nicht wörtlich aus dem Englischen übersetzen.
- UNPERSÖNLICH: niemals „du“, „dein“, „Sie“ oder „Ihr“. Stattdessen Passiv, „man“ oder Infinitiv.
- Freundlich, klar, ruhig; keine Werbesprache, keine Ausrufe-Häufung.
- Schlicht und erklärend statt Slogans: lieber ein vollständiger Satz, der sagt, was etwas tut.
- Ungefähr gleich lang wie bisher; Tasten, Schilder und Navigation kurz halten.
- HTML (<b>, <code>, <a href=…>, <p>, <pre>, <span class=…>), Platzhalter (%system%, %colour%, %class%, %nr%, %base%) und Zeilenumbrüche genau so übernehmen. Befehle in <code> oder <pre> nie ändern.
- Typografie: „…“, Gedankenstrich – mit Leerzeichen, … als ein Zeichen.

FESTE NAMEN (so heißen die Teile im Programm)
Spur, Clip, Pattern, Song, Noten, Reiter Sound, Mixer, Zeitleiste, Loopraster, Regler, Taste, Tonart, Tonleiter, Akkord, Sample, Pad, Drummer, Einstellungen, Rückgängig, Neue Idee, Variieren, Arrangieren, Einfach, Komplett, Lektionen, Bakelit, Glut, Bedienungsanleitung. Bahnhofsnamen im Abfahrtsplan (z. B. „Ideenfeld Ost“, „Sample Gbf“) bleiben unverändert.

ANTWORT
Nur ein JSON-Array, sonst nichts: [{"id": 1, "de": "…"}, …], in derselben Reihenfolge. Ist ein Text schon gut, ihn unverändert zurückgeben.'''


def flatten(d, prefix=''):
    out = {}
    if isinstance(d, dict):
        for k, v in d.items():
            out.update(flatten(v, f'{prefix}.{k}' if prefix else str(k)))
    elif isinstance(d, list):
        for i, v in enumerate(d):
            out.update(flatten(v, f'{prefix}[{i}]'))
    elif isinstance(d, str):
        out[prefix] = d
    return out


def where(path):
    base = re.sub(r'\[\d+\]', '', path)
    for key, text in sorted(WHERE, key=lambda kv: -len(kv[0])):
        if base.startswith(key):
            return text
    return 'Website'


def ask(items):
    parts = [f'[{x["id"]}] Ort: {x["where"]}\nEnglisch: {x["en"]}\nBisher: {x["de"]}' for x in items]
    body = {'model': 'kolibri', 'temperature': 0.3, 'max_tokens': 8000,
            'messages': [{'role': 'system', 'content': SYSTEM},
                         {'role': 'user', 'content': 'Überarbeite diese Texte der deutschen Website:\n\n' + '\n\n'.join(parts)}]}
    req = urllib.request.Request(URL, json.dumps(body).encode(), {'Content-Type': 'application/json'})
    d = json.load(urllib.request.urlopen(req, timeout=900))
    text = d['choices'][0]['message'].get('content') or ''
    m = re.search(r'\[\s*\{.*\}\s*\]', text, re.S)
    if not m:
        raise ValueError(f'no JSON ({d["choices"][0].get("finish_reason")})')
    return json.loads(m.group(0))


def tokens(s):
    return sorted(re.findall(r'<[^>]+>|%\w+%|\n', s))


def main():
    out_dir = sys.argv[1]
    os.makedirs(out_dir, exist_ok=True)
    en = flatten(yaml.safe_load(open(f'{ROOT}/site/_data/en.yml')))
    de = flatten(yaml.safe_load(open(f'{ROOT}/site/_data/de.yml')))
    items = []
    for path, text in de.items():
        if SKIP.search(re.sub(r'\[\d+\]', '', path)) or path not in en or not text.strip():
            continue
        items.append({'id': len(items) + 1, 'path': path, 'where': where(path), 'en': en[path], 'de': text})
    json.dump(items, open(f'{out_dir}/site-items.json', 'w'), ensure_ascii=False, indent=1)
    res_path = f'{out_dir}/site-out.json'
    res = json.load(open(res_path)) if os.path.exists(res_path) else {}
    todo = [x for x in items if str(x['id']) not in res]
    print(f'{len(items)} texts, {len(todo)} to do', flush=True)
    for i in range(0, len(todo), BATCH):
        part = todo[i:i + BATCH]
        t0 = time.time()
        got = []
        for attempt in range(3):
            try:
                got = ask(part)
                break
            except Exception as e:
                print(f'  batch {i // BATCH + 1}: {e}', flush=True)
        byid = {g.get('id'): g.get('de', '') for g in got if isinstance(g, dict)}
        for x in part:
            new = byid.get(x['id'])
            if new is None:
                continue
            problems = []
            if tokens(new) != tokens(x['de']):
                problems.append('Markup oder Platzhalter verändert')
            if re.search(r'\b(du|dein\w*|dir|dich|Ihr\w*)\b', new):
                problems.append('duzt oder siezt')
            res[str(x['id'])] = {'de': new, 'problems': problems}
        json.dump(res, open(res_path, 'w'), ensure_ascii=False, indent=1)
        print(f'batch {i // BATCH + 1}/{(len(todo) + BATCH - 1) // BATCH}: {len(byid)} back in {time.time() - t0:.0f} s', flush=True)
    # the page to compare
    e = html.escape
    rows, last = [], None
    for x in items:
        if x['where'] != last:
            rows.append(f'<tr class="sec"><td colspan="3">{e(x["where"])}</td></tr>')
            last = x['where']
        r = res.get(str(x['id']), {})
        new = r.get('de', '—')
        same = new.strip() == x['de'].strip()
        probs = ''.join(f'<span class="bad">{e(p)}</span>' for p in r.get('problems', []))
        rows.append(f'<tr class="{"same" if same else ""}"><td>{e(x["en"])}</td><td class="now">{e(x["de"])}</td>'
                    f'<td class="new">{e(new) if not same else "<i>unverändert</i>"}{probs}</td></tr>')
    changed = sum(1 for x in items if res.get(str(x['id']), {}).get('de', x['de']).strip() != x['de'].strip())
    page = f'''<!doctype html><html lang="de"><meta charset="utf-8"><title>Kolibri: Website</title><style>
:root {{ --paper:#f4efe5; --ink:#2b2623; --soft:#6e6054; --rule:#cbbfac; --sec:#e1d9c9; --new:#fbf3dc; --bad:#c4361c; }}
@media (prefers-color-scheme: dark) {{ :root:not([data-theme="light"]) {{ --paper:#221d1a; --ink:#f3ece0; --soft:#c2b3a1; --rule:#4a3f37; --sec:#2b2623; --new:#2e2620; --bad:#ff7a5c; }} }}
body {{ margin:0; padding:20px 16px 40px; background:var(--paper); color:var(--ink); font:15px/1.45 system-ui, sans-serif; }}
h1 {{ font-size:22px; margin:0 0 4px; }} p.sub {{ color:var(--soft); margin:0 0 16px; max-width:70em; }}
table {{ border-collapse:collapse; width:100%; table-layout:fixed; }}
th {{ text-align:left; font-size:12px; letter-spacing:.08em; text-transform:uppercase; color:var(--soft); padding:6px 8px; border-bottom:2px solid var(--rule); }}
td {{ vertical-align:top; padding:7px 8px; border-bottom:1px solid var(--rule); overflow-wrap:anywhere; }}
tr.sec td {{ background:var(--sec); font-weight:600; }} td.now {{ color:var(--soft); }} td.new {{ background:var(--new); }}
tr.same td {{ opacity:.55; }} .bad {{ display:block; color:var(--bad); font-size:12px; font-weight:600; margin-top:4px; }}
@media (max-width:700px) {{ table, tbody, tr, td {{ display:block; width:auto; }} th {{ display:none; }} }}
</style><h1>Kolibri überarbeitet die deutsche Website</h1>
<p class="sub">{len(items)} Texte, {changed} davon geändert. Links das Englische, in der Mitte die bisherige deutsche Fassung, rechts Kolibris. Blass: unverändert. Rot: was die Prüfung bemängelt. Der Pitch der Eröffnung ist ausgenommen.</p>
<table><tr><th>Englisch</th><th>Bisher</th><th>Kolibri</th></tr>{"".join(rows)}</table></html>'''
    open(f'{out_dir}/kolibri-website.html', 'w').write(page)
    print(f'page written: {changed} changed of {len(items)}')


main()
