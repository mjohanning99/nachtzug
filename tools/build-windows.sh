#!/bin/bash
# Builds Nachtzug for Windows and packs it as an installer (Nachtzug-Setup.exe), or a zip if Inno Setup is not there.
#
# Run it in the "MSYS2 UCRT64" terminal (https://www.msys2.org: install it, then open "MSYS2 UCRT64" from the
# Start menu), from the nachtzug folder:
#
#   tools/build-windows.sh
#
# The first run installs the compiler, Qt, SDL, CMake and Ninja with pacman. For the installer, install Inno Setup
# (https://jrsoftware.org/isinfo.php, or: winget install JRSoftware.InnoSetup); without it you get a zip.
set -euo pipefail
cd "$(dirname "$0")/.."

[ "${MSYSTEM:-}" = "UCRT64" ] || { echo "Please run this in the \"MSYS2 UCRT64\" terminal."; exit 1; }
P=mingw-w64-ucrt-x86_64
need=()
for pkg in gcc cmake ninja qt6-base qt6-tools qt6-translations SDL2 ntldd; do
    pacman -Q "$P-$pkg" >/dev/null 2>&1 || need+=("$P-$pkg")
done
[ ${#need[@]} -eq 0 ] || pacman -S --needed --noconfirm "${need[@]}"

cmake -S . -B build-windows -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-windows
EXE=build-windows/Nachtzug.exe
"$EXE" --selftest; echo "self-test passed"   # in the build folder, with MSYS2's Qt and its window-less platform

# everything the program needs, in one folder: Qt (with its German texts), SDL and the compiler's own libraries
DIST=build-windows/Nachtzug
rm -rf "$DIST"; mkdir -p "$DIST"
cp "$EXE" "$DIST/"
windeployqt6 --release --no-system-d3d-compiler --no-opengl-sw --translations de "$DIST/Nachtzug.exe"
# the libraries of the program and of the Qt plug-ins that windeployqt does not copy (SDL, the compiler's runtime …)
for f in "$DIST"/Nachtzug.exe "$DIST"/platforms/*.dll "$DIST"/styles/*.dll; do
    [ -f "$f" ] || continue
    ntldd -R "$f" 2>/dev/null | grep -i "ucrt64" | sed -E 's/.*=> (.*) \(0x.*/\1/' | tr '\\' '/' | while read -r dll; do
        dll="$(cygpath -u "$dll")"
        [ -f "$DIST/$(basename "$dll")" ] || cp "$dll" "$DIST/"
    done
done
# MSYS2's SDL2 may be "sdl2-compat", which loads SDL3 when the program starts: it has to be there too
if [ -f /ucrt64/bin/SDL3.dll ]; then cp -f /ucrt64/bin/SDL3.dll "$DIST/"; fi

# the finished folder must run on its own: with MSYS2 out of the way
( PATH="/c/Windows/System32:/c/Windows" NZ_TEST_PLATFORM=windows "$DIST/Nachtzug.exe" --selftest ) && echo "self-test of the finished program passed" \
    || { echo "The finished program does not run on its own: see the messages above."; exit 1; }

ISCC="$(command -v iscc || true)"
for c in "/c/Program Files (x86)/Inno Setup 6/ISCC.exe" "/c/Program Files/Inno Setup 6/ISCC.exe" "$LOCALAPPDATA/Programs/Inno Setup 6/ISCC.exe"; do
    [ -z "$ISCC" ] && [ -f "$c" ] && ISCC="$c"
done
if [ -n "$ISCC" ]; then
    "$ISCC" //Qp "//DSourceDir=$(cygpath -w "$DIST")" "//DOutDir=$(cygpath -w build-windows)" assets/windows/nachtzug.iss
    echo
    echo "Done: build-windows/Nachtzug-Setup.exe"
else
    ( cd build-windows && rm -f Nachtzug-windows.zip && powershell -NoProfile -Command "Compress-Archive -Path Nachtzug -DestinationPath Nachtzug-windows.zip" )
    echo
    echo "Done: build-windows/Nachtzug-windows.zip (install Inno Setup for an installer instead)"
fi
echo "Windows may warn about an unknown publisher the first time: choose More info, then Run anyway."
