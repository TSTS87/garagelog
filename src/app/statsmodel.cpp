// Implementation of the statistics model (role: app).

#include "statsmodel.h"

#include "fuelentry.h"
#include "servicelistmodel.h"
#include "vehiclerepository.h"

#include <KLocalizedString>

#include <cmath>

namespace garagelog::app {

namespace {

QVariant nullVariant()
{
    return QVariant::fromValue(nullptr);
}

} // namespace

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
    m_distanceM = m_costPerKmC = m_costPerMonthC = m_averageConsumption = nullVariant();
    m_consumptionValues.clear();
    m_consumptionDates.clear();
    m_categoryCosts.clear();
    m_error.clear();

    if (!m_vehicleId.isEmpty()) {
        if (const auto vehicle = core::VehicleRepository(m_database).byId(m_vehicleId)) {
            m_electric = core::quantityUnitFor(vehicle->powertrain) == core::QuantityUnit::WattHour;
        }
        core::StatsService stats(m_database);
        auto keepError = [&] {
            if (m_error.isEmpty()) {
                m_error = stats.errorString();
            }
        };

        m_totals = stats.totalCost(m_vehicleId, m_dates);
        keepError();
        if (const auto d = stats.distance(m_vehicleId, m_dates)) {
            m_distanceM = QVariant::fromValue(*d);
        }
        if (const auto c = stats.costPerKm(m_vehicleId, m_dates)) {
            m_costPerKmC = *c;
        }
        if (const auto c = stats.costPerMonth(m_vehicleId, m_dates)) {
            m_costPerMonthC = QVariant::fromValue(static_cast<qint64>(std::llround(*c)));
        }
        if (const auto a = stats.averageConsumption(m_vehicleId, m_dates)) {
            m_averageConsumption = *a;
        }
        keepError();

        // Raw database units; the display unit (l/100 km, mpg, ...) is Format's job.
        for (const core::ConsumptionPoint &p : stats.consumptionSeries(m_vehicleId, m_dates)) {
            m_consumptionValues.append(p.quantityPer100Km);
            m_consumptionDates.append(p.date.toString(Qt::ISODate));
        }
        keepError();

        for (const core::CategoryCost &c : stats.costByCategory(m_vehicleId, m_dates)) {
            const QString name = c.isFuel ? (m_electric ? i18n("Charging") : i18n("Fuel")) : ServiceListModel::categoryLabel(c.category);
            m_categoryCosts.append(QVariantMap{
                {QStringLiteral("name"), name},
                {QStringLiteral("costC"), QVariant::fromValue(c.costC)},
                {QStringLiteral("isFuel"), c.isFuel},
            });
        }
        keepError();
    }
    Q_EMIT changed();
}

} // namespace garagelog::app
