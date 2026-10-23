// CSV import of fuel entries from other apps (role: core).
// Reads a CSV file, lets the caller map its columns to fuel entry fields
// (CsvFuelMapping), checks every row in a dry run (preview) and imports
// the valid rows. Recognises Fuelio's export layout (SPEC §4.6).

#pragma once

#include "fuelentry.h"

#include <QChar>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace garagelog::core {

class Database;

/**
 * @brief Which CSV column holds which fuel entry field, and how values are written.
 *
 * Column numbers are 0-based; -1 = not in the file. Date, odometer and
 * quantity are required, plus total cost or unit price (the other one is
 * calculated). Odometer is read in km, quantity in l (or kWh), money in
 * the currency's main unit, unit price per l (or kWh).
 */
struct CsvFuelMapping {
    int headerRow = 0;             ///< row with the column names; data starts in the next row
    int dateColumn = -1;
    int odometerColumn = -1;
    int quantityColumn = -1;
    int totalCostColumn = -1;
    int unitPriceColumn = -1;
    int fullTankColumn = -1;       ///< -1 = every row is a full tank
    int stationColumn = -1;
    int notesColumn = -1;
    QString dateFormat = QStringLiteral("yyyy-MM-dd"); ///< QDate::fromString() format
    QChar decimalSeparator = QLatin1Char('.');          ///< '.' or ','; the other one is ignored as group separator

    bool operator==(const CsvFuelMapping &) const = default;
};

/** Result of a dry run: the entries that would be imported and the rows that would not. */
struct CsvFuelPreview {
    QList<FuelEntry> entries;  ///< valid rows, vehicleId still empty
    QList<int> lines;          ///< 1-based file row of each entry, same index
    QStringList errors;        ///< one message per invalid row, e.g. "Row 7: invalid date "31.02.2024""
};

/**
 * @brief Loads CSV text once, then previews and imports it with any mapping.
 *
 * The data rows are the rows after mapping.headerRow up to the first empty
 * row or row starting with "##" (Fuelio starts its next section that way).
 */
class CsvFuelImporter
{
public:
    explicit CsvFuelImporter(const Database &database);

    /** Parses @p text; the delimiter (',', ';' or tab) is detected. */
    void load(const QString &text);

    /** Reads a UTF-8 file and calls load(). false if the file cannot be read. */
    bool loadFile(const QString &path);

    /** All parsed rows, including headers and other sections. */
    QList<QStringList> rows() const;

    /** Column names of the header row of @p mapping (empty if out of range). */
    QStringList header(const CsvFuelMapping &mapping) const;

    /**
     * @brief Guesses a mapping from the column names.
     *
     * Finds Fuelio's "## Log" section (header in the next row) and its
     * columns; otherwise uses row 0 and matches common English and German
     * names ("Date"/"Datum", "Odometer"/"Kilometerstand", "Liters"/"Menge",
     * "Total"/"Betrag", ...). The decimal separator is guessed from the
     * first data rows. Columns it cannot find stay -1.
     */
    CsvFuelMapping suggestMapping() const;

    /** Dry run: converts every data row, nothing is written. */
    CsvFuelPreview preview(const CsvFuelMapping &mapping) const;

    /**
     * @brief Imports the valid rows of preview(@p mapping) for @p vehicleId in one transaction.
     * @return Number of imported entries, or -1 if nothing was imported
     *         because a database write failed (errorString set). Invalid
     *         rows are skipped; preview() lists them.
     */
    int importInto(const QString &vehicleId, const CsvFuelMapping &mapping);

    /** Reason for the last failure. */
    QString errorString() const;

private:
    const Database &m_database;
    QList<QStringList> m_rows;
    QString m_error;
};

} // namespace garagelog::core
