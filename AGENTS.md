# AGENTS.md: working on Nachtzug

Instructions for AI coding agents, and for anyone, making changes to Nachtzug. Please read this before editing. The
[README](README.md) says what Nachtzug is and how to build it; its “For developers” section maps the code.

## What this project is

Nachtzug is a small music program for acid and Berlin School music, made for **beginners**: people who love music
but can’t play the keyboard and know no music theory. Every decision is weighed against that person first. It is
not a full DAW and isn’t meant to become one; a feature that makes Nachtzug harder to understand needs a very good
reason.

C++20, Qt 6 Widgets, SDL2 for sound, CMake. Linux, macOS and Windows. About 26,000 lines in `src/`.

## Promises to the person using it

These are what make Nachtzug friendly. Don’t break them.

1. **Every note fits.** Notes are stored as scale degrees, not pitches, so a track can only play notes of the song’s
   scale. The way out of the scale (Shift for “the right wrong notes”) is deliberate and marked as such (`alt`).
2. **Changes while playing land in time.** Anything that changes the music while the song plays goes through
   `App::onBar()`, which holds it until the next bar line. Use `App::edit()` only for changes that may happen at once.
3. **Everything can be undone.** Every change to the song is one undo step, named in plain words (`begin("New idea")`,
   `edit(f, "Clean up")`). Big changes pass `announce` so a note offers Undo. Never change `App::song` without
   `begin()` first.
4. **Nothing is lost.** Unsaved work has a spare copy (`src/autosave.*`). Songs are `.nachtzug` files: plain JSON
   with `"format": "nachtzug-1"`. **Every song saved by an earlier version must still open**: new fields need a
   default in `Song::fromJson`, and a saved field is never renamed or given a new meaning. If a change can’t avoid
   it, it needs a new format name and a conversion of the old one.
5. **Everything explains itself.** Every visible control has a tooltip in plain words: `--check-tooltips` must print
   `0 controls without a tooltip`.
6. **The music made with it belongs to the person who made it.** No sounds, presets or demo songs that come with
   strings attached: the sample library is synthesised by `tools/samplegen` and released as CC0.

## How the code fits together

- **The audio thread** (`src/engine.*`, SDL’s callback) plays an immutable `Song` snapshot. The GUI thread reaches
  it only through `setSong()`, `push()` and the atomics in `engine.h`. On the audio thread: no locks, no memory
  allocation, no Qt objects, no file access. If you need something new across the threads, add an atomic or an
  `EngineEvent`.
- **Sound generation** is in `src/dsp.h` (header only, no Qt). Keep it that way so `tools/samplegen` can use it too.
- **The song** (`src/model.*`) is a plain value; the pattern generators, the auto drummer and Clean up sit beside it
  and need no window, so the tests can run them.
- **The window** is split into one file per panel; `src/widgets.h` and the four files behind it hold the shared look
  (themes, screwed wells, knobs, the display). Use those instead of styling controls by hand, so all six colour themes
  keep working.

### A map of the code

Every `.cpp` file starts with a note: what it is for, **What’s here** (its main functions and where they are called
from) and **Changes in the song file**. Read the note before the file; a new file gets one in the same shape.

| File | What it holds | Window? |
|---|---|---|
| `dsp.h` | Sound generation: oscillators, the ladder filter, envelopes, voices, the drum machine, echo, reverb, limiter | no, and no Qt |
| `model.*` | The song, its tracks, clips and notes; scales, chords, the synth settings’ names and explanations | no |
| `songfile.cpp` | Saving and opening `.nachtzug` files, with the defaults for older songs | no |
| `sounds.cpp` | The presets, drum kits, rolled and mutated sounds | no |
| `ideas.cpp`, `drummer.cpp`, `cleanup.cpp`, `patterns.h` | The pattern generators, Vary, new tracks, the demo song and Arrange; the auto drummer; Clean up; their shared helpers | no |
| `engine.*` | The audio engine (the audio thread) and rendering to WAV, stems and the note log | no |
| `samples.*` | The built-in sample library and your own: reading WAV and AIFF, importing, recording, chopping | no |
| `mysounds.*` | Your own sounds as `.nzsound` files, stars and “used lately” | no |
| `midiio.*`, `midi.*`, `midifile.*` | MIDI on each system’s own service; notes, learned knobs and the clock; the MIDI file export | `midi.cpp` adds the knobs’ menus |
| `app.*` | The App: the one place the song is changed (undo, `onBar()`), the transport, recording, the loop grid’s launching, the sound card | no |
| `autosave.*` | The spare copy of unsaved work | asks at start-up |
| `main.cpp`, `cli.*` | Starting the program; the command line, `--render` and the pictures | |
| `selftest.*`, `guitest.*`, `parity.*` | The self-test, the GUI test and tooltip check, the parity record | |
| `mainwindow.*` | The window: top bar, menus, how the panels are arranged, the letter keys as an instrument, the guide | yes |
| `inspector.*` | The column beside the timeline: the selected track at a glance | yes |
| `editor.*` | The Notes tab: the note grid, the on-screen keys and chord pads, the keys around them | yes |
| `soundpanel.*` | The Sound tab: every knob (Complete) or a few (Simple), presets, the drums’ samples | yes |
| `mixer.*` | The Mixer tab: a strip per track and the master | yes |
| `timeline.*`, `loops.*` | The song timeline with its knob lines, and the loop grid | yes |
| `widgets.h`, `theme.cpp`, `drawing.cpp`, `panels.cpp`, `controls.cpp` | The look: themes and style sheet; painted parts and icons; wells and folding sections; knobs, switches, meters, the display | yes |
| `soundlibrary.*`, `savesounddialog.*` | The sound library (the card index) and “Save sound…” | yes |
| `exportdialog.*`, `settingsdialog.*`, `chopdialog.*`, `welcome.*` | Export; Settings and recording a sample; chopping a loop; the first-start window | yes |
| `lessons.*` | The guided lessons over the real window | yes |

### Short names

The same short names mean the same thing everywhere; don’t bring in new one-letter names for anything else.

| Name | What it holds |
|---|---|
| `deg` | a scale degree (0 is the root; `DEG_MIN` to `DEG_MAX` in the note grid), with `alt` the semitone off the scale |
| `tick` | a position in ticks: `PPQ` (96) to a quarter note, `BAR` (384) to a bar |
| `bar` | a bar number, from 0 |
| `s`, `t`, `c`, `n` | a `Song`, a `Track`, a `Clip`, a `Note` |
| `id` | a track’s or clip’s id, which stays the same while tracks move; an index into `song.tracks` is a `row` or `i` |
| `scene` | a column of the loop grid (on screen it is called a column) |
| `a`, `app` | the App (`a` as a constructor’s argument, `app` as the member) |
| `p` | a `QPainter` in paint code |
| `e` | the event in an event handler |
| `ppt` | pixels per tick, the zoom of the timeline and the note grid |
| `i`, `j`; `x`, `y` | counters; positions on screen |

### Invariants that are easy to break

- **A song change without `begin()`.** Every change to `App::song` is an undo step: `begin()` first, or `edit()` /
  `onBar()`, which call it. Then `touch()` (which `edit()` calls) sends the song to the engine and repaints.
- **A change while playing that bypasses `onBar()`.** It lands mid-bar and the music stumbles.
- **Allocating, locking or touching Qt on the audio thread.** Everything `Engine::process()` calls runs there. A
  `std::vector` that grows, a `QString`, a `qDebug()` or a mutex is enough to cause clicks.
- **A new song field without a default in `Song::fromJson`,** or a saved field renamed or given a new meaning: older
  songs stop opening, or open wrong.
- **A random choice outside the seeded generator.** The ideas take their randomness from the generator they are
  given; `std::random_device` or the App’s own generator in their place breaks parity and the tests.
- **Qt in `dsp.h`.** `tools/samplegen` builds it without Qt.
- **A control without a tooltip,** or a text without `tr()`.
- **Styling a control by hand** instead of with the look’s parts: it looks right in one theme and wrong in five.

### Known traps

- **Track ids and indexes.** `App::selTrack`, `EngineEvent::track`, `NoteEvent::track` and `Song::track(id)` use
  ids; the screen order and MIDI targets such as `"vol:2"` (the third track) use indexes into `song.tracks`.
  `Song::trackIndex()` turns an id into an index.
- **Other notes on other systems.** The C++ library’s random distributions differ (GCC on Linux and Windows, Clang
  on a Mac), so a seed gives other patterns on a Mac: tests check what holds for any pattern, never particular notes.
- **Timing checks on busy machines.** GitHub’s Mac runners stall; a check that waits a fixed time or sleeps between
  messages fails there. Where the code measures time, let the check hand it the times instead: the incoming clock
  check gives each tick its own (`Midi::incoming(…, when)`), so it hears the clock’s tempo however busy the machine.
- **The Mac’s Clang is stricter than GCC,** and the release builds stop on any warning: check new code with Clang and
  `-Wall -Wextra -Werror` before a release.
- **Qt 6.4 is the oldest Qt supported** (Debian 12, Ubuntu 24.04). A newer call, such as `QFont::setFeature` (6.7),
  needs a `QT_VERSION_CHECK` around it.
- **No SVG in the downloads.** The packages carry no SVG plugin; small pictures in the style sheet are drawn as PNG,
  and the self-test checks they load.
- **The samples are an ordinary Qt resource.** `BIG_RESOURCES` breaks link-time optimised builds (openSUSE, Fedora)
  with “No data signature found”.
- **Windows (MSYS2):** a path “Nachtzug” also means `Nachtzug.exe`, so emptying a folder called Nachtzug beside the
  program deletes the program; `tools/build-windows.sh` packs into `build-windows/package/Nachtzug` for that reason.
- **macOS (Homebrew):** `sdl2` is sdl2-compat, which loads SDL3 at start; `macdeployqt` can’t see that, so
  `tools/build-macos.sh` copies SDL3 into the app by hand.
- **`nachtzug --version` doesn’t exist;** it opens the window and waits. Use `QT_QPA_PLATFORM=offscreen` for any run
  without a screen.

## Building and testing

Build in one folder per source tree and leave it there. A full rebuild takes long, so don’t delete the build folder
or switch branches underneath it without a reason.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Before every commit, all three must pass:

```sh
QT_QPA_PLATFORM=offscreen SDL_AUDIODRIVER=dummy ./build/nachtzug --selftest        # self-test passed
QT_QPA_PLATFORM=offscreen SDL_AUDIODRIVER=dummy ./build/nachtzug --guitest         # gui test passed
QT_QPA_PLATFORM=offscreen SDL_AUDIODRIVER=dummy ./build/nachtzug --check-tooltips  # 0 controls without a tooltip
```

For a change that shouldn’t change what Nachtzug does, also run `tools/parity.sh` (see “Code style”).

The idea generators make other choices on other systems from the same seed: the C++ library's random distributions
differ (GCC's on Linux and Windows, Clang's on a Mac), so the demo song itself has other notes on a Mac. A check must
hold for any pattern, never expect particular notes; `tools/parity.sh` compares one system with itself.

The self-test lives in `src/selftest.cpp`: one function per feature, run in order, each a list of
`check(condition, "what should be true")` lines. A new feature gets a function of its own at the end of `selfTest()`,
with its checks written as sentences about what should be true. The GUI test and the tooltip check are in
`src/guitest.cpp`.

The packaged builds (`tools/build-appimage.sh`, `tools/build-macos.sh`, `tools/build-windows.sh`) each run the
self-test twice: once in the build folder, and once on the finished package with nothing else around it. On GitHub
all three are built for every version tag (`.github/workflows/release.yml`); **Run workflow** on the Actions page
builds them without releasing anything.

## Code style

- **Tabs for indentation**, spaces only to line up a continued line. **120 characters** a line.
- **One statement per line.** A short `if (x) return;` (or `continue;`, `break;`) may stay on one line.
- **clang-format keeps it so**, with the settings in `.clang-format`: run `tools/format.sh` to check and
  `tools/format.sh --write` to fix, before every commit. It changes only spaces and line breaks. A table that reads
  better as written (the colour themes, the presets) sits between `// clang-format off` and `// clang-format on`.
- **No warnings.** The release builds stop on one (`NACHTZUG_WERROR`); fix it rather than silence it.
- **A tidy-up changes nothing**, and `tools/parity.sh` proves it: `tools/parity.sh baseline` before, `tools/parity.sh
  check` after. It compares what the song code does with fixed seeds (`src/parity.cpp`), the songs rendered to WAV and
  54 screenshots, byte for byte. A change to the sound or the look is expected to differ; anything else must not.

## Words

The words are as much a part of Nachtzug as the code.

- **Plain words for beginners.** “Cutoff” is fine because it’s on every synthesiser, but say what it does. Avoid jargon
  where a plain word works, and never make the reader feel stupid. Warm and encouraging, not cute.
- **British English** everywhere in English: colour, synthesiser, organise, centre.
- **Proper typography** in everything a person reads (UI, tooltips, manuals, README, the website): curly quotes
  (‘ ’ “ ”, and „ “ in German), apostrophes (’), en dashes for ranges and breaks (–), … for an ellipsis, × for
  dimensions and multiplication, ♯ and ♭ in note names, and a narrow no-break space (U+202F) in “z. B.” and between
  a number and its unit.
- **German**: impersonal everywhere, in the program, the manual and the website: passive voice, “man” or an
  infinitive, never “du” or “Sie”, like a classic operation manual.
- **Every text in the code is translatable**: `tr("…")`, or `QCoreApplication::translate()` outside a QObject. After
  changing texts, run `cmake --build build --target update_translations` and translate the new entries in
  `i18n/nachtzug_de.ts`; leave no entry unfinished.

## Looks

**Read [docs/design-language.md](docs/design-language.md) before you change or add anything that shows on screen.**
It sets out the philosophy, the colours of every theme, the typefaces, radii, spacing and every component with its
object name and states, with a checklist for new work; [docs/design-language.png](docs/design-language.png) shows it
on one page. Build new screens from the parts it lists, and any change to the look starts as mockups for the owner
to choose from.

- **Type**: the program’s own interface is set in **Nachtzug Grotesk** (in the Bakelite look; see the design
  language’s open questions for the other themes). The manual, the website and the logo use **Nachtzug Antiqua**,
  with the Initialen for drop caps. The name “Nachtzug” is set in the Grotesk with its tz ligature and never
  letter-spaced, as spacing switches ligatures off. The fonts are built by the scripts in
  `tools/fonts/` and are under the SIL Open Font License; edit the scripts, not the font files.
- **Look and feel**: a 1970s and ’80s instrument: Bakelite, screwed panels, enamel, warm light. Six colour themes;
  check a change in a light and a dark one (`--screenshot out.png --theme NAME`).
- **Pictures that show the program** are taken by scripts, never by hand, so they can be taken again when it
  changes: `tools/readme_shots.py` (the README), `tools/site_shots.py` (the website), `docs/manual/shots.py en|de`
  (the manuals). If you change how something looks, retake the pictures that show it.

## Documentation

When a change shows on screen, update what describes it, in both languages:

| What | Where | How it’s made |
|---|---|---|
| The operation manuals | `docs/manual/manual.html`, `manual.de.html` | `python3 docs/manual/build.py` and `build.py de` (WeasyPrint) write `docs/Nachtzug-Manual.pdf` and `docs/Nachtzug-Handbuch.pdf`; then `tools/site_covers.sh` |
| The built-in guide and lessons | `src/mainwindow.cpp`, `src/lessons.cpp` | translated with the rest |
| The README and website | `README.md`, `site/` | see `site/README.md` |
| What changed | `CHANGELOG.md` | one section per version, in plain words |
| What comes next | `docs/roadmap.md`, `docs/polish-list.md` (private repository only) | the larger steps in order; the small fixes the owner noticed |

## Licences

The code is GPL-3.0-or-later, the samples and demo songs CC0, the manuals, texts and pictures CC BY-SA 4.0, the
fonts OFL ([LICENSING.md](LICENSING.md)). Add nothing that doesn’t fit these. A new library that ends up in a
download goes into [docs/THIRD-PARTY.md](docs/THIRD-PARTY.md) with its licence.

## Commits

Small, focused commits whose message says what changed and why, in plain words. Run the three tests first.
Don’t commit build folders, `dist/` or generated files that aren’t already in git.
