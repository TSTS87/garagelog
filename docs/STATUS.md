# STATUS – Übergabedatei zwischen Sessions

**Diese Datei ist die einzige Wahrheit über den aktuellen Stand.** Claude Code liest sie als Erstes und schreibt sie als Letztes. Nach `/clear` geht nur über diese Datei die Arbeit richtig weiter.

Regeln:
- Nur Fakten, die aus Code und Git ableitbar sind. Nichts „geplant", nichts „sollte".
- Kurz halten (< 60 Zeilen). Alte Meilensteine nur als eine Zeile in der Historie.
- Pflichtfelder immer ausfüllen, auch wenn „keine".

---

## Aktueller Meilenstein
M15 – Politur

## Zustand
- Letzter Commit: `3bf0fc1` feat: add settings page, unit conversion, garage files and data menu (+ docs)
- Build grün: ja (Qt 6.12, KF 6.31, keine Warnungen) · Tests grün: ja (22/22)
- App startet: ja (offscreen, Screenshots aller Seiten bis M14). Sichtprüfung am echten Plasma-Desktop durch Thomas steht aus (M04–M14), ebenso native FileDialog/FolderDialog

## Offene Punkte im aktuellen Meilenstein
- alle (noch nicht begonnen)

## Verschoben / Abweichungen vom Plan
- Breeze hat nur `car`-Icon → alle Fahrzeugtypen zeigen `car` (`VehicleListModel::iconForType()`); eigene Icons frühestens M15 (Thomas fragen)
- Datumseingabe als Textfeld (Locale-Kurzformat oder ISO), kein Datepicker ohne kirigami-addons – ggf. M15 mit ADR
- Präzisierte Regeln in DATA_MODEL.md: Verbrauch (M05), Fälligkeit (M09), Distanz/Kosten pro Monat/Durchschnittsverbrauch (M11)
- M12: Kategorie-Balken als Rectangles, nur die Verbrauchslinie nutzt KQuickCharts (ADR-009). Zeiträume enden immer „heute“
- M13/M14: JSON-Import nur in Garage ohne Fahrzeuge (ADR-010); CSV-Export in DB-Einheiten; CSV-Tankimport erwartet km/l/kWh unabhängig von den Einstellungen; Fuelio-Beispiel in `tests/data/` ist handgeschrieben
- M14: Einheiten mi + l zeigen Verbrauch weiter in l/100 km (mpg nur mit Gallonen); Währung ändert nur das Symbol
- M14: Auswertungs-Tab in schmaler Spalte (Formular daneben) schneidet die Zeitraum-Zeile rechts ab → M15

## Bekannte Probleme
- Kirigami-intern: „Created graphical object was not placed in the graphics scene" bei jedem `pageStack.push(Component)`; Binding-Loop in `InlineMessage` bei schmalem Fenster
- Offscreen fehlen Icons und KDE-Style; KQuickCharts zeichnet nur mit `QT_QUICK_BACKEND=rhi QSG_RHI_BACKEND=opengl` (Software-Backend lässt Linie weg, Vulkan stürzt offscreen ab)
- Flatpak (M17): Garage außerhalb von `~/.local/share` braucht Dateisystem-Rechte – das Dokumentportal gibt nur Einzeldateien frei, `attachments/` daneben wäre unerreichbar
- `origin/main` = GitHub, Auth über `gh` (Konto TSTS87); `git push origin main` erlaubt, Force-Push gesperrt

## Hinweise für die nächste Session
- QML-Singletons (`import org.tmx.garagelog`, alle in main.cpp registriert): `Vehicles`, `FuelEntries`, `ServiceEntries`, `Intervals`, `DueIntervals`, `Stats`, `Format`, `Garage`, `DataTransfer`. Reload-Kette: jede Datenänderung → `Vehicles.reload` → `reloaded` → Fuel/Intervals/Due/Stats; `ServiceEntries` wird bei Garage-/Einheitenwechsel und Import direkt neu geladen
- Format rechnet Einheiten um (`distanceLabel`, `volumeLabel`, `currencySymbol` als NOTIFY-Properties für Formular-Labels); `quantityInput/parseQuantity/unitPriceInput/parseUnitPrice` brauchen das `electric`-Flag. Neue Anzeige von Distanz/Menge/Geld immer über Format
- Core-Klassen: Repositories (Vehicle, Fuel, Service, Category, Interval, Settings), `AttachmentStore`, `Odometer`, `FuelStats`, `DueCalculator`, `StatsService`, `JsonExporter/Importer`, `CsvExporter`, `CsvFuelImporter`. Alle halten `const Database&` und holen die Verbindung pro Aufruf; `Garage` öffnet die Datei in-place neu
- Formulare nach ADR-008 (`ScrollablePage`, Signale `saved`/`canceled`, Navigation nur in Main.qml: `pushForm`, `showHome`, `showVehicle`, `showDue`, `showSettings`, `openCsvImport`)
- QML ↔ C++: Daten als ISO-Text, nie JS Date; `QVariant::fromValue(nullptr)` = JS `null`; ComboBox-Index aus Options-Array berechnen; nach Modellwechsel Binding neu setzen (`CsvImportPage.ColumnPicker`). Eigene Property nie `header` nennen (kollidiert mit `Page.header`)
- `dueStatusName()` statt `toString()` (kollidiert mit QTest). applib definiert `TRANSLATION_DOMAIN="garagelog"`
- **Testen:** `QT_FORCE_STDERR_LOGGING=1` (sonst Logs im Journal), eigene Garage per `XDG_DATA_HOME=<scratch>/xdgN XDG_CONFIG_HOME=<scratch>/cfgN` (sonst wird `~/.config/garagelogrc` überschrieben); `rm -rf` ist gesperrt
- Offscreen-Screenshots: Scratch-Programm gegen `build/lib/*.a` + `qrc_qml.cpp.o` + `qrc_migrations_init.cpp.o`, Singletons zusätzlich als Context-Properties, JS-Schritte per `QQmlExpression`, `grabWindow()`; nicht im Repo
- Migrationen: `src/core/migrations/NNNN_*.sql` + Eintrag in src/core/CMakeLists.txt; keine Trigger mit `;`. Neue QML-Dateien in `qt_add_resources` (src/app/CMakeLists.txt)
- `AppDataLocation` stimmt nur ohne OrganizationName – nicht ändern. `QGuiApplication` statt `QApplication` (Plasma-Style bräuchte Widgets + ADR). App-ID `org.tmx.GarageLog` ist Platzhalter – Thomas entscheidet

## Historie (eine Zeile pro abgeschlossenem Meilenstein)
- M01 Projektgerüst `1471f96` · M02 Datenbank & Migration `196f452` · M03 Fahrzeuge (Core) `8a035e5` · M04 Fahrzeuge (UI) `1137dde` – alle 2026-10-23
- M05 Tankeinträge (Core) `68e0da8` · M06 Tankeinträge (UI) `cdcaecc` – 2026-10-23
- M07 Service (Core) – 2026-10-23 – `4cbeb67`
- M08 Service (UI) – 2026-10-23 – `c70a9dc`
- M09 Intervalle & Fälligkeit (Core) – 2026-10-23 – `47a0d50`
- M10 Intervalle & Fälligkeit (UI) – 2026-10-23 – `2e119f6`
- M11 Auswertung (Core) – 2026-10-23 – `6de335f`
- M12 Auswertung (UI) – 2026-10-23 – `38b660a`
- M13 Export/Import (Core) – 2026-10-23 – `1fb7225`
- M14 Export/Import & Einstellungen (UI) – 2026-10-23 – `3bf0fc1`
