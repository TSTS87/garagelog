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

const QString Odometer::convertOffsetsSql = QStringLiteral(
    "UPDATE vehicles SET "
    "  odometer_reading_m = ("
    "    SELECT CASE"
    "      WHEN m IS NULL OR vehicles.odometer_offset_m * 2 > m THEN MAX(0, vehicles.odometer_offset_m)"
    "      ELSE MAX(0, m + vehicles.odometer_offset_m)"
    "    END"
    "    FROM (SELECT MAX(x) AS m FROM ("
    "      SELECT MAX(odometer_m) AS x FROM fuel_entries WHERE vehicle_id = vehicles.id"
    "      UNION ALL SELECT MAX(odometer_m) FROM service_entries WHERE vehicle_id = vehicles.id"
    "      UNION ALL SELECT MAX(odometer_end_m) FROM trips WHERE vehicle_id = vehicles.id"
    "    ))"
    "  ),"
    "  odometer_offset_m = 0 "
    "WHERE odometer_offset_m <> 0");

std::optional<qint64> Odometer::current(const QString &vehicleId)
{
    m_error.clear();
    QSqlQuery query(m_database.connection());
    // One query over all four sources: MAX() of an empty set is NULL, the
    // outer MAX() skips those NULLs, and COALESCE turns "no reading" into 0.
    // Selecting from vehicles makes an unknown id return no row at all,
    // which distinguishes it from a vehicle without entries.
    query.prepare(QStringLiteral(
        "SELECT COALESCE(("
        "  SELECT MAX(m) FROM ("
        "    SELECT MAX(odometer_m) AS m FROM fuel_entries WHERE vehicle_id = v.id"
        "    UNION ALL SELECT MAX(odometer_m) FROM service_entries WHERE vehicle_id = v.id"
        "    UNION ALL SELECT MAX(odometer_end_m) FROM trips WHERE vehicle_id = v.id"
        "    UNION ALL SELECT v.odometer_reading_m"
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
    return query.value(0).toLongLong();
}

QString Odometer::errorString() const
{
    return m_error;
}

} // namespace garagelog::core
