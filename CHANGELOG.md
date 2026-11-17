# Changes

## 0.3.5 (17 November 2026)

- **Sharp on Linux with Wayland.** The AppImage was missing part of Qt’s Wayland support, so on a Wayland desktop it quietly ran through X11 instead: on a scaled screen the cursor came out tiny and ignored the cursor theme. It now runs on Wayland itself.
- **Cursors that say what a click does.** A pencil over the note grid, a handle at the end of a note or clip, scissors when cutting, and up and down arrows over knobs and faders. They’re on unless switched off, in the welcome window or in Settings.
- **Zoom with a magnifying glass.** The zoom keys above the timeline show a magnifying glass with a minus or a plus instead of the word “Zoom”, so the row fits on a laptop’s screen.
- **Fits a laptop.** With the timeline beside the tabs, the keys above it and in the Notes section wrap onto more rows when there is no room, so the window goes down to about 1170 pixels wide instead of 1780.
- **The sound library from the menu.** File → Sound library… opens it from anywhere, not only from the bottom of a preset list.
- **Save sound… and Library side by side**, as a joined pair at the end of the sound row, and every section’s contents sit in the middle between its screws.
- **Mixer faders like the other sliders**: a sunk groove and a moulded cap in the Bakelite look.
- **Small fixes.** Joined keys such as Save sound… | Library have their lip on the same line as the others on high-resolution screens; in Chords mode only the pad pressed lights up, not its twin in the next row; notes end inside their pattern; drop-down lists show a separator as a thin line instead of an empty row; the German guide speaks impersonally throughout.
- **The website and the README start with Nachtzug itself.** The section on who it’s for, and on the programs to move on to, comes near the end now, and the animation at the top shows today’s look.
- **What Nachtzug is, in plain words.** Software centres, the menus and the README now say “Make electronic music, such as acid or Berlin School, without knowing any music theory” instead of “DAW made for beginners”. The README and the website’s licences page also say how Nachtzug was made: with the help of AI tools, while the program itself contains no AI; LICENSING.md and the licences page explain what that means for copyright.
- **The libraries’ sources on every release.** Each download carries libraries such as Qt, and some of their licences ask that their exact source code go along with them. The release now has it beside each download, and each download lists every library inside it with its version and licence.
- **A home of its own.** The website is now at nachtzugmusic.com, on a server in Germany that keeps no record of visits; the old address forwards there, so links in older versions keep working. It also says how to get in touch, lists what changed in every version, and plays the demo song, to hear what Nachtzug sounds like before downloading it.
- **Windows knows the version.** The installer told Windows that every version was 0.1; “Installed apps” now shows the real one.
- **Homebrew on a Mac.** `brew install mjohanning99/nachtzug/nachtzug` installs Nachtzug, and `brew upgrade` keeps it up to date.
- **A steadier self-test.** The check that Nachtzug follows the tempo of an incoming MIDI clock no longer depends on how busy the computer is; on GitHub’s busy Macs it had held up two releases.

## 0.3.4 (17 November 2026)

- **Builds on Debian 12 and Ubuntu 24.04.** Building Nachtzug needed Qt 6.7, which these systems don’t have; it now builds with Qt 6.4 or newer, and looks just the same.
- **More at home on Linux.** Installed from source or from a package, Nachtzug now shows up properly in software centres such as GNOME Software and KDE Discover, with its description, pictures and what each version changed. Songs open in Nachtzug with a double-click, and the icon comes in every size the desktop asks for.
- **For packagers**, a short section in the README: what the source contains and what gets installed.
- **Tidier source.** Some working notes had been published with the source by mistake; they are no longer part of it.

## 0.3.3 (17 November 2026)

- **The downloads for 0.3.2.** One of the self-test’s checks needs exact timing and failed on GitHub’s busy Mac, so 0.3.2 came out without downloads; the check now gives a busy computer room, and this version brings the downloads, with everything 0.3.2 changed.

## 0.3.2 (16 November 2026)

- **Linux packages build again.** Packages for openSUSE and Fedora, which are built with link-time optimisation, stopped with “No data signature found”. The sample library is now built in a way that works with any compiler settings, and a source folder without the samples stops at once with a clear message.

## 0.3.1 (16 November 2026)

- **The old ligatures.** In Nachtzug Antiqua, ct and st have their historical arc again, a bow from the c or s onto the t, as in old books, and they show in all running text on the website and in the manuals, together with the tz that joins the name.

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
