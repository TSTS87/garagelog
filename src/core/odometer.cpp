// Implementation of the odometer derivation (role: core).

#include "odometer.h"

#include "database.h"

#include <QSqlError>
#include <QSqlQuery>

namespace garagelog::core {

Odometer::Odometer(const Database &database)
    : m_database(database)
{
}

std::optional<qint64> Odometer::current(const QString &vehicleId)
{
    const auto values = offsetAndMaximum(vehicleId);
    if (!values) {
        return std::nullopt;
    }
    return values->first + values->second;
}

std::optional<qint64> Odometer::offsetFor(const QString &vehicleId, qint64 desiredM)
{
    m_error.clear();
    if (vehicleId.isEmpty()) {
        return desiredM;
    }
    const auto values = offsetAndMaximum(vehicleId);
    if (!values) {
        return std::nullopt;
    }
    return desiredM - values->second;
}

std::optional<std::pair<qint64, qint64>> Odometer::offsetAndMaximum(const QString &vehicleId)
{
    m_error.clear();
    QSqlQuery query(m_database.connection());
    // One query over all three sources: MAX() of an empty set is NULL, the
    // outer MAX() skips those NULLs, and COALESCE turns "no entries" into 0.
    // Selecting from vehicles makes an unknown id return no row at all,
    // which distinguishes it from a vehicle without entries.
    query.prepare(QStringLiteral(
        "SELECT v.odometer_offset_m, COALESCE(("
        "  SELECT MAX(m) FROM ("
        "    SELECT MAX(odometer_m) AS m FROM fuel_entries WHERE vehicle_id = v.id"
        "    UNION ALL SELECT MAX(odometer_m) FROM service_entries WHERE vehicle_id = v.id"
        "    UNION ALL SELECT MAX(odometer_end_m) FROM trips WHERE vehicle_id = v.id"
        "  )"
        "), 0) "
        "FROM vehicles v WHERE v.id = ?"));
    query.addBindValue(vehicleId);
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot compute odometer: %1").arg(query.lastError().text());
        return std::nullopt;
    }
    if (!query.next()) {
        m_error = QStringLiteral("No vehicle with id %1").arg(vehicleId);
        return std::nullopt;
    }
    return std::pair{query.value(0).toLongLong(), query.value(1).toLongLong()};
}

QString Odometer::errorString() const
{
    return m_error;
}

} // namespace garagelog::core
