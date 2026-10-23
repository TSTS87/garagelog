# Contributing to GarageLog

Thanks for your interest! GarageLog is a small project; bug reports, translations and focused pull requests are all welcome.

## Reporting bugs
Open an issue at <https://github.com/TSTS87/garagelog/issues> with:
- GarageLog version (Settings, or `garagelog --version`) and how you installed it (Flatpak, source)
- what you did, what you expected, what happened
- if possible, a JSON export (Data → Export Everything) that reproduces the problem – remove anything private first

## Building and testing
Arch/CachyOS:
```
sudo pacman -S --needed base-devel cmake extra-cmake-modules qt6-base qt6-declarative \
    kirigami ki18n kcoreaddons kconfig kquickcharts gettext appstream desktop-file-utils
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build
./build/bin/garagelog
```
Other distributions need the same Qt 6 (≥ 6.6) and KDE Frameworks 6 packages under their own names.

Flatpak (needs `org.flatpak.Builder` from Flathub):
```
flatpak run org.flatpak.Builder --user --install --force-clean build-flatpak \
    packaging/flatpak/io.github.TSTS87.GarageLog.yml
flatpak run io.github.TSTS87.GarageLog
```

Every push runs the same build and tests on GitHub Actions (`.github/workflows/ci.yml`).

## Project layout
| Path | Role |
|---|---|
| `src/core/` | Logic library: models, repositories, calculations, import/export. Qt Core + Qt Sql only, no GUI. |
| `src/app/` | QML bindings (list models, controllers) and `main.cpp`. |
| `qml/` | User interface (Kirigami). No business logic, no SQL. |
| `src/core/migrations/` | Numbered, additive SQL migrations. |
| `tests/` | QtTest unit tests, one executable per test. |
| `tools/` | Developer helpers (test data generator, message extraction, icon rendering). |
| `docs/` | Specification, data model, decisions (ADRs), roadmap. |

Before larger changes, read `docs/DECISIONS.md` and open an issue to discuss the idea.

## Code rules
- C++20, Qt 6, KDE Frameworks 6. No new dependencies without an ADR in `docs/DECISIONS.md`.
- Money in cents, distances in metres, volumes in millilitres, energy in Wh – as integers. Convert only for display.
- Dates as ISO-8601 text (`YYYY-MM-DD`), timestamps as UTC with `Z`.
- Every calculation in core gets a unit test before the UI uses it.
- Comments: every file starts with a short header (purpose, role), every class and non-trivial function has a Doxygen comment including units and special cases; inside functions explain *why*, not *what*. English only.

## Commits
[Conventional Commits](https://www.conventionalcommits.org/): `feat:`, `fix:`, `test:`, `docs:`, `build:`, `ci:` plus a short body explaining what and why. Run `cmake --build build && ctest --test-dir build` before every commit.

## Translations
User-visible texts use KI18n (`i18n()`, `i18nc()`, `i18np()`) in C++ and QML.
1. `tools/extract-messages.sh` updates `po/garagelog.pot` and merges new strings into existing `po/<lang>/garagelog.po`.
2. New language: `msginit -i po/garagelog.pot -o po/<lang>/garagelog.po -l <lang>`.
3. Translate with any PO editor (Lokalize, Poedit) and check with `msgfmt --check --statistics po/<lang>/garagelog.po`.

Messages from `src/core` are English only, because the core library deliberately has no KI18n dependency.

## License
By contributing you agree that your contribution is licensed under GPL-3.0-or-later, like the rest of the project.
