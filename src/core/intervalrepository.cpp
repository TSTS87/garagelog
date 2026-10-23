// Implementation of the maintenance_intervals data access (role: core).

#include "intervalrepository.h"

#include "database.h"
#include "dbvalues.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace garagelog::core {

using namespace db;

namespace {

const QString selectColumns = QStringLiteral(
    "SELECT i.id, i.vehicle_id, i.category_id, i.name, i.every_m, i.every_months, i.active, i.notes, "
    "i.created_at, i.updated_at FROM maintenance_intervals i");

MaintenanceInterval readInterval(const QSqlQuery &query)
{
    MaintenanceInterval i;
    i.id = query.value(0).toString();
    i.vehicleId = query.value(1).toString();
    i.categoryId = query.value(2).toString();
    i.name = query.value(3).toString();
    if (!query.value(4).isNull()) {
        i.everyM = query.value(4).toLongLong();
    }
    if (!query.value(5).isNull()) {
        i.everyMonths = query.value(5).toInt();
    }
    i.active = query.value(6).toInt() != 0;
    i.notes = query.value(7).toString();
    i.createdAt = fromDbTimestamp(query.value(8));
    i.updatedAt = fromDbTimestamp(query.value(9));
    return i;
}

/** Binds the user-editable fields in the order used by INSERT and UPDATE below. */
void bindEditableFields(QSqlQuery &query, const MaintenanceInterval &i)
{
    query.addBindValue(i.vehicleId);
    query.addBindValue(i.categoryId);
    query.addBindValue(i.name);
    query.addBindValue(i.everyM ? QVariant(*i.everyM) : QVariant());
    query.addBindValue(i.everyMonths ? QVariant(*i.everyMonths) : QVariant());
    query.addBindValue(i.active ? 1 : 0);
    query.addBindValue(nullIfEmpty(i.notes));
}

void normalize(MaintenanceInterval &i)
{
    i.name = i.name.trimmed();
    i.notes = i.notes.trimmed();
}

} // namespace

IntervalRepository::IntervalRepository(const Database &database)
    : m_database(database)
{
}

bool IntervalRepository::validate(const MaintenanceInterval &interval)
{
    if (interval.vehicleId.isEmpty()) {
        m_error = QStringLiteral("Interval needs a vehicle");
    } else if (interval.categoryId.isEmpty()) {
        m_error = QStringLiteral("Interval needs a category");
    } else if (interval.name.trimmed().isEmpty()) {
        m_error = QStringLiteral("Interval needs a name");
    } else if (!interval.everyM && !interval.everyMonths) {
        m_error = QStringLiteral("Interval needs a distance, a number of months, or both");
    } else if ((interval.everyM && *interval.everyM <= 0) || (interval.everyMonths && *interval.everyMonths <= 0)) {
        m_error = QStringLiteral("Interval distance and months must be greater than zero");
    } else {
        return true;
    }
    return false;
}

bool IntervalRepository::create(MaintenanceInterval &interval)
{
    m_error.clear();
    if (!validate(interval)) {
        return false;
    }
    MaintenanceInterval i = interval;
    normalize(i);
    i.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    i.createdAt = nowUtc();
    i.updatedAt = i.createdAt;

    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral(
        "INSERT INTO maintenance_intervals (id, vehicle_id, category_id, name, every_m, every_months, active, notes, "
        "created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(i.id);
    bindEditableFields(query, i);
    query.addBindValue(toDbTimestamp(i.createdAt));
    query.addBindValue(toDbTimestamp(i.updatedAt));
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot create interval: %1").arg(query.lastError().text());
        return false;
    }
    interval = i;
    return true;
}

bool IntervalRepository::update(MaintenanceInterval &interval)
{
    m_error.clear();
    if (!validate(interval)) {
        return false;
    }
    MaintenanceInterval i = interval;
    normalize(i);
    const QDateTime now = nowUtc();
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral(
        "UPDATE maintenance_intervals SET vehicle_id = ?, category_id = ?, name = ?, every_m = ?, every_months = ?, "
        "active = ?, notes = ?, updated_at = ? WHERE id = ?"));
    bindEditableFields(query, i);
    query.addBindValue(toDbTimestamp(now));
    query.addBindValue(i.id);
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot update interval: %1").arg(query.lastError().text());
        return false;
    }
    if (query.numRowsAffected() != 1) {
        m_error = QStringLiteral("No interval with id %1").arg(i.id);
        return false;
    }
    i.updatedAt = now;
    interval = i;
    return true;
}

std::optional<MaintenanceInterval> IntervalRepository::byId(const QString &id)
{
    m_error.clear();
    QSqlQuery query(m_database.connection());
    query.prepare(selectColumns + QStringLiteral(" WHERE i.id = ?"));
    query.addBindValue(id);
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot read interval: %1").arg(query.lastError().text());
        return std::nullopt;
    }
    if (!query.next()) {
        m_error = QStringLiteral("No interval with id %1").arg(id);
        return std::nullopt;
    }
    return readInterval(query);
}

QList<MaintenanceInterval> IntervalRepository::byVehicle(const QString &vehicleId)
{
    m_error.clear();
    QSqlQuery query(m_database.connection());
    query.prepare(selectColumns + QStringLiteral(" WHERE i.vehicle_id = ? ORDER BY i.name COLLATE NOCASE, i.created_at"));
    query.addBindValue(vehicleId);
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot list intervals: %1").arg(query.lastError().text());
        return {};
    }
    QList<MaintenanceInterval> result;
    while (query.next()) {
        result.append(readInterval(query));
    }
    return result;
}

QList<MaintenanceInterval> IntervalRepository::allActive()
{
    m_error.clear();
    QSqlQuery query(m_database.connection());
    // Archived vehicles are out of use, so their reminders would only be noise.
    if (!query.exec(selectColumns
                    + QStringLiteral(" JOIN vehicles v ON v.id = i.vehicle_id WHERE i.active = 1 AND v.archived = 0 "
                                     "ORDER BY i.name COLLATE NOCASE, i.created_at"))) {
        m_error = QStringLiteral("Cannot list intervals: %1").arg(query.lastError().text());
        return {};
    }
    QList<MaintenanceInterval> result;
    while (query.next()) {
        result.append(readInterval(query));
    }
    return result;
}

bool IntervalRepository::remove(const QString &id)
{
    m_error.clear();
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral("DELETE FROM maintenance_intervals WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot delete interval: %1").arg(query.lastError().text());
        return false;
    }
    if (query.numRowsAffected() != 1) {
        m_error = QStringLiteral("No interval with id %1").arg(id);
        return false;
    }
    return true;
}

QString IntervalRepository::errorString() const
{
    return m_error;
}

} // namespace garagelog::core
