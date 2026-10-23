# Datenmodell (SQLite) – verbindlich

Einheiten in der DB: Geld in **Cent** (INTEGER), Distanz in **Metern** (INTEGER), Volumen in **Millilitern** bzw. Energie in **Wattstunden** (INTEGER). Datum `TEXT` ISO-8601 `YYYY-MM-DD`, Zeitstempel `TEXT` UTC `YYYY-MM-DDTHH:MM:SSZ`. IDs sind `TEXT` UUIDv4 (stabil bei Export/Import und späterem Sync).

`trips` ist bereits enthalten, damit Phase 2 ohne Schemaumbau startet – in Phase 1 wird die Tabelle angelegt, aber nicht in der UI verwendet.

## Migration 0001 – Grundschema

```sql
CREATE TABLE schema_version (
  version INTEGER NOT NULL
);
INSERT INTO schema_version VALUES (1);

CREATE TABLE vehicles (
  id            TEXT PRIMARY KEY,
  name          TEXT NOT NULL,
  type          TEXT NOT NULL CHECK (type IN ('car','motorcycle','scooter','ebike','other')),
  make          TEXT,
  model         TEXT,
  year          INTEGER,
  plate         TEXT,
  powertrain    TEXT NOT NULL CHECK (powertrain IN ('petrol','diesel','electric','hybrid','other')),
  odometer_offset_m INTEGER NOT NULL DEFAULT 0,  -- manuelle Korrektur, wird zum höchsten Eintrag addiert
  archived      INTEGER NOT NULL DEFAULT 0,
  notes         TEXT,
  created_at    TEXT NOT NULL,
  updated_at    TEXT NOT NULL
);

CREATE TABLE fuel_entries (
  id            TEXT PRIMARY KEY,
  vehicle_id    TEXT NOT NULL REFERENCES vehicles(id) ON DELETE RESTRICT,
  date          TEXT NOT NULL,
  odometer_m    INTEGER NOT NULL,
  quantity      INTEGER NOT NULL,           -- ml (Kraftstoff) oder Wh (Strom), je nach vehicles.powertrain
  total_cost_c  INTEGER NOT NULL,
  full_tank     INTEGER NOT NULL DEFAULT 1,
  station       TEXT,
  notes         TEXT,
  created_at    TEXT NOT NULL,
  updated_at    TEXT NOT NULL
);
CREATE INDEX idx_fuel_vehicle_date ON fuel_entries(vehicle_id, date, odometer_m);

CREATE TABLE service_categories (
  id            TEXT PRIMARY KEY,
  name          TEXT NOT NULL UNIQUE,
  builtin       INTEGER NOT NULL DEFAULT 0,  -- 1 = mitgeliefert, nicht löschbar
  icon          TEXT
);

CREATE TABLE service_entries (
  id            TEXT PRIMARY KEY,
  vehicle_id    TEXT NOT NULL REFERENCES vehicles(id) ON DELETE RESTRICT,
  category_id   TEXT NOT NULL REFERENCES service_categories(id),
  date          TEXT NOT NULL,
  odometer_m    INTEGER,
  description   TEXT NOT NULL,
  cost_c        INTEGER NOT NULL DEFAULT 0,
  performed_by  TEXT CHECK (performed_by IN ('self','shop')),
  shop_name     TEXT,
  notes         TEXT,
  created_at    TEXT NOT NULL,
  updated_at    TEXT NOT NULL
);
CREATE INDEX idx_service_vehicle_date ON service_entries(vehicle_id, date);

CREATE TABLE attachments (
  id            TEXT PRIMARY KEY,
  service_entry_id TEXT NOT NULL REFERENCES service_entries(id) ON DELETE CASCADE,
  filename      TEXT NOT NULL,              -- Originalname
  stored_name   TEXT NOT NULL,              -- <uuid>.<ext> in attachments/
  mime          TEXT,
  size_bytes    INTEGER,
  created_at    TEXT NOT NULL
);

CREATE TABLE maintenance_intervals (
  id            TEXT PRIMARY KEY,
  vehicle_id    TEXT NOT NULL REFERENCES vehicles(id) ON DELETE CASCADE,
  category_id   TEXT NOT NULL REFERENCES service_categories(id),
  name          TEXT NOT NULL,
  every_m       INTEGER,                    -- NULL = kein Distanzintervall
  every_months  INTEGER,                    -- NULL = kein Zeitintervall
  active        INTEGER NOT NULL DEFAULT 1,
  notes         TEXT,
  created_at    TEXT NOT NULL,
  updated_at    TEXT NOT NULL
);

-- Phase 2 (angelegt, in Phase 1 ungenutzt)
CREATE TABLE trips (
  id            TEXT PRIMARY KEY,
  vehicle_id    TEXT NOT NULL REFERENCES vehicles(id) ON DELETE RESTRICT,
  date          TEXT NOT NULL,
  title         TEXT,
  distance_m    INTEGER NOT NULL,
  duration_s    INTEGER,
  ascent_m      INTEGER,
  odometer_start_m INTEGER,
  odometer_end_m   INTEGER,
  gpx_stored_name  TEXT,                    -- Datei in tracks/, NULL bei manuellem Eintrag
  notes         TEXT,
  created_at    TEXT NOT NULL,
  updated_at    TEXT NOT NULL
);
CREATE INDEX idx_trips_vehicle_date ON trips(vehicle_id, date);

CREATE TABLE settings (
  key           TEXT PRIMARY KEY,
  value         TEXT NOT NULL
);
-- Keys: distance_unit (km|mi), volume_unit (l|gal_us|gal_uk), currency (ISO-4217), locale
```

## Mitgelieferte Service-Kategorien (Seed, builtin=1)
oil_change, tires, brakes, chain, inspection, mot (TÜV/HU), insurance, tax, repair, battery, coolant, filters, other

## Abgeleitete Werte (nicht gespeichert, im Core berechnet)
- `currentOdometer(vehicle)` = max(odometer_m aus fuel_entries, service_entries, trips.odometer_end_m) + odometer_offset_m
- `consumption(fuelEntryN)` = Σ quantity aller Einträge nach dem letzten full_tank vor N bis einschließlich N (Teiltanks zählen mit) / (odometer(N) − odometer(letzter full_tank vor N)) · 100 km; Kosten/100 km analog mit Σ total_cost_c. Nur wenn N ein Volltank ist und es einen vorherigen Volltank gibt; sonst NULL
  - Tacho rückwärts (odometer(N) ≤ odometer(vorheriger Volltank)): NULL, N wird trotzdem neue Basis für den nächsten Volltank
  - Teiltanks vor dem ersten Volltank fließen nicht ein
- `intervalDue(interval)`:
  - letzter Service mit gleicher category_id und date ≤ heute → `lastDate`, `lastOdo`
  - dueOdo = lastOdo + every_m (falls every_m), dueDate = lastDate + every_months (falls every_months)
  - Status = schlechtester der beiden Teilstatus: `ok` (> 20 % Rest), `soon` (≤ 20 %), `overdue`
  - Ohne bisherigen Service: Status `unknown`, Hinweis „Letzten Service eintragen"
- `costPerKm(vehicle, from, to)` = (Σ fuel.total_cost_c + Σ service.cost_c) / Distanz im Zeitraum

## Invarianten (Tests schreiben)
1. Zwei fuel_entries desselben Fahrzeugs mit gleichem Datum und gleichem Tacho → Warnung in UI, in DB erlaubt
2. Vehicle mit Einträgen kann nicht gelöscht, nur archiviert werden (RESTRICT)
3. Export → Import in leere DB → identischer Datenbestand (Roundtrip-Test)
4. Migration auf leerer DB und auf DB mit Version n−1 funktioniert
