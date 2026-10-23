// FuelEntry value type and the unit of its quantity (role: core).
// Plain data, no database access; FuelRepository reads and writes it.
// Units follow DATA_MODEL.md: metres, cents, ml (fuel) or Wh (electricity).

#pragma once

#include "vehicle.h"

#include <QDate>
#include <QDateTime>
#include <QString>

namespace garagelog::core {

/** What FuelEntry::quantity counts; decided by the vehicle's powertrain. */
enum class QuantityUnit {
    Millilitre, ///< liquid fuel (petrol, diesel, hybrid, other)
    WattHour,   ///< electricity (charging an electric vehicle)
};

/** Unit of fuel_entries.quantity for a vehicle with @p powertrain: Wh for Electric, ml otherwise. */
QuantityUnit quantityUnitFor(Powertrain powertrain);

/**
 * @brief One row of the fuel_entries table: a refuelling or charging event.
 *
 * id, createdAt and updatedAt are set by FuelRepository; callers fill the
 * rest. Empty QStrings stand for NULL in the optional text columns.
 */
struct FuelEntry {
    QString id;                ///< UUIDv4 without braces; empty until created
    QString vehicleId;         ///< owning vehicle, required
    QDate date;                ///< day of the fill-up, required
    qint64 odometerM = 0;      ///< odometer reading in metres
    qint64 quantity = 0;       ///< ml or Wh, see quantityUnitFor(); must be > 0
    qint64 totalCostC = 0;     ///< total price in cents
    bool fullTank = true;      ///< false = partial fill, cannot end a consumption segment
    QString station;           ///< free text, source of FuelStats::stationSuggestions()
    QString notes;
    QDateTime createdAt;       ///< UTC, whole seconds
    QDateTime updatedAt;       ///< UTC, whole seconds

    bool operator==(const FuelEntry &) const = default;
};

} // namespace garagelog::core
