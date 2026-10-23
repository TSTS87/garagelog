// Implementation of the fuel calculations (role: core).

#include "fuelstats.h"

#include "database.h"

#include <QSqlError>
#include <QSqlQuery>

namespace garagelog::core {

namespace {

/** 100 km in metres, the reference distance for all per-100-km values. */
constexpr double metresPer100Km = 100'000.0;

} // namespace

FuelStats::FuelStats(const Database &database)
    : m_database(database)
{
}

QList<std::optional<Consumption>> FuelStats::consumption(const QList<FuelEntry> &entries)
{
    QList<std::optional<Consumption>> result;
    result.reserve(entries.size());

    // Odometer of the last full tank; nullopt until the first full tank,
    // because before it we do not know how full the tank was.
    std::optional<qint64> startOdometerM;
    // Everything filled since the last full tank, partial fills included:
    // the fuel of a partial fill is burnt on the way to the next full tank.
    qint64 quantity = 0;
    qint64 costC = 0;

    for (const FuelEntry &entry : entries) {
        quantity += entry.quantity;
        costC += entry.totalCostC;

        if (!entry.fullTank) {
            result.append(std::nullopt);
            continue;
        }

        const bool hasDistance = startOdometerM && entry.odometerM > *startOdometerM;
        if (hasDistance) {
            Consumption c;
            c.distanceM = entry.odometerM - *startOdometerM;
            c.quantity = quantity;
            c.costC = costC;
            c.quantityPer100Km = static_cast<double>(quantity) * metresPer100Km / static_cast<double>(c.distanceM);
            c.costPer100KmC = static_cast<double>(costC) * metresPer100Km / static_cast<double>(c.distanceM);
            result.append(c);
        } else {
            result.append(std::nullopt);
        }

        // A full tank always restarts the measurement, even after an odometer
        // going backwards, so one typo spoils at most this and the next value.
        startOdometerM = entry.odometerM;
        quantity = 0;
        costC = 0;
    }
    return result;
}

QStringList FuelStats::stationSuggestions(const QString &vehicleId)
{
    m_error.clear();
    QSqlQuery query(m_database.connection());
    // The repository trims and stores empty names as NULL, so GROUP BY on the
    // raw text is enough to remove duplicates.
    query.prepare(QStringLiteral(
        "SELECT station FROM fuel_entries WHERE vehicle_id = ? AND station IS NOT NULL "
        "GROUP BY station ORDER BY MAX(date) DESC, MAX(created_at) DESC, station"));
    query.addBindValue(vehicleId);
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot list stations: %1").arg(query.lastError().text());
        return {};
    }

    QStringList result;
    while (query.next()) {
        result.append(query.value(0).toString());
    }
    return result;
}

QString FuelStats::errorString() const
{
    return m_error;
}

} // namespace garagelog::core
