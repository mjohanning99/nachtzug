#!/bin/bash
# Builds Nachtzug.app on a Mac and packs it into Nachtzug.dmg, with Qt and SDL inside, so it runs
# on Macs that have neither installed.
#
#   tools/build-macos.sh            build for the Mac it runs on (Apple Silicon or Intel)
#
# Needs the Xcode command line tools (xcode-select --install) and Homebrew (https://brew.sh).
# The first run installs qt, sdl2, cmake and ninja with Homebrew if they are missing.
set -euo pipefail
cd "$(dirname "$0")/.."

command -v brew >/dev/null || { echo "Homebrew is needed: see https://brew.sh"; exit 1; }
for pkg in qt sdl2 cmake ninja; do   # sdl2 brings sdl3 along (Homebrew's sdl2 is sdl2-compat)
    brew list --formula "$pkg" >/dev/null 2>&1 || brew install "$pkg"
done
QT="$(brew --prefix qt)"

APP=build-macos/Nachtzug.app
# always start from a freshly linked program: the steps below change it in place, and an app left half done by an
# earlier run that was stopped (Ctrl+C) no longer matches its signature, so macOS stops it at once ("Killed: 9")
rm -rf "$APP"
cmake -S . -B build-macos -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QT;$(brew --prefix sdl2)"
cmake --build build-macos

# before Qt is copied in: the self-test runs without a window, which needs Homebrew Qt's "offscreen" plugin
"$APP/Contents/MacOS/Nachtzug" --selftest; echo "self-test passed"

# Qt's own German texts (the Save / Discard buttons and so on)
mkdir -p "$APP/Contents/Resources/translations"
cp "$QT"/share/qt/translations/qtbase_de.qm "$APP/Contents/Resources/translations/" 2>/dev/null || true
# from here on the app is changed in place: if the script is stopped, take the half-made app away again
trap 'echo; echo "Stopped: the half-made app was removed. Run the script again to build it."; rm -rf "$APP"; exit 130' INT TERM

# copy Qt, SDL and their libraries into the program, then sign it for this Mac (no Apple developer account needed)
echo "Copying Qt and SDL into the app. This takes a minute or two and prints little, please wait…"
"$QT/bin/macdeployqt" "$APP" -always-overwrite 2>&1 | grep -v -e 'QtPdf' -e 'QtSvg' -e 'QtVirtualKeyboard' -e 'libbrotli' -e 'libsharpyuv' -e 'libwebp' -e 'using QList' || true

# Homebrew's sdl2 is now "sdl2-compat": a small SDL2 that loads SDL3 when the program starts, looking for it next
# to itself. macdeployqt cannot see that, so SDL3 is put there by hand; without it the program stops at once.
FW="$APP/Contents/Frameworks"
if [ -d "$(brew --prefix)/opt/sdl3/lib" ] || brew list --formula sdl3 >/dev/null 2>&1; then
    SDL3="$(brew --prefix sdl3)/lib"
    cp -fL "$SDL3/libSDL3.0.dylib" "$FW/libSDL3.0.dylib"
    chmod u+w "$FW/libSDL3.0.dylib"
    install_name_tool -id @rpath/libSDL3.0.dylib "$FW/libSDL3.0.dylib"
    ln -sf libSDL3.0.dylib "$FW/libSDL3.dylib"
fi

# Qt brings plug-ins Nachtzug does not use (PDF, SVG and WebP pictures, an on-screen keyboard), some of them missing
# their libraries in a Homebrew Qt: leave them out
P="$APP/Contents/PlugIns"
rm -rf "$P/platforminputcontexts" "$P/virtualkeyboard" "$P/iconengines"
find "$P/imageformats" -name '*.dylib' ! -name 'libqjpeg.dylib' ! -name 'libqgif.dylib' -delete 2>/dev/null || true
# the licences: Nachtzug's own and those of Qt, SDL and the rest it carries (docs/THIRD-PARTY.md says which)
L="$APP/Contents/Resources/licences"; rm -rf "$L"; mkdir -p "$L"
cp LICENSE LICENSING.md docs/THIRD-PARTY.md "$L/"; cp -R LICENSES "$L/"
tools/library-sources.sh --list "$APP" "$L"   # sources.txt: each library's formula, version, licence and source
echo "Signing the app for this Mac…"
codesign --force --deep --sign - "$APP"

# the finished program, with only what is inside it, must still start and pass its self-test
# (on the Mac's own platform: the window-less one is not packed in; the self-test opens no window)
env -u DYLD_LIBRARY_PATH -u DYLD_FALLBACK_LIBRARY_PATH NZ_TEST_PLATFORM=cocoa "$APP/Contents/MacOS/Nachtzug" --selftest \
    && echo "self-test of the finished program passed" \
    || { echo "The finished program does not run on its own: see the messages above."; exit 1; }

trap - INT TERM
rm -f build-macos/Nachtzug.dmg
echo "Packing Nachtzug.dmg…"
hdiutil create -volname Nachtzug -srcfolder "$APP" -ov -format UDZO build-macos/Nachtzug.dmg
echo
echo "Done: build-macos/Nachtzug.app and build-macos/Nachtzug.dmg"
echo "On another Mac, the first time: click Done when macOS warns, then Open Anyway in System Settings → Privacy & Security."
