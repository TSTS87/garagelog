# STATUS – Übergabedatei zwischen Sessions

**Diese Datei ist die einzige Wahrheit über den aktuellen Stand.** Claude Code liest sie als Erstes und schreibt sie als Letztes. Nach `/clear` geht nur über diese Datei die Arbeit richtig weiter.

Regeln:
- Nur Fakten, die aus Code und Git ableitbar sind. Nichts „geplant", nichts „sollte".
- Kurz halten (< 60 Zeilen). Alte Meilensteine nur als eine Zeile in der Historie.
- Pflichtfelder immer ausfüllen, auch wenn „keine".

---

## Aktueller Meilenstein
M20 – Audit-Fixes (Punkte 1–10 in ROADMAP.md). Danach: Phase 2 (Touren) hat noch keine Meilensteine, Thomas plant sie

## Zustand
- Letzter Commit: `8ab510b` docs: show the new look in screenshots and spec (+ dieser docs-Commit). M19-Look ist noch in keinem Release; metainfo-Screenshots zeigen weiter v1.0.1. Tags `v1.0.0` = `b23c57c`, `v1.0.1` = `c92c694` (nur neue Screenshots), beide als GitHub-Release. Repo ist öffentlich
- Build grün: ja (Qt 6.12, KF 6.31, keine Warnungen) · Tests grün: ja (26/26, inkl. desktop-file-validate, appstreamcli, msgfmt; de.po 260/260)
- App startet: ja (offscreen ~400 ms; Flatpak `io.github.TSTS87.GarageLog` lokal gebaut/installiert, erstes Bild 357 ms). CI (GitHub Actions: Arch-Container + Flatpak) grün. M19 hell + dunkel per Screenshot in virtueller KWin-Sitzung geprüft; Sichtprüfung am echten Plasma-Desktop durch Thomas steht aus (M04–M19, Liste in `docs/RELEASE_CHECK_1.0.md`), ebenso native FileDialog/FolderDialog (auch über das Portal im Flatpak)
- Testgarage für Thomas (bleibt bestehen): `~/GarageLog-Testgarage/garage.sqlite` – 10.000 Tank-, 389 Serviceeinträge, 3 Intervalle; öffnen über Einstellungen → Garage öffnen

## Offene Punkte im aktuellen Meilenstein
- Thomas: GitHub-Sponsors-Profil freischalten (Links in metainfo, README, `.github/FUNDING.yml` zeigen schon darauf)
- Thomas: Flathub-PR mit `packaging/flathub/io.github.TSTS87.GarageLog.yml` (v1.0.1) einreichen – Schritte in `packaging/flathub/README.md`
- Thomas: offene Kriterien 3 und 5 aus `docs/RELEASE_CHECK_1.0.md` am Desktop prüfen
- Thomas: neuen Look (M19) am Desktop ansehen; entscheiden, ob er als 1.1.0 veröffentlicht wird (dann metainfo-Release + Screenshot-URLs auf neuen Tag, Flathub-Manifest)

## Verschoben / Abweichungen vom Plan
- M19: Formulare, Einstellungen und CSV-Import behalten den Standard-Kirigami-Look (FormLayout); nur Hauptfenster, Listen, Auswertung, Seitenleiste sind umgestaltet. Fahrzeugfarbe folgt dem Typ (keine eigene Farbwahl, stünde nicht in DATA_MODEL.md)
- Datumseingabe als Textfeld (Locale-Kurzformat oder ISO), kein Datepicker ohne kirigami-addons – weiter offen, bräuchte ADR
- Präzisierte Regeln in DATA_MODEL.md: Verbrauch (M05), Fälligkeit (M09), Distanz/Kosten pro Monat/Durchschnittsverbrauch (M11)
- M12: Kategorie-Balken als Rectangles, nur die Verbrauchslinie nutzt KQuickCharts (ADR-009). Zeiträume enden immer „heute“
- M13/M14: JSON-Import nur in Garage ohne Fahrzeuge (ADR-010); CSV-Export in DB-Einheiten; CSV-Tankimport erwartet km/l/kWh unabhängig von den Einstellungen; Fuelio-Beispiel in `tests/data/` ist handgeschrieben
- M14: Einheiten mi + l zeigen Verbrauch weiter in l/100 km (mpg nur mit Gallonen); Währung ändert nur das Symbol
- M16: Fehlermeldungen aus `src/core` bleiben englisch (Core darf kein KI18n nutzen); übersetzt sind alle Texte aus `src/app` und `qml/` (de.po 255/255). App-ID jetzt `io.github.TSTS87.GarageLog` (ADR-013), QML-URI bleibt `org.tmx.garagelog`
- M18: GitHub-Release ohne Binärpaket (nur Quellcode). v1.0.1 statt verschobenem v1.0.0-Tag für die neuen Screenshots. Flathub-Manifest baut aus GitHub, Manifest-Lint sauber; Repo-Lint meldet nur Screenshot-/Icon-Spiegelung (macht Flathubs Pipeline)
- M15: Enter auf nicht-editierbarer ComboBox speichert nur über `FormComboBox`; schreibgeschützte Datei erst *nach* dem Öffnen: bestehende Verbindung schreibt weiter, Banner warnt trotzdem (ADR-011)

## Bekannte Probleme
- KQuickCharts `AxisLabels` erzeugt in der Karte keine Beschriftung → eigene 5 Labels in StatsTab (M19)
- Kirigami-intern: „Created graphical object was not placed in the graphics scene" bei jedem `pageStack.push(Component)`; Binding-Loop in `InlineMessage` bei schmalem Fenster
- Offscreen fehlen Icons und KDE-Style; KQuickCharts zeichnet nur mit `QT_QUICK_BACKEND=rhi QSG_RHI_BACKEND=opengl` (Software-Backend lässt Linie weg, Vulkan stürzt offscreen ab)
- Flatpak: nur `~/Documents` freigegeben (ADR-014) – Garagen woanders sind ohne Flatseal-Freigabe nicht mit Anhängen nutzbar
- `origin/main` = GitHub, Auth über `gh` (Konto TSTS87); `git push origin main` erlaubt, Force-Push gesperrt

## Hinweise für die nächste Session
- QML-Singletons (`import org.tmx.garagelog`, alle in main.cpp registriert): `Vehicles`, `FuelEntries`, `ServiceEntries`, `Intervals`, `DueIntervals`, `Stats`, `Format`, `Garage`, `DataTransfer`, `Style` (QML-Datei, Design-Tokens). Reload-Kette: jede Datenänderung → `Vehicles.reload` → `reloaded` → Fuel/Intervals/Due/Stats; `ServiceEntries` wird bei Garage-/Einheitenwechsel und Import direkt neu geladen
- Format rechnet Einheiten um (`distanceLabel`, `volumeLabel`, `currencySymbol` als NOTIFY-Properties für Formular-Labels); `quantityInput/parseQuantity/unitPriceInput/parseUnitPrice` brauchen das `electric`-Flag. Neue Anzeige von Distanz/Menge/Geld immer über Format
- Core-Klassen: Repositories (Vehicle, Fuel, Service, Category, Interval, Settings), `AttachmentStore`, `Odometer`, `FuelStats`, `DueCalculator`, `StatsService`, `JsonExporter/Importer`, `CsvExporter`, `CsvFuelImporter`. Alle halten `const Database&` und holen die Verbindung pro Aufruf; `Garage` öffnet die Datei in-place neu
- Look (ADR-015): Bausteine `Card`, `IconTile`, `EntryRow` (Listenzeile), `PillBar`, `HeroCard`, `StatTile`, `StatusChip`, `SectionHeader`, `EmptyState`; Typfarben in `Style.qml`. Eigene Icons `qml/icons/*.svg` sind schwarze Masken → immer `Kirigami.Icon { isMask: true; color: … }` (Alpha in `color` wird ignoriert → `opacity`), nie direkt in `PlaceholderMessage.icon`. Neue Texte auf Karten/Hero: Farben aus Kirigami.Theme oder Weiß auf Verlauf
- Formulare nach ADR-008 (`ScrollablePage`, Signale `saved`/`canceled`, Navigation nur in Main.qml: `pushForm`, `showHome`, `showVehicle`, `showDue`, `showSettings`, `openCsvImport`). Tastatur (M15): Notizen als `NotesArea`, Auswahlfelder als `FormComboBox { onSubmitted: form.save() }`, `Keys.onReturnPressed` am FormLayout, Save `StandardKey.Save`, Cancel `"Escape"`
- `Garage.fileState` ("ok"/"missing"/"readOnly") + Banner im Fenster-`header` (Kirigami verschiebt den Header um die Drawer-Breite, daher `x: -parent.x`). `Vehicles.archivedCount` für den Platzhalter
- Flatpak lokal: `flatpak run org.flatpak.Builder --user --install --force-clean build-flatpak packaging/flatpak/io.github.TSTS87.GarageLog.yml` (State-Dir muss unter $HOME liegen, nicht /tmp). `timeout` beendet `flatpak run` nicht → `flatpak kill io.github.TSTS87.GarageLog`. CI testet als Nicht-root (root ignoriert Schreibschutz)
- Testdaten: `build/bin/garagelog-gendata <datei.sqlite> [anzahl]` (tools/, ADR-012; kennt kein `--help`, legt sonst eine Datei dieses Namens an; Anzahl 0 wird abgelehnt); Startzeit messen mit `QT_LOGGING_RULES="garagelog.startup.debug=true"`
- QML ↔ C++: Daten als ISO-Text, nie JS Date; `QVariant::fromValue(nullptr)` = JS `null`; ComboBox-Index aus Options-Array berechnen; nach Modellwechsel Binding neu setzen (`CsvImportPage.ColumnPicker`). Eigene Property nie `header` nennen (kollidiert mit `Page.header`)
- Übersetzung: `tools/extract-messages.sh` erneuert `po/garagelog.pot` und mergt in `po/*/garagelog.po`; danach Übersetzungen ergänzen (msgfmt-CTest prüft Syntax). Icon-PNGs: `tools/render-icons.sh`. ECM-`appstreamtest` prüft die *installierte* metainfo (install_manifest.txt) → nach Änderungen `cmake --install build --prefix <scratch>`
- Screenshots (`docs/screenshots/`, seit 1.0.1): Scratch-Harness (s. oben) mit `QT_QPA_PLATFORM=wayland` in `dbus-run-session kwin_wayland --virtual --width 1440 --height 900 --exit-with-session <skript>`, Aufnahme je Schritt per `spectacle -b -n -a -o <datei>` (mit Breeze-Rahmen); Harness braucht `setDesktopFileName` fürs App-Icon. Offscreen-Variante: mit `QT_QPA_PLATFORMTHEME=kde` und einer `kdeglobals` mit `[Icons] Theme=breeze` im XDG_CONFIG_HOME (sonst fehlen Icons); Demo-Garage = 2× `garagelog-gendata` + Umbenennen per sqlite3
- `dueStatusName()` statt `toString()` (kollidiert mit QTest). applib definiert `TRANSLATION_DOMAIN="garagelog"`
- **Testen:** `QT_FORCE_STDERR_LOGGING=1` (sonst Logs im Journal), eigene Garage per `XDG_DATA_HOME=<scratch>/xdgN XDG_CONFIG_HOME=<scratch>/cfgN` (sonst wird `~/.config/garagelogrc` überschrieben); `rm -rf` ist gesperrt
- Screenshot-Harness (M19, nur Scratch): Kopie von main.cpp + Schrittdatei `$HARNESS_STEPS` (`js <ausdruck>` im Kontext von Main.qml, `wait <ms>`, `shot <png>` = grabWindow, `exec <shell>` z. B. spectacle); Vehicles zusätzlich als Context-Property; Tab wechseln über `objectName: "sectionBar"` der PillBar. Gebaut per eigener CMakeLists gegen `build/lib/*.a` + qrc-Objekte (find_package `KF6I18n`/`KF6Config` einzeln)
- Offscreen-Screenshots: Scratch-Programm gegen `build/lib/*.a` + `qrc_qml.cpp.o` + `qrc_migrations_init.cpp.o`, Singletons zusätzlich als Context-Properties, JS-Schritte per `QQmlExpression`, `grabWindow()`; nicht im Repo
- Migrationen: `src/core/migrations/NNNN_*.sql` + Eintrag in src/core/CMakeLists.txt; keine Trigger mit `;`. Neue QML-Dateien in `qt_add_resources` (src/app/CMakeLists.txt)
- `AppDataLocation` stimmt nur ohne OrganizationName – nicht ändern. `QGuiApplication` statt `QApplication` (Plasma-Style bräuchte Widgets + ADR).

## Historie (eine Zeile pro abgeschlossenem Meilenstein)
- M01 Projektgerüst `1471f96` · M02 Datenbank & Migration `196f452` · M03 Fahrzeuge (Core) `8a035e5` · M04 Fahrzeuge (UI) `1137dde` – alle 2026-10-23
- M05 Tankeinträge (Core) `68e0da8` · M06 Tankeinträge (UI) `cdcaecc` – 2026-10-23
- M07 Service (Core) `4cbeb67` · M08 Service (UI) `c70a9dc` – 2026-10-23
- M09 Intervalle & Fälligkeit (Core) `47a0d50` · M10 (UI) `2e119f6` · M11 Auswertung (Core) `6de335f` · M12 (UI) `38b660a` – 2026-10-23
- M13 Export/Import (Core) `1fb7225` · M14 Export/Import & Einstellungen (UI) `3bf0fc1` – 2026-10-23
- M15 Politur – 2026-10-23 – `57e3f58`
- M16 Übersetzung & Metadaten – 2026-10-23 – `140a854`
- M17 Flatpak & CI – 2026-10-24 – `2bf0132`
- M18 Release 1.0 – 2026-10-24 – Tag `v1.0.0` (`b23c57c`); 1.0.1 mit echten Screenshots `c92c694`, Repo öffentlich
- M19 Neues Erscheinungsbild – 2026-10-24 – `8ab510b` (noch nicht released)
