// Implementation of the vehicle statistics (role: core).

#include "statsservice.h"

#include "categoryrepository.h"
#include "database.h"
#include "dbvalues.h"
#include "fuelrepository.h"
#include "fuelstats.h"

#include <QHash>
#include <QSqlError>
#include <QSqlQuery>

#include <algorithm>

namespace garagelog::core {

using namespace db;

namespace {

/** Average month length in days, so a year is exactly twelve months. */
constexpr double daysPerMonth = 365.25 / 12.0;

/**
 * Runs @p sql with the vehicle id bound first and "date >= from" /
 * "date <= to" appended for the closed sides of @p period. @p sql must end
 * in a WHERE clause on the vehicle (and may be followed by @p tail).
 */
bool execForPeriod(QSqlQuery &query, const QString &sql, const QString &vehicleId, const Period &period,
                   const QString &tail = {})
{
    QString text = sql;
    if (period.from.isValid()) {
        text += QStringLiteral(" AND date >= ?");
    }
    if (period.to.isValid()) {
        text += QStringLiteral(" AND date <= ?");
    }
    query.prepare(text + tail);
    query.addBindValue(vehicleId);
    if (period.from.isValid()) {
        query.addBindValue(toDbDate(period.from));
    }
    if (period.to.isValid()) {
        query.addBindValue(toDbDate(period.to));
    }
    return query.exec();
}

/** All odometer readings of a vehicle (fuel and service), as SQL source. */
const QString readings = QStringLiteral(
    "(SELECT vehicle_id, date, odometer_m FROM fuel_entries "
    " UNION ALL SELECT vehicle_id, date, odometer_m FROM service_entries WHERE odometer_m IS NOT NULL)");

} // namespace

bool Period::contains(const QDate &date) const
{
    return (!from.isValid() || date >= from) && (!to.isValid() || date <= to);
}

StatsService::StatsService(const Database &database)
    : m_database(database)
{
}

CostTotals StatsService::totalCost(const QString &vehicleId, const Period &period)
{
    m_error.clear();
    CostTotals totals;
    QSqlQuery query(m_database.connection());
    if (!execForPeriod(query, QStringLiteral("SELECT COALESCE(SUM(total_cost_c), 0) FROM fuel_entries WHERE vehicle_id = ?"),
                       vehicleId, period)
        || !query.next()) {
        m_error = QStringLiteral("Cannot sum fuel costs: %1").arg(query.lastError().text());
        return {};
    }
    totals.fuelC = query.value(0).toLongLong();
    if (!execForPeriod(query, QStringLiteral("SELECT COALESCE(SUM(cost_c), 0) FROM service_entries WHERE vehicle_id = ?"),
                       vehicleId, period)
        || !query.next()) {
        m_error = QStringLiteral("Cannot sum service costs: %1").arg(query.lastError().text());
        return {};
    }
    totals.serviceC = query.value(0).toLongLong();
    return totals;
}

std::optional<qint64> StatsService::distance(const QString &vehicleId, const Period &period)
{
    m_error.clear();
    QSqlQuery query(m_database.connection());
    if (!execForPeriod(query, QStringLiteral("SELECT MIN(odometer_m), MAX(odometer_m) FROM ") + readings + QStringLiteral(" WHERE vehicle_id = ?"),
                       vehicleId, period)
        || !query.next()) {
        m_error = QStringLiteral("Cannot read odometers: %1").arg(query.lastError().text());
        return std::nullopt;
    }
    if (query.value(1).isNull()) {
        return std::nullopt;
    }
    qint64 base = query.value(0).toLongLong();
    const qint64 end = query.value(1).toLongLong();

    // The km from the last reading before the period to the first one in
    // it were driven in the period, too.
    if (period.from.isValid()) {
        QSqlQuery before(m_database.connection());
        before.prepare(QStringLiteral("SELECT MAX(odometer_m) FROM ") + readings + QStringLiteral(" WHERE vehicle_id = ? AND date < ?"));
        before.addBindValue(vehicleId);
        before.addBindValue(toDbDate(period.from));
        if (!before.exec() || !before.next()) {
            m_error = QStringLiteral("Cannot read odometers: %1").arg(before.lastError().text());
            return std::nullopt;
        }
        if (!before.value(0).isNull()) {
            base = before.value(0).toLongLong();
        }
    }
    if (end <= base) {
        return std::nullopt;
    }
    return end - base;
}

std::optional<double> StatsService::costPerKm(const QString &vehicleId, const Period &period)
{
    const auto metres = distance(vehicleId, period);
    if (!metres) {
        return std::nullopt;
    }
    const CostTotals totals = totalCost(vehicleId, period);
    return static_cast<double>(totals.totalC()) * 1000.0 / static_cast<double>(*metres);
}

std::optional<double> StatsService::costPerMonth(const QString &vehicleId, const Period &period)
{
    m_error.clear();
    if (!period.to.isValid()) {
        return std::nullopt;
    }
    QDate start = period.from;
    if (!start.isValid()) {
        QSqlQuery query(m_database.connection());
        query.prepare(QStringLiteral(
            "SELECT MIN(date) FROM (SELECT date FROM fuel_entries WHERE vehicle_id = ? "
            "UNION ALL SELECT date FROM service_entries WHERE vehicle_id = ?)"));
        query.addBindValue(vehicleId);
        query.addBindValue(vehicleId);
        if (!query.exec() || !query.next()) {
            m_error = QStringLiteral("Cannot read first entry: %1").arg(query.lastError().text());
            return std::nullopt;
        }
        if (query.value(0).isNull()) {
            return std::nullopt;
        }
        start = fromDbDate(query.value(0));
    }
    const CostTotals totals = totalCost(vehicleId, period);
    if (!m_error.isEmpty()) {
        return std::nullopt;
    }
    const double months = std::max(1.0, static_cast<double>(start.daysTo(period.to) + 1) / daysPerMonth);
    return static_cast<double>(totals.totalC()) / months;
}

QList<CategoryCost> StatsService::costByCategory(const QString &vehicleId, const Period &period)
{
    m_error.clear();
    QList<CategoryCost> result;

    const CostTotals totals = totalCost(vehicleId, period);
    if (!m_error.isEmpty()) {
        return {};
    }
    if (totals.fuelC > 0) {
        result.append({true, ServiceCategory(), totals.fuelC});
    }

    QHash<QString, ServiceCategory> categories;
    CategoryRepository categoryRepo(m_database);
    for (const ServiceCategory &c : categoryRepo.all()) {
        categories.insert(c.id, c);
    }

    QSqlQuery query(m_database.connection());
    if (!execForPeriod(query, QStringLiteral("SELECT category_id, SUM(cost_c) FROM service_entries WHERE vehicle_id = ?"),
                       vehicleId, period, QStringLiteral(" GROUP BY category_id HAVING SUM(cost_c) > 0"))) {
        m_error = QStringLiteral("Cannot sum costs by category: %1").arg(query.lastError().text());
        return {};
    }
    while (query.next()) {
        result.append({false, categories.value(query.value(0).toString()), query.value(1).toLongLong()});
    }

    // stable_sort keeps fuel before a service category of the same amount.
    std::stable_sort(result.begin(), result.end(), [](const CategoryCost &a, const CategoryCost &b) {
        return a.costC > b.costC;
    });
    return result;
}

QList<ConsumptionPoint> StatsService::consumptionSeries(const QString &vehicleId, const Period &period)
{
    m_error.clear();
    // Segments may start before the period, so all entries are needed.
    FuelRepository repo(m_database);
    const QList<FuelEntry> entries = repo.byVehicle(vehicleId);
    if (!repo.errorString().isEmpty()) {
        m_error = repo.errorString();
        return {};
    }
    const auto consumption = FuelStats::consumption(entries);

    QList<ConsumptionPoint> result;
    for (qsizetype i = 0; i < entries.size(); ++i) {
        if (consumption.at(i) && period.contains(entries.at(i).date)) {
            result.append({entries.at(i).date, entries.at(i).odometerM, consumption.at(i)->quantityPer100Km,
                           consumption.at(i)->costPer100KmC});
        }
    }
    return result;
}

std::optional<double> StatsService::averageConsumption(const QString &vehicleId, const Period &period)
{
    m_error.clear();
    FuelRepository repo(m_database);
    const QList<FuelEntry> entries = repo.byVehicle(vehicleId);
    if (!repo.errorString().isEmpty()) {
        m_error = repo.errorString();
        return std::nullopt;
    }
    const auto consumption = FuelStats::consumption(entries);

    // Weighted by distance: summing quantities and distances is exact,
    // averaging the per-segment values would overweight short segments.
    qint64 quantity = 0;
    qint64 metres = 0;
    for (qsizetype i = 0; i < entries.size(); ++i) {
        if (consumption.at(i) && period.contains(entries.at(i).date)) {
            quantity += consumption.at(i)->quantity;
            metres += consumption.at(i)->distanceM;
        }
    }
    if (metres <= 0) {
        return std::nullopt;
    }
    return static_cast<double>(quantity) * 100'000.0 / static_cast<double>(metres);
}

QString StatsService::errorString() const
{
    return m_error;
}

} // namespace garagelog::core
