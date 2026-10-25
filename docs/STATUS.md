# STATUS – Übergabedatei zwischen Sessions

**Diese Datei ist die einzige Wahrheit über den aktuellen Stand.** Claude Code liest sie als Erstes und schreibt sie als Letztes. Nach `/clear` geht nur über diese Datei die Arbeit richtig weiter.

Regeln:
- Nur Fakten, die aus Code und Git ableitbar sind. Nichts „geplant", nichts „sollte".
- Kurz halten (< 60 Zeilen). Alte Meilensteine nur als eine Zeile in der Historie.
- Pflichtfelder immer ausfüllen, auch wenn „keine".

---

## Aktueller Meilenstein
Keiner – M20 (Audit-Fixes) abgeschlossen. Phase 2 (Touren) hat in ROADMAP.md noch keine Meilensteine: TSTS87 plant sie, bevor weitergearbeitet wird

## Zustand
- Letzter Code-Commit: `d284255` build: prepare release 1.1.0 (+ dieser docs-Commit). M19-Look und M20 sind noch in keinem Release; Tags `v1.0.0` = `0c8fb47`, `v1.0.1` = `e48617b`. Repo öffentlich
- Build grün: ja (Qt 6.12, KF 6.31, keine Warnungen) · Tests grün: ja (25/25 inkl. desktop-file-validate, appstreamcli, msgfmt; de.po 262/262), auch mit ASAN/UBSan (`halt_on_error`)
- App startet: ja (offscreen). Schema jetzt Version 2 (Migration 0002): eine mit dieser Version geöffnete Garage lehnt v1.0.x als „newer“ ab. Sichtprüfung am Plasma-Desktop (M04–M20, `docs/RELEASE_CHECK_1.0.md`) und native Datei-Dialoge durch TSTS87 stehen aus
- Testgarage für TSTS87: `~/GarageLog-Testgarage/garage.sqlite` (10.000 Tank-, 389 Serviceeinträge) – wird beim nächsten Öffnen auf Schema 2 migriert

## Offene Punkte
- TSTS87: GitHub-Sponsors freischalten; Flathub-PR (`packaging/flathub/README.md`); Kriterien 3 und 5 aus `docs/RELEASE_CHECK_1.0.md` am Desktop prüfen
- Release 1.1.0 (M19 + M20) vorbereitet in `d284255` (Version, metainfo-Release mit Schema-2-Hinweis, Screenshot-URLs auf `v1.1.0`, mehr Keywords). Fehlt: Desktop-Prüfung durch TSTS87, dann Tag `v1.1.0` + GitHub-Release, danach Flathub-Manifest auf Tag/Commit setzen, Lint, Flathub-PR. Bis der Tag existiert, meldet `appstreamcli validate` die Screenshots als nicht gefunden (erwartet)
- Screenshots zeigen erfundene Demodaten (Kennzeichen „M-GL 219“); Demo-Garage und Aufnahmeweg aus M19 sind nicht mehr vorhanden (lagen in /tmp)
- TSTS87: „Über“-Seite im Drawer fehlt (SPEC §6 sagt jetzt „noch offen“) – bauen (KAboutData/Kirigami.AboutPage) oder streichen?
- TSTS87: Historie lokal umgeschrieben (2026-10-25): alle Commits und Tags auf `285398251+TSTS87@users.noreply.github.com`, Klarname aus Dateien entfernt, alle Hashes neu, Force-Push durch TSTS87 erfolgt und auf GitHub geprüft (Commits, Tags, Releases). Offen, optional: GitHub-Support um Löschen der gecachten alten Commits bitten. Lokaler Branch `worktree-agent-a5ae8aa2dc7f36b5b` enthält noch die alte Historie – nie pushen

## Verschoben / Abweichungen vom Plan
- M20 bewusst nicht gemacht: gemeinsame Test-Fixture (zu viel Umbau für ~50 Zeilen), Dunkelmodus/Hover-Farbe in Style (brauchen das Kirigami.Theme des Elements), `reuseItems`/Loader für StatsTab/Diagramm-Downsampling (kein messbarer Bedarf), Single-Instance-Lock
- M20: Korrektur des Tachostands nach unten geht nicht mehr über das Formular (ADR-016); Zukunftsdaten werden weiter ohne Warnung angenommen
- Datumseingabe als Textfeld, kein Datepicker ohne kirigami-addons (bräuchte ADR)
- M13/M14: JSON-Import nur in leere Garage (ADR-010); CSV-Tankimport erwartet km/l/kWh; mi + l zeigen l/100 km
- M16: Fehlermeldungen aus `src/core` bleiben englisch (Core ohne KI18n)
- M18: GitHub-Release nur Quellcode; Flathub-Manifest baut aus GitHub (Tag v1.0.1)

## Bekannte Probleme
- KQuickCharts `AxisLabels` beschriftet nicht → eigene Labels in StatsTab
- Kirigami-intern: „Created graphical object was not placed in the graphics scene" bei jedem `pageStack.push(Component)`; Binding-Loop in `InlineMessage` bei schmalem Fenster
- Offscreen fehlen Icons/KDE-Style; KQuickCharts zeichnet nur mit `QT_QUICK_BACKEND=rhi QSG_RHI_BACKEND=opengl`
- Flatpak: nur `~/Documents` freigegeben (ADR-014); `origin/main` = GitHub über `gh`, Force-Push gesperrt

## Hinweise für die nächste Session
- QML-Singletons (main.cpp): `Vehicles`, `FuelEntries`, `ServiceEntries`, `Intervals`, `DueIntervals`, `Stats`, `Format`, `Garage`, `DataTransfer`, `Style` (QML). Reload-Kette: Datenänderung → `Vehicles.reload` → `reloaded` → Intervals/Due/Stats; `FuelEntries` lädt nach eigenem Speichern selbst und sonst nur bei `vehicleSaved`, Garagenwechsel, Import, Einheiten
- Bausteine: Formulare erben von `FormPage.qml` (Aktionen, Fehlerkopf, Löschdialog, `closing`-Sperre; Formular bindet `isNew`/`canSave`, behandelt `onSaveRequested`/`onRemoveRequested`, ruft `finishSave`/`finishRemove`). Listen = `ListCard.qml`. List-Models erben von `VehicleScopedListModel`. `Main.closeForm(page)` poppt nur die oberste Seite
- Look (ADR-015): `Card`, `IconTile`, `EntryRow`, `PillBar`, `HeroCard`, `StatTile`, `StatusChip`, `SectionHeader`, `EmptyState`; Typfarben + `fuelIcon()` in `Style.qml`; eigene Icons sind Masken (`Kirigami.Icon { isMask: true }`). Nutzertexte in Labels mit `textFormat: Text.PlainText`
- Core: Obergrenzen in `valuelimits.h` (nicht `limits.h` nennen – überdeckt den System-Header). `Database::open(path, OpenMode)` + `Database::probe()`: öffnet auf neuer Verbindung, fremde Dateien werden abgelehnt. Anhangspfade nur über `AttachmentStore::filePath()` (prüft Namen/Symlinks). `StatsService::summary()` für den Tab
- Tachostand = max(Einträge, `vehicles.odometer_reading_m`) (ADR-016); `odometer_offset_m` ist immer 0
- Übersetzung: `tools/extract-messages.sh`, dann de.po ergänzen. Icons: `tools/render-icons.sh`. Testdaten: `build/bin/garagelog-gendata <datei> [anzahl]`
- Testen: `QT_FORCE_STDERR_LOGGING=1`, eigene `XDG_DATA_HOME`/`XDG_CONFIG_HOME` (sonst wird `~/.config/garagelogrc` überschrieben); `rm -rf` ist gesperrt
- Chaos-Harness (Audit, nicht im Repo, gitignored): `.claude/worktrees/agent-a5ae8aa2dc7f36b5b/chaos/` – lädt echte `Main.qml` offscreen, `chaos/run.sh <szenario>` (u. a. `single_enter`, `escape`, `price_lag`, `screens` mit `CHAOS_SHOTS=<ordner>`). Vorher `src qml tests tools po packaging` + `CMakeLists.txt` dorthin rsyncen, `add_subdirectory(chaos)` anhängen, `cmake --build build` (ASAN/UBSan-Build). Hat eine eigene, ältere Kopie der Signal-Verdrahtung aus main.cpp
- Screenshots für metainfo: virtuelle KWin-Sitzung (`dbus-run-session kwin_wayland --virtual …`) + `spectacle`, siehe Git-Historie von M19

## Historie (eine Zeile pro abgeschlossenem Meilenstein)
- M01–M04 Gerüst, DB, Fahrzeuge `2909a65` · M05–M08 Tanken/Service `c517ad0` · M09–M12 Intervalle/Auswertung `adb82ef` · M13–M14 Export/Import `cbf5bbc` – 2026-10-23
- M15 Politur `428d4ce` · M16 Übersetzung `d69b63a` – 2026-10-23 · M17 Flatpak & CI `d169eae` – 2026-10-24
- M18 Release 1.0 – 2026-10-24 – Tag `v1.0.0` (`0c8fb47`), 1.0.1 `e48617b`, Repo öffentlich
- M19 Neues Erscheinungsbild – 2026-10-24 – `a41041c` (noch nicht released)
- M20 Audit-Fixes – 2026-10-24 – `a43fa69` (Datenschutz-, Sicherheits-, Performance-, Redundanz- und Chaos-Audit; noch nicht released)
