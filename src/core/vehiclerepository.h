// Data access for the vehicles table (role: core).
// The only place that writes vehicle SQL; the app layer (M04) wraps it in a
// list model. Works on the connection of an open Database.

#pragma once

#include "vehicle.h"

#include <QList>
#include <QString>

#include <optional>

namespace garagelog::core {

class Database;

/**
 * @brief Create, read, update, archive and delete vehicles.
 *
 * Methods returning bool report the reason for a false result in
 * errorString(). The Database must stay open for the lifetime of the
 * repository; the connection is fetched per call, never cached.
 */
class VehicleRepository
{
public:
    explicit VehicleRepository(const Database &database);

    /**
     * @brief Inserts a new vehicle.
     * @param vehicle In: the user-editable fields. Out: id, createdAt and
     *        updatedAt are filled in (a given id is ignored).
     * @return false if the name is blank or the insert fails.
     */
    bool create(Vehicle &vehicle);

    /**
     * @brief Writes all user-editable fields of an existing vehicle.
     * @param vehicle Identified by id; createdAt is not changed. Out:
     *        updatedAt is set to now.
     * @return false if the name is blank, no vehicle has this id or the
     *         update fails.
     */
    bool update(Vehicle &vehicle);

    /** The vehicle with @p id, or nullopt if there is none (or on a query error). */
    std::optional<Vehicle> byId(const QString &id);

    /**
     * @brief All vehicles, sorted by name (case-insensitive).
     * @param includeArchived false = only active vehicles (the normal list).
     * @return Empty list on a query error; errorString() is set then.
     */
    QList<Vehicle> all(bool includeArchived = false);

    /** Hides the vehicle from the active list; its entries stay. false if the id is unknown. */
    bool archive(const QString &id);

    /** Reverses archive(). false if the id is unknown. */
    bool unarchive(const QString &id);

    /**
     * @brief Deletes a vehicle permanently.
     * @return false if the id is unknown or the vehicle still has fuel,
     *         service or trip entries (ON DELETE RESTRICT); such vehicles can
     *         only be archived. Its maintenance intervals are deleted with it.
     */
    bool remove(const QString &id);

    /** Reason for the last failed call. */
    QString errorString() const;

private:
    /** Checks the user-editable fields; sets m_error and returns false on the first problem. */
    bool validate(const Vehicle &vehicle);

    /** Shared body of archive()/unarchive(). */
    bool setArchived(const QString &id, bool archived);

    const Database &m_database;
    QString m_error;
};

} // namespace garagelog::core
