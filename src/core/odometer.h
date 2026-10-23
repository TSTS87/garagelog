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
     * = highest of fuel_entries.odometer_m, service_entries.odometer_m and
     * trips.odometer_end_m of this vehicle, plus vehicles.odometer_offset_m.
     * NULL odometer values (e.g. a tax payment) are ignored. Archived
     * vehicles are handled like active ones.
     *
     * @param vehicleId id of the vehicle.
     * @return Metres. Without any entry with an odometer value the result is
     *         just the offset (0 for a new vehicle). nullopt if the vehicle
     *         does not exist or the query fails; errorString() says which.
     */
    std::optional<qint64> current(const QString &vehicleId);

    /** Reason for the last nullopt from current(). */
    QString errorString() const;

private:
    const Database &m_database;
    QString m_error;
};

} // namespace garagelog::core
