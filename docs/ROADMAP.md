# Roadmap – Phase 1 in Session-Meilensteinen

Jeder Meilenstein ist auf **eine Claude-Code-Session (2–4 h)** zugeschnitten und endet mit einem lauffähigen, committeten Zustand. Danach: `/clear`, neuer Chat, Prompt aus `docs/SESSION_PROMPT.md`.

Der aktuelle Stand steht ausschließlich in **`docs/STATUS.md`** – nicht hier. Diese Datei ist der Plan, STATUS.md ist der Zeiger.

Definition of Done (DoD) für jeden Meilenstein:
1. Alle Punkte des Meilensteins erledigt oder in STATUS.md als „verschoben nach Mxx" vermerkt
2. `cmake --build build && ctest --test-dir build` grün
3. App startet ohne Absturz
4. Commits mit Conventional Commits, Arbeitsbaum sauber
5. STATUS.md aktualisiert (Pflichtfelder, siehe Datei)

---

## M01 – Projektgerüst
- CMake: `core` (Static-Lib), `app` (Executable), `tests` (QtTest), `enable_testing()`
- `main.cpp` startet leeres `Kirigami.ApplicationWindow` mit Titel „GarageLog"
- Build-Anleitung in README stimmt mit tatsächlich nötigen Paketen überein
- Test `test_smoke` (trivial, nur damit ctest läuft)

## M02 – Datenbank & Migration
- `core/Database`: öffnet/erstellt SQLite unter `QStandardPaths::AppDataLocation`, Foreign Keys an
- Migrationsrunner: liest `core/migrations/NNNN_*.sql` aus Qt-Resource, führt fehlende aus, pflegt `schema_version`
- `0001_initial.sql` exakt nach DATA_MODEL.md, inkl. Seed der Service-Kategorien
- Tests: Migration auf leerer DB → Version 1; zweiter Aufruf idempotent; alle Tabellen existieren

## M03 – Fahrzeuge (Core)
- `core/Vehicle` (Struct), `core/VehicleRepository` (create, update, byId, all(includeArchived), archive, unarchive)
- `core/Odometer::current(vehicleId)` nach DATA_MODEL.md (max aus allen Quellen + offset)
- Tests: CRUD, Archivieren, Odometer mit 0/1/n Einträgen, Löschen mit Einträgen schlägt fehl

## M04 – Fahrzeuge (UI)
- `app/VehicleListModel` (QAbstractListModel, Rollen: id, name, type, icon, odometer)
- GlobalDrawer mit Fahrzeugliste + „Fahrzeug hinzufügen"
- `VehiclePage` mit Kopfbereich (Name, Typ-Icon, Tachostand), noch ohne Tabs
- Formular Fahrzeug anlegen/bearbeiten/archivieren
- Manueller Test: 2 Fahrzeuge anlegen, bearbeiten, archivieren, App neu starten → Daten da

## M05 – Tankeinträge (Core)
- `core/FuelEntry`, `core/FuelRepository` (CRUD, byVehicle sortiert nach date, odometer)
- `core/FuelStats::consumption(entries)` – Verbrauch zwischen Volltanks, Kosten/100 km; Elektro-Fall (Wh)
- `core/FuelStats::stationSuggestions(vehicleId)`
- Tests: 5 Fälle Verbrauch (normal, Teiltank dazwischen, erster Eintrag, Elektro, Tacho rückwärts)

## M06 – Tankeinträge (UI)
- `app/FuelListModel`, Tab-Struktur in VehiclePage (Tanken | Service | Intervalle | Auswertung – die anderen drei leer)
- Liste: Datum, Tacho, Menge, Preis, Verbrauch
- Formular: Preis/Menge/Einheitspreis – zwei eingeben, dritter wird berechnet; Volltank-Schalter; Station mit Autovervollständigung
- Plausibilitätswarnung (Kirigami.InlineMessage) bei Tacho < Vorgänger

## M07 – Service (Core)
- `core/ServiceEntry`, `core/ServiceRepository`, `core/CategoryRepository` (builtin nicht löschbar, eigene anlegbar)
- `core/AttachmentStore`: kopiert Datei nach `<garage>/attachments/<uuid>.<ext>`, löscht bei Entry-Löschung
- Tests: CRUD, Kategorie-Schutz, Attachment-Kopie und -Löschung im Temp-Ordner

## M08 – Service (UI)
- `app/ServiceListModel`, Tab „Service" mit Liste (Datum, Kategorie, Beschreibung, Kosten, Büroklammer-Icon bei Anhang)
- Formular inkl. Kategorie-Auswahl (mit „Neue Kategorie…"), Werkstatt/selbst, Datei anhängen (QtQuick.Dialogs FileDialog), Anhang öffnen (QDesktopServices)

## M09 – Intervalle & Fälligkeit (Core)
- `core/MaintenanceInterval`, `core/IntervalRepository`
- `core/DueCalculator::evaluate(interval, today)` → {status, dueOdometer, dueDate, remainingM, remainingDays}
- Tests: alle Kombinationen (nur km / nur Monate / beides) × (ok / soon / overdue / unknown) = 12 Fälle

## M10 – Intervalle & Fälligkeit (UI)
- Tab „Intervalle": Liste mit Statusfarbe, Formular
- `DuePage` (alle Fahrzeuge, nach Dringlichkeit sortiert), Eintrag im GlobalDrawer
- Statuspunkt im VehiclePage-Kopf (schlechtester Status)

## M11 – Auswertung (Core)
- `core/StatsService`: totalCost, costPerKm, costPerMonth, costByCategory, consumptionSeries – jeweils mit Zeitraum (from, to)
- Entscheidung Diagrammbibliothek (neuer ADR, löst ADR-007 ab): KQuickCharts vs QtCharts – Verfügbarkeit in org.kde.Platform-Runtime prüfen
- Tests: 4 Kennzahlen mit bekannten Daten nachrechnen

## M12 – Auswertung (UI)
- Tab „Auswertung": Kennzahlkacheln, Zeitraumfilter (Jahr / 12 Monate / gesamt), Verbrauchslinie, Kategorie-Balken

## M13 – Export/Import (Core)
- `core/JsonExporter`/`JsonImporter` (alle Tabellen, `schema_version` im Header, Attachments als Ordnerkopie)
- `core/CsvExporter` je Tabelle; `core/CsvFuelImporter` mit Spaltenzuordnung (Mapping-Struct) und Dry-Run (liefert Fehlerliste)
- Tests: JSON-Roundtrip identisch; CSV-Import mit Fuelio-Beispieldatei (in `tests/data/`)

## M14 – Export/Import & Einstellungen (UI)
- SettingsPage: Einheiten (km/mi, l/gal), Währung, Garage-Datei anzeigen/wechseln/„speichern unter"
- Menü „Daten": JSON exportieren/importieren, CSV exportieren, CSV-Tankimport mit Spaltenzuordnungs-Dialog und Vorschau

## M15 – Politur
- Tastaturbedienung aller Formulare (Enter speichert, Esc schließt), Fokusreihenfolge
- Leere Zustände (Kirigami.PlaceholderMessage) auf jeder Liste
- Performance-Test: Script erzeugt 10.000 Tankeinträge → Liste flüssig, Start < 1 s
- Fehlerbehandlung DB-Fehler (Datei schreibgeschützt, Datei gelöscht während Laufzeit)

## M16 – Übersetzung & Metadaten
- KI18n eingerichtet, alle Strings in `i18n()`, `.pot` erzeugt, `de.po` vollständig
- `metainfo.xml` vollständig (Screenshots, URLs, Release 1.0.0), `.desktop` geprüft mit `desktop-file-validate`, `appstreamcli validate`
- App-Icon (SVG) + gerenderte PNGs 48/64/128/256

## M17 – Flatpak & CI
- `flatpak-builder` baut lokal; App läuft aus dem Flatpak, Dateidialoge funktionieren (Portals)
- GitHub Actions: Build + Tests (Container mit KDE-Paketen) bei jedem Push
- README mit Screenshots, CONTRIBUTING.md, LICENSE

## M18 – Release 1.0
- SPEC §7 Qualitätskriterien einzeln prüfen und ehrlich protokollieren
- Git-Tag v1.0.0, GitHub-Release
- Flathub-Einreichung vorbereiten (Repo-Struktur, Manifest mit Git-Tag statt `dir`)
- GitHub Sponsors / Flathub-Donation-Link

## M19 – Neues Erscheinungsbild (nach v1.0.1, auf Wunsch von Thomas)
- Recherche aktueller App-Gestaltung (2026): Dashboard zuerst, Bento-Kacheln, große Kennzahlen, Pill-Navigation, Karten mit weichen Schatten; Ergebnis in ADR-015
- Eigene Linien-Icons je Fahrzeugtyp und für Tanken/Service/Intervalle/Auswertung (SVG in `qml/icons/`, einfarbig, folgen dem Farbschema)
- Fahrzeugseite: Hero-Karte (Verlauf in Typfarbe, Kennzeichen-Schild, großer Tachostand, nächste Fälligkeit), Pill-Umschalter statt TabBar, schwebender Hinzufügen-Knopf
- Listen: Zeilen mit Icon-Kachel, Monatsüberschriften, Beträge hervorgehoben; Fällig-Seite nach Status gruppiert
- Auswertung als Bento-Raster; Seitenleiste mit Marken-Kopf, Fahrzeug-Kacheln, Zähler-Badge
- DoD: hell + dunkel lesbar, Tastaturbedienung unverändert, keine neue Abhängigkeit, Build/Tests grün, Screenshots erneuert

---

## Phase 2 – Touren (nach v1.0, eigene Meilensteine dann hier ergänzen)
- GPX-Parser im Core (Distanz, Dauer, Höhenmeter) + Tests mit echten Dateien
- TripRepository, Tab „Touren", manueller Eintrag
- Trip erhöht Tachostand → fließt in DueCalculator ein
- Touren-Statistik

## Phase 3 – Karte (nur bei Nachfrage)
## Phase 4 – Optional: OBD-II, Windows-Build, Pro-Modul Belegerkennung
