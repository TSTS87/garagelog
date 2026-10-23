# STATUS – Übergabedatei zwischen Sessions

**Diese Datei ist die einzige Wahrheit über den aktuellen Stand.** Claude Code liest sie als Erstes und schreibt sie als Letztes. Nach `/clear` geht nur über diese Datei die Arbeit richtig weiter.

Regeln:
- Nur Fakten, die aus Code und Git ableitbar sind. Nichts „geplant", nichts „sollte".
- Kurz halten (< 60 Zeilen). Alte Meilensteine nur als eine Zeile in der Historie.
- Pflichtfelder immer ausfüllen, auch wenn „keine".

---

## Aktueller Meilenstein
M11 – Auswertung (Core)

## Zustand
- Letzter Commit: `2e119f6` feat: add intervals tab, due overview and status in the vehicle header (+ docs)
- Build grün: ja (`cmake --build build`, Qt 6.12, KF 6.31, cmake 4.4, keine Warnungen)
- Tests grün: ja (17/17, zuletzt neu: `test_intervallistmodel`)
- App startet: ja (offscreen; Intervall-Tab, -Formular, Fällig-Seite per Screenshot geprüft). Sichtprüfung am echten Desktop (M04, M06, M08, M10) und der native FileDialog stehen aus Sichtprüfung am echten Plasma-Desktop durch Thomas steht weiterhin aus (M04 + M06)

## Offene Punkte im aktuellen Meilenstein
- alle (noch nicht begonnen)

## Verschoben / Abweichungen vom Plan
- M04: Breeze hat nur ein `car`-Icon, keins für Motorrad/Roller/E-Bike → alle Typen zeigen vorerst `car`; Zuordnung zentral in `VehicleListModel::iconForType()`. Eigene Icons frühestens M15 (Thomas fragen)
- M04: zusätzlich `Odometer::offsetFor()` (Core, getestet) für die manuelle Tachokorrektur im Formular
- M05: Verbrauchsformel mit Thomas präzisiert (DATA_MODEL.md): Teiltanks seit letztem Volltank zählen mit (Menge + Kosten); Tacho rückwärts → kein Wert, Eintrag wird neue Basis
- M05: zusätzlich `core/dbvalues.h` (gemeinsame Datum/Zeit/NULL-Helfer, namespace `core::db`, neue Konvertierungen nur dort) und `quantityUnitFor(Powertrain)` in fuelentry.h
- M06: zusätzlich Löschen von Tankeinträgen (Formular-Aktion mit `Kirigami.PromptDialog`); Plausibilität warnt auch bei Duplikat (gleiches Datum + Tacho, DATA_MODEL Invariante 1)
- M10: Fällig-Seite zeigt nur aktive Intervalle nicht archivierter Fahrzeuge; Drawer-Eintrag „Due (n overdue)“. Intervall-Formular ohne „Neue Kategorie…“ (Kategorien legt man im Service-Formular an)
- M09: Fälligkeitsregeln präzisiert und in DATA_MODEL.md ergänzt (Fälligkeitstag = overdue, Service ohne Tacho → nur Zeitteil, Zukunftseinträge zählen nicht); zusätzlich `IntervalRepository::allActive()` (ohne archivierte Fahrzeuge) und `DueCalculator::moreUrgent()` für die Sortierung
- M08: Anhänge eines neuen Eintrags werden erst beim Speichern kopiert (`newAttachments` in `ServiceEntries.save`); bestehende Anhänge werden sofort (mit Rückfrage) gelöscht. `garagelog_applib` linkt jetzt KF6::I18n (Übersetzung der Builtin-Kategorien in `ServiceListModel::categoryLabel`)
- M07: zusätzlich `CategoryRepository::byName()`, `AttachmentStore::entriesWithAttachments()` (für das Büroklammer-Icon M08); Kategorie in Benutzung → Löschen verweigert mit Klartext
- M06: Währung fest € und Datumseingabe als Textfeld (Locale-Kurzformat mit 4-stelligem Jahr oder ISO), da kein Datepicker ohne neue Abhängigkeit (kirigami-addons) – ggf. M15 mit ADR

## Bekannte Probleme
- Kirigami-intern, nicht unser Code: „Created graphical object was not placed in the graphics scene" bei jedem `pageStack.push(Component)`; Binding-Loop in `InlineMessage` bei schmalem Fenster mit Aktion
- Offscreen fehlen Icons und der KDE-Style (Schriftgrößen in der Drawer-Liste wirkten vertauscht) – am echten Desktop prüfen
- `origin/main` = GitHub, Auth über `gh` (Konto TSTS87); Claude darf `git push origin main`, Force-Push ist in `.claude/settings.json` gesperrt

## Hinweise für die nächste Session
- App-Schicht `garagelog_applib` (ohne GUI testbar): QML-Singletons `Vehicles`, `FuelEntries`, `Format` (`import org.tmx.garagelog`), in main.cpp registriert; neue Models genauso
- App-API (M10): `Intervals` (pro Fahrzeug, Properties `worstStatus`, `nextDue`, `overdueCount`) und `DueIntervals` (alle Fahrzeuge), beide `IntervalListModel`, laden bei `Vehicles.reloaded` neu. `Format.dueSummary()`, `Format.parseCount()`, Komponente `StatusDot.qml`
- App-API (M08): `ServiceEntries` (`ServiceListModel`, wie FuelEntries; `categories()`, `addCategory()`, `attachments(id)` mit file://-URL, `removeAttachment`). `entriesChanged` → `Vehicles.reload`. Intervall-Model (M10) genauso anbinden
- App-API (M06): `FuelEntries` (`FuelListModel`, vehicleId wird von VehiclePage gesetzt, neueste zuerst, save/get/remove/plausibility/stationSuggestions). main.cpp verbindet `entriesChanged` → `Vehicles.reload` und `Vehicles.reloaded` → `FuelEntries.reload`. Service-Model (M08) genauso anbinden. Preis-Dreisatz in `core/fuelprice.h` (Einheitspreis in Tausendstel Währung pro l/kWh, nicht gespeichert)
- VehiclePage hat Tabs (TabBar + StackLayout); Auswertung ist noch Platzhalter-Item – in M12 ersetzen. „Hinzufügen“-Aktion folgt dem Tab
- Offscreen-Screenshots: Scratch-Programm gegen `build/lib/*.a` + `qrc_qml.cpp.o` + `qrc_migrations_init.cpp.o`, registriert die Singletons zusätzlich als Context-Properties (sonst sieht `QQmlExpression` sie nicht), führt JS-Schritte aus und macht `grabWindow()`; nicht im Repo
- Core-API (M09): `IntervalRepository`, `DueCalculator::evaluate(interval, today)` (DB) bzw. statisch mit `LastService` + Tacho (rein, getestet) → `DueResult` {status, last/due Datum+Tacho, remainingM/-Days, remainingFraction}; `dueStatusName()` liefert "overdue"/"soon"/"ok"/"unknown" (nicht `toString` nennen – kollidiert mit QTest)
- Core-API (M07): `ServiceRepository` (odometerM optional, `PerformedBy` Unknown/Self/Shop = NULL/'self'/'shop'; `remove()` löscht auch Anhang-Dateien), `CategoryRepository` (all(): Builtins in Seed-Reihenfolge, dann eigene nach Name; Builtin-`name` ist Übersetzungsschlüssel), `AttachmentStore` (Ordner = Verzeichnis der DB-Datei + `/attachments`, `filePath(a)` zum Öffnen)
- Core-API (M05): `FuelRepository`, `FuelStats::consumption(entries)` (indexgleich zu byVehicle), `stationSuggestions`; Mengeneinheit `quantityUnitFor(powertrain)`. Seed-Kategorien: feste UUIDs in 0001, `name` = stabiler Schlüssel (z. B. `oil_change`)
- **Qt-Logs landen ohne Terminal im Journal** → beim Testen immer `QT_FORCE_STDERR_LOGGING=1` setzen, sonst sieht man keine QML-Warnungen. Testgarage statt echter Datei: `XDG_DATA_HOME=<scratch>/xdgN ./build/bin/garagelog` (Ordner neu anlegen, `rm -rf` ist gesperrt)
- Formulare nach ADR-008: eigene `ScrollablePage`, Signale `saved(id)`/`canceled()`, Navigation nur in Main.qml (`pushForm`, `openForm`, `openFuelForm`, `showVehicle`)
- QML ↔ C++: Daten als ISO-Text (`Format.parseDate/date/today`), nie als JS Date; leeres `QVariant()` kommt als `undefined` an, `QVariant::fromValue(nullptr)` als `null`; ComboBox-Index aus dem Options-Array berechnen, nicht per `indexOfValue()` im Binding
- UI-Texte englisch mit `i18n()` (KI18n eingerichtet, `KLocalizedQmlContext`); deutsche Übersetzung erst M16
- Core-API (M03): `VehicleRepository`, `Odometer.current(id)` in Metern; Enums mit `toDbString()`/`…FromDb()` in vehicle.h. Structs: `QDateTime` UTC ganze Sekunden, leere QStrings = NULL
- DB-Zugriff: `core::Database` besitzt die Verbindung; Repositories speichern `const Database&` und holen sie pro Aufruf über `connection()` → Database muss länger leben als Repo/Model. Foreign Keys sind an
- `AppDataLocation` ergibt nur dann `~/.local/share/garagelog/`, wenn KEIN OrganizationName gesetzt ist (nur Domain gesetzt) – nicht ändern
- Migrationen: `src/core/migrations/NNNN_*.sql`, zusätzlich in `qt_add_resources` in src/core/CMakeLists.txt eintragen. Der Runner setzt `schema_version` selbst (UPDATE nach jeder Datei); keine Trigger mit `;` im Body (Splitter kann das nicht). Neue QML-Dateien in `qt_add_resources` in src/app/CMakeLists.txt eintragen
- `main.cpp` nutzt `QGuiApplication`; für den Plasma-Look (Style `org.kde.desktop`) wäre `QApplication` + Qt6::Widgets nötig → nur mit ADR entscheiden
- App-ID im Manifest ist Platzhalter `org.tmx.GarageLog` – Thomas entscheidet (siehe Kommentar in `packaging/flatpak/*.yml`)

## Historie (eine Zeile pro abgeschlossenem Meilenstein)
- M01 Projektgerüst – 2026-10-23 – `1471f96`
- M02 Datenbank & Migration – 2026-10-23 – `196f452`
- M03 Fahrzeuge (Core) – 2026-10-23 – `8a035e5`
- M04 Fahrzeuge (UI) – 2026-10-23 – `1137dde`
- M05 Tankeinträge (Core) – 2026-10-23 – `68e0da8`
- M06 Tankeinträge (UI) – 2026-10-23 – `cdcaecc`
- M07 Service (Core) – 2026-10-23 – `4cbeb67`
- M08 Service (UI) – 2026-10-23 – `c70a9dc`
- M09 Intervalle & Fälligkeit (Core) – 2026-10-23 – `47a0d50`
- M10 Intervalle & Fälligkeit (UI) – 2026-10-23 – `2e119f6`
