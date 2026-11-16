#!/usr/bin/env python3
# Collects the strings of some parts of Nachtzug for Kolibri: each with its English, the current German, where it sits
# in the program (the code around it), what kind of text it is, and the comment beside it in the code.
# Usage: collect.py CONTEXT[,CONTEXT…] > batch.json
import html, json, re, sys

ROOT = '/home/hex/Projects/nachtzug/'
FILES = {'SettingsDialog': ['src/settingsdialog.cpp'], 'WelcomeDialog': ['src/welcome.cpp'],
         'MainWindow': ['src/mainwindow.cpp'], 'EditorPanel': ['src/editor.cpp'], 'NoteEditor': ['src/editor.cpp'],
         'SoundPanel': ['src/soundpanel.cpp'], 'MixerPanel': ['src/mixer.cpp'], 'Timeline': ['src/timeline.cpp'],
         'LoopGrid': ['src/loops.cpp'], 'ExportDialog': ['src/exportdialog.cpp'], 'ChopDialog': ['src/chopdialog.cpp'],
         'Inspector': ['src/inspector.cpp'], 'LessonOverlay': ['src/lessons.cpp'], 'App': ['src/app.cpp'],
         'Lcd': ['src/controls.cpp'], 'RecordDialog': ['src/settingsdialog.cpp', 'src/samples.cpp'],
         'Autosave': ['src/autosave.cpp', 'src/main.cpp'], 'KeyPads': ['src/editor.cpp'], 'Knob': ['src/controls.cpp'],
         'Fader': ['src/mixer.cpp'], 'VuMeter': ['src/controls.cpp'], 'ChopView': ['src/chopdialog.cpp'],
         'Midi': ['src/midi.cpp'], 'samples': ['src/samples.cpp', 'src/sounds.cpp'],
         'Model': ['src/model.cpp', 'src/ideas.cpp', 'src/sounds.cpp', 'src/inspector.cpp', 'src/mixer.cpp',
                   'src/soundpanel.cpp', 'src/editor.cpp', 'src/theme.cpp', 'src/panels.cpp']}

# what kind of text it is, from the call it sits in
KINDS = [(r'setToolTip|setStatusTip', 'Tooltip (erscheint, wenn die Maus darauf ruht)'),
         (r'setWindowTitle', 'Fenstertitel'), (r'addTab', 'Name eines Reiters'),
         (r'addRow\(\s*tr', 'Beschriftung links neben einem Feld im Formular'),
         (r'new Switch', 'Beschriftung eines Schalters (an/aus)'),
         (r'hintLabel', 'Hinweis in kleiner Schrift unter einem Feld'),
         (r'addItem|setItemText', 'Eintrag in einer Ausklappliste'),
         (r'QPushButton|button\(|plain\(', 'Beschriftung einer Taste'),
         (r'QMessageBox|warning|information|question', 'Meldung in einem Dialogfenster'),
         (r'QGroupBox|group\(', 'Überschrift eines Abschnitts'),
         (r'setCard', 'Auswahlkarte: Überschrift oder Text darunter'),
         (r'setText', 'Text auf dem Bildschirm (Beschriftung oder Taste)')]


def cpp_literal(s):
    return s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n')


def main():
    contexts = sys.argv[1].split(',')
    ts = open(ROOT + 'i18n/nachtzug_de.ts').read()
    out = []
    for name, body in re.findall(r'<context>\s*<name>(.*?)</name>(.*?)</context>', ts, re.S):
        if name not in contexts:
            continue
        code = {f: open(ROOT + f).read().split('\n') for f in FILES.get(name, [])}
        for msg in re.findall(r'<message[^>]*>(.*?)</message>', body, re.S):
            src = html.unescape(re.search(r'<source>(.*?)</source>', msg, re.S).group(1))
            com = re.search(r'<comment>(.*?)</comment>', msg, re.S)
            de = re.search(r'<translation[^>]*>(.*?)</translation>', msg, re.S)
            item = {'id': len(out) + 1, 'context': name, 'en': src, 'note': html.unescape(com.group(1)) if com else '',
                    'de_now': html.unescape(de.group(1)) if de else ''}
            # find it in the code by the start of its literal (long strings are split over lines)
            needle = cpp_literal(src)[:38]
            for f, lines in code.items():
                for i, line in enumerate(lines):
                    if needle in line:
                        lo, hi = max(0, i - 3), min(len(lines), i + 3)
                        item['code'] = '\n'.join(l.strip() for l in lines[lo:hi])
                        here = '\n'.join(lines[max(0, i - 2):i + 1])
                        item['kind'] = next((k for pat, k in KINDS if re.search(pat, here)), 'Text im Programm')
                        # the function it is in: which page or part of the window
                        for j in range(i, -1, -1):
                            m = re.match(r'^\S.*?(\w+)::(\w+)\(', lines[j])
                            if m:
                                item['where'] = f'{m.group(1)}::{m.group(2)}'
                                break
                        # the comment just above, if any
                        cm = [lines[j].strip()[2:].strip() for j in range(max(0, i - 3), i) if lines[j].strip().startswith('//')]
                        if cm:
                            item['code_comment'] = ' '.join(cm)
                        break
                if 'code' in item:
                    break
            out.append(item)
    json.dump(out, sys.stdout, ensure_ascii=False, indent=1)


main()
