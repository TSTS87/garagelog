// Implementation of the statistics model (role: app).

#include "statsmodel.h"

#include "fuelentry.h"
#include "servicelistmodel.h"
#include "variantutil.h"
#include "vehiclerepository.h"

#include <KLocalizedString>

#include <cmath>

namespace garagelog::app {

StatsModel::StatsModel(const core::Database &database, QObject *parent)
    : QObject(parent)
    , m_database(database)
{
}

core::Period StatsModel::periodFor(const QString &key, const QDate &today)
{
    if (key == QLatin1String("year")) {
        return {QDate(today.year(), 1, 1), today};
    }
    if (key == QLatin1String("12months")) {
        return {today.addYears(-1).addDays(1), today};
    }
    return {QDate(), today};
}

QString StatsModel::vehicleId() const
{
    return m_vehicleId;
}

void StatsModel::setVehicleId(const QString &id)
{
    if (m_vehicleId == id) {
        return;
    }
    m_vehicleId = id;
    Q_EMIT vehicleIdChanged();
    reload();
}

QString StatsModel::period() const
{
    return m_period;
}

void StatsModel::setPeriod(const QString &period)
{
    if (m_period == period) {
        return;
    }
    m_period = period;
    Q_EMIT periodChanged();
    reload();
}

QString StatsModel::periodFrom() const
{
    return m_dates.from.toString(Qt::ISODate);
}

QString StatsModel::periodTo() const
{
    return m_dates.to.toString(Qt::ISODate);
}

bool StatsModel::electric() const
{
    return m_electric;
}

qint64 StatsModel::fuelCostC() const
{
    return m_totals.fuelC;
}

qint64 StatsModel::serviceCostC() const
{
    return m_totals.serviceC;
}

qint64 StatsModel::totalCostC() const
{
    return m_totals.totalC();
}

QVariant StatsModel::distanceM() const
{
    return m_distanceM;
}

QVariant StatsModel::costPerKmC() const
{
    return m_costPerKmC;
}

QVariant StatsModel::costPerMonthC() const
{
    return m_costPerMonthC;
}

QVariant StatsModel::averageConsumption() const
{
    return m_averageConsumption;
}

QVariantList StatsModel::consumptionValues() const
{
    return m_consumptionValues;
}

QVariantList StatsModel::consumptionDates() const
{
    return m_consumptionDates;
}

QVariantList StatsModel::categoryCosts() const
{
    return m_categoryCosts;
}

QString StatsModel::errorString() const
{
    return m_error;
}

void StatsModel::reload()
{
    m_dates = periodFor(m_period, QDate::currentDate());
    m_electric = false;
    m_totals = {};
    m_distanceM = m_costPerKmC = m_costPerMonthC = m_averageConsumption = jsNull();
    m_consumptionValues.clear();
    m_consumptionDates.clear();
    m_categoryCosts.clear();
    m_error.clear();

    if (!m_vehicleId.isEmpty()) {
        if (const auto vehicle = core::VehicleRepository(m_database).byId(m_vehicleId)) {
            m_electric = core::usesWattHours(vehicle->powertrain);
        }
        // One pass over totals, distance and fuel entries (StatsService::summary()).
        core::StatsService stats(m_database);
        const core::StatsSummary summary = stats.summary(m_vehicleId, m_dates);
        m_error = stats.errorString();

        m_totals = summary.totals;
        if (summary.distanceM) {
            m_distanceM = QVariant::fromValue(*summary.distanceM);
        }
        if (summary.costPerKmC) {
            m_costPerKmC = *summary.costPerKmC;
        }
        if (summary.costPerMonthC) {
            m_costPerMonthC = QVariant::fromValue(static_cast<qint64>(std::llround(*summary.costPerMonthC)));
        }
        if (summary.averageConsumption) {
            m_averageConsumption = *summary.averageConsumption;
        }

        // Raw database units; the display unit (l/100 km, mpg, ...) is Format's job.
        for (const core::ConsumptionPoint &p : summary.consumption) {
            m_consumptionValues.append(p.quantityPer100Km);
            m_consumptionDates.append(p.date.toString(Qt::ISODate));
        }

        for (const core::CategoryCost &c : summary.categories) {
            const QString name = c.isFuel ? (m_electric ? i18n("Charging") : i18n("Fuel")) : ServiceListModel::categoryLabel(c.category);
            m_categoryCosts.append(QVariantMap{
                {QStringLiteral("name"), name},
                {QStringLiteral("costC"), QVariant::fromValue(c.costC)},
                {QStringLiteral("isFuel"), c.isFuel},
            });
        }
    }
    Q_EMIT changed();
}

} // namespace garagelog::app
