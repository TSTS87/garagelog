// Data access for the service_entries table (role: core).
// The only place that writes service SQL; deleting an entry also deletes
// its attachment files through AttachmentStore.

#pragma once

#include "serviceentry.h"

#include <QList>
#include <QString>

#include <optional>

namespace garagelog::core {

class Database;

/**
 * @brief Create, read, update and delete service entries.
 *
 * Methods returning bool report the reason for a false result in
 * errorString(). The Database must stay open for the lifetime of the
 * repository; the connection is fetched per call, never cached.
 */
class ServiceRepository
{
public:
    explicit ServiceRepository(const Database &database);

    /**
     * @brief Inserts a new service entry.
     * @param entry In: the user-editable fields. Out: id, createdAt and
     *        updatedAt are filled in (a given id is ignored).
     * @return false if a field is invalid, vehicle or category do not exist
     *         (foreign keys) or the insert fails.
     */
    bool create(ServiceEntry &entry);

    /**
     * @brief Writes all user-editable fields of an existing entry.
     * @param entry Identified by id; createdAt is not changed. Out: updatedAt = now.
     * @return false if a field is invalid, no entry has this id or the update fails.
     */
    bool update(ServiceEntry &entry);

    /** The entry with @p id, or nullopt if there is none (or on a query error). */
    std::optional<ServiceEntry> byId(const QString &id);

    /**
     * @brief All entries of a vehicle in chronological order (date, odometer, creation time).
     *
     * Entries without odometer sort before those with one on the same day.
     * @return Empty list for an unknown vehicle or on a query error.
     */
    QList<ServiceEntry> byVehicle(const QString &vehicleId);

    /**
     * @brief Deletes an entry and its attachments permanently.
     *
     * The attachment rows go with the entry (ON DELETE CASCADE); their
     * files are deleted afterwards. A file that cannot be deleted stays as
     * orphan and is reported in errorString(), but the call still succeeds.
     *
     * @return false if the id is unknown or the delete fails.
     */
    bool remove(const QString &id);

    /** Reason for the last failed call (or the orphan-file note after remove()). */
    QString errorString() const;

private:
    /** Checks the user-editable fields; sets m_error and returns false on the first problem. */
    bool validate(const ServiceEntry &entry);

    const Database &m_database;
    QString m_error;
};

} // namespace garagelog::core
