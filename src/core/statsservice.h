// Cost and consumption statistics of one vehicle (role: core).
// Key figures for the statistics tab: total cost, cost per km, cost per
// month, cost by category and the consumption series, each for a period.
// Nothing is stored; everything is derived from fuel and service entries.

#pragma once

#include "fuelentry.h"
#include "serviceentry.h"

#include <QDate>
#include <QList>
#include <QString>

#include <optional>

namespace garagelog::core {

class Database;

/**
 * @brief Inclusive date range; an invalid from or to leaves that side open.
 *
 * "Last 12 months" is {today - 1 year + 1 day, today}, "whole time" is
 * {invalid, today}. The UI computes these from the current date.
 */
struct Period {
    QDate from; ///< first day included, invalid = since the first entry
    QDate to;   ///< last day included, invalid = no upper limit

    /** true if @p date lies within the period. */
    bool contains(const QDate &date) const;
};

/** Costs of a period split by source, in cents. */
struct CostTotals {
    qint64 fuelC = 0;     ///< sum of fuel_entries.total_cost_c
    qint64 serviceC = 0;  ///< sum of service_entries.cost_c
    qint64 totalC() const { return fuelC + serviceC; }

    bool operator==(const CostTotals &) const = default;
};

/** One bar of the cost distribution: a service category, or fuel (isFuel, no category). */
struct CategoryCost {
    bool isFuel = false;
    ServiceCategory category; ///< empty for fuel
    qint64 costC = 0;         ///< cents, always > 0 in results

    bool operator==(const CategoryCost &) const = default;
};

/** One point of the consumption line: a full tank that closes a segment. */
struct ConsumptionPoint {
    QDate date;
    qint64 odometerM = 0;
    double quantityPer100Km = 0; ///< ml or Wh per 100 km (see quantityUnitFor())
    double costPer100KmC = 0;    ///< cents per 100 km

    bool operator==(const ConsumptionPoint &) const = default;
};

/** Every key figure of the statistics tab for one vehicle and period; see StatsService for each value. */
struct StatsSummary {
    CostTotals totals;                        ///< cents
    std::optional<qint64> distanceM;          ///< see StatsService::distance()
    std::optional<double> costPerKmC;         ///< cents per km
    std::optional<double> costPerMonthC;      ///< cents per month
    QList<CategoryCost> categories;           ///< highest first
    QList<ConsumptionPoint> consumption;      ///< oldest first
    std::optional<double> averageConsumption; ///< ml or Wh per 100 km
};

/**
 * @brief Statistics queries for one vehicle and one period.
 *
 * Entries count for a period by their date. The Database must stay open
 * for the lifetime of this object; read errors leave errorString() set
 * and return empty/zero results.
 */
class StatsService
{
public:
    explicit StatsService(const Database &database);

    /** Fuel and service costs of entries in @p period, in cents. */
    CostTotals totalCost(const QString &vehicleId, const Period &period);

    /**
     * @brief Distance driven in @p period, in metres.
     *
     * Highest odometer reading in the period (fuel and service entries)
     * minus the base: the highest reading before the period if there is
     * one, otherwise the lowest reading in the period. So the km between
     * the last fill-up before and the first one in the period count.
     *
     * @return nullopt if the period has no reading or the result is not > 0.
     */
    std::optional<qint64> distance(const QString &vehicleId, const Period &period);

    /**
     * @brief Total cost per km in @p period (DATA_MODEL costPerKm), in cents per km.
     * @return nullopt without a positive distance (see distance()).
     */
    std::optional<double> costPerKm(const QString &vehicleId, const Period &period);

    /**
     * @brief Average total cost per month in @p period, in cents.
     *
     * The period starts at from, or at the first entry of the vehicle if
     * from is open, and ends at to (which must then be valid; the UI passes
     * today). Its length in months is days / (365.25 / 12), at least one
     * month, so a single early entry does not explode the average.
     *
     * @return nullopt if the vehicle has no entries or to is open.
     */
    std::optional<double> costPerMonth(const QString &vehicleId, const Period &period);

    /**
     * @brief Costs by service category plus fuel, highest first; zero sums are left out.
     */
    QList<CategoryCost> costByCategory(const QString &vehicleId, const Period &period);

    /**
     * @brief Consumption of every full tank in @p period that closes a segment (FuelStats::consumption()), oldest first.
     *
     * The segment may start before the period; only its end date counts.
     */
    QList<ConsumptionPoint> consumptionSeries(const QString &vehicleId, const Period &period);

    /**
     * @brief Average consumption in @p period: sum of quantities / sum of distances of the segments in consumptionSeries().
     * @return ml or Wh per 100 km; nullopt without any segment.
     */
    std::optional<double> averageConsumption(const QString &vehicleId, const Period &period);

    /**
     * @brief All figures above at once, as the statistics tab needs them.
     *
     * Reads totals, distance and the fuel entries once instead of once per
     * figure (the single functions each repeat those reads), which matters
     * with thousands of entries because the tab reloads after every save.
     */
    StatsSummary summary(const QString &vehicleId, const Period &period);

    /** Reason for the last failed read. */
    QString errorString() const;

private:
    /** Date of the first fuel or service entry; nullopt without entries or on error (m_error set). */
    std::optional<QDate> firstEntryDate(const QString &vehicleId);
    /** costByCategory() with the totals already known. */
    QList<CategoryCost> categoriesFor(const QString &vehicleId, const Period &period, const CostTotals &totals);
    /** costPerMonth() with the totals already known. */
    std::optional<double> perMonth(const QString &vehicleId, const Period &period, const CostTotals &totals);
    /** All fuel entries of the vehicle; empty with m_error set on failure. */
    QList<FuelEntry> fuelEntries(const QString &vehicleId);

    const Database &m_database;
    QString m_error;
};

} // namespace garagelog::core
