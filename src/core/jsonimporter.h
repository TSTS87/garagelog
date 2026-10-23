// Import of a complete JSON export into an empty garage (role: core).
// Counterpart of JsonExporter: reads <dir>/garagelog.json, inserts every
// row in one transaction and copies <dir>/attachments/ into the garage.

#pragma once

#include <QString>
#include <QStringList>

namespace garagelog::core {

class Database;

/**
 * @brief Restores an export directory into a garage without vehicles.
 *
 * Only a garage without vehicles is accepted, so nothing can be merged by
 * accident (DATA_MODEL invariant 3: export -> import into an empty garage
 * gives identical data). Built-in categories already exist with the same
 * fixed ids and are skipped; settings overwrite existing keys. Exports
 * with a newer schema than the garage are refused; older ones work
 * because migrations only add columns.
 */
class JsonImporter
{
public:
    explicit JsonImporter(const Database &database);

    /**
     * @brief Checks an export without changing anything (header, schema, garage empty).
     * @return false with errorString() set if importFrom() would refuse it.
     */
    bool check(const QString &directory);

    /**
     * @brief Imports all tables and attachment files from @p directory.
     * @return false if check() fails, a row cannot be inserted (everything
     *         is rolled back) or an attachment cannot be copied. Missing
     *         attachment files are skipped and listed in warnings().
     */
    bool importFrom(const QString &directory);

    /** Number of rows inserted by the last successful importFrom(), all tables together. */
    int importedRows() const;

    /** Problems that did not stop the import. */
    QStringList warnings() const;

    /** Reason for the last failure. */
    QString errorString() const;

private:
    const Database &m_database;
    int m_importedRows = 0;
    QStringList m_warnings;
    QString m_error;
};

} // namespace garagelog::core
