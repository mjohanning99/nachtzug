#!/bin/sh
# Copies the two manuals from docs/ into downloads/, as the website offers them, the licence texts into
# _includes/licences/ for the licences page, and CHANGELOG.md into _includes/ for the page of changes.
# The website's build does this every time; run it once before "jekyll serve" too.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$here/downloads"
cp "$here/../docs/Nachtzug-Manual.pdf" "$here/../docs/Nachtzug-Handbuch.pdf" "$here/downloads/"
# the licence texts, shown in full on the licences page (one copy each in the repository, here only while building)
mkdir -p "$here/_includes/licences"
cp "$here/../LICENSE" "$here/_includes/licences/GPL-3.0.txt"
cp "$here/../LICENSES/"*.txt "$here/_includes/licences/"
cp "$here/../assets/fonts/nachtzug/OFL.txt" "$here/_includes/licences/OFL-1.1.txt"
# what changed in each version: the page of changes shows CHANGELOG.md itself, so it is never behind
cp "$here/../CHANGELOG.md" "$here/_includes/changelog.md"
echo "Copied the manuals, the licence texts and the changelog."
