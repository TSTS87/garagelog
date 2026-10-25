# GarageLog – Projektanweisungen für Claude Code

## Was das ist
Native Linux-Desktop-App (Qt 6 / QML / Kirigami, C++20) zur Verwaltung mehrerer Fahrzeuge:
Tankeinträge, Wartung/Service, Intervalle mit Erinnerung, Kostenübersicht.
Lokal, offline, kein Konto, keine Cloud, keine Telemetrie. Eine SQLite-Datei pro Garage.
Phase 2 (später): GPX-Touren-Import, der den Tachostand automatisch fortschreibt. Phase 3: Kartenanzeige.

Zielgruppe: Linux-Nutzer mit Auto, Motorrad, Roller, E-Fahrzeug. Veröffentlichung auf Flathub.
Lizenz: GPL-3.0-or-later. Entwickler: TSTS87. Arbeitssprache im Repo: Englisch (Code, Commits, Doku); Kommunikation mit TSTS87: Deutsch.

## Dokumente und wann sie gelesen werden
TSTS87 arbeitet pro Meilenstein in einer frischen Session (`/clear`). Es gibt deshalb KEIN Gedächtnis zwischen Sessions außer diesen Dateien und Git.

| Datei | Wann lesen |
|---|---|
| `docs/STATUS.md` | **Immer als Erstes.** Einzige Wahrheit über den Stand. Als Letztes aktualisieren. |
| `docs/ROADMAP.md` | Nur den Abschnitt des aktuellen Meilensteins (steht in STATUS.md) |
| `docs/DATA_MODEL.md` | Wenn der Meilenstein DB, Repositories oder Berechnungen berührt |
| `docs/SPEC.md` | Wenn der Meilenstein UI berührt – nur den passenden Abschnitt |
| `docs/DECISIONS.md` | Vor jeder Architekturentscheidung; neue dort anhängen |

Nicht alle Dokumente komplett lesen – das kostet Kontext. Gezielt lesen, was der Meilenstein braucht.

## Ablauf einer Session
1. `docs/STATUS.md` lesen. `git status` + `git log --oneline -5` prüfen. Bei Widerspruch zwischen STATUS.md und Repo: anhalten, TSTS87 fragen.
2. Den Abschnitt des aktuellen Meilensteins in `docs/ROADMAP.md` lesen, nur den.
3. In 2 Sätzen sagen, was ansteht. Dann arbeiten, Punkt für Punkt.
4. Nach jedem Punkt: Build + Tests, Commit.
5. Vor Ende der Session (auch wenn der Meilenstein nicht fertig ist!) `docs/STATUS.md` aktualisieren: Meilenstein, letzter Commit-Hash, Build/Tests/Start-Status, offene Punkte, Abweichungen, Hinweise für die nächste Session.
6. Ist der Meilenstein fertig (DoD aus ROADMAP.md erfüllt): STATUS.md auf den nächsten Meilenstein setzen, Historienzeile anhängen, Commit `docs: complete M0x`, dann **alles nach GitHub pushen** (`git push origin main`, Auth läuft über `gh`), und TSTS87 sagen: „M0x fertig, gepusht, du kannst /clear machen."
   - Jeder gepushte Commit hat eine aussagekräftige Message: Conventional-Commit-Zeile + Body (Englisch), der kurz erklärt, was und warum geändert wurde.
   - Der `docs: complete M0x`-Commit fasst im Body zusammen, was der Meilenstein geliefert hat.
   - Schlägt der Push fehl: nicht forcen, TSTS87 fragen.
7. TSTS87 3-Satz-Bericht auf Deutsch geben.

Wenn TSTS87 mitten in einem Meilenstein aufhört: STATUS.md trotzdem aktualisieren und committen. Ein halb fertiger Meilenstein mit ehrlichem STATUS.md ist in Ordnung; ein veraltetes STATUS.md ist das einzige echte Problem.

## Tech-Stack (fest)
- C++20, CMake ≥ 3.21, Qt 6.6+ (Core, Gui, Qml, Quick, QuickControls2, Sql)
- KDE Frameworks 6: Kirigami, KI18n, KCoreAddons, KConfig (optional: KIconThemes)
- SQLite über QSqlDatabase; Migrationen als nummerierte SQL-Dateien in `src/core/migrations/`
- Tests: QtTest (Core-Lib muss ohne GUI testbar sein)
- Paketierung: Flatpak (org.kde.Platform 6.x Runtime), Manifest in `packaging/flatpak/`
- Keine weiteren Abhängigkeiten ohne Eintrag in DECISIONS.md

## Architektur-Regeln
- `src/core/` = reine Logik-Bibliothek (Modelle, Repository-Klassen, Berechnungen, Import/Export). Darf Qt Core + Qt Sql nutzen, KEIN QtQuick, KEIN Kirigami.
- `src/app/` = QML-Anbindung (QAbstractListModel-Ableitungen, Controller als QObject mit Q_PROPERTY/Q_INVOKABLE), `main.cpp`.
- `qml/` = reine Oberfläche. Keine Geschäftslogik in QML (kein Rechnen von Verbrauch, keine SQL).
- Alle Geldbeträge als Integer in Cent, Distanzen als Integer in Metern, Volumen als Integer in Milliliter. Umrechnung nur in der Anzeige-Schicht.
- Datumsangaben als ISO-8601-Text in SQLite (`YYYY-MM-DD`), Zeitpunkte als UTC mit `Z`.
- Jede Migration ist additiv und unumkehrbar; Schemaversion in Tabelle `schema_version`.
- Core-Funktionen mit Berechnungen (Verbrauch, Kosten/km, Fälligkeit) bekommen einen Unit-Test, bevor sie in der UI verwendet werden.

## Arbeitsweise
- Nur der aktuelle Meilenstein aus STATUS.md. Nicht springen, nicht vorbereiten.
- Kleine Commits, Conventional Commits (`feat:`, `fix:`, `test:`, `docs:`, `build:`).
- Vor jedem Commit: `cmake --build build && ctest --test-dir build`.
- Keine Features außerhalb der aktuellen Phase anfangen – auch nicht „nur das Datenfeld schon mal anlegen", sofern es nicht in DATA_MODEL.md steht.
- Wenn etwas im SPEC unklar ist: kurz nachfragen (Deutsch, eine Frage, konkret), nicht raten.
- Wenn ein Build auf der Maschine von TSTS87 fehlschlägt: einen Befehl vorschlagen, auf Ausgabe warten, dann den nächsten.
- TSTS87 kennt C++ gut, QML/Kirigami neu: bei QML-Code kurz erklären, warum ein Pattern gewählt wurde (1–2 Sätze), nicht mehr.

## Kommentare im Code (Pflicht)
TSTS87 liest den Code zum Lernen (QML/Kirigami sind neu für ihn) und muss ihn nach Wochen Pause wieder verstehen. Deshalb:
- Jede Datei beginnt mit einem Kopfkommentar: Zweck der Datei, Rolle in der Architektur (core/app/qml), 2–4 Zeilen.
- Jede Klasse und jede nicht-triviale Funktion bekommt einen Doxygen-Kommentar (`/** ... */`): was sie tut, Parameter, Rückgabe, Einheiten (Cent/Meter/ml/Wh!), Sonderfälle (z. B. „liefert nullopt, wenn kein vorheriger Volltank existiert").
- Innerhalb von Funktionen: das **Warum** kommentieren, nicht das Was. `// skip partial fills, consumption is only defined between two full tanks` ja; `// increment i` nein.
- In QML: jedes Kirigami-Element, das zum ersten Mal im Projekt verwendet wird, mit 1–2 Zeilen erklären (was es ist, warum hier). Bindings, die nicht offensichtlich sind, kommentieren.
- SQL-Migrationen: jede Tabelle mit einem Kommentarblock, jede nicht selbsterklärende Spalte mit Zeilenkommentar.
- Tests: jeder Testfall mit einem Satz, welches Verhalten er absichert.
- Sprache der Kommentare: Englisch (Repo ist öffentlich geplant). Kurz, präzise, keine Romane.

## Build (lokal, CachyOS/Arch)
```
sudo pacman -S --needed base-devel cmake extra-cmake-modules qt6-base qt6-declarative kirigami ki18n kcoreaddons kconfig kquickcharts
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/bin/garagelog
```

## Nicht-Ziele (nicht bauen, nicht vorbereiten)
Cloud-Sync, Benutzerkonten, Online-Fahrzeugdatenbanken, OBD-II (frühestens Phase 4), Routing/Tourenplanung, Windows/macOS-Build (erst nach v1.0).
