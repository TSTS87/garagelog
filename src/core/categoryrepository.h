// Data access for the service_categories table (role: core).
// Lists the seeded built-in categories and manages the user's own ones;
// built-ins are protected against renaming and deletion.

#pragma once

#include "serviceentry.h"

#include <QList>
#include <QString>

#include <optional>

namespace garagelog::core {

class Database;

/**
 * @brief Read all categories, create, rename and delete user categories.
 *
 * Methods returning bool report the reason for a false result in
 * errorString(). The Database must stay open for the lifetime of the
 * repository.
 */
class CategoryRepository
{
public:
    explicit CategoryRepository(const Database &database);

    /**
     * @brief All categories: built-ins first (in seed order of their keys), then user categories by name.
     * @return Empty list on a query error (errorString set).
     */
    QList<ServiceCategory> all();

    /** The category with @p id, or nullopt if there is none. */
    std::optional<ServiceCategory> byId(const QString &id);

    /** The category whose stored name is exactly @p name (built-in key or user text), or nullopt. */
    std::optional<ServiceCategory> byName(const QString &name);

    /**
     * @brief Adds a user category.
     * @param category In: name (trimmed, required, unique) and optional icon;
     *        builtin is always stored as false. Out: id is filled in.
     * @return false for a blank or already used name, or on a database error.
     */
    bool create(ServiceCategory &category);

    /**
     * @brief Renames a user category or changes its icon.
     * @return false for built-ins, unknown ids, blank or duplicate names.
     */
    bool update(ServiceCategory &category);

    /**
     * @brief Deletes a user category.
     * @return false for built-ins, unknown ids, and categories still used by
     *         a service entry or maintenance interval (the foreign keys keep
     *         them; the message says so).
     */
    bool remove(const QString &id);

    /** Reason for the last failed call. */
    QString errorString() const;

private:
    /** Checks name and uniqueness; @p ownId is skipped in the duplicate check (for update()). */
    bool validateName(const QString &name, const QString &ownId);

    const Database &m_database;
    QString m_error;
};

} // namespace garagelog::core
