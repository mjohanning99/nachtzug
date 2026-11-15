#!/bin/sh
# Copies the two manuals from docs/ into downloads/, as the website offers them.
# The website's build does this every time; run it once before "jekyll serve" too.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$here/downloads"
cp "$here/../docs/Nachtzug-Manual.pdf" "$here/../docs/Nachtzug-Handbuch.pdf" "$here/downloads/"
echo "Copied the manuals."
