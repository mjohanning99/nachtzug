# Changes

## 0.3.0 (16 November 2026)

- **Sounds of your own.** “Save sound…” keeps a sound you have made as a small file, with a name, a group and a star. The preset list shows the starred and recent ones, and “All sounds…” opens the sound library: a card index where every sound can be heard first, then used, renamed, shared or deleted. Sound files can be dragged into it.
- **Note lengths.** Dragging a note’s end snaps to the grid; with Shift, any length.
- **Choices as joined keys.** Export, Chop and Arrange use the same keys as the welcome window instead of round option buttons.
- **Small fixes.** The welcome window’s choices work in pairs again, and the Arrange window shows its whole introduction.
- **A tidier look.** One rust and one red throughout, the name never letter-spaced on the website, and a design language (docs/design-language.md) for anyone who would like to help.

## 0.2.1 (16 November 2026)

- **The Mac download builds again.** One line the Mac’s compiler refuses had stopped the 0.2.0 release, so 0.2.0 came out without downloads; this version brings them, with everything 0.2.0 changed.
- **Who it’s for**, near the top of the README and on the website: Nachtzug is a starting point, not a fully featured DAW, with programs to move on to.

## 0.2.0 (16 November 2026)

A new look, inside and out, and many small things made nicer to use.

- **A new top bar.** Transport, song and output in three plates of the same height, with a real VU meter. At the top left, the square sign with “Nachtzug” under it.
- **Keys with symbols.** Every key shows a symbol and a word, or only the symbol: chosen in the welcome window or in Settings. Tick boxes are now sliding switches, and drop-down lists and menus have rounded corners.
- **Tidier windows.** Scroll bars stay hidden unless wanted (Settings → Window; Shift and the mouse wheel scroll sideways). The note grid opens on the clip’s notes. The timeline’s clips have the loop grid’s colours, and the labels are shorter.
- **Help in two tabs.** F1 opens a guide in five steps and a second tab with every key and mouse move. Both point to the operation manual, which Help → Operation manual opens.
- **German without “du”.** The German program speaks impersonally now, like the manual.
- **Small fixes.** Clean up puts every note on the grid chosen. Tap tempo shows the tempo it set. Folding a part of the inspector no longer makes it wider, and the closed inspector’s tab is an even key down the edge. The icon has the right size in the Mac Dock, and the fader handles and drop-down arrows show in every download.
- **A new logo and cover.** The tz on a lighter enamel sign, and Hans Baluschek’s painting “Großstadtbahnhof” (1904) on the cover. The tz ligatures in Nachtzug’s own fonts run as one even rail.
- **A website**, in English and German, laid out like a railway station: the downloads as tickets, the licences and credits in full, an Impressum and a privacy notice.
- **The manuals** have new pictures, a page of credits and licences, and Windows on the cover.
- **Under the bonnet.** The code is split into smaller files and laid out evenly, with a check that it still behaves exactly the same. For anyone who would like to help, AGENTS.md explains the layout.

## 0.1.1 (15 November 2026)

- **Ready-made downloads for Mac and Windows.** A disk image for Macs with Apple Silicon (macOS 15 or newer) and an installer for Windows 10 and 11, built and self-tested on GitHub for every release. This is the first Windows version: please tell me if anything goes wrong.
- The Windows build script no longer deletes the program it has just built (MSYS2 counts “Nachtzug” and “Nachtzug.exe” as the same name).
- The Mac and Windows downloads carry the licences of everything inside them.
- Forms for bug reports and ideas on GitHub.

## 0.1.0 (15 November 2026)

The first public version of Nachtzug, a little music machine for acid and Berlin School music.

- Synthesiser tracks whose notes are always in the song’s scale, played on the letter keys, a MIDI keyboard or drawn in the note grid. Hold Shift for the right wrong notes.
- Ideas at the press of a button (acid lines, Berlin School sequences, chords, melodies, beats), Vary, and Clean up for what you played in.
- An auto drummer with seven styles, Energy and fills.
- A full synthesiser per track with pictures of what each knob does, and a Simple page with eight big knobs.
- A song timeline, a loop grid for jamming, Arrange for me, a mixer, recorded knob moves, samples and chopped loops.
- Export as WAV, as one file per track, or as MIDI.
- English and German, five lessons, and a printed operation manual in both languages.
- For Linux (an AppImage for distributions from 2025 on, or built from source) and macOS (built with one script). Windows is ready but not yet tried on a real Windows computer.
