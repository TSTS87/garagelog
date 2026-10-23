// CSV export of the garage tables (role: core).
// One file per table with the database column names as header, for use in
// spreadsheets (SPEC §4.6). Values keep the database units (metres, cents,
// ml/Wh), which the column names state (odometer_m, total_cost_c, ...).

#pragma once

#include <QString>
#include <QStringList>

namespace garagelog::core {

class Database;

/**
 * @brief Writes tables as RFC 4180 CSV files (UTF-8, comma, CRLF).
 *
 * NULL is written as an empty field. Only the tables of
 * exportformat::tables can be exported.
 */
class CsvExporter
{
public:
    explicit CsvExporter(const Database &database);

    /**
     * @brief Writes one table to @p filePath (overwritten if it exists).
     * @return false for an unknown table or on a read/write error.
     */
    bool exportTable(const QString &table, const QString &filePath);

    /**
     * @brief Writes every table to <directory>/<table>.csv.
     * @return false on the first failure; already written files stay.
     */
    bool exportAll(const QString &directory);

    /** Reason for the last failure. */
    QString errorString() const;

private:
    const Database &m_database;
    QString m_error;
};

} // namespace garagelog::core
