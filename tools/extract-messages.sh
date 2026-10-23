#!/bin/sh
# Regenerates the translation template po/garagelog.pot from src/app and qml/
# and merges new strings into every existing po/<lang>/garagelog.po (role: developer tool).
# Run from anywhere: tools/extract-messages.sh. Needs GNU gettext (xgettext, msgmerge).
set -e
cd "$(dirname "$0")/.."

# KI18n call forms used in the project; ki18n* are the deferred variants used for static tables.
KEYWORDS="-ki18n:1 -ki18nc:1c,2 -ki18np:1,2 -ki18ncp:1c,2,3 -kki18n:1 -kki18nc:1c,2 -kki18np:1,2 -kki18ncp:1c,2,3"
COMMON="--from-code=UTF-8 --add-comments=i18n: --package-name=garagelog --msgid-bugs-address=https://github.com/TSTS87/garagelog/issues"
POT=po/garagelog.pot

# C++ first, then QML parsed as JavaScript and joined into the same template.
find src/app -name '*.cpp' | sort | xargs xgettext -C $KEYWORDS $COMMON -o "$POT"
find qml -name '*.qml' | sort | xargs xgettext -L JavaScript $KEYWORDS $COMMON -j -o "$POT"

for po in po/*/garagelog.po; do
    [ -f "$po" ] || continue
    msgmerge --quiet --update --backup=none "$po" "$POT"
done
