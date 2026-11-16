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
