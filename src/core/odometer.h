// Current odometer reading of a vehicle, derived from its entries (role: core).
// Implements currentOdometer() from DATA_MODEL.md; the value is never stored,
// so it always reflects the latest fuel, service and trip entries.

#pragma once

#include <QString>

#include <optional>

namespace garagelog::core {

class Database;

/**
 * @brief Computes odometer readings from the entries of an open garage.
 *
 * The Database must stay open for the lifetime of this object.
 */
class Odometer
{
public:
    explicit Odometer(const Database &database);

    /**
     * @brief Current odometer of a vehicle in metres.
     *
     * = highest of fuel_entries.odometer_m, service_entries.odometer_m,
     * trips.odometer_end_m and the manually entered vehicles.odometer_reading_m
     * of this vehicle (ADR-016). NULL odometer values (e.g. a tax payment)
     * are ignored. Archived vehicles are handled like active ones.
     *
     * @param vehicleId id of the vehicle.
     * @return Metres; 0 for a vehicle without any reading. nullopt if the
     *         vehicle does not exist or the query fails; errorString() says which.
     */
    std::optional<qint64> current(const QString &vehicleId);

    /** Reason for the last nullopt from current(). */
    QString errorString() const;

    /**
     * Converts odometer_offset_m of schema 1 into odometer_reading_m and sets
     * the offset to 0. Same statement as migration 0002; JsonImporter runs it
     * after importing an export of schema 1.
     */
    static const QString convertOffsetsSql;

private:
    const Database &m_database;
    QString m_error;
};

} // namespace garagelog::core
