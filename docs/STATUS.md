# STATUS – Übergabedatei zwischen Sessions

**Diese Datei ist die einzige Wahrheit über den aktuellen Stand.** Claude Code liest sie als Erstes und schreibt sie als Letztes. Nach `/clear` geht nur über diese Datei die Arbeit richtig weiter.

Regeln:
- Nur Fakten, die aus Code und Git ableitbar sind. Nichts „geplant", nichts „sollte".
- Kurz halten (< 60 Zeilen). Alte Meilensteine nur als eine Zeile in der Historie.
- Pflichtfelder immer ausfüllen, auch wenn „keine".

---

## Aktueller Meilenstein
M04 – Fahrzeuge (UI)

## Zustand
- Letzter Commit: `8a035e5` feat: derive current odometer from vehicle entries
- Build grün: ja (`cmake --build build`, Qt 6.12, KF 6.31, cmake 4.4, keine Warnungen)
- Tests grün: ja (5/5: `appstreamtest`, `test_smoke`, `test_database`, `test_vehiclerepository`, `test_odometer`)
- App startet: ja (offscreen 3 s ohne Fehler)

## Offene Punkte im aktuellen Meilenstein
- alle (noch nicht begonnen)

## Verschoben / Abweichungen vom Plan
- M03: `VehicleRepository::remove()` zusätzlich zur Roadmap-Liste, weil der Test „Löschen mit Einträgen schlägt fehl" eine Löschfunktion braucht

## Bekannte Probleme
- Noch nie nach GitHub gepusht: `origin/main` enthält nur den von GitHub erzeugten Commit `b29d6f1` (Einzeilen-README), ohne gemeinsame Historie mit lokal → erster Push braucht `--force` (Thomas muss zustimmen)
- `git push` ist in `.claude/settings.json` unter `deny` gesperrt, CLAUDE.md verlangt aber Push nach jedem Meilenstein → Thomas muss den Eintrag entfernen, sonst pusht er selbst
- GitHub-Auth läuft über `gh` (`gh auth setup-git`, Konto TSTS87)

## Hinweise für die nächste Session
- Core-API für M04: `VehicleRepository(const Database&)` (create/update/byId/all(includeArchived)/archive/unarchive/remove, Fehler via `errorString()`), `Odometer(const Database&).current(id)` in Metern; Enums `VehicleType`/`Powertrain` mit `toDbString()`/`…FromDb()` in vehicle.h
- Repositories speichern `const Database&` und holen die Verbindung pro Aufruf → Database muss länger leben als Repo/Model
- Zeitstempel in Structs: `QDateTime` UTC, ganze Sekunden; leere QStrings = NULL in optionalen Textspalten
- DB-Zugriff: `core::Database` (database.h) besitzt eine eindeutig benannte QSqlDatabase-Verbindung; Repositories holen sie über `connection()`. Foreign Keys sind an (geprüft)
- `main.cpp` öffnet `Database::defaultFilePath()` vor der QML-Engine; bei Fehler `qCritical` + Exit 1 (noch kein Dialog)
- `AppDataLocation` ergibt nur dann `~/.local/share/garagelog/`, wenn KEIN OrganizationName gesetzt ist (nur Domain gesetzt) – nicht ändern
- Migrationen: `src/core/migrations/NNNN_*.sql`, zusätzlich in `qt_add_resources` in src/core/CMakeLists.txt eintragen. Der Runner setzt `schema_version` selbst (UPDATE nach jeder Datei); keine Trigger mit `;` im Body (Splitter kann das nicht)
- Seed-Kategorien haben feste UUIDs in 0001; `name` ist ein stabiler Schlüssel (z. B. `oil_change`), Übersetzung erst in der UI
- `main.cpp` nutzt `QGuiApplication`; für den Plasma-Look (Style `org.kde.desktop`) wäre `QApplication` + Qt6::Widgets nötig → nur mit ADR entscheiden
- QML wird per `qt_add_resources` als `qrc:/qml/Main.qml` eingebettet (src/app/CMakeLists.txt); neue QML-Dateien dort eintragen
- App-ID im Manifest ist Platzhalter `org.tmx.GarageLog` – Thomas entscheidet (siehe Kommentar in `packaging/flatpak/*.yml`)

## Historie (eine Zeile pro abgeschlossenem Meilenstein)
- M01 Projektgerüst – 2026-10-23 – `1471f96`
- M02 Datenbank & Migration – 2026-10-23 – `196f452`
- M03 Fahrzeuge (Core) – 2026-10-23 – `8a035e5`
