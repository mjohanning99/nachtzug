#!/bin/sh
# Builds Nachtzug as an AppImage: one file that runs on most Linux computers without installing anything.
#
#     tools/build-appimage.sh            -> dist/Nachtzug-x86_64.AppImage
#
# The release build (.github/workflows/release.yml) uses it, and it works the same on any Debian or Ubuntu computer that has
# the build tools (see NEEDS below). It builds in build-appimage/, checks the program with its self-test, then packs
# it, Qt and SDL with linuxdeploy. linuxdeploy and its Qt plugin are fetched once, at fixed versions with their
# checksums checked, and kept in ~/.cache/nachtzug-tools.
#
# The AppImage runs on Linux systems at least as new as the one it was built on (their C library must be as new),
# so a build on Debian 13 runs on distributions from 2025 on.
set -eu
export LC_ALL=C.UTF-8

ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD="$ROOT/build-appimage"
APPDIR="$BUILD/AppDir"
DIST="$ROOT/dist"
TOOLS="${XDG_CACHE_HOME:-$HOME/.cache}/nachtzug-tools"
NEEDS="cmake g++ qt6-base-dev qt6-base-dev-tools qt6-tools-dev qt6-l10n-tools qt6-qpa-plugins qt6-wayland libsdl2-dev libasound2-dev libgl-dev file"

LD_VERSION=1-alpha-20251107-1
LD_SHA=c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d
QT_VERSION=1-alpha-20250213-1
QT_SHA=15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724

# ---- the tools it needs
missing=""
for t in cmake g++ curl file; do command -v "$t" >/dev/null 2>&1 || missing="$missing $t"; done
QMAKE=$(command -v qmake6 || ls /usr/lib/qt6/bin/qmake 2>/dev/null || true)
[ -n "$QMAKE" ] || missing="$missing qmake6"
if [ -n "$missing" ]; then
    echo "Missing:$missing"
    echo "On Debian or Ubuntu: sudo apt install $NEEDS curl"
    exit 1
fi

fetch() {   # fetch NAME URL SHA256: into $TOOLS once, checked
    if [ ! -x "$TOOLS/$1" ] || ! echo "$3  $TOOLS/$1" | sha256sum -c --status; then
        mkdir -p "$TOOLS"
        echo "Fetching $1"
        curl -fsSL -o "$TOOLS/$1.part" "$2"
        echo "$3  $TOOLS/$1.part" | sha256sum -c --quiet
        mv "$TOOLS/$1.part" "$TOOLS/$1"
        chmod +x "$TOOLS/$1"
    fi
}
fetch linuxdeploy-x86_64.AppImage "https://github.com/linuxdeploy/linuxdeploy/releases/download/$LD_VERSION/linuxdeploy-x86_64.AppImage" "$LD_SHA"
fetch linuxdeploy-plugin-qt-x86_64.AppImage "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/$QT_VERSION/linuxdeploy-plugin-qt-x86_64.AppImage" "$QT_SHA"

# ---- build and check
echo "Building"
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr >/dev/null
cmake --build "$BUILD" -j "$(nproc)"
echo "Checking (self-test)"
QT_QPA_PLATFORM=offscreen SDL_AUDIODRIVER=dummy "$BUILD/nachtzug" --selftest

# ---- pack
echo "Packing"
rm -rf "$APPDIR"
DESTDIR="$APPDIR" cmake --install "$BUILD" >/dev/null
mkdir -p "$DIST"
cd "$BUILD"
# containers have no FUSE: the tools unpack themselves instead of mounting
export APPIMAGE_EXTRACT_AND_RUN=1 NO_STRIP=1
# Wayland next to X11, and offscreen for the checks and for --render without a window
export QMAKE EXTRA_PLATFORM_PLUGINS="libqwayland-generic.so;libqoffscreen.so"
export LDAI_OUTPUT="$DIST/Nachtzug-x86_64.AppImage"
VERSION=$(git -C "$ROOT" describe --tags --always 2>/dev/null || echo dev)
export LINUXDEPLOY_OUTPUT_VERSION="$VERSION"
"$TOOLS/linuxdeploy-x86_64.AppImage" --appdir "$APPDIR" \
    --desktop-file "$APPDIR/usr/share/applications/nachtzug.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/512x512/apps/nachtzug.png" \
    --plugin qt >"$BUILD/linuxdeploy.log" 2>&1 || { tail -40 "$BUILD/linuxdeploy.log"; exit 1; }

# the rest of Qt's Wayland support, which linuxdeploy leaves out: without the shell integration (xdg-shell) the
# Wayland plugin can't open a window, and Nachtzug falls back to X11 through XWayland, small and unthemed cursor and
# all on a scaled screen. The second linuxdeploy run below brings in the libraries these plugins need.
PLUGINS=$("$QMAKE" -query QT_INSTALL_PLUGINS)
DEPS=""
for d in wayland-shell-integration wayland-decoration-client wayland-graphics-integration-client; do
    [ -d "$PLUGINS/$d" ] || { echo "Missing: $PLUGINS/$d (Debian: qt6-wayland)"; exit 1; }
    mkdir -p "$APPDIR/usr/plugins/$d"
    cp "$PLUGINS/$d"/*.so "$APPDIR/usr/plugins/$d/"
    DEPS="$DEPS --deploy-deps-only=$APPDIR/usr/plugins/$d"
done

# the licences of everything the AppImage carries: Nachtzug's own, and Debian's copyright file of each package
# that a bundled library comes from (Debian keeps those complete); sources.txt lists each package's source
DOC="$APPDIR/usr/share/doc/nachtzug"
mkdir -p "$DOC/third-party"
cp "$ROOT/LICENSE" "$ROOT/LICENSING.md" "$ROOT/docs/THIRD-PARTY.md" "$DOC/"
cp -r "$ROOT/LICENSES" "$DOC/"
if command -v dpkg >/dev/null 2>&1; then
    for lib in "$APPDIR"/usr/lib/*.so* "$APPDIR"/usr/plugins/*/*.so; do
        [ -e "$lib" ] || continue
        pkg=$(dpkg -S "$(basename "$lib")" 2>/dev/null | head -1 | cut -d: -f1)
        [ -n "$pkg" ] && [ -f "/usr/share/doc/$pkg/copyright" ] && cp "/usr/share/doc/$pkg/copyright" "$DOC/third-party/$pkg.txt"
    done
    echo "  licences of $(ls "$DOC/third-party" | wc -l) bundled packages"
    "$ROOT/tools/library-sources.sh" --list "$APPDIR" "$DOC"
fi

# shellcheck disable=SC2086 # DEPS is a list of options
"$TOOLS/linuxdeploy-x86_64.AppImage" --appdir "$APPDIR" $DEPS --output appimage >>"$BUILD/linuxdeploy.log" 2>&1 || { tail -40 "$BUILD/linuxdeploy.log"; exit 1; }

# ---- check the packed program too
echo "Checking the AppImage"
QT_QPA_PLATFORM=offscreen SDL_AUDIODRIVER=dummy "$LDAI_OUTPUT" --appimage-extract-and-run --selftest >/dev/null
ls -l "$LDAI_OUTPUT"
echo "Done: $LDAI_OUTPUT ($VERSION)"
