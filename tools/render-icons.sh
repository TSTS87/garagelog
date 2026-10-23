#!/bin/sh
# Renders the app icon SVG to the PNG sizes AppStream/Flathub expect (role: developer tool).
# Run after changing packaging/icons/*.svg; the PNGs are committed. Needs rsvg-convert.
set -e
cd "$(dirname "$0")/../packaging/icons"
for size in 48 64 128 256; do
    rsvg-convert -w $size -h $size io.github.TSTS87.GarageLog.svg -o io.github.TSTS87.GarageLog-$size.png
done
