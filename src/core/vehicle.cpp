// Enum <-> database text mapping for Vehicle (role: core).

#include "vehicle.h"

#include <array>
#include <utility>

namespace garagelog::core {

namespace {

// Single table per enum so both directions can never drift apart.
// The texts must match the CHECK constraints in migrations/0001_initial.sql.
const std::array<std::pair<VehicleType, const char *>, 5> vehicleTypeNames{{
    {VehicleType::Car, "car"},
    {VehicleType::Motorcycle, "motorcycle"},
    {VehicleType::Scooter, "scooter"},
    {VehicleType::EBike, "ebike"},
    {VehicleType::Other, "other"},
}};

const std::array<std::pair<Powertrain, const char *>, 5> powertrainNames{{
    {Powertrain::Petrol, "petrol"},
    {Powertrain::Diesel, "diesel"},
    {Powertrain::Electric, "electric"},
    {Powertrain::Hybrid, "hybrid"},
    {Powertrain::Other, "other"},
}};

template<typename Enum, std::size_t N>
QString toText(const std::array<std::pair<Enum, const char *>, N> &table, Enum value)
{
    for (const auto &[e, name] : table) {
        if (e == value) {
            return QString::fromLatin1(name);
        }
    }
    return {};
}

template<typename Enum, std::size_t N>
std::optional<Enum> fromText(const std::array<std::pair<Enum, const char *>, N> &table, const QString &text)
{
    for (const auto &[e, name] : table) {
        if (text == QLatin1String(name)) {
            return e;
        }
    }
    return std::nullopt;
}

} // namespace

QString toDbString(VehicleType type)
{
    return toText(vehicleTypeNames, type);
}

QString toDbString(Powertrain powertrain)
{
    return toText(powertrainNames, powertrain);
}

std::optional<VehicleType> vehicleTypeFromDb(const QString &text)
{
    return fromText(vehicleTypeNames, text);
}

std::optional<Powertrain> powertrainFromDb(const QString &text)
{
    return fromText(powertrainNames, text);
}

} // namespace garagelog::core
