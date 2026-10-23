// Data access for the maintenance_intervals table (role: core).
// The only place that writes interval SQL; DueCalculator reads the
// matching service entries separately.

#pragma once

#include "maintenanceinterval.h"

#include <QList>
#include <QString>

#include <optional>

namespace garagelog::core {

class Database;

/**
 * @brief Create, read, update and delete maintenance intervals.
 *
 * Methods returning bool report the reason for a false result in
 * errorString(). The Database must stay open for the lifetime of the repository.
 */
class IntervalRepository
{
public:
    explicit IntervalRepository(const Database &database);

    /**
     * @brief Inserts a new interval.
     * @param interval In: user-editable fields. Out: id and timestamps filled in.
     * @return false if a field is invalid (see validate()), vehicle or
     *         category do not exist, or the insert fails.
     */
    bool create(MaintenanceInterval &interval);

    /** Writes all user-editable fields; updatedAt = now. false for invalid fields or unknown ids. */
    bool update(MaintenanceInterval &interval);

    /** The interval with @p id, or nullopt. */
    std::optional<MaintenanceInterval> byId(const QString &id);

    /** Intervals of one vehicle sorted by name (active and paused). Empty on error. */
    QList<MaintenanceInterval> byVehicle(const QString &vehicleId);

    /** Active intervals of all non-archived vehicles, for the "due" overview. Empty on error. */
    QList<MaintenanceInterval> allActive();

    /** Deletes an interval permanently. false if the id is unknown. */
    bool remove(const QString &id);

    /** Reason for the last failed call. */
    QString errorString() const;

private:
    /**
     * Checks the user-editable fields: vehicle, category and name required,
     * at least one of everyM/everyMonths, both > 0 if set.
     */
    bool validate(const MaintenanceInterval &interval);

    const Database &m_database;
    QString m_error;
};

} // namespace garagelog::core
