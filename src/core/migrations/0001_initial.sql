-- Migration 0001: initial schema (role: core, embedded as Qt resource).
-- Mirrors docs/DATA_MODEL.md exactly. Units: money in cents, distance in metres,
-- volume in millilitres / energy in watt-hours. Dates are 'YYYY-MM-DD',
-- timestamps UTC 'YYYY-MM-DDTHH:MM:SSZ'. IDs are UUIDv4 text.
-- Never edit this file after release; schema changes go into a new 000N file.

-- ---------------------------------------------------------------------------
-- schema_version: single row holding the number of the last applied migration.
-- The migration runner updates it after each file; it is created here so the
-- runner can detect an empty database by the table's absence.
-- ---------------------------------------------------------------------------
CREATE TABLE schema_version (
  version INTEGER NOT NULL
);
INSERT INTO schema_version VALUES (1);

-- ---------------------------------------------------------------------------
-- vehicles: one row per car, motorcycle, scooter, e-bike, ...
-- Rows with entries cannot be deleted (RESTRICT below), only archived.
-- ---------------------------------------------------------------------------
CREATE TABLE vehicles (
  id            TEXT PRIMARY KEY,
  name          TEXT NOT NULL,
  type          TEXT NOT NULL CHECK (type IN ('car','motorcycle','scooter','ebike','other')),
  make          TEXT,
  model         TEXT,
  year          INTEGER,
  plate         TEXT,                        -- licence plate
  powertrain    TEXT NOT NULL CHECK (powertrain IN ('petrol','diesel','electric','hybrid','other')),
  odometer_offset_m INTEGER NOT NULL DEFAULT 0,  -- manual correction in metres, added to the highest recorded odometer
  archived      INTEGER NOT NULL DEFAULT 0,  -- boolean: 1 = hidden from the active vehicle list
  notes         TEXT,
  created_at    TEXT NOT NULL,
  updated_at    TEXT NOT NULL
);

-- ---------------------------------------------------------------------------
-- fuel_entries: refuelling or charging events. Consumption is derived in core
-- between two full tanks, never stored.
-- ---------------------------------------------------------------------------
CREATE TABLE fuel_entries (
  id            TEXT PRIMARY KEY,
  vehicle_id    TEXT NOT NULL REFERENCES vehicles(id) ON DELETE RESTRICT,
  date          TEXT NOT NULL,
  odometer_m    INTEGER NOT NULL,            -- odometer reading in metres
  quantity      INTEGER NOT NULL,            -- ml (fuel) or Wh (electricity), depending on vehicles.powertrain
  total_cost_c  INTEGER NOT NULL,            -- total price in cents
  full_tank     INTEGER NOT NULL DEFAULT 1,  -- boolean: 0 = partial fill, excluded as consumption end point
  station       TEXT,
  notes         TEXT,
  created_at    TEXT NOT NULL,
  updated_at    TEXT NOT NULL
);
CREATE INDEX idx_fuel_vehicle_date ON fuel_entries(vehicle_id, date, odometer_m);

-- ---------------------------------------------------------------------------
-- service_categories: grouping for service entries and intervals.
-- Built-in rows are seeded at the end of this file; users may add their own.
-- ---------------------------------------------------------------------------
CREATE TABLE service_categories (
  id            TEXT PRIMARY KEY,
  name          TEXT NOT NULL UNIQUE,        -- built-ins: stable key (e.g. 'oil_change'), translated in the UI
  builtin       INTEGER NOT NULL DEFAULT 0,  -- 1 = shipped with the app, cannot be deleted
  icon          TEXT                         -- freedesktop icon name, NULL = default icon
);

-- ---------------------------------------------------------------------------
-- service_entries: maintenance, repairs and recurring costs (insurance, tax).
-- ---------------------------------------------------------------------------
CREATE TABLE service_entries (
  id            TEXT PRIMARY KEY,
  vehicle_id    TEXT NOT NULL REFERENCES vehicles(id) ON DELETE RESTRICT,
  category_id   TEXT NOT NULL REFERENCES service_categories(id),
  date          TEXT NOT NULL,
  odometer_m    INTEGER,                     -- metres; NULL for odometer-independent costs like tax
  description   TEXT NOT NULL,
  cost_c        INTEGER NOT NULL DEFAULT 0,  -- cents
  performed_by  TEXT CHECK (performed_by IN ('self','shop')),
  shop_name     TEXT,
  notes         TEXT,
  created_at    TEXT NOT NULL,
  updated_at    TEXT NOT NULL
);
CREATE INDEX idx_service_vehicle_date ON service_entries(vehicle_id, date);

-- ---------------------------------------------------------------------------
-- attachments: receipts/photos of a service entry. The file itself lives in
-- the attachments/ directory next to the database (ADR-005), not as BLOB.
-- ---------------------------------------------------------------------------
CREATE TABLE attachments (
  id            TEXT PRIMARY KEY,
  service_entry_id TEXT NOT NULL REFERENCES service_entries(id) ON DELETE CASCADE,
  filename      TEXT NOT NULL,               -- original file name as chosen by the user
  stored_name   TEXT NOT NULL,               -- <uuid>.<ext> inside attachments/
  mime          TEXT,
  size_bytes    INTEGER,
  created_at    TEXT NOT NULL
);

-- ---------------------------------------------------------------------------
-- maintenance_intervals: recurring service rules per vehicle, by distance
-- and/or time. Due status is derived in core from the last matching service.
-- ---------------------------------------------------------------------------
CREATE TABLE maintenance_intervals (
  id            TEXT PRIMARY KEY,
  vehicle_id    TEXT NOT NULL REFERENCES vehicles(id) ON DELETE CASCADE,
  category_id   TEXT NOT NULL REFERENCES service_categories(id),
  name          TEXT NOT NULL,
  every_m       INTEGER,                     -- distance interval in metres, NULL = none
  every_months  INTEGER,                     -- time interval in months, NULL = none
  active        INTEGER NOT NULL DEFAULT 1,  -- boolean: 0 = paused, no reminders
  notes         TEXT,
  created_at    TEXT NOT NULL,
  updated_at    TEXT NOT NULL
);

-- ---------------------------------------------------------------------------
-- trips: Phase 2 (GPX import). Created now so Phase 2 needs no schema rebuild
-- (ADR-004); unused in Phase 1.
-- ---------------------------------------------------------------------------
CREATE TABLE trips (
  id            TEXT PRIMARY KEY,
  vehicle_id    TEXT NOT NULL REFERENCES vehicles(id) ON DELETE RESTRICT,
  date          TEXT NOT NULL,
  title         TEXT,
  distance_m    INTEGER NOT NULL,
  duration_s    INTEGER,                     -- seconds
  ascent_m      INTEGER,
  odometer_start_m INTEGER,
  odometer_end_m   INTEGER,
  gpx_stored_name  TEXT,                     -- file in tracks/, NULL for manual entries
  notes         TEXT,
  created_at    TEXT NOT NULL,
  updated_at    TEXT NOT NULL
);
CREATE INDEX idx_trips_vehicle_date ON trips(vehicle_id, date);

-- ---------------------------------------------------------------------------
-- settings: key/value store for per-garage preferences.
-- Keys: distance_unit (km|mi), volume_unit (l|gal_us|gal_uk), currency (ISO-4217), locale
-- ---------------------------------------------------------------------------
CREATE TABLE settings (
  key           TEXT PRIMARY KEY,
  value         TEXT NOT NULL
);

-- ---------------------------------------------------------------------------
-- Seed: built-in service categories. The UUIDs are fixed literals so every
-- garage shares the same IDs, which keeps export/import between garages stable.
-- ---------------------------------------------------------------------------
INSERT INTO service_categories (id, name, builtin) VALUES
  ('c8d1b3b4-a66f-4682-8ab3-7b3181a13b91', 'oil_change', 1),
  ('0a1bdd49-ac97-4f0f-9ce3-0b1472805479', 'tires',      1),
  ('c0efe4b4-aed4-4608-a855-82a4c627aea3', 'brakes',     1),
  ('089395e3-63e6-4180-bd37-29fb7cedd74d', 'chain',      1),
  ('b6065c4c-ef33-4f96-b395-f81f88624ac8', 'inspection', 1),
  ('868dca35-a408-464e-8754-446019787c46', 'mot',        1),  -- TÜV/HU, statutory roadworthiness test
  ('a35af9f2-02c2-4d1d-a471-9e402d7281c3', 'insurance',  1),
  ('c73a0bec-0e94-4d53-84be-f4307ea371b3', 'tax',        1),
  ('0ece6676-4470-4be1-83ea-a607616f33b4', 'repair',     1),
  ('a8c7e8e5-9288-41d3-a0c5-046a9957bb29', 'battery',    1),
  ('c864efd7-a112-46e5-a13c-524ccf87191e', 'coolant',    1),
  ('2ab2971e-6439-4060-8374-31322a301392', 'filters',    1),
  ('7f42c1ed-94a9-407a-9b27-28f31c46fb99', 'other',      1);
