# GarageLog – Spezifikation v1.0 (Phase 1)

## 1. Problem
Linux-Nutzer haben kein natives, offline-fähiges Programm, um Tanken, Wartung und Kosten mehrerer Fahrzeuge zu erfassen. Bestehende Lösungen: Handy-Apps mit Konto/Werbung (Fuelio, Drivvo), Self-hosted-Web (LubeLogger, Hammond – Docker nötig), Tabellenkalkulation.

## 2. Zielgruppe
Privatpersonen mit 1–5 Fahrzeugen (Auto, Motorrad, Roller, E-Fahrzeug, Oldtimer). Keine Flotten, keine Werkstätten.

## 3. Grundsätze
- Lokal und offline. Keine Netzwerkzugriffe in Phase 1 (Flatpak ohne `--share=network`).
- Eine Datei = eine Garage (`~/.local/share/garagelog/garage.sqlite`), per Datei-Dialog auch anderswo ablegbar (Nextcloud-Ordner etc.).
- Minimalistisch: ein Fenster, Seitenleiste mit Fahrzeugen, Hauptbereich mit Tabs. Keine Assistenten, keine Onboarding-Tour.
- Fehler zeigen, nicht verschlucken (Kirigami InlineMessage).
- Alles per Tastatur bedienbar; Formulare mit Enter speicherbar.
- Mehrsprachig vorbereitet (KI18n): Deutsch und Englisch ab v1.0.

## 4. Funktionsumfang v1.0
### 4.1 Fahrzeuge
- Anlegen/Bearbeiten/Archivieren (nicht löschen, solange Einträge existieren)
- Felder: Name, Typ (Auto, Motorrad, Roller, E-Bike, Sonstiges), Marke, Modell, Baujahr, Kennzeichen, Antrieb (Benzin, Diesel, Elektro, Hybrid, Sonstiges), Tachostand aktuell (abgeleitet aus letztem Eintrag, manuell korrigierbar), Notiz
- Pro Fahrzeug ein Icon nach Typ (eigene Linien-Icons, ADR-015)

### 4.2 Tank-/Ladeeinträge
- Felder: Datum, Tachostand, Menge (l oder kWh je nach Antrieb), Gesamtpreis, Preis/Einheit (eines von beiden eingeben, das andere wird berechnet), Volltank ja/nein, Tankstelle (Freitext, Autovervollständigung aus bisherigen), Notiz
- Berechnet: Verbrauch l/100 km bzw. kWh/100 km zwischen zwei Volltank-Einträgen; Kosten/100 km
- Plausibilitätsprüfung: Tachostand < vorheriger Eintrag → Warnung, nicht Blockade

### 4.3 Service-/Wartungseinträge
- Felder: Datum, Tachostand, Art (freie Kategorie mit Vorschlägen: Ölwechsel, Reifen, Bremsen, Kette, Inspektion, TÜV/HU, Versicherung, Steuer, Reparatur, Sonstiges), Beschreibung, Kosten, Werkstatt/selbst gemacht, Notiz
- Anhänge: Dateien (PDF/Bild) werden als Kopie in `<garage-ordner>/attachments/<uuid>.<ext>` abgelegt, nicht in die DB

### 4.4 Intervalle & Erinnerungen
- Pro Fahrzeug beliebige Intervalle: Name, alle X km und/oder alle Y Monate, verknüpfte Service-Art
- Fälligkeit wird aus letztem passendem Serviceeintrag + Intervall berechnet
- Statusfarben: grün (> 20 % Rest), gelb (≤ 20 %), rot (überfällig)
- Übersichtsseite „Fällig" über alle Fahrzeuge
- Keine System-Benachrichtigungen in v1.0 (nur beim Öffnen sichtbar)

### 4.5 Auswertung
- Pro Fahrzeug: Gesamtkosten, Kosten/km, Kosten pro Monat, Verbrauchsverlauf (einfaches Liniendiagramm, QtCharts oder eigenes Canvas), Kostenverteilung nach Kategorie
- Zeitraumfilter: Jahr, letzte 12 Monate, gesamt

### 4.6 Import/Export
- Export: CSV je Tabelle, JSON-Komplettexport (alle Tabellen, schemaversioniert)
- Import: JSON-Komplettexport (eigenes Format), CSV-Import für Tankeinträge mit Spaltenzuordnung (Fuelio/Drivvo-Nutzer abholen)
- Backup: „Garage speichern unter…" kopiert SQLite + attachments/

## 5. Explizit NICHT in v1.0
GPX/Touren (Phase 2), Karten (Phase 3), OBD (Phase 4), Sync, Konten, Fahrzeugdatenbank, Benachrichtigungsdienst, Mehrbenutzer, Belegerkennung/OCR.

## 6. UI-Struktur (Kirigami)
```
ApplicationWindow
├─ GlobalDrawer: Fahrzeugliste (+ „Alle fällig", Einstellungen, Über)
└─ PageRow
   ├─ VehiclePage(vehicleId)   – Kopf: Name, Tacho, nächste Fälligkeit
   │   └─ Pill-Umschalter (ADR-015): Tanken | Service | Intervalle | Auswertung
   ├─ DuePage                  – alle fälligen Intervalle
   └─ SettingsPage             – Einheiten (km/mi, l/gal), Währung, Garage-Datei
Formulare als Kirigami.OverlaySheet oder eigene Seite (Entscheidung in DECISIONS.md)
```

## 7. Qualitätskriterien für v1.0-Release
- Alle Berechnungen (Verbrauch, Fälligkeit, Kosten/km) unit-getestet
- Flatpak baut aus dem Repo mit `flatpak-builder`
- Neues Fahrzeug + erster Tankeintrag in < 60 s ohne Doku
- Start < 1 s auf 5 Jahre alter Hardware
- Keine Abstürze bei leerer DB, bei 10.000 Einträgen flüssig
- README mit Screenshots, AppStream-Metadaten (`metainfo.xml`) vollständig
