# GarageLog

Native Linux desktop app for tracking fuel, maintenance and costs across your vehicles. Local-first, offline, no account.

Status: pre-alpha, in development. See `docs/ROADMAP.md`.

Built with Qt 6, Kirigami and C++20. License: GPL-3.0-or-later.

## Build (Arch/CachyOS)
```
sudo pacman -S --needed base-devel cmake extra-cmake-modules qt6-base qt6-declarative kirigami ki18n kcoreaddons kconfig
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build
./build/bin/garagelog
```
