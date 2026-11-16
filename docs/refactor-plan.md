# Refactoring plan

Written 15 November 2026, after version 0.1.1. The decisions are near the end.

## Why

Nachtzug works, and its outline is sound: one file per panel, a clean line between the audio thread and the
window, a comment at the top of most files. But a few parts have grown too big to read comfortably, and someone
new to the code (a person or a model) has to hold too much in their head at once. The aim is code that is easy to
find your way around and safe to change, **without changing anything Nachtzug does**.

| Where | What makes it hard |
|---|---|
| `src/main.cpp` | `selfTest()` is 861 lines and `main()` 650; the command line, the GUI test, screenshots and the tooltip check all live inside `main()` |
| `src/model.cpp` (78 KB) | the song, saving and loading, the idea generators, the auto drummer and Clean up in one file |
| `src/widgets.cpp` | the colour themes mixed in with knobs, switches and meters |
| long builders | `EditorPanel`’s constructor (248 lines), `Timeline::paintEvent` (246), `MainWindow`’s constructor and top bar (190, 180), `MixerPanel::rebuild` (161), `LoopGrid::paintEvent` (159) |
| line length | lines up to 352 characters, about 300 past 160 |
| layout | several statements on one line in places, e.g. `if (tab) tab->click(); QApplication::processEvents();` (`main.cpp:1301`), which the compiler warns about |
| file notes | the headers have a one-line note, but 21 of the 22 `.cpp` files start straight with `#include`, and none says what is in it or where it is called from |
| warnings | `-Wall -Wextra` are on, but nobody looks at them; the Windows build shows several |

## Ideas taken from Amulets of Anubis

Amulets’ AGENTS.md sets out how its code should look and how changes are proved safe. Most of it carries over:

| In Amulets | In Nachtzug |
|---|---|
| Tabs for indentation, spaces only to line up a continued line; `.editorconfig` says so to editors | the same, with an `.editorconfig` (decision D1) |
| Plain style: one statement per line, braces on the same line, lines up to about 110 characters, a blank line between functions | the same (D2, D4) |
| **Prettier keeps it so**, with `engine/tools/format.sh` to check or fix | **clang-format** keeps it so, with a `.clang-format` and `tools/format.sh` (check) / `--write` (fix); the release build checks it |
| **Every file starts with a note**: what it is for, “What’s here” (its main functions and where they are called from), “Changes in the save” | every `.cpp` and `.h` starts with a note: what it is for, **What’s here**, and **Changes in the song file** (what it reads or writes in a `.nachtzug` file, or “none”) |
| **Names say what they hold**; the same short names mean the same thing everywhere (`sq`, `row`, `col`, `tile`, `pen`) | a short list in AGENTS.md: `deg` a scale degree, `tick` a position in ticks, `bar`, `clip`, `trk` a track, `s` the song, `p` a painter, `a` the App; `i`, `j` for counters, `x`, `y` for positions; no new one-letter names for anything else |
| `01-core.js` is kept browser-free so the simulators can run it | `dsp.h` stays free of Qt (`tools/samplegen` uses it); the idea generators and the drummer stay free of widgets, so the tests can run them without a window |
| **A tidy-up is proved by playing the same seeded games before and after** (`parity.js`) | `tools/parity.sh`: the same songs rendered to WAV and the same screens photographed, before and after, compared byte for byte and pixel for pixel |
| A map of the code in AGENTS.md (file, what it holds, browser-free?) | the same table in AGENTS.md (file, what it holds, Qt-free?), replacing the README’s shorter one |
| **Invariants that are easy to break** and **Known traps** sections | the same in AGENTS.md: e.g. a song change without `begin()`, changes while playing that bypass `onBar()`, allocating on the audio thread, a new song field without a default; MSYS2 taking `Nachtzug` for `Nachtzug.exe`, Homebrew’s SDL2 needing SDL3 beside it |
| **Watch the engine’s size**: say how many lines a change added | each step of this plan reports the lines before and after; later changes say what they added |
| Vale checks the words (British spelling, machine-written habits such as em dashes, “not X, but Y”, “delve”) | optional, for the README, manuals and the program’s texts (D3) |

Not carried over, because Nachtzug is a different kind of program: the content/code seam, the single HTML file,
balance simulators, the save-code rules.

## Ground rules

1. **Nothing changes for the person using Nachtzug.** No new features, no fixes slipped in, no changed sounds. A
   bug found on the way is written down and fixed afterwards, in a commit of its own.
2. **Every step is proved** before it is committed:
   - `tools/parity.sh` shows identical WAVs and identical screenshots;
   - `--selftest`, `--guitest` and `--check-tooltips` pass;
   - it builds without new warnings on Linux, and on macOS and Windows through the scratch repository.
3. **One step, one commit**, with the lines before and after in its message. The formatting change is a commit on
   its own, so the real changes stay readable in the history.
4. **No new dependencies.** C++20, Qt 6, SDL2, CMake, as now. clang-format is a tool for writing the code, not
   for building it.

## The steps

### 0. The safety net (done)

Done on 15 November 2026: `src/parity.cpp` (`nachtzug --parity folder`) and `tools/parity.sh`. Screenshot runs
now use a fixed seed, so the loop grid and Arrange fill the same way every time. Two runs give 60 identical files.
What was planned:


`tools/parity.sh baseline` and `tools/parity.sh check`. It renders the demo song and the example songs to WAV,
each also arranged short, medium and long (`--render … --arrange 0|1|2`). It also takes screenshots of every tab,
two themes (one light, one dark), every dialog, the Simple page, the guide and a lesson step, in English and
German, at a fixed window size. Before anything else: check that the same song renders the same bytes twice in a
row. If something is random (an unseeded generator, a time stamp), it gets a fixed seed in the test runs first.

### 1. Style and warnings (done)

Done on 15 November 2026. `.clang-format`, `.editorconfig` and `tools/format.sh`; the code reformatted in one commit
(15,994 lines became 23,167; parity identical); the one warning (an unused function) removed; the release builds stop
on a warning (`NACHTZUG_WERROR`). The formatting is checked here rather than on GitHub, as Debian’s older
clang-format lays out a few things differently. What was planned:


- `.editorconfig` and `.clang-format` as decided (D1, D2, D4); `tools/format.sh`.
- One commit that only reformats `src/` and `tools/samplegen/`. Parity proves it changed nothing.
- `-Wall -Wextra` are already on, but nobody looks: fix every warning (each fix checked by parity), so a new one
  stands out later.
- The release workflow checks the formatting and fails on a warning.

### 2. Split `main.cpp` (done)

Done on 15 November 2026. `main.cpp` went from 2,608 lines to 285: `cli.cpp` (the options, `--render` and the
pictures, 327 lines), `selftest.cpp` (the session and 20 feature sections, 1,647 lines) and `guitest.cpp` (608
lines). Each file keeps only the includes it uses. The compiler caught one hidden clash on the way: a local
`QJsonObject o` in the screenshot code would have hidden the options. What was planned:


| New file | What moves there |
|---|---|
| `src/selftest.cpp` | the self-test, as one function per feature (playing, editing, undo, the loop grid, recording, files, export, samples, MIDI…), each a list of `check()` lines; `selfTest()` just calls them in order |
| `src/guitest.cpp` | the GUI test and the tooltip check |
| `src/cli.cpp` | reading the command line, `--render`, `--screenshot` and its options |
| `src/main.cpp` | starting the program: a few dozen lines |

### 3. Split `model.cpp` (done)

Done on 15 November 2026. `model.cpp` went from 1,873 lines to 406 (the song and its tables): `songfile.cpp`
(saving and opening, 340), `sounds.cpp` (presets, kits, rolled sounds, 262), `ideas.cpp` (generators, Vary, new tracks,
the demo song, Arrange, 558), `drummer.cpp` (184) and `cleanup.cpp` (183); the random helpers and `add()` they share
are in `patterns.h`. Parity identical. What was planned:


| New file | What moves there |
|---|---|
| `src/model.cpp` | the song, tracks, clips, notes, scales and chords |
| `src/songfile.cpp` | saving and loading `.nachtzug` files (`toJson`, `fromJson`), with the defaults for old songs in one place |
| `src/ideas.cpp` | the pattern generators (acid, Berlin School, chords, melodies, beats), Vary, and the auto drummer |
| `src/cleanup.cpp` | Clean up |
| `src/presets.cpp` | the presets and sound rolling, if they are big enough to deserve it |

### 4. Split `widgets.cpp` (done)

Done on 15 November 2026, in four rather than two, as the themes alone were 1,140 lines: `theme.cpp` (colours,
fonts, palette, style sheet, 442), `drawing.cpp` (wells, displays, keys, icons, the logo, 539), `panels.cpp` (the two
program-wide filters: wells and rounded lists, folding sections; tidyForm, 262) and `controls.cpp` (Knob, Lcd,
VuMeter, Switch, Toast, FlowLayout, Emblem, ElideLabel, 703). `widgets.h` stays the one header; `theme::mix` became
public. Parity identical. What was planned:


`src/theme.cpp` (the six colour themes, the screwed wells, glass, the logo) and `src/controls.cpp` (knobs,
switches, meters, folding sections, the display). `widgets.h` stays as the one header to include, so no other file
has to change.

### 5. The long functions

Each long constructor and paint function is cut into named pieces, one per part of the screen
(`buildTransport()`, `paintClips()`, `paintPlayhead()`…), in the order they appear. Nothing moves on screen;
parity’s screenshots prove it.

### 6. Notes, names and the map

- A note at the top of every file: what it is for, **What’s here**, **Changes in the song file**.
- The list of names in AGENTS.md; names that don’t follow it are renamed where they’re confusing (not everywhere).
- AGENTS.md gets the code map, **Invariants that are easy to break** and **Known traps**. The README’s project
  layout table points there instead of keeping its own.

### 7. (If wanted) Vale for the words

A Vale setup for `README.md`, `docs/`, the manuals and the program’s English texts (pulled from
`i18n/nachtzug_de.ts`): British spelling, and the habits of machine-written prose. It only points things out.

## What stays as it is

- **`Engine::process`**, the 271-line audio loop. It is long, but it runs in real time, and splitting it can cost
  speed or cause clicks in ways a byte comparison won’t explain. Only with listening tests, later, if ever.
- **Undo by keeping a copy of the whole song** per step. Simple, and fast enough at this size.
- **The overall design**: the App, the engine, the panels. Nothing is rewritten; things are moved, cut up and
  named.

## Decisions

Made on 15 November 2026:

- **D1 tabs** for indentation, spaces only to line up a continued line.
- **D2 120 characters** a line.
- **D4 one statement per line**, except that a short `if (x) return;` (or `continue;`, `break;`) may stay on one
  line: C++ can be a little more verbose.
- **D3 Vale**: open; decided when step 7 comes.

## How long

Roughly a session for steps 0 and 1, one each for steps 2 and 3, and one for 4 to 6 together. Every step ends
with a commit in the private repository; the public repository gets it all with the next version.
