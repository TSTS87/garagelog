// Values derived from fuel entries: consumption and station suggestions (role: core).
// Implements consumption() from DATA_MODEL.md; nothing here is stored, so the
// results always follow the current entries.

#pragma once

#include "fuelentry.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace garagelog::core {

class Database;

/**
 * @brief Consumption and cost of the segment that ends at one full tank.
 *
 * The segment starts at the previous full tank and includes all partial
 * fills in between. Quantities are ml or Wh, depending on the vehicle
 * (see quantityUnitFor()); conversion to l or kWh happens in the UI.
 */
struct Consumption {
    qint64 distanceM = 0;          ///< metres driven since the previous full tank, always > 0
    qint64 quantity = 0;           ///< ml or Wh filled in this segment, partial fills included
    qint64 costC = 0;              ///< cents paid in this segment, partial fills included
    double quantityPer100Km = 0;   ///< ml/100 km or Wh/100 km
    double costPer100KmC = 0;      ///< cents per 100 km

    bool operator==(const Consumption &) const = default;
};

/**
 * @brief Calculations on the fuel entries of one vehicle.
 *
 * consumption() is a pure function. stationSuggestions() reads the
 * database, so the Database must stay open for the lifetime of this object.
 */
class FuelStats
{
public:
    explicit FuelStats(const Database &database);

    /**
     * @brief Consumption for each entry of one vehicle.
     *
     * Only a full tank that follows an earlier full tank gets a value:
     * quantity and cost of all entries after that earlier full tank up to
     * and including this one, divided by the distance between the two.
     * Every full tank becomes the start of the next segment, even one
     * without a value. Gives nullopt for:
     * - partial fills (their quantity counts towards the next full tank),
     * - the first full tank (and partial fills before it),
     * - a full tank whose odometer is not above the previous full tank
     *   (odometer typo or reset); it still becomes the new start.
     *
     * @param entries All entries of one vehicle in chronological order, as
     *        FuelRepository::byVehicle() returns them.
     * @return One element per entry, same index as in @p entries.
     */
    static QList<std::optional<Consumption>> consumption(const QList<FuelEntry> &entries);

    /**
     * @brief Previously used stations of a vehicle for autocompletion.
     *
     * Distinct, non-empty station names, the most recently used first
     * (by date, then creation time).
     *
     * @return Empty list for a vehicle without stations or on a query
     *         error; errorString() is set only in the error case.
     */
    QStringList stationSuggestions(const QString &vehicleId);

    /** Reason for the last failed stationSuggestions(). */
    QString errorString() const;

private:
    const Database &m_database;
    QString m_error;
};

} // namespace garagelog::core
