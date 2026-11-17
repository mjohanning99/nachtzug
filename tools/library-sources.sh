#!/bin/bash
# Lists every library a finished download of Nachtzug carries, with the package it comes from, its version, its
# licence and where its source code is; and fetches that source, so it can go on the release beside the download.
#
#   tools/library-sources.sh --list  FOLDER OUT    write OUT/sources.txt: the list only (the build scripts do this)
#   tools/library-sources.sh         FOLDER OUT    the list, and the exact source of the packages that need it in OUT
#
# FOLDER is the finished program: the AppImage's AppDir on Linux, Nachtzug.app on a Mac, the package folder on Windows.
# It must run on the computer that built FOLDER, as it asks that computer's package manager (Debian's dpkg and apt,
# Homebrew, or MSYS2's pacman) where each library came from.
#
# Which sources are fetched: on a Mac and on Windows, those of every package under a copyleft licence (GPL, LGPL, MPL
# and the like), which ask that their source go wherever the program goes; Homebrew and MSYS2 keep only recent
# versions, so a copy goes on the release. On Linux, the source of every package, as Debian's copyright files don't
# say the licence in a form a script can read. Debian's own archive (snapshot.debian.org) keeps them for good too.
#
# On Linux, fetching needs Debian's source lists ("deb-src"): the release workflow switches them on.
set -euo pipefail

LIST_ONLY=0
if [ "${1:-}" = "--list" ]; then LIST_ONLY=1; shift; fi
[ $# -eq 2 ] || { sed -n '2,9p' "$0"; exit 2; }
FOLDER="$(cd "$1" && pwd)"
mkdir -p "$2"
OUT="$(cd "$2" && pwd)"
LIST="$OUT/sources.txt"
COPYLEFT='GPL|MPL|EPL|CDDL|EUPL|OSL|CPL'

# one line per package: name, version, licence, then the source's addresses (several, separated by spaces)
ROWS="$(mktemp)"
trap 'rm -f "$ROWS"' EXIT
row() { printf '%s\t%s\t%s\t%s\n' "$1" "$2" "$3" "$4" >> "$ROWS"; }
warn() { echo "library-sources: $*" >&2; }

# ---- Linux (Debian): the package of each library, its source package and that one's version
linux_list() {
	local lib pkg seen=" "
	for lib in "$FOLDER"/usr/lib/*.so* "$FOLDER"/usr/plugins/*/*.so; do
		[ -e "$lib" ] || continue
		pkg=$(dpkg -S "$(basename "$lib")" 2>/dev/null | head -1 | cut -d: -f1) || true
		[ -n "$pkg" ] || { warn "no Debian package has $(basename "$lib")"; continue; }
		case "$seen" in *" $pkg "*) continue ;; esac
		seen="$seen$pkg "
		local src ver
		read -r src ver < <(dpkg-query -W -f='${source:Package} ${source:Version}\n' "$pkg" | head -1)
		row "$src" "$ver" "see the package's copyright file" "https://snapshot.debian.org/package/$src/$ver/"
	done
}

linux_fetch() {
	local src ver dir
	while IFS=$'\t' read -r src ver _ _; do
		dir="$OUT/$src-$ver"
		[ -d "$dir" ] && continue
		mkdir -p "$dir"
		( cd "$dir" && apt-get source --download-only -qq -o APT::Sandbox::User=root "$src=$ver" >/dev/null ) \
			|| { warn "apt-get could not fetch the source of $src $ver (are the deb-src lists switched on?)"; return 1; }
	done < "$ROWS"
}

# ---- macOS (Homebrew): the formula each library was installed by, from the path it was copied from
mac_origin() {   # the Homebrew file a file inside the app was copied from
	local f="$1" name base prefix
	prefix="$(brew --prefix)"
	name="$(basename "$f")"
	case "$f" in
		*/Frameworks/*.framework) base="$prefix/lib/$name" ;;
		*/Frameworks/*) base="$prefix/lib/$name" ;;
		*/PlugIns/*) base="$prefix/share/qt/plugins/$(basename "$(dirname "$f")")/$name" ;;
		*/translations/*.qm) base="$prefix/share/qt/translations/$name" ;;
	esac
	if [ -n "${base:-}" ] && [ -e "$base" ]; then
		realpath "$base"
		return
	fi
	# not linked into the prefix (SDL3 is copied in by hand from its own folder): look in the formulae themselves
	find "$prefix/Cellar" -name "$name" -not -path '*/include/*' 2>/dev/null | head -1
}

# a formula's licence and source, for the version installed: its main archive, any patches, and the formula itself
# (how Homebrew built it) at the commit the installed version came from
MAC_FORMULA='
import json, sys
f = json.load(sys.stdin)["formulae"][0]
installed = sys.argv[1].split("_")[0]
if f["versions"]["stable"] != installed:   # an older install: the formula now points at another source
    note = "library-sources: %s %s is installed, the formula is now for %s"
    note %= (f["name"], installed, f["versions"]["stable"])
    if sys.argv[2] == "1": sys.exit(note + ": upgrade it (brew upgrade) and build again")
    print(note, file=sys.stderr)
lic = f.get("license") or "unknown"
lic = lic if isinstance(lic, str) else json.dumps(lic)
stable = f["urls"]["stable"]
urls = ["git+%s#%s" % (stable["url"], stable["tag"]) if stable.get("tag") else stable["url"]]
urls += [p["url"] for p in f.get("patches") or [] if isinstance(p, dict) and p.get("url")]
if f.get("tap_git_head") and f.get("ruby_source_path"):
    core = "https://raw.githubusercontent.com/Homebrew/homebrew-core/"
    urls.append(core + f["tap_git_head"] + "/" + f["ruby_source_path"])
print("\t".join([f["name"], sys.argv[1], lic, " ".join(urls)]))
'

mac_list() {
	local f origin formula ver seen=" "
	while IFS= read -r f; do
		origin="$(mac_origin "$f")"
		[[ "$origin" == */Cellar/* ]] || { warn "no Homebrew formula has $(basename "$f")"; continue; }
		formula="$(sed -E 's|.*/Cellar/([^/]+)/([^/]+)/.*|\1|' <<< "$origin")"
		ver="$(sed -E 's|.*/Cellar/([^/]+)/([^/]+)/.*|\2|' <<< "$origin")"
		case "$seen" in *" $formula "*) continue ;; esac
		seen="$seen$formula "
		# when fetching, the installed version must be the formula's, or the source would be another version's
		brew info --json=v2 "$formula" | python3 -c "$MAC_FORMULA" "$ver" "$((1 - LIST_ONLY))" >> "$ROWS"
	done < <(find "$FOLDER/Contents/Frameworks" -mindepth 1 -maxdepth 1 -not -type l \
			\( -name '*.framework' -o -name '*.dylib' \)
		find "$FOLDER/Contents/PlugIns" -name '*.dylib' 2>/dev/null
		find "$FOLDER/Contents/Resources/translations" -name '*.qm' 2>/dev/null)
}

# ---- Windows (MSYS2): the package that owns each DLL, and its source package on MSYS2's server
win_list() {
	local f rel origin pkg ver base lic seen=" "
	while IFS= read -r f; do
		rel="${f#"$FOLDER"/}"
		case "$rel" in
			*/*.dll) origin="/ucrt64/share/qt6/plugins/$rel" ;;
			translations/*) origin="/ucrt64/share/qt6/$rel" ;;
			*) origin="/ucrt64/bin/$rel" ;;
		esac
		pkg="$(pacman -Qqo "$origin" 2>/dev/null)" || { warn "no MSYS2 package has $rel"; continue; }
		case "$seen" in *" $pkg "*) continue ;; esac
		seen="$seen$pkg "
		ver="$(pacman -Q "$pkg" | cut -d' ' -f2)"
		# the package's own record names its source package (%BASE%) and its licences (%LICENSE%)
		local desc="/var/lib/pacman/local/$pkg-$ver/desc"
		base="$(awk '/^%BASE%$/ {getline; print; exit}' "$desc")"
		lic="$(awk '/^%LICENSE%$/ {on=1; next} /^$/ {on=0} on' "$desc" | paste -sd' ' -)"
		row "$pkg" "$ver" "${lic:-unknown}" "https://repo.msys2.org/mingw/sources/${base:-$pkg}-$ver.src.tar.zst"
	done < <(find "$FOLDER" \( -name '*.dll' -o -name '*.qm' \) | sort)
}

# ---- fetching by address (Mac and Windows): only the packages under a copyleft licence
fetch_urls() {
	local name ver lic urls url dir
	while IFS=$'\t' read -r name ver lic urls; do
		grep -qE "$COPYLEFT" <<< "$lic" || continue
		dir="$OUT/$name-$ver"
		mkdir -p "$dir"
		for url in $urls; do
			if [[ "$url" == git+* ]]; then
				url="${url#git+}"
				git clone -q --depth 1 --branch "${url##*#}" "${url%#*}" "$dir/source" && rm -rf "$dir/source/.git"
			else
				curl -fsSL --retry 4 -o "$dir/$(basename "${url%%\?*}")" "$url"
			fi || { warn "could not fetch $url"; return 1; }
		done
	done < "$ROWS"
}

case "$(uname -s)" in
	Darwin) mac_list ;;
	*_NT*) win_list ;;   # MSYS2: MINGW64_NT-…, UCRT64_NT-… and so on
	*) command -v dpkg >/dev/null || { warn "dpkg is needed (the AppImage is built on Debian)"; exit 1; }; linux_list ;;
esac
[ -s "$ROWS" ] || { warn "found no libraries in $FOLDER"; exit 1; }
sort -u -o "$ROWS" "$ROWS"

{
	echo "# The libraries inside this download of Nachtzug: the package each comes from, its version, its licence and"
	echo "# where its source code is. The sources are also on each release, beside the download (docs/THIRD-PARTY.md)."
	printf '# package\tversion\tlicence\tsource\n'
	cat "$ROWS"
} > "$LIST"
echo "library-sources: $(wc -l < "$ROWS") packages listed in $LIST"

[ "$LIST_ONLY" = 1 ] && exit 0
case "$(uname -s)" in
	Darwin | *_NT*) fetch_urls ;;
	*) linux_fetch ;;
esac
echo "library-sources: sources in $OUT ($(du -sh "$OUT" | cut -f1))"
