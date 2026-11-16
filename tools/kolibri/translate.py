#!/usr/bin/env python3
# Sends the collected strings to Kolibri in small batches, each with where it sits and what it is for, and asks for
# natural, impersonal German. Checks placeholders and markup in what comes back. Usage: translate.py batch.json out.json
import json, re, sys, time, urllib.request

URL = 'http://127.0.0.1:8080/v1/chat/completions'
BATCH = 12

WHERE = [('WelcomeDialog', 'Begrüßungsfenster, das beim allerersten Start erscheint'),
         ('generalPage', 'Fenster „Einstellungen“, Reiter „Allgemein“'),
         ('audioPage', 'Fenster „Einstellungen“, Reiter „Audio“ (Soundkarte: Ausgang, Eingang, Puffer)'),
         ('fillAudio', 'Fenster „Einstellungen“, Reiter „Audio“ (Soundkarte: Ausgang, Eingang, Puffer)'),
         ('applyAudio', 'Fenster „Einstellungen“, Reiter „Audio“: Meldung, wenn eine Einstellung nicht klappt'),
         ('samplesPage', 'Fenster „Einstellungen“, Reiter „Samples“ (eigene Klänge verwalten)'),
         ('fillSamples', 'Fenster „Einstellungen“, Reiter „Samples“ (die Liste der Klänge)'),
         ('midiPage', 'Fenster „Einstellungen“, Reiter „MIDI“ (Keyboards, Controller, Clock)'),
         ('SettingsDialog', 'Fenster „Einstellungen“')]

SYSTEM = '''Du bist ein erfahrener deutscher Software-Übersetzer und Lektor. Du übersetzt die Texte eines Musikprogramms aus dem Englischen ins Deutsche.

DAS PROGRAMM
Nachtzug ist ein kleines, kostenloses Musikprogramm (Sequencer und Synthesizer) für Acid und Berliner Schule: Basslines, Sequenzen, Flächen (Pads) und Drums, die sich wiederholen und von selbst spielen. Es ist für Menschen gemacht, die Musik lieben, aber kein Instrument spielen und keine Musiktheorie kennen. Jede Note liegt in der Tonleiter des Songs, darum kann nichts falsch klingen. Auf Knopfdruck schreibt das Programm Ideen, ein Drummer spielt von selbst, und jede Änderung lässt sich rückgängig machen. Kein Profi-Studio, sondern ein freundliches, einfaches Werkzeug, das wie ein altes Gerät aussieht (Tasten, Regler, Schalter).

STIL
- Natürliches, idiomatisches Deutsch, wie es ein deutscher Muttersprachler für ein Programm schreiben würde. Nicht wörtlich übersetzen.
- UNPERSÖNLICH: niemals „du“, „dein“, „Sie“ oder „Ihr“. Stattdessen:
  * Tasten, Menüpunkte, Reiter, Feldnamen: kurzes Substantiv oder Infinitiv („Neue Idee“, „Song speichern…“).
  * Tooltips und Hinweise: Infinitiv, Passiv oder „man“ („Zum Ändern ziehen.“, „Hier wird eingestellt, …“, „Damit lässt sich …“).
  * Meldungen: Passiv oder „man“ („Die Datei ließ sich nicht öffnen.“).
- Freundlich, ruhig, klar; keine Werbesprache. Fachbegriffe nur, wo nötig.
- Kurz bleiben: Beschriftungen von Tasten, Feldern, Reitern und Listeneinträgen höchstens etwa so lang wie die bisherige deutsche Fassung, sonst passen sie nicht auf den Bildschirm.
- Typografie: deutsche Anführungszeichen „…“, Gedankenstrich – mit Leerzeichen, Auslassungspunkte … (ein Zeichen).
- Platzhalter (%1, %2, %n), HTML (<b>…</b>, <br>) und Zeichen wie ▾ ▶ ● ⚙ genau so übernehmen.

FESTE BEGRIFFE (so heißen die Teile im Programm, immer genau so)
Spur, Clip, Pattern, Song, Noten (Reiter NOTEN), Sound (Reiter SOUND), Mixer (Reiter MIXER), Zeitleiste, Loopraster, Loops, Regler, Taste, Tonart, Tonleiter, Akkord, Sample, Pad, Drummer, Einstellungen, Rückgängig, Wiederholen, Exportieren, Lektionen, Begrüßungsfenster, Neue Idee, Variieren, Aufräumen, Behalten, Stumm, Solo, Arrangieren, Swing, Lautstärke, Tap, Klick, Im Takt, Eingang, Ausgang, Pegel, Scrollbalken.
Die bisherige deutsche Fassung duzt und klingt teils übersetzt; sie darf als Hilfe dienen, muss aber nicht übernommen werden.

ANTWORT
Nur ein JSON-Array, sonst nichts: [{"id": 1, "de": "…"}, …], eine Zeile pro Text, in derselben Reihenfolge.'''


def where(item):
    w = item.get('where', '') + ' ' + item['context']
    return next((text for key, text in WHERE if key in w), item['context'])


def ask(items):
    lines = []
    for x in items:
        part = [f'[{x["id"]}] Ort: {where(x)}', f'Art: {x.get("kind", "Text im Programm")}']
        if x.get('code_comment'):
            part.append(f'Zweck laut Programmcode: {x["code_comment"]}')
        if x.get('note'):
            part.append(f'Hinweis: {x["note"]}')
        part.append(f'Englisch: {x["en"]}')
        if x.get('de_now'):
            part.append(f'Bisher (duzt, nicht verbindlich): {x["de_now"]}')
        lines.append('\n'.join(part))
    body = {'model': 'kolibri', 'temperature': 0.3, 'max_tokens': 8000,
            'messages': [{'role': 'system', 'content': SYSTEM},
                         {'role': 'user', 'content': 'Übersetze diese Texte:\n\n' + '\n\n'.join(lines)}]}
    req = urllib.request.Request(URL, json.dumps(body).encode(), {'Content-Type': 'application/json'})
    d = json.load(urllib.request.urlopen(req, timeout=900))
    c = d['choices'][0]
    text = c['message'].get('content') or ''
    m = re.search(r'\[\s*\{.*\}\s*\]', text, re.S)
    if not m:
        raise ValueError(f'no JSON ({c.get("finish_reason")}): {text[:300]}')
    return json.loads(m.group(0))


def tokens(s):  # what must survive the translation
    return sorted(re.findall(r'%\d|%n|<[^>]+>|[▾▶●○⚙✂←→↑↓]', s))


def main():
    items = json.load(open(sys.argv[1]))
    out = {}
    try:
        out = {int(k): v for k, v in json.load(open(sys.argv[2])).items()}
    except (FileNotFoundError, ValueError):
        pass
    todo = [x for x in items if x['id'] not in out]
    for i in range(0, len(todo), BATCH):
        part = todo[i:i + BATCH]
        t0 = time.time()
        for attempt in range(3):
            try:
                got = ask(part)
                break
            except Exception as e:
                print(f'  batch {i // BATCH + 1}: {e}', flush=True)
                got = []
        byid = {g.get('id'): g.get('de', '') for g in got if isinstance(g, dict)}
        for x in part:
            de = byid.get(x['id'])
            if de is None:
                continue
            problems = []
            if tokens(de) != tokens(x['en']):
                problems.append('Platzhalter oder Zeichen fehlen')
            if re.search(r'\b(du|dein\w*|Sie|Ihr\w*|dich|dir)\b', de):
                problems.append('duzt oder siezt')
            out[x['id']] = {'de': de, 'problems': problems}
        json.dump(out, open(sys.argv[2], 'w'), ensure_ascii=False, indent=1)
        print(f'batch {i // BATCH + 1}/{(len(todo) + BATCH - 1) // BATCH}: {len(byid)} back in {time.time() - t0:.0f} s', flush=True)


main()
