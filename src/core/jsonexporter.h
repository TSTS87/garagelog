// Complete export of a garage as JSON plus attachment files (role: core).
// Writes <dir>/garagelog.json with every table and copies the attachment
// files to <dir>/attachments/ (SPEC §4.6). JsonImporter reads it back.

#pragma once

#include <QString>
#include <QStringList>

namespace garagelog::core {

class Database;

/** Constants of the export format, shared by exporter, importer and CSV export. */
namespace exportformat {

/** Value of the "format" key, identifies the file as a GarageLog export. */
inline const QString formatName = QStringLiteral("garagelog-export");

/** File name of the JSON document inside the export directory. */
inline const QString jsonFileName = QStringLiteral("garagelog.json");

/**
 * All data tables in foreign-key order: a table only references tables
 * listed before it, so importing in this order never violates a key.
 * schema_version is not a data table; it is stored in the JSON header.
 */
inline const QStringList tables{
    QStringLiteral("vehicles"),
    QStringLiteral("service_categories"),
    QStringLiteral("fuel_entries"),
    QStringLiteral("service_entries"),
    QStringLiteral("attachments"),
    QStringLiteral("maintenance_intervals"),
    QStringLiteral("trips"),
    QStringLiteral("settings"),
};

} // namespace exportformat

/**
 * @brief Writes the whole garage into an export directory.
 *
 * JSON layout:
 * @code
 * { "format": "garagelog-export", "schema_version": 1, "app_version": "0.1.0",
 *   "exported_at": "2026-10-23T10:00:00Z",
 *   "tables": { "vehicles": [ { "id": "...", "name": "Golf", ... } ], ... } }
 * @endcode
 * Rows keep the database column names and units (metres, cents, ml/Wh);
 * NULL becomes JSON null. Rows are sorted by their first column so two
 * exports of the same data are byte-identical apart from exported_at.
 */
class JsonExporter
{
public:
    explicit JsonExporter(const Database &database);

    /**
     * @brief Exports into @p directory (created if missing).
     * @return false if the directory already contains an export, a table
     *         cannot be read or a file cannot be written (errorString set).
     *         A missing attachment file is skipped and listed in warnings().
     */
    bool exportTo(const QString &directory);

    /** Problems that did not stop the export, e.g. missing attachment files. */
    QStringList warnings() const;

    /** Reason for the last failed exportTo(). */
    QString errorString() const;

private:
    const Database &m_database;
    QStringList m_warnings;
    QString m_error;
};

} // namespace garagelog::core
