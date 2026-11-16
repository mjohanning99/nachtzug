# Roadmap

What comes next for Nachtzug, in order. Small fixes to the look and feel are in [polish-list.md](polish-list.md);
this page is for the larger steps. Anything that shows on screen starts as mockups to choose from.

## 1. Finish the refactor (now)

The plan is [refactor-plan.md](refactor-plan.md); steps 0 to 4 are done (the parity check, the code laid out by
clang-format with no warnings, main.cpp, model.cpp and widgets.cpp split). Still to do, each proved by
`tools/parity.sh`:

1. Cut the long builders and paint functions into named pieces.
2. A note at the top of every file, the short names, and a code map, invariants and known traps in AGENTS.md.
3. Vale for the words, if wanted.

## 2. After 0.2.0

- On the owner's Mac: check whether the Mac-only edges (a white edge round a level meter, light lines by the knobs, a
  frame not level with a drop-down) are gone; they do not show on GitHub's Mac machines.
- On GitHub, by hand: the new social preview (Settings → Social preview, `assets/logo/social-preview.png`) and the
  repository's description.

## 2a. Chords on every note, and a fuller sound (next)

**"Each note plays" sounds bad (open).** With a chord shape such as "Seventh chord", "Follow chords" on, octave +1
and the arpeggio off, the owner hears it as very discordant, the other shapes too. A first guess, building the chord
from the song chord's tones round the note (`Song::chordVoicing`), sounded the same to the owner and was taken out
again (16 November 2026). The cause is still to find; worth checking next: how the chord's voices meet the patch
(Voices: Mono with a chord, glide, unison and detune, FM, drive on four notes at once), the chord's register and
spacing (four notes within one octave, an octave up), the voices' levels (a chord four times as loud into the
drive), and listening to one held note with each shape on a plain saw sound before anything else.

**A fuller sound (ideas, to choose; screens start as mockups).** ★ marks the assistant's picks.

- ★ An equaliser per track with a picture: low cut, low, mid (with its frequency) and high, as a curve to drag in the
  Sound tab's effects; the mixer strip gets the three knobs (this is "A finished sound" below, made concrete).
- ★ On the Simple page only a "Tone" knob (darker to brighter) and a "Low cut" switch; the full EQ on the Complete page.
- An EQ and the "glue" compressor on the whole song, before the limiter.
- ★ Filter: high-pass, band-pass and notch besides the low-pass types, and a release of its own (today A, D, S only).
- ★ Echo: time in note values, feedback, tone, ping-pong; Berlin School lives on its echo.
- ★ LFO: in time with the tempo, volume (tremolo) and pan as targets, a slow fade-in for delayed vibrato.
- Oscillators: hard sync and ring modulation, a fine tune for oscillator 2 apart from the detune.
- Velocity: how much a harder note opens the filter and raises the volume.
- Reverb: size and damping.
- ★ Saving sounds (agreed with the owner, 16 November 2026; mockups made, the library's look still to choose):
  - The preset drop-down stays short: starred sounds and the ones used lately on top, then the built-in groups as
    now, and "All sounds…" at the bottom, which opens the sound library. Beside it a key "Save sound…": name, group
    (the track's suggested), optional words to find it by, a star to show it in the drop-down.
  - The sound library, a window of its own: groups, a search, listening first (a short phrase on the track, nothing
    changed until "Use"), star, rename, delete, share as a file and bring one in (dropping a file on it too).
    Three looks drawn: A card index (groups left, cards, the chosen sound right), B ticket archive (tickets coloured
    by group, filters on top), C plain list (one line per sound, sortable).
    **Chosen: A, the card index.** Built in the program's own style, not the mockup's: no pills (words to find a sound
    by as small engraved captions or soft text), the listen key a normal key with a lip and the ▶ symbol like the
    transport keys (not round), and otherwise only the program's existing pieces: Bakelite keys, fields, switches,
    captions, screwed panels.
  - Building it, step by step: **1. done** (16 November 2026): `src/mysounds.cpp` saves, reads, renames, moves,
    deletes and brings in sounds, stars them and remembers the ones used lately; songs and sound files share
    `patchToJson`/`patchFromJson`; a self-test check. **2. done** (16 November 2026): "Save sound…" (a card
    with a plus) beside "New sound" in the inspector and on the Complete page, and its dialog (`savesounddialog.cpp`:
    name, group as joined keys, words, a star switch; `--dialog savesound` for pictures). **3. done:** the drop-down
    lists only the starred sounds and the ones used lately under "My sounds", and "All sounds…" at the bottom (the
    tall separators between the groups are gone; the headings mark them). **4. done** (`soundlibrary.cpp`): the card
    index: shelves as keys on the left (My sounds, Starred, the four groups, Ready-made), a search and the cards in
    the middle (a normal key with ▶ to listen first, the words as soft text), the chosen sound on the right (use,
    star, rename, share as a file, delete), "Bring in a file…" and files dropped on the window; `--dialog library`.
    **5. done:** a manual section "Keeping a sound of your own"
    (5.2, both languages, with a picture of the library taken by `shots.py` from four sample sounds), index entries,
    a sentence in the help's guide, and the arrows, Space and Return in the card list. Maybe later: the sound's
    picture (the filter curve) on the library's right side. The owner still has to try it all with a mouse.
  - One small file per sound in the user's data folder (for example `~/.local/share/Nachtzug/sounds/<group>/`),
    outside the song, so every song sees them and they can be backed up and shared.
- A mastering tool (the owner's wish, for later): the finished song made louder and even, before export. A first
  idea: a "Master" page with a few plain controls (Loudness to a target such as streaming level, Warmth, Width,
  Punch, a low and high shelf), a meter of loudness and peaks, and before/after listening; built on the EQ and the
  glue compressor above.

## 2b. The German texts (later)

The German program is impersonal now, like the manual: no "du" or "Sie". The welcome window and the Settings dialog
were reworked with the local model Kolibri (`tools/kolibri/`); the rest was made impersonal in a quick first pass
without it. The owner would like Kolibri to polish the rest later. When it is done:

- Run the rest the same way, with Kolibri's version as the default and only clear mistakes fixed; show the owner the
  rows that were changed, not all of them.
- Put the owner's own pitch (see the website's opening) and twenty or thirty approved pairs from the pilot into the
  prompt as examples, so Kolibri follows the owner's voice and the program's names, not only the rules.
- Decide the welcome window's two choices: keep "Ich bin neu dabei" / "Ich kenne mich mit Synthesizern aus", or
  Kolibri's "Neu im Programm" / "Synthesizer-Kenntnisse" (then the manual changes too).
- Bring the German manual in line with the program's new words (for example "On-Screen-Tasten").
- Maybe later: a LoRA for Kolibri, trained on approved pairs, if this style is wanted again and again.

## 3. Waiting on a decision

- **Samples**: today a sample only goes on a drum pad, which nothing in the timeline or the loop grid leads to.
  A drop a WAV or AIFF on the timeline or the loop grid; B "A sample…" in the Add track menu; C only help (a guide
  section and a lesson). Assistant's pick: A and B together.

## 4. Towards a proper program

Not a DAW, but what similar beginner programs have and Nachtzug does not yet. In the order suggested; ★ marks the
assistant's picks.

1. ★ **Recent songs** (File → Open recent) and a start window with a few **starter songs**, one per style.
2. ★ **Samples on the timeline** (the decision above): what Music Maker-style programs are built round.
3. ★ **MP3 export** (for the phone, the car, a cassette), and reading MP3, FLAC and OGG samples, which Nachtzug
   refuses today.
4. ★ **A long journey**: "Continue for … minutes", which lets the patterns evolve slowly over a long piece (new ideas,
   knob lines, parts coming and going), for the thirty-minute song Nachtzug was made for.
5. ★ **A finished sound**: three EQ knobs per track in the mixer (low, mid, high) and a gentle "glue" compressor on
   the whole song.
6. **Named parts** on the timeline (Intro, Build-up, Break…), which Arrange for me already makes but doesn't show.
7. **Tempo and key changes** over the song, drawn like knob lines.
8. **A count-in** before recording, and **recording the input** (a voice, a guitar) into the song; today it is heard
   but not kept.
9. **Softer and harder notes** than accent alone (three levels), and a **looseness** knob per track for a played feel.
10. **MIDI file import** (export exists).
11. Other time signatures (3/4, 6/8, 7/8): least needed for acid and Berlin School.

## Later, maybe

- The faders could feel more real, "liquid": mockups were made (a studio fader, a glass tube, soft rubber) and the
  owner chose to leave them as they are for now.
