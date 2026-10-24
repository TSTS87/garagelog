// Implementation of the service_entries data access (role: core).

#include "servicerepository.h"

#include "attachmentstore.h"
#include "database.h"
#include "dbvalues.h"
#include "valuelimits.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace garagelog::core {

using namespace db;

namespace {

const QString selectColumns = QStringLiteral(
    "SELECT id, vehicle_id, category_id, date, odometer_m, description, cost_c, performed_by, "
    "shop_name, notes, created_at, updated_at FROM service_entries");

ServiceEntry readEntry(const QSqlQuery &query)
{
    ServiceEntry e;
    e.id = query.value(0).toString();
    e.vehicleId = query.value(1).toString();
    e.categoryId = query.value(2).toString();
    e.date = fromDbDate(query.value(3));
    if (!query.value(4).isNull()) {
        e.odometerM = query.value(4).toLongLong();
    }
    e.description = query.value(5).toString();
    e.costC = query.value(6).toLongLong();
    // The CHECK constraint only allows valid texts, so the fallback never applies.
    e.performedBy = performedByFromDb(query.value(7).toString()).value_or(PerformedBy::Unknown);
    e.shopName = query.value(8).toString();
    e.notes = query.value(9).toString();
    e.createdAt = fromDbTimestamp(query.value(10));
    e.updatedAt = fromDbTimestamp(query.value(11));
    return e;
}

/** Binds the user-editable fields in the order used by INSERT and UPDATE below. */
void bindEditableFields(QSqlQuery &query, const ServiceEntry &e)
{
    query.addBindValue(e.vehicleId);
    query.addBindValue(e.categoryId);
    query.addBindValue(toDbDate(e.date));
    query.addBindValue(e.odometerM ? QVariant(*e.odometerM) : QVariant());
    query.addBindValue(e.description);
    query.addBindValue(e.costC);
    query.addBindValue(nullIfEmpty(toDbString(e.performedBy)));
    query.addBindValue(nullIfEmpty(e.shopName));
    query.addBindValue(nullIfEmpty(e.notes));
}

void normalize(ServiceEntry &e)
{
    e.description = e.description.trimmed();
    e.shopName = e.shopName.trimmed();
    e.notes = e.notes.trimmed();
}

} // namespace

ServiceRepository::ServiceRepository(const Database &database)
    : m_database(database)
{
}

bool ServiceRepository::validate(const ServiceEntry &entry)
{
    if (entry.vehicleId.isEmpty()) {
        m_error = QStringLiteral("Service entry needs a vehicle");
    } else if (entry.categoryId.isEmpty()) {
        m_error = QStringLiteral("Service entry needs a category");
    } else if (!limits::isStorableDate(entry.date)) {
        m_error = QStringLiteral("Service entry needs a valid date between %1 and %2").arg(limits::minYear).arg(limits::maxYear);
    } else if (entry.odometerM && *entry.odometerM < 0) {
        m_error = QStringLiteral("Odometer must not be negative");
    } else if (entry.odometerM && *entry.odometerM > limits::maxDistanceM) {
        m_error = QStringLiteral("Odometer is too large");
    } else if (entry.description.trimmed().isEmpty()) {
        m_error = QStringLiteral("Service entry needs a description");
    } else if (entry.costC < 0) {
        m_error = QStringLiteral("Cost must not be negative");
    } else if (entry.costC > limits::maxMoneyC) {
        m_error = QStringLiteral("Cost is too large");
    } else {
        return true;
    }
    return false;
}

bool ServiceRepository::create(ServiceEntry &entry)
{
    m_error.clear();
    if (!validate(entry)) {
        return false;
    }

    ServiceEntry e = entry;
    normalize(e);
    e.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    e.createdAt = nowUtc();
    e.updatedAt = e.createdAt;

    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral(
        "INSERT INTO service_entries (id, vehicle_id, category_id, date, odometer_m, description, cost_c, "
        "performed_by, shop_name, notes, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(e.id);
    bindEditableFields(query, e);
    query.addBindValue(toDbTimestamp(e.createdAt));
    query.addBindValue(toDbTimestamp(e.updatedAt));
    // Unknown vehicle or category ids are rejected by the foreign keys.
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot create service entry: %1").arg(query.lastError().text());
        return false;
    }

    entry = e;
    return true;
}

bool ServiceRepository::update(ServiceEntry &entry)
{
    m_error.clear();
    if (!validate(entry)) {
        return false;
    }

    ServiceEntry e = entry;
    normalize(e);
    const QDateTime now = nowUtc();
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral(
        "UPDATE service_entries SET vehicle_id = ?, category_id = ?, date = ?, odometer_m = ?, description = ?, "
        "cost_c = ?, performed_by = ?, shop_name = ?, notes = ?, updated_at = ? WHERE id = ?"));
    bindEditableFields(query, e);
    query.addBindValue(toDbTimestamp(now));
    query.addBindValue(e.id);
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot update service entry: %1").arg(query.lastError().text());
        return false;
    }
    if (query.numRowsAffected() != 1) {
        m_error = QStringLiteral("No service entry with id %1").arg(e.id);
        return false;
    }

    e.updatedAt = now;
    entry = e;
    return true;
}

std::optional<ServiceEntry> ServiceRepository::byId(const QString &id)
{
    m_error.clear();
    QSqlQuery query(m_database.connection());
    query.prepare(selectColumns + QStringLiteral(" WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot read service entry: %1").arg(query.lastError().text());
        return std::nullopt;
    }
    if (!query.next()) {
        m_error = QStringLiteral("No service entry with id %1").arg(id);
        return std::nullopt;
    }
    return readEntry(query);
}

QList<ServiceEntry> ServiceRepository::byVehicle(const QString &vehicleId)
{
    m_error.clear();
    QSqlQuery query(m_database.connection());
    // SQLite sorts NULL first, so odometer-less costs come before the
    // workshop visit of the same day; created_at keeps the rest stable.
    // Read once front to back: without forward-only Qt caches every row a
    // second time, which doubles the memory for thousands of entries.
    query.setForwardOnly(true);
    query.prepare(selectColumns + QStringLiteral(" WHERE vehicle_id = ? ORDER BY date, odometer_m, created_at"));
    query.addBindValue(vehicleId);
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot list service entries: %1").arg(query.lastError().text());
        return {};
    }
    QList<ServiceEntry> result;
    while (query.next()) {
        result.append(readEntry(query));
    }
    return result;
}

bool ServiceRepository::remove(const QString &id)
{
    m_error.clear();
    AttachmentStore attachments(m_database);
    // The names must be read now; after the DELETE the cascade has removed the rows.
    const QStringList files = attachments.storedNamesOf(id);

    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral("DELETE FROM service_entries WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot delete service entry: %1").arg(query.lastError().text());
        return false;
    }
    if (query.numRowsAffected() != 1) {
        m_error = QStringLiteral("No service entry with id %1").arg(id);
        return false;
    }
    if (!attachments.removeFiles(files)) {
        m_error = attachments.errorString();
    }
    return true;
}

QString ServiceRepository::errorString() const
{
    return m_error;
}

} // namespace garagelog::core
