// Implementation of the vehicles data access (role: core).

#include "vehiclerepository.h"

#include "database.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTimeZone>
#include <QUuid>

namespace garagelog::core {

namespace {

/** Column list shared by all SELECTs, matching the order readVehicle() expects. */
const QString selectColumns = QStringLiteral(
    "SELECT id, name, type, make, model, year, plate, powertrain, odometer_offset_m, "
    "archived, notes, created_at, updated_at FROM vehicles");

/** Current time in UTC, truncated to seconds so it survives the ISO text roundtrip unchanged. */
QDateTime nowUtc()
{
    return QDateTime::fromSecsSinceEpoch(QDateTime::currentSecsSinceEpoch(), QTimeZone::UTC);
}

/** UTC timestamp as stored in SQLite: YYYY-MM-DDTHH:MM:SSZ. */
QString toDbTimestamp(const QDateTime &time)
{
    return time.toUTC().toString(Qt::ISODate);
}

QDateTime fromDbTimestamp(const QVariant &value)
{
    return QDateTime::fromString(value.toString(), Qt::ISODate).toUTC();
}

/** Empty QString -> SQL NULL, so optional text columns do not fill up with ''. */
QVariant nullIfEmpty(const QString &text)
{
    return text.isEmpty() ? QVariant() : QVariant(text);
}

/**
 * Reads the current row of a query built on selectColumns.
 * Returns nullopt if type/powertrain hold text the enums do not know,
 * which only happens if someone edited the file by hand.
 */
std::optional<Vehicle> readVehicle(const QSqlQuery &query)
{
    const auto type = vehicleTypeFromDb(query.value(2).toString());
    const auto powertrain = powertrainFromDb(query.value(7).toString());
    if (!type || !powertrain) {
        return std::nullopt;
    }

    Vehicle v;
    v.id = query.value(0).toString();
    v.name = query.value(1).toString();
    v.type = *type;
    v.make = query.value(3).toString();
    v.model = query.value(4).toString();
    if (!query.value(5).isNull()) {
        v.year = query.value(5).toInt();
    }
    v.plate = query.value(6).toString();
    v.powertrain = *powertrain;
    v.odometerOffsetM = query.value(8).toLongLong();
    v.archived = query.value(9).toInt() != 0;
    v.notes = query.value(10).toString();
    v.createdAt = fromDbTimestamp(query.value(11));
    v.updatedAt = fromDbTimestamp(query.value(12));
    return v;
}

/** Binds the user-editable fields in the order used by INSERT and UPDATE below. */
void bindEditableFields(QSqlQuery &query, const Vehicle &v)
{
    query.addBindValue(v.name.trimmed());
    query.addBindValue(toDbString(v.type));
    query.addBindValue(nullIfEmpty(v.make));
    query.addBindValue(nullIfEmpty(v.model));
    query.addBindValue(v.year ? QVariant(*v.year) : QVariant());
    query.addBindValue(nullIfEmpty(v.plate));
    query.addBindValue(toDbString(v.powertrain));
    query.addBindValue(v.odometerOffsetM);
    query.addBindValue(v.archived ? 1 : 0);
    query.addBindValue(nullIfEmpty(v.notes));
}

} // namespace

VehicleRepository::VehicleRepository(const Database &database)
    : m_database(database)
{
}

bool VehicleRepository::create(Vehicle &vehicle)
{
    m_error.clear();
    if (vehicle.name.trimmed().isEmpty()) {
        m_error = QStringLiteral("Vehicle name must not be empty");
        return false;
    }

    // Work on a copy so the caller's object is only changed on success.
    Vehicle v = vehicle;
    v.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    v.name = v.name.trimmed();
    v.createdAt = nowUtc();
    v.updatedAt = v.createdAt;

    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral(
        "INSERT INTO vehicles (id, name, type, make, model, year, plate, powertrain, "
        "odometer_offset_m, archived, notes, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(v.id);
    bindEditableFields(query, v);
    query.addBindValue(toDbTimestamp(v.createdAt));
    query.addBindValue(toDbTimestamp(v.updatedAt));
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot create vehicle: %1").arg(query.lastError().text());
        return false;
    }

    vehicle = v;
    return true;
}

bool VehicleRepository::update(Vehicle &vehicle)
{
    m_error.clear();
    if (vehicle.name.trimmed().isEmpty()) {
        m_error = QStringLiteral("Vehicle name must not be empty");
        return false;
    }

    const QDateTime now = nowUtc();
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral(
        "UPDATE vehicles SET name = ?, type = ?, make = ?, model = ?, year = ?, plate = ?, "
        "powertrain = ?, odometer_offset_m = ?, archived = ?, notes = ?, updated_at = ? "
        "WHERE id = ?"));
    bindEditableFields(query, vehicle);
    query.addBindValue(toDbTimestamp(now));
    query.addBindValue(vehicle.id);
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot update vehicle: %1").arg(query.lastError().text());
        return false;
    }
    if (query.numRowsAffected() != 1) {
        m_error = QStringLiteral("No vehicle with id %1").arg(vehicle.id);
        return false;
    }

    vehicle.name = vehicle.name.trimmed();
    vehicle.updatedAt = now;
    return true;
}

std::optional<Vehicle> VehicleRepository::byId(const QString &id)
{
    m_error.clear();
    QSqlQuery query(m_database.connection());
    query.prepare(selectColumns + QStringLiteral(" WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot read vehicle: %1").arg(query.lastError().text());
        return std::nullopt;
    }
    if (!query.next()) {
        m_error = QStringLiteral("No vehicle with id %1").arg(id);
        return std::nullopt;
    }
    auto vehicle = readVehicle(query);
    if (!vehicle) {
        m_error = QStringLiteral("Vehicle %1 has an unknown type or powertrain").arg(id);
    }
    return vehicle;
}

QList<Vehicle> VehicleRepository::all(bool includeArchived)
{
    m_error.clear();
    QSqlQuery query(m_database.connection());
    // NOCASE so "audi" and "BMW" sort as a user expects; ASCII-only, good enough for names.
    query.prepare(selectColumns
                  + (includeArchived ? QString() : QStringLiteral(" WHERE archived = 0"))
                  + QStringLiteral(" ORDER BY name COLLATE NOCASE, created_at"));
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot list vehicles: %1").arg(query.lastError().text());
        return {};
    }

    QList<Vehicle> result;
    while (query.next()) {
        // A hand-edited row with an unknown enum value is skipped rather than
        // hiding all other vehicles; errorString() tells about it.
        if (auto vehicle = readVehicle(query)) {
            result.append(*vehicle);
        } else {
            m_error = QStringLiteral("Skipped vehicle %1 with unknown type or powertrain").arg(query.value(0).toString());
        }
    }
    return result;
}

bool VehicleRepository::archive(const QString &id)
{
    return setArchived(id, true);
}

bool VehicleRepository::unarchive(const QString &id)
{
    return setArchived(id, false);
}

bool VehicleRepository::setArchived(const QString &id, bool archived)
{
    m_error.clear();
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral("UPDATE vehicles SET archived = ?, updated_at = ? WHERE id = ?"));
    query.addBindValue(archived ? 1 : 0);
    query.addBindValue(toDbTimestamp(nowUtc()));
    query.addBindValue(id);
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot change archive state: %1").arg(query.lastError().text());
        return false;
    }
    if (query.numRowsAffected() != 1) {
        m_error = QStringLiteral("No vehicle with id %1").arg(id);
        return false;
    }
    return true;
}

bool VehicleRepository::remove(const QString &id)
{
    m_error.clear();
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral("DELETE FROM vehicles WHERE id = ?"));
    query.addBindValue(id);
    // Entries reference the vehicle with ON DELETE RESTRICT, so SQLite itself
    // refuses the delete; we rely on that instead of counting entries first.
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot delete vehicle (it still has entries, archive it instead): %1")
                      .arg(query.lastError().text());
        return false;
    }
    if (query.numRowsAffected() != 1) {
        m_error = QStringLiteral("No vehicle with id %1").arg(id);
        return false;
    }
    return true;
}

QString VehicleRepository::errorString() const
{
    return m_error;
}

} // namespace garagelog::core
