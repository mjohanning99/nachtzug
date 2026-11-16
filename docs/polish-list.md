# Polish list

What the owner noticed while using Nachtzug, from 15 November 2026 on, and where each point stands. Newest
decisions first within each part. Anything that changes the look starts as mockups to choose from.

## Done

- Chosen from mockups and built: tabs like a card index (the chosen one runs into its page); dividers with a raised
  handle like a drawer pull, amber under the mouse; one display colour for the transport's display, the tempo, key and
  scale boxes and the meters; folded sections as long keys with their summary; the undo note as an ivory plate with a
  real Undo key; symbols with short words on the keys above the timeline (Copy, Loop, Cut, scissors, Delete,
  Arrange), the "Selected clip" and "Hover over anything for help" labels gone.
- Record, when on, showed a red dot on the red key: the dot was always drawn red. Now it is cream on the red key.
- Drop-down lists and menus open with rounded corners, like the boxes and keys they come from, and a list's items
  light up softly as the menus' do (the current one looked like a little text field).
- No more tick boxes: Follow chords, the drummer's two, and those in Settings, Arrange and Chop are switches like
  Loop, Click and In time, with their name beside them. Every switch now slides across when switched.
- How notes are played is laid out like the drummer's settings: labels in one column (plain, as the drummer's), the
  boxes in another, each with its own label (Arpeggio speed and range too). In the inspector both sections share
  one label width, so their boxes line up.
- Every form is laid out the same on every system (on a Mac the labels would have been flush right and the boxes
  narrow); the settings window's labels are flush left now, as everywhere else.
- The Mixer tab has an icon: three faders.
- The hints beside Bars, Loop and Chords, and Cells, Chords and the column keys in the loop grid, are gone: short
  labels, and the tooltips say the rest (the loop grid's labels got tooltips of their own).
- The coloured edge under each mixer strip's name follows the plate's rounded corners.
- The mixer strips: the selected strip's ring was drawn outside the strip and cut off to a stray orange line (now a
  whole ring inside the edge); Mute and Solo covered the lower screws (now between them, with slimmer padding,
  "Stumm" fits); in the flat looks the selected strip's thicker border moved its keys up a pixel.
- The logo at the top left was too big: now the square icon, smaller, with "Nachtzug" under it (option A).
- The fader handles, drop-down arrows and divider grips were missing in every download (SVG pictures, no SVG
  plugin): now drawn as PNG, and the self-test checks it.
- Tap tempo showed no new tempo (the tempo box held the keyboard from the start).
- Folding a section of the inspector made it wider than the window.
- The closed inspector's tab: a grey block at its foot in the panel looks; in the flat looks the whole window fell
  apart.
- The timeline's clips in the loop grid's colours; no plate behind the clip names.
- "Timeline on top" unticked: beside the tabs in Song and Loops alike.
- Clean up puts every note on the grid you chose.
- The app icon's rim thinner (16 of 512); the right size in the Mac Dock.
- The drop-downs and number boxes in Bakelite had a thicker shaded top border than their sides, which met the rounded
  corners in a step: on a Retina screen a frame that was not level with the box. Now one even border, with the shade
  painted inside it.
- The note grid opened on its top rows, empty for the bass and the sequence: it centred on the notes before it had
  its height. Now it waits for its real size (and for its tab to be shown), and stays centred through resizes until
  you scroll it yourself. The GUI test checks it.
- Scroll bars are hidden in the window (the wheel and the trackpad scroll as before, Shift + wheel sideways in the
  notes, the timeline and the loops); Settings → General → Show scroll bars brings them back. Dialogs keep theirs.
- Keys with symbols, chosen from mockups (the owner took every pick): Mute (speaker), Solo (headphones) and the track
  rows' Stop show the symbol alone; New idea (bulb), Vary, Clean up (notes onto the grid), Clear, Accent, Slide,
  Chance, Repeat, Lean, Keep (pin), New sound (die, was Roll new sound), Mutate, Stop all, Column and Delete a symbol
  and a word. Settings → Keys: symbols only, the word in the tooltip. The welcome window asks it, with the timeline
  above or beside the notes and the scroll bars.
- The top bar, rebuilt from mockup A (lower round): three units of one width, each a row of names in small engraved
  capitals over two rows of controls, every control as tall as a key so the rows line up across the bar; the display
  keeps its oscilloscope in a thin frame like the tempo box; key and scale are keys with the value on them; the level
  is two smaller VU meters with a real scale (numbers, red from 0, the pivot under a cover). Pictures of the program
  (README, website, manuals) to be retaken.

## Bugs, clear what to do

- On the Mac only (fine in the AppImage): a thin white edge round the left level meter, and thin light lines above the
  knobs and beside them. Not in screenshots of today's code on GitHub's Mac machines (macOS 15, its own drawing, at
  1× and 2×): to check on the owner's Mac with the next version, whose display is a real Retina one.

## The look: mockups first

- The faders (Volume and the like) could feel more real, "liquid": mockups made (studio, glass tube, rubber); left
  as they are for now.

## To do

The larger steps, and this one, are also in [roadmap.md](roadmap.md), in order.

- Samples: today a sample only goes on a drum pad (the drum track's Sound tab, Built-in under a drum), which nothing in
  the timeline or the loop grid leads to. Options: A drop a WAV or AIFF on the timeline or the loop grid, and it goes
  on a free pad of a "Samples" drum track with a clip that plays it where it was dropped; B "A sample…" in the Add
  track menu (the library, your samples, or a file); C only help: a section in the F1 guide and a lesson "Your own
  sounds". My pick: A and B together. Waiting for the owner's choice.
