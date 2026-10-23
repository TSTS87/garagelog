// Export and import actions for QML (role: app).
// Thin layer over core::JsonExporter/JsonImporter, core::CsvExporter and
// core::CsvFuelImporter for the "Data" menu and the CSV import page.
// Exposed to QML as singleton "DataTransfer".

#pragma once

#include "csvfuelimporter.h"

#include <QObject>
#include <QStringList>
#include <QVariantMap>

#include <memory>

namespace garagelog::core {
class Database;
}

namespace garagelog::app {

/**
 * @brief Runs exports/imports and keeps the loaded CSV file between the steps of the import page.
 *
 * Folders and files may be file:// URLs or paths. Results are reported
 * through errorString, warnings and lastPath; dataImported() tells the
 * models to reload.
 */
class DataTransfer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString errorString READ errorString NOTIFY resultChanged)
    Q_PROPERTY(QStringList warnings READ warnings NOTIFY resultChanged)
    /** Folder written by the last successful export. */
    Q_PROPERTY(QString lastPath READ lastPath NOTIFY resultChanged)

public:
    /** How many preview rows previewCsv() returns at most; the count covers all rows. */
    static constexpr int previewLimit = 50;

    /** @param database Must be open and outlive this object. */
    explicit DataTransfer(const core::Database &database, QObject *parent = nullptr);

    QString errorString() const;
    QStringList warnings() const;
    QString lastPath() const;

    /**
     * @brief Writes a complete JSON export into a new subfolder "garagelog-export-YYYY-MM-DD" of @p folder.
     *
     * A number is appended if that subfolder exists ("...-2").
     */
    Q_INVOKABLE bool exportJson(const QString &folder);

    /** Imports an export folder (the one containing garagelog.json) into the open, empty garage. */
    Q_INVOKABLE bool importJson(const QString &folder);

    /** Writes one CSV file per table into a new subfolder "garagelog-csv-YYYY-MM-DD" of @p folder. */
    Q_INVOKABLE bool exportCsv(const QString &folder);

    /** Loads a CSV file for the fuel import page. */
    Q_INVOKABLE bool loadCsv(const QString &file);

    /** Column names of row @p headerRow of the loaded CSV. */
    Q_INVOKABLE QStringList csvHeader(int headerRow) const;

    /** Suggested mapping as map; keys as in toMapping(). */
    Q_INVOKABLE QVariantMap suggestCsvMapping() const;

    /**
     * @brief Dry run of the fuel import.
     * @param mapping Keys headerRow, dateColumn, odometerColumn, quantityColumn,
     *        totalCostColumn, unitPriceColumn, fullTankColumn, stationColumn,
     *        notesColumn (ints, -1 = none), dateFormat, decimalSeparator ("." or ",").
     * @return {count: valid rows, entries: up to previewLimit maps {line, date (ISO),
     *         odometerM, quantity, totalCostC, fullTank, station}, errors: list of texts}.
     */
    Q_INVOKABLE QVariantMap previewCsv(const QVariantMap &mapping) const;

    /** Imports the valid rows for @p vehicleId. @return Number imported, -1 on failure (errorString set). */
    Q_INVOKABLE int importCsv(const QString &vehicleId, const QVariantMap &mapping);

    /** CsvFuelMapping <-> QVariantMap, public for tests. */
    static core::CsvFuelMapping toMapping(const QVariantMap &map);
    static QVariantMap fromMapping(const core::CsvFuelMapping &mapping);

Q_SIGNALS:
    void resultChanged();
    /** Rows were added to the garage; models must reload. */
    void dataImported();

private:
    /** Creates "<folder>/<base>-YYYY-MM-DD" (or "...-2", ...) and returns its path; empty on failure. */
    QString newSubfolder(const QString &folder, const QString &base);
    void setResult(const QString &error, const QStringList &warnings = {}, const QString &path = {});

    const core::Database &m_database;
    std::unique_ptr<core::CsvFuelImporter> m_csv;
    QString m_error;
    QStringList m_warnings;
    QString m_lastPath;
};

} // namespace garagelog::app
