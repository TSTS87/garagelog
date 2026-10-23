# STATUS – Übergabedatei zwischen Sessions

**Diese Datei ist die einzige Wahrheit über den aktuellen Stand.** Claude Code liest sie als Erstes und schreibt sie als Letztes. Nach `/clear` geht nur über diese Datei die Arbeit richtig weiter.

Regeln:
- Nur Fakten, die aus Code und Git ableitbar sind. Nichts „geplant", nichts „sollte".
- Kurz halten (< 60 Zeilen). Alte Meilensteine nur als eine Zeile in der Historie.
- Pflichtfelder immer ausfüllen, auch wenn „keine".

---

## Aktueller Meilenstein
M05 – Tankeinträge (Core)

## Zustand
- Letzter Commit: `1137dde` docs: record ADR-008 (forms as pages)
- Build grün: ja (`cmake --build build`, Qt 6.12, KF 6.31, cmake 4.4, keine Warnungen)
- Tests grün: ja (6/6: `appstreamtest`, `test_smoke`, `test_database`, `test_vehiclerepository`, `test_odometer`, `test_vehiclelistmodel`)
- App startet: ja (offscreen mit Testdaten; Abläufe Anlegen/Bearbeiten/Tacho korrigieren/Archivieren/Neustart per Wegwerf-Harness geprüft, Screenshots ok). Sichtprüfung am echten Plasma-Desktop durch Thomas steht aus

## Offene Punkte im aktuellen Meilenstein
- alle (noch nicht begonnen)

## Verschoben / Abweichungen vom Plan
- M04: Breeze hat nur ein `car`-Icon, keins für Motorrad/Roller/E-Bike → alle Typen zeigen vorerst `car`; Zuordnung zentral in `VehicleListModel::iconForType()`. Eigene Icons frühestens M15 (Thomas fragen)
- M04: zusätzlich `Odometer::offsetFor()` (Core, getestet) für die manuelle Tachokorrektur im Formular

## Bekannte Probleme
- Kirigami-intern, nicht unser Code: „Created graphical object was not placed in the graphics scene" bei jedem `pageStack.push(Component)`; Binding-Loop in `InlineMessage` bei schmalem Fenster mit Aktion
- Offscreen fehlen Icons und der KDE-Style (Schriftgrößen in der Drawer-Liste wirkten vertauscht) – am echten Desktop prüfen
- `origin/main` = GitHub, Auth über `gh` (Konto TSTS87); Claude darf `git push origin main`, Force-Push ist in `.claude/settings.json` gesperrt

## Hinweise für die nächste Session
- **Qt-Logs landen ohne Terminal im Journal** → beim Testen immer `QT_FORCE_STDERR_LOGGING=1` setzen, sonst sieht man keine QML-Warnungen
- Testgarage statt echter Datei: `XDG_DATA_HOME=<scratch>/xdgN ./build/bin/garagelog` (Ordner neu anlegen, `rm -rf` ist gesperrt)
- App-Schicht: `garagelog_applib` (src/app, ohne GUI testbar) enthält `VehicleListModel` und `Format`; in main.cpp als QML-Singletons `Vehicles`/`Format` (`import org.tmx.garagelog`) registriert. Neue Models genauso
- Formulare nach ADR-008: eigene `ScrollablePage`, Signale `saved(id)`/`canceled()`, Navigation nur in Main.qml (`openForm`, `showVehicle`)
- QML ↔ C++: leeres `QVariant()` kommt als `undefined` an, `QVariant::fromValue(nullptr)` als `null`; ComboBox-Index aus dem Options-Array berechnen, nicht per `indexOfValue()` im Binding
- UI-Texte englisch mit `i18n()` (KI18n eingerichtet, `KLocalizedQmlContext`); deutsche Übersetzung erst M16
- Core-API (M03): `VehicleRepository(const Database&)` (create/update/byId/all(includeArchived)/archive/unarchive/remove, Fehler via `errorString()`), `Odometer(const Database&).current(id)` in Metern; Enums `VehicleType`/`Powertrain` mit `toDbString()`/`…FromDb()` in vehicle.h
- Repositories speichern `const Database&` und holen die Verbindung pro Aufruf → Database muss länger leben als Repo/Model
- Zeitstempel in Structs: `QDateTime` UTC, ganze Sekunden; leere QStrings = NULL in optionalen Textspalten
- DB-Zugriff: `core::Database` (database.h) besitzt eine eindeutig benannte QSqlDatabase-Verbindung; Repositories holen sie über `connection()`. Foreign Keys sind an (geprüft)
- `main.cpp` öffnet `Database::defaultFilePath()` vor der QML-Engine; bei Fehler `qCritical` + Exit 1 (noch kein Dialog)
- `AppDataLocation` ergibt nur dann `~/.local/share/garagelog/`, wenn KEIN OrganizationName gesetzt ist (nur Domain gesetzt) – nicht ändern
- Migrationen: `src/core/migrations/NNNN_*.sql`, zusätzlich in `qt_add_resources` in src/core/CMakeLists.txt eintragen. Der Runner setzt `schema_version` selbst (UPDATE nach jeder Datei); keine Trigger mit `;` im Body (Splitter kann das nicht)
- Seed-Kategorien haben feste UUIDs in 0001; `name` ist ein stabiler Schlüssel (z. B. `oil_change`), Übersetzung erst in der UI
- `main.cpp` nutzt `QGuiApplication`; für den Plasma-Look (Style `org.kde.desktop`) wäre `QApplication` + Qt6::Widgets nötig → nur mit ADR entscheiden
- QML wird per `qt_add_resources` unter `qrc:/qml/` eingebettet (src/app/CMakeLists.txt); neue QML-Dateien dort eintragen
- App-ID im Manifest ist Platzhalter `org.tmx.GarageLog` – Thomas entscheidet (siehe Kommentar in `packaging/flatpak/*.yml`)

## Historie (eine Zeile pro abgeschlossenem Meilenstein)
- M01 Projektgerüst – 2026-10-23 – `1471f96`
- M02 Datenbank & Migration – 2026-10-23 – `196f452`
- M03 Fahrzeuge (Core) – 2026-10-23 – `8a035e5`
- M04 Fahrzeuge (UI) – 2026-10-23 – `1137dde`
