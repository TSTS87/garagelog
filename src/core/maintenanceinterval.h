// MaintenanceInterval value type (role: core).
// Plain data for one row of maintenance_intervals; IntervalRepository reads
// and writes it, DueCalculator derives the due status from it.

#pragma once

#include <QDateTime>
#include <QString>

#include <optional>

namespace garagelog::core {

/**
 * @brief A recurring service rule of one vehicle: every X metres and/or every Y months.
 *
 * At least one of everyM and everyMonths is set (checked by
 * IntervalRepository). The rule is fulfilled by the latest service entry
 * of the same vehicle with the same category.
 */
struct MaintenanceInterval {
    QString id;                      ///< UUIDv4 without braces; empty until created
    QString vehicleId;               ///< owning vehicle; the interval is deleted with it (CASCADE)
    QString categoryId;              ///< service category whose entries reset the interval
    QString name;                    ///< display name, e.g. "Oil change", required
    std::optional<qint64> everyM;    ///< distance interval in metres, nullopt = none
    std::optional<int> everyMonths;  ///< time interval in months, nullopt = none
    bool active = true;              ///< false = paused, not shown as due
    QString notes;
    QDateTime createdAt;             ///< UTC, whole seconds
    QDateTime updatedAt;             ///< UTC, whole seconds

    bool operator==(const MaintenanceInterval &) const = default;
};

} // namespace garagelog::core
