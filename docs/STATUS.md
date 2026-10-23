# STATUS – Übergabedatei zwischen Sessions

**Diese Datei ist die einzige Wahrheit über den aktuellen Stand.** Claude Code liest sie als Erstes und schreibt sie als Letztes. Nach `/clear` geht nur über diese Datei die Arbeit richtig weiter.

Regeln:
- Nur Fakten, die aus Code und Git ableitbar sind. Nichts „geplant", nichts „sollte".
- Kurz halten (< 60 Zeilen). Alte Meilensteine nur als eine Zeile in der Historie.
- Pflichtfelder immer ausfüllen, auch wenn „keine".

---

## Aktueller Meilenstein
M03 – Fahrzeuge (Core)

## Zustand
- Letzter Commit: `196f452` feat: open default garage database on startup
- Build grün: ja (`cmake --build build`, Qt 6.12, KF 6.31, cmake 4.4)
- Tests grün: ja (3/3: `appstreamtest`, `test_smoke`, `test_database` mit 10 Testfällen)
- App startet: ja (offscreen 3 s ohne Fehler; legt `~/.local/share/garagelog/garage.sqlite` an, Version 1, 13 Kategorien)

## Offene Punkte im aktuellen Meilenstein
- alle (noch nicht begonnen)

## Verschoben / Abweichungen vom Plan
- keine

## Bekannte Probleme
- `git push` ist in `.claude/settings.json` gesperrt → Thomas pusht selbst (VS Code „Synchronisieren" oder `git push`)

## Hinweise für die nächste Session
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
