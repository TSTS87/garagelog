// Statistics of one vehicle for QML (role: app).
// Wraps core::StatsService for the statistics tab: turns the period filter
// (year / 12 months / all) into dates and exposes the key figures and chart
// data as properties. Exposed to QML as singleton "Stats".

#pragma once

#include "statsservice.h"

#include <QDate>
#include <QObject>
#include <QVariant>
#include <QVariantList>

namespace garagelog::core {
class Database;
}

namespace garagelog::app {

/**
 * @brief Key figures and chart series of vehicleId for the selected period.
 *
 * All values are recomputed in reload() (on vehicle, period or data
 * changes) and announced with one changed() signal. Money in cents,
 * distances in metres; optional figures are null when not computable.
 */
class StatsModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString vehicleId READ vehicleId WRITE setVehicleId NOTIFY vehicleIdChanged)
    /** "year" (1 Jan until today), "12months" (last 12 months) or "all". */
    Q_PROPERTY(QString period READ period WRITE setPeriod NOTIFY periodChanged)
    /** First and last day of the period as ISO text; from is "" for "all" without entries. */
    Q_PROPERTY(QString periodFrom READ periodFrom NOTIFY changed)
    Q_PROPERTY(QString periodTo READ periodTo NOTIFY changed)
    /** true for electric vehicles (consumption in kWh). */
    Q_PROPERTY(bool electric READ electric NOTIFY changed)
    Q_PROPERTY(qint64 fuelCostC READ fuelCostC NOTIFY changed)
    Q_PROPERTY(qint64 serviceCostC READ serviceCostC NOTIFY changed)
    Q_PROPERTY(qint64 totalCostC READ totalCostC NOTIFY changed)
    /** Metres driven in the period, or null. */
    Q_PROPERTY(QVariant distanceM READ distanceM NOTIFY changed)
    /** Cents per km (double), or null. */
    Q_PROPERTY(QVariant costPerKmC READ costPerKmC NOTIFY changed)
    /** Average cents per month, rounded (qint64), or null. */
    Q_PROPERTY(QVariant costPerMonthC READ costPerMonthC NOTIFY changed)
    /** ml or Wh per 100 km (double), or null; Format.consumption() formats it. */
    Q_PROPERTY(QVariant averageConsumption READ averageConsumption NOTIFY changed)
    /** Chart values in ml or Wh per 100 km (doubles), oldest first; Format.consumptionValue() converts them for display. */
    Q_PROPERTY(QVariantList consumptionValues READ consumptionValues NOTIFY changed)
    /** ISO dates belonging to consumptionValues, same index. */
    Q_PROPERTY(QVariantList consumptionDates READ consumptionDates NOTIFY changed)
    /** Cost distribution, highest first: list of maps {name (translated), costC, isFuel}. */
    Q_PROPERTY(QVariantList categoryCosts READ categoryCosts NOTIFY changed)
    Q_PROPERTY(QString errorString READ errorString NOTIFY changed)

public:
    /** @param database Must be open and outlive the model. */
    explicit StatsModel(const core::Database &database, QObject *parent = nullptr);

    /**
     * @brief Dates of a period key relative to @p today.
     *
     * "year" = 1 Jan of today's year .. today, "12months" = today - 1 year
     * + 1 day .. today, anything else ("all") = open start .. today. The end
     * is today, not 31 Dec, so cost per month is not diluted by the future.
     */
    static core::Period periodFor(const QString &key, const QDate &today);

    QString vehicleId() const;
    void setVehicleId(const QString &id);
    QString period() const;
    void setPeriod(const QString &period);
    QString periodFrom() const;
    QString periodTo() const;
    bool electric() const;
    qint64 fuelCostC() const;
    qint64 serviceCostC() const;
    qint64 totalCostC() const;
    QVariant distanceM() const;
    QVariant costPerKmC() const;
    QVariant costPerMonthC() const;
    QVariant averageConsumption() const;
    QVariantList consumptionValues() const;
    QVariantList consumptionDates() const;
    QVariantList categoryCosts() const;
    QString errorString() const;

    /** Recomputes everything for today's date. */
    Q_INVOKABLE void reload();

Q_SIGNALS:
    void vehicleIdChanged();
    void periodChanged();
    void changed();

private:
    const core::Database &m_database;
    QString m_vehicleId;
    QString m_period = QStringLiteral("12months");

    core::Period m_dates;
    bool m_electric = false;
    core::CostTotals m_totals;
    QVariant m_distanceM;
    QVariant m_costPerKmC;
    QVariant m_costPerMonthC;
    QVariant m_averageConsumption;
    QVariantList m_consumptionValues;
    QVariantList m_consumptionDates;
    QVariantList m_categoryCosts;
    QString m_error;
};

} // namespace garagelog::app
