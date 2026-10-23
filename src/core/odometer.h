// Current odometer reading of a vehicle, derived from its entries (role: core).
// Implements currentOdometer() from DATA_MODEL.md; the value is never stored,
// so it always reflects the latest fuel, service and trip entries.

#pragma once

#include <QString>

#include <optional>
#include <utility>

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

    /**
     * @brief Offset that makes current() return @p desiredM.
     *
     * Used when the user corrects the odometer by hand: the entries stay as
     * they are, only vehicles.odometer_offset_m changes. The result may be
     * negative if the desired value is below the highest entry.
     *
     * @param vehicleId id of the vehicle; empty = a vehicle not created yet,
     *        which has no entries, so the offset equals @p desiredM.
     * @param desiredM desired odometer in metres.
     * @return Offset in metres, nullopt if the vehicle does not exist or the
     *         query fails.
     */
    std::optional<qint64> offsetFor(const QString &vehicleId, qint64 desiredM);

    /** Reason for the last nullopt from current() or offsetFor(). */
    QString errorString() const;

private:
    /** Offset and highest recorded entry (0 without entries) of a vehicle, nullopt on error. */
    std::optional<std::pair<qint64, qint64>> offsetAndMaximum(const QString &vehicleId);

    const Database &m_database;
    QString m_error;
};

} // namespace garagelog::core
