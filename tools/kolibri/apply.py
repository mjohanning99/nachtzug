#!/usr/bin/env python3
# Writes the chosen German into the translation file: Kolibri's, or the fix where there is one (null: keep the current).
# Usage: apply.py batch.json out.json fixes.json
import html, json, re, sys
TS = '/home/hex/Projects/nachtzug/i18n/nachtzug_de.ts'
items = json.load(open(sys.argv[1]))
out = {int(k): v for k, v in json.load(open(sys.argv[2])).items()}
fixes = {int(k): v for k, v in json.load(open(sys.argv[3])).items()}
ts = open(TS).read()
def esc(s):
    return html.escape(s, quote=False).replace('"', '&quot;').replace("'", '&apos;')
changed = 0
for x in items:
    if x['id'] in fixes:
        new = fixes[x['id']]
        if new is None:
            continue
    elif x['id'] in out:
        new = out[x['id']]['de']
    else:
        continue
    # the message in its context, found by its source text
    cm = re.search(r'(<context>\s*<name>' + re.escape(x['context']) + r'</name>)(.*?)(</context>)', ts, re.S)
    body = cm.group(2)
    srcs = [esc(x['en']), html.escape(x['en'], quote=False)]
    done = False
    for src in srcs:
        pat = re.compile(r'(<source>' + re.escape(src) + r'</source>\s*(?:<comment>.*?</comment>\s*)?)<translation[^>]*>.*?</translation>', re.S)
        if pat.search(body):
            body, n = pat.subn(lambda m: m.group(1) + '<translation>' + esc(new) + '</translation>', body, count=1)
            done = True
            break
    if not done:
        print('NOT FOUND', x['id'], x['en'][:60])
        continue
    ts = ts[:cm.start(2)] + body + ts[cm.end(2):]
    changed += 1
open(TS, 'w').write(ts)
print('written', changed)
