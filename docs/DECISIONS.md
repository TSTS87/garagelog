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

## ADR-007 (2026-10-23) Diagramme: offen
Optionen: QtCharts (einfach, in Qt 6 wieder verfügbar, GPL-kompatibel), Kirigami-Charts-Addon (KQuickCharts, nativer Look), eigenes Canvas. Entscheidung in M6, Tendenz KQuickCharts, da bereits in der KDE-Runtime enthalten.

## ADR-008 (2026-10-23) Formulare als eigene Seite im PageRow, nicht als OverlaySheet
Grund: Auf breiten Fenstern erscheint das Formular als zweite Spalte neben der Fahrzeugseite (Kontext bleibt sichtbar), auf schmalen als eigene Seite – ohne Sonderfall. Lange Formulare (Tanken, Service mit Anhang) scrollen in `Kirigami.ScrollablePage` zuverlässig; ein OverlaySheet wird bei vielen Feldern und Bildschirmtastatur unhandlich. Ein Muster für alle Formulare (Fahrzeug M04, Tanken M06, Service M08): Seite mit Aktionen Speichern/Abbrechen, Signal `saved`/`canceled`, Navigation entscheidet `Main.qml`.
Alternativen: `Kirigami.OverlaySheet` bzw. `Kirigami.Dialog` (modal, für kurze Rückfragen weiter erlaubt); FormCard aus kirigami-addons (schöner, aber zusätzliche Abhängigkeit).

<!-- Nächste: ADR-009 -->
