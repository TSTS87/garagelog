-- Migration 0002: manual odometer as absolute reading (role: core, embedded as Qt resource).
-- ADR-016: the odometer typed into the vehicle form is a real reading, so the
-- current odometer is max(highest entry, reading) instead of entry + offset,
-- which counted the start reading of a new vehicle twice.
-- Never edit this file after release.

-- ---------------------------------------------------------------------------
-- vehicles.odometer_reading_m replaces odometer_offset_m, which stays (migrations
-- are additive) but is always 0 from now on.
-- ---------------------------------------------------------------------------
ALTER TABLE vehicles ADD COLUMN odometer_reading_m INTEGER;  -- manually entered reading in metres; NULL = none

-- ---------------------------------------------------------------------------
-- Convert existing offsets. The old form stored the typed value as offset for
-- a new vehicle (offset = start reading) and desired - highest entry for a
-- later correction. An offset of more than half the highest entry can only
-- be a start reading; smaller ones are corrections and keep the shown value.
-- Same statement as Odometer::convertOffsetsSql (used for old JSON exports).
-- ---------------------------------------------------------------------------
UPDATE vehicles SET
  odometer_reading_m = (
    SELECT CASE
      WHEN m IS NULL OR vehicles.odometer_offset_m * 2 > m THEN MAX(0, vehicles.odometer_offset_m)
      ELSE MAX(0, m + vehicles.odometer_offset_m)
    END
    FROM (SELECT MAX(x) AS m FROM (
      SELECT MAX(odometer_m) AS x FROM fuel_entries WHERE vehicle_id = vehicles.id
      UNION ALL SELECT MAX(odometer_m) FROM service_entries WHERE vehicle_id = vehicles.id
      UNION ALL SELECT MAX(odometer_end_m) FROM trips WHERE vehicle_id = vehicles.id
    ))
  ),
  odometer_offset_m = 0
WHERE odometer_offset_m <> 0;
