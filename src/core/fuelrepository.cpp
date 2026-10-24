// Implementation of the fuel_entries data access (role: core).

#include "fuelrepository.h"

#include "database.h"
#include "dbvalues.h"
#include "valuelimits.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace garagelog::core {

using namespace db;

namespace {

/** Column list shared by all SELECTs, matching the order readEntry() expects. */
const QString selectColumns = QStringLiteral(
    "SELECT id, vehicle_id, date, odometer_m, quantity, total_cost_c, full_tank, "
    "station, notes, created_at, updated_at FROM fuel_entries");

/** Reads the current row of a query built on selectColumns. */
FuelEntry readEntry(const QSqlQuery &query)
{
    FuelEntry e;
    e.id = query.value(0).toString();
    e.vehicleId = query.value(1).toString();
    e.date = fromDbDate(query.value(2));
    e.odometerM = query.value(3).toLongLong();
    e.quantity = query.value(4).toLongLong();
    e.totalCostC = query.value(5).toLongLong();
    e.fullTank = query.value(6).toInt() != 0;
    e.station = query.value(7).toString();
    e.notes = query.value(8).toString();
    e.createdAt = fromDbTimestamp(query.value(9));
    e.updatedAt = fromDbTimestamp(query.value(10));
    return e;
}

/** Binds the user-editable fields in the order used by INSERT and UPDATE below. */
void bindEditableFields(QSqlQuery &query, const FuelEntry &e)
{
    query.addBindValue(e.vehicleId);
    query.addBindValue(toDbDate(e.date));
    query.addBindValue(e.odometerM);
    query.addBindValue(e.quantity);
    query.addBindValue(e.totalCostC);
    query.addBindValue(e.fullTank ? 1 : 0);
    query.addBindValue(nullIfEmpty(e.station));
    query.addBindValue(nullIfEmpty(e.notes));
}

/** Trims the free-text fields so " Shell " and "Shell" end up as one station suggestion. */
void normalize(FuelEntry &e)
{
    e.station = e.station.trimmed();
    e.notes = e.notes.trimmed();
}

} // namespace

FuelRepository::FuelRepository(const Database &database)
    : m_database(database)
{
}

bool FuelRepository::validate(const FuelEntry &entry)
{
    if (entry.vehicleId.isEmpty()) {
        m_error = QStringLiteral("Fuel entry needs a vehicle");
    } else if (!limits::isStorableDate(entry.date)) {
        m_error = QStringLiteral("Fuel entry needs a valid date between %1 and %2").arg(limits::minYear).arg(limits::maxYear);
    } else if (entry.odometerM < 0) {
        m_error = QStringLiteral("Odometer must not be negative");
    } else if (entry.odometerM > limits::maxDistanceM) {
        m_error = QStringLiteral("Odometer is too large");
    } else if (entry.quantity <= 0) {
        m_error = QStringLiteral("Quantity must be greater than zero");
    } else if (entry.quantity > limits::maxQuantity) {
        m_error = QStringLiteral("Quantity is too large");
    } else if (entry.totalCostC < 0) {
        m_error = QStringLiteral("Total cost must not be negative");
    } else if (entry.totalCostC > limits::maxMoneyC) {
        m_error = QStringLiteral("Total cost is too large");
    } else {
        return true;
    }
    return false;
}

bool FuelRepository::create(FuelEntry &entry)
{
    m_error.clear();
    if (!validate(entry)) {
        return false;
    }

    // Work on a copy so the caller's object is only changed on success.
    FuelEntry e = entry;
    normalize(e);
    e.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    e.createdAt = nowUtc();
    e.updatedAt = e.createdAt;

    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral(
        "INSERT INTO fuel_entries (id, vehicle_id, date, odometer_m, quantity, total_cost_c, "
        "full_tank, station, notes, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(e.id);
    bindEditableFields(query, e);
    query.addBindValue(toDbTimestamp(e.createdAt));
    query.addBindValue(toDbTimestamp(e.updatedAt));
    // An unknown vehicle_id is rejected by the foreign key, no extra lookup needed.
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot create fuel entry: %1").arg(query.lastError().text());
        return false;
    }

    entry = e;
    return true;
}

bool FuelRepository::update(FuelEntry &entry)
{
    m_error.clear();
    if (!validate(entry)) {
        return false;
    }

    FuelEntry e = entry;
    normalize(e);
    const QDateTime now = nowUtc();
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral(
        "UPDATE fuel_entries SET vehicle_id = ?, date = ?, odometer_m = ?, quantity = ?, "
        "total_cost_c = ?, full_tank = ?, station = ?, notes = ?, updated_at = ? WHERE id = ?"));
    bindEditableFields(query, e);
    query.addBindValue(toDbTimestamp(now));
    query.addBindValue(e.id);
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot update fuel entry: %1").arg(query.lastError().text());
        return false;
    }
    if (query.numRowsAffected() != 1) {
        m_error = QStringLiteral("No fuel entry with id %1").arg(e.id);
        return false;
    }

    e.updatedAt = now;
    entry = e;
    return true;
}

std::optional<FuelEntry> FuelRepository::byId(const QString &id)
{
    m_error.clear();
    QSqlQuery query(m_database.connection());
    query.prepare(selectColumns + QStringLiteral(" WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot read fuel entry: %1").arg(query.lastError().text());
        return std::nullopt;
    }
    if (!query.next()) {
        m_error = QStringLiteral("No fuel entry with id %1").arg(id);
        return std::nullopt;
    }
    return readEntry(query);
}

QList<FuelEntry> FuelRepository::byVehicle(const QString &vehicleId)
{
    m_error.clear();
    QSqlQuery query(m_database.connection());
    // Matches idx_fuel_vehicle_date; created_at makes the order of exact
    // duplicates (same day and odometer) stable.
    query.prepare(selectColumns
                  + QStringLiteral(" WHERE vehicle_id = ? ORDER BY date, odometer_m, created_at"));
    query.addBindValue(vehicleId);
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot list fuel entries: %1").arg(query.lastError().text());
        return {};
    }

    QList<FuelEntry> result;
    while (query.next()) {
        result.append(readEntry(query));
    }
    return result;
}

bool FuelRepository::remove(const QString &id)
{
    m_error.clear();
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral("DELETE FROM fuel_entries WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot delete fuel entry: %1").arg(query.lastError().text());
        return false;
    }
    if (query.numRowsAffected() != 1) {
        m_error = QStringLiteral("No fuel entry with id %1").arg(id);
        return false;
    }
    return true;
}

QString FuelRepository::errorString() const
{
    return m_error;
}

} // namespace garagelog::core
