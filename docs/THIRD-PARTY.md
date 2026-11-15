# What Nachtzug is built on

Nachtzug is built with other people’s free software. This page lists it, with the licence of each part. Thank you to everyone who made these!

## In every build

| Part | What it does | Licence |
|---|---|---|
| [Qt 6](https://www.qt.io) | the window, the controls, text and pictures | LGPL 3.0 ([text](../LICENSES/LGPL-3.0-only.txt)) |
| [SDL 2](https://www.libsdl.org) | the sound card | zlib ([text](../LICENSES/Zlib.txt)) |
| ALSA (Linux), Core MIDI (macOS), WinMM (Windows) | MIDI | the system’s own; not bundled |

## Fonts

| Font | Licence |
|---|---|
| Nachtzug Antiqua, Initialen, Ornamente and Grotesk (our own, built on EB Garamond and Jost) | SIL Open Font License 1.1 (`assets/fonts/nachtzug/OFL.txt`) |
| [Jost](https://github.com/indestructible-type/Jost) | SIL Open Font License 1.1 (`assets/fonts/OFL-Jost.txt`) |
| [Courier Prime](https://github.com/quoteunquoteapps/CourierPrime) | SIL Open Font License 1.1 (`assets/fonts/OFL-CourierPrime.txt`) |
| In the manuals also [EB Garamond](https://github.com/octaviopardo/EBGaramond12) italic, [Crimson Pro](https://github.com/Fonthausen/CrimsonPro), [Barlow](https://github.com/jpt/barlow) and [Michroma](https://github.com/googlefonts/Michroma-font) | SIL Open Font License 1.1 (`docs/manual/fonts/OFL-*.txt`) |

## In the Linux AppImage

The AppImage is one file that carries the libraries Nachtzug needs, taken from Debian 13. Besides Qt and SDL, these include the X11, XCB and Wayland libraries, xkbcommon, libdecor, PulseAudio, libsystemd, GLib, D-Bus, libcap, libffi, ICU, PCRE2, double-conversion, md4c, libb2, graphite2, libpng, libjpeg, zstd, Brotli, bzip2, libsndfile with FLAC, Ogg, Vorbis, Opus, mpg123 and LAME, libsamplerate, and the GCC runtime. Most are under the MIT, BSD or zlib licences; Qt, GLib, PulseAudio, libsystemd, libsndfile, mpg123, LAME, graphite2 and libasyncns are under the LGPL ([2.1](../LICENSES/LGPL-2.1-or-later.txt), [3.0](../LICENSES/LGPL-3.0-only.txt)).

- **The full licence of each one** is inside the AppImage, in `usr/share/doc/nachtzug/third-party/` (Debian’s copyright file of each package). To look: `./Nachtzug-x86_64.AppImage --appimage-extract`.
- **Their source code** is Debian’s: [sources.debian.org](https://sources.debian.org), by the package names listed in `usr/share/doc/nachtzug/packages.txt`.
- **Replacing a library** (as the LGPL allows): unpack the AppImage with `--appimage-extract`, swap the library in `squashfs-root/usr/lib/` for your own build, and start `squashfs-root/AppRun`.

## In the macOS and Windows downloads

The Mac disk image and the Windows installer carry Qt and SDL too (SDL 2 as a small layer over SDL 3), with the libraries Qt needs: on a Mac taken from [Homebrew](https://brew.sh), on Windows from [MSYS2](https://www.msys2.org). Among them are ICU, PCRE2, HarfBuzz, FreeType, libpng, libjpeg, zlib, zstd, Brotli, double-conversion, md4c, GLib on Windows, and on Windows the GCC and MinGW runtime (under the GCC Runtime Library Exception). They are under the MIT, BSD, zlib and similar licences, and Qt and GLib under the LGPL ([2.1](../LICENSES/LGPL-2.1-or-later.txt), [3.0](../LICENSES/LGPL-3.0-only.txt)).

- **The licences** are inside each download: on a Mac in `Nachtzug.app/Contents/Resources/licences/`, on Windows in the `licences` folder where Nachtzug is installed.
- **Their source code** is that of the Homebrew and MSYS2 packages of the same names (formulae.brew.sh, packages.msys2.org).
- **Replacing a library**: on Windows, swap the DLL in Nachtzug’s folder; on a Mac, swap the library in `Nachtzug.app/Contents/Frameworks/` and sign the app again for your Mac (`codesign --force --deep --sign - Nachtzug.app`).

## The pictures

The cover painting and the train pictures were made with [Z-Image Turbo](https://huggingface.co/Tongyi-MAI/Z-Image-Turbo) (Apache 2.0) and [FLUX 3 Image Klein](https://huggingface.co/black-forest-labs/flux-3-image-klein) (FLUX Kommunity License) from our own sketches, scenes and photo references, and chosen and edited by hand. Neither model claims any rights to the pictures.
