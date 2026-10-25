#!/bin/sh
# Builds a single-file Flatpak bundle of a release tag for the GitHub release page (role: developer tool).
# Usage: tools/build-bundle.sh v1.2.3  ->  build/GarageLog-1.2.3.flatpak
# Needs org.flatpak.Builder from Flathub. The tag only has to exist locally.
set -e
tag="$1"
[ -n "$tag" ] || { echo "usage: $0 vX.Y.Z" >&2; exit 1; }
cd "$(dirname "$0")/.."
commit=$(git rev-parse "$tag^{commit}")
manifest=bundle-manifest.yml

# Derive from the Flathub manifest so both build the same way, but fetch the local
# repository. A bundle carries only the app ref, not the .Locale extension, so the
# translations must stay inside the app.
sed -e "s#url: https://github.com/TSTS87/garagelog.git#url: file://$PWD#" \
    -e "s/^        tag: .*/        tag: $tag/" \
    -e "s/^        commit: .*/        commit: $commit/" \
    -e "s/^command: garagelog$/command: garagelog\nseparate-locales: false/" \
    packaging/flathub/io.github.TSTS87.GarageLog.yml > "$manifest"
trap 'rm -f "$manifest"' EXIT

flatpak run --no-documents-portal org.flatpak.Builder --user --force-clean \
    --repo=repo-flatpak build-flatpak "$manifest"
# --runtime-repo lets Discover/GNOME Software fetch the KDE runtime from Flathub.
flatpak build-bundle --runtime-repo=https://dl.flathub.org/repo/flathub.flatpakrepo \
    repo-flatpak "build/GarageLog-${tag#v}.flatpak" io.github.TSTS87.GarageLog
echo "build/GarageLog-${tag#v}.flatpak"
