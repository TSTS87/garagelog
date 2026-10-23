# Architekturentscheidungen (ADR-Kurzform)

Format: Nummer, Datum, Entscheidung, Grund, Alternativen. Neue Einträge unten anhängen, alte nie löschen (nur „ersetzt durch #n" ergänzen).

## ADR-001 (2026-10-23) Qt 6 + QML + Kirigami statt GTK/libadwaita oder Flutter
Grund: Thomas kennt C++ am besten, nutzt KDE Plasma; Kirigami liefert Plasma-nativen Look, läuft aber auch unter GNOME; Qt ermöglicht späteren Windows-Build ohne Neuschreiben. Flutter-Linux sieht nicht nativ aus und ist auf Flathub unüblich; GTK hätte Rust oder C bedeutet.

## ADR-002 (2026-10-23) SQLite, eine Datei pro Garage, UUID-Schlüssel
Grund: Offline, kopierbar, in Nextcloud-Ordner legbar, kein Server. UUIDs statt Autoincrement, damit Export/Import und ein späterer Sync ohne ID-Konflikte funktionieren.

## ADR-003 (2026-10-23) Integer-Einheiten (Cent, Meter, ml, Wh) in der DB
Grund: Keine Fließkomma-Rundungsfehler bei Geld; Einheitenumrechnung (km/mi, l/gal) ausschließlich in der Anzeige.

## ADR-004 (2026-10-23) `trips`-Tabelle ab Migration 0001
Grund: Phase 2 soll ohne Schemaumbau starten; Tabelle kostet nichts, solange UI sie ignoriert.

## ADR-005 (2026-10-23) Anhänge als Dateien neben der DB, nicht als BLOB
Grund: DB bleibt klein und schnell, Dateien bleiben mit normalen Werkzeugen lesbar; Backup = Ordner kopieren.

## ADR-006 (2026-10-23) GPL-3.0-or-later, Open-Core nur für klar abgegrenzte Zusatzmodule
Grund: Reputation auf GitHub und Flathub-Akzeptanz. Mögliche spätere Pro-Module (Beleg-KI, Windows-Store-Version) als separate Repos/Plugins, der Kern bleibt vollständig frei.

## ADR-007 (2026-10-23) Diagramme: offen – ersetzt durch ADR-009
Optionen: QtCharts (einfach, in Qt 6 wieder verfügbar, GPL-kompatibel), Kirigami-Charts-Addon (KQuickCharts, nativer Look), eigenes Canvas. Entscheidung in M6, Tendenz KQuickCharts, da bereits in der KDE-Runtime enthalten.

## ADR-008 (2026-10-23) Formulare als eigene Seite im PageRow, nicht als OverlaySheet
Grund: Auf breiten Fenstern erscheint das Formular als zweite Spalte neben der Fahrzeugseite (Kontext bleibt sichtbar), auf schmalen als eigene Seite – ohne Sonderfall. Lange Formulare (Tanken, Service mit Anhang) scrollen in `Kirigami.ScrollablePage` zuverlässig; ein OverlaySheet wird bei vielen Feldern und Bildschirmtastatur unhandlich. Ein Muster für alle Formulare (Fahrzeug M04, Tanken M06, Service M08): Seite mit Aktionen Speichern/Abbrechen, Signal `saved`/`canceled`, Navigation entscheidet `Main.qml`.
Alternativen: `Kirigami.OverlaySheet` bzw. `Kirigami.Dialog` (modal, für kurze Rückfragen weiter erlaubt); FormCard aus kirigami-addons (schöner, aber zusätzliche Abhängigkeit).

## ADR-009 (2026-10-23) Diagramme mit KQuickCharts (ersetzt ADR-007)
Entscheidung: Verbrauchslinie und Kategorie-Balken der Auswertung (M12) mit KQuickCharts (`import org.kde.quickcharts`, KDE Framework).
Grund: Geprüft in der installierten Runtime org.kde.Platform 6.11: KQuickCharts, QtCharts und QtGraphs sind alle enthalten, für Flatpak also kein Zusatzmodul nötig. KQuickCharts übernimmt Farben und Stil von Kirigami (nativer Plasma-Look, Dark Mode), ist leichtgewichtig (GPU-Shader, keine Widgets) und lokal bereits installiert (`kquickcharts`); QtCharts ist in Qt 6 veraltet (Nachfolger QtGraphs) und fehlt lokal; QtGraphs ist auf 3D/aufwendige Graphen ausgelegt. Die Daten liefert C++ als einfache Listen (`StatsService`), die Diagramme sind reine Anzeige.
Abhängigkeit: nur zur Laufzeit (QML-Import), kein CMake-`find_package`; Arch-Paket `kquickcharts` in der Build-Anleitung ergänzt.
Alternativen: QtCharts (veraltet, nicht installiert), QtGraphs (schwergewichtig), eigenes Canvas (mehr Code, kein Theme-Support).

## ADR-010 (2026-10-23) Einheiten pro Garage, zuletzt geöffnete Garage in KConfig, Import nur in leere Garage
Entscheidung: Einheiten (km/mi, l/US-gal/imp-gal) und Währung stehen in der Tabelle `settings` der Garage und wirken nur in der Anzeige (`Format`); gespeichert wird weiter in Metern/ml/Cent (ADR-003). Elektrofahrzeuge bleiben bei kWh; mit Gallonen wird Verbrauch in mpg angezeigt. Welche Garage-Datei beim Start geöffnet wird, steht außerhalb der Garage in `~/.config/garagelogrc` (KConfig, `General/GaragePath`), Rückfall auf die Standard-Garage. JSON-Import nur in eine Garage ohne Fahrzeuge („Neue Garage“ anlegen, dann importieren).
Grund: Eine kopierte Garage behält ihre Einheiten; Einheitenwechsel kann nie Daten verfälschen. KConfig ist bereits im Tech-Stack. Zusammenführen zweier Garagen bräuchte Konfliktregeln (gleiche Fahrzeuge, Kategorienamen) – für v1.0 unnötig riskant.
Alternativen: Einheiten global in KConfig (passt nicht zu „eine Datei = eine Garage“); Import mit Zusammenführen (später möglich, da UUIDs).

## ADR-011 (2026-10-23) Garage-Datei überwachen statt Schreibzugriffe sperren
Entscheidung: `Database::fileState()` prüft per `QFileInfo`, ob die offene Datei noch existiert und Datei samt Ordner beschreibbar sind (Journal liegt daneben). `GarageController` ruft das über einen `QFileSystemWatcher` (Datei + Ordner) und bei jeder Fensteraktivierung auf; `Main.qml` zeigt dann ein Banner mit „Kopie speichern…“ (`saveGarageAs`, `VACUUM INTO`). Schreibfehler einer schreibgeschützten Garage meldet SQLite selbst; sie erscheinen wie bisher in den Formularen.
Grund: Unter Linux bleibt eine gelöschte, offene Datei über die Verbindung les- und schreibbar – SQLite meldet keinen Fehler, Änderungen gingen erst beim Beenden verloren. Gerade deshalb lassen sich die Daten aber noch per `VACUUM INTO` retten; eine Schreibsperre würde das Retten nicht verbessern, nur Code in jedem Repository kosten. Kein direkter Zugriff auf die SQLite-C-API (wäre eine neue Abhängigkeit).
Alternativen: vor jedem Schreiben prüfen (verteilt über alle Repositories), Verbindung bei Fehlen schließen (Daten wären verloren).

## ADR-012 (2026-10-23) Entwickler-Werkzeuge in `tools/`
Entscheidung: Nicht ausgelieferte Hilfsprogramme liegen in `tools/` (eigene CMake-Ziele, nicht installiert, nicht im Flatpak). Erstes: `garagelog-gendata` und die Bibliothek `garagelog_sampledata` (Testgarage mit 10.000 Tankeinträgen, M15), die auch `test_performance` nutzt. Sie verwenden nur `garagelog_core`.
Grund: Der Generator gehört weder in die Kern-Bibliothek (keine Produktfunktion) noch nur in die Tests (Thomas soll sich eine volle Garage ansehen können).

<!-- Nächste: ADR-013 -->
