// Vehicle value type and its enum <-> database text mapping (role: core).
// Plain data, no database access; VehicleRepository reads and writes it.
// Units follow DATA_MODEL.md: distances in metres, timestamps in UTC.

#pragma once

#include <QDateTime>
#include <QString>

#include <optional>

namespace garagelog::core {

/** Kind of vehicle; stored as text in vehicles.type (CHECK constraint in 0001). */
enum class VehicleType {
    Car,
    Motorcycle,
    Scooter,
    EBike,
    Other,
};

/**
 * @brief Drive type; decides whether fuel_entries.quantity means ml or Wh.
 *
 * Stored as text in vehicles.powertrain. Electric means Wh, every other
 * value means ml (hybrid is treated as fuel).
 */
enum class Powertrain {
    Petrol,
    Diesel,
    Electric,
    Hybrid,
    Other,
};

/** Database text for @p type, e.g. "motorcycle". */
QString toDbString(VehicleType type);

/** Database text for @p powertrain, e.g. "electric". */
QString toDbString(Powertrain powertrain);

/** Parses vehicles.type; nullopt for text the schema does not allow. */
std::optional<VehicleType> vehicleTypeFromDb(const QString &text);

/** Parses vehicles.powertrain; nullopt for text the schema does not allow. */
std::optional<Powertrain> powertrainFromDb(const QString &text);

/**
 * @brief One row of the vehicles table.
 *
 * id, createdAt and updatedAt are set by VehicleRepository; callers fill
 * the rest. Empty QStrings stand for NULL in the optional text columns.
 */
struct Vehicle {
    QString id;                       ///< UUIDv4 without braces; empty until created
    QString name;                     ///< display name, required (not blank)
    VehicleType type = VehicleType::Car;
    QString make;
    QString model;
    std::optional<int> year;          ///< model year, nullopt = unknown
    QString plate;                    ///< licence plate
    Powertrain powertrain = Powertrain::Petrol;
    qint64 odometerOffsetM = 0;       ///< metres added to the highest recorded odometer (see Odometer)
    bool archived = false;            ///< true = hidden from the active vehicle list
    QString notes;
    QDateTime createdAt;              ///< UTC, whole seconds
    QDateTime updatedAt;              ///< UTC, whole seconds

    bool operator==(const Vehicle &) const = default;
};

} // namespace garagelog::core
