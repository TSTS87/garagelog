// Minimal CSV reading and writing (role: core).
// RFC 4180 style: fields separated by a delimiter, quoted with '"' when
// they contain the delimiter, quotes or line breaks; '""' is a literal quote.
// Used by CsvExporter and CsvFuelImporter.

#pragma once

#include <QChar>
#include <QList>
#include <QString>
#include <QStringList>

namespace garagelog::core::csv {

/**
 * @brief Splits CSV text into rows of fields.
 *
 * Quoted fields may contain the delimiter and line breaks. Both "\n" and
 * "\r\n" end a row; a final line break does not create an empty row.
 * A UTF-8 byte order mark at the start is ignored.
 */
QList<QStringList> parse(const QString &text, QChar delimiter);

/**
 * @brief Guesses the delimiter: the one of ',', ';' and tab that appears
 *        most often outside quotes in the first lines. ',' if none appears.
 */
QChar detectDelimiter(const QString &text);

/** One field quoted if needed, e.g. `a"b` -> `"a""b"`. */
QString escape(const QString &field, QChar delimiter = QLatin1Char(','));

/** One row joined with @p delimiter and terminated with "\r\n" (RFC 4180). */
QString row(const QStringList &fields, QChar delimiter = QLatin1Char(','));

} // namespace garagelog::core::csv
