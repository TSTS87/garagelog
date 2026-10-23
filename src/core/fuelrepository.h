// Data access for the fuel_entries table (role: core).
// The only place that writes fuel SQL; the app layer (M06) wraps it in a
// list model. Works on the connection of an open Database.

#pragma once

#include "fuelentry.h"

#include <QList>
#include <QString>

#include <optional>

namespace garagelog::core {

class Database;

/**
 * @brief Create, read, update and delete fuel entries.
 *
 * Methods returning bool report the reason for a false result in
 * errorString(). The Database must stay open for the lifetime of the
 * repository; the connection is fetched per call, never cached.
 *
 * Plausibility (odometer lower than the previous entry, duplicates with the
 * same date and odometer) is not checked here: such entries are allowed in
 * the database and only produce a warning in the UI (DATA_MODEL.md).
 */
class FuelRepository
{
public:
    explicit FuelRepository(const Database &database);

    /**
     * @brief Inserts a new fuel entry.
     * @param entry In: the user-editable fields. Out: id, createdAt and
     *        updatedAt are filled in (a given id is ignored).
     * @return false if a field is invalid (see validate()), the vehicle does
     *         not exist (foreign key) or the insert fails.
     */
    bool create(FuelEntry &entry);

    /**
     * @brief Writes all user-editable fields of an existing entry.
     * @param entry Identified by id; createdAt is not changed. Out:
     *        updatedAt is set to now.
     * @return false if a field is invalid, no entry has this id or the
     *         update fails.
     */
    bool update(FuelEntry &entry);

    /** The entry with @p id, or nullopt if there is none (or on a query error). */
    std::optional<FuelEntry> byId(const QString &id);

    /**
     * @brief All entries of a vehicle in chronological order.
     *
     * Sorted by date, then odometer, then creation time; this is the order
     * FuelStats::consumption() expects.
     *
     * @return Empty list for an unknown vehicle or on a query error;
     *         errorString() is set only in the error case.
     */
    QList<FuelEntry> byVehicle(const QString &vehicleId);

    /** Deletes an entry permanently. false if the id is unknown. */
    bool remove(const QString &id);

    /** Reason for the last failed call. */
    QString errorString() const;

private:
    /** Checks the user-editable fields; sets m_error and returns false on the first problem. */
    bool validate(const FuelEntry &entry);

    const Database &m_database;
    QString m_error;
};

} // namespace garagelog::core
