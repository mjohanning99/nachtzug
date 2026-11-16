#!/bin/sh
# Copies the two manuals from docs/ into downloads/, as the website offers them, and the licence texts into
# _includes/licences/ for the licences page.
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
echo "Copied the manuals and the licence texts."
