// Implementation of the CSV table export (role: core).

#include "csvexporter.h"

#include "csv.h"
#include "database.h"
#include "jsonexporter.h"

#include <QDir>
#include <QSaveFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

namespace garagelog::core {

CsvExporter::CsvExporter(const Database &database)
    : m_database(database)
{
}

namespace {

/**
 * Text of one cell. Spreadsheets run text cells that start with = + - @
 * (or a tab/CR before them) as formulas, so a station name like
 * =HYPERLINK(...) from an imported file would become live in LibreOffice
 * or Excel; such text gets a leading apostrophe, which spreadsheets hide.
 * Numbers come from INTEGER columns as numbers and stay untouched.
 */
QString cellText(const QVariant &value)
{
    if (value.isNull()) {
        return {};
    }
    const QString text = value.toString();
    if (value.typeId() == QMetaType::QString && !text.isEmpty()
        && QStringLiteral("=+-@\t\r").contains(text.front())) {
        return QLatin1Char('\'') + text;
    }
    return text;
}

} // namespace

bool CsvExporter::exportTable(const QString &table, const QString &filePath)
{
    m_error.clear();
    // The name goes into SQL text, so only the known tables are allowed.
    if (!exportformat::tables.contains(table)) {
        m_error = QStringLiteral("Unknown table %1").arg(table);
        return false;
    }
    QSqlQuery query(m_database.connection());
    if (!query.exec(QStringLiteral("SELECT * FROM %1 ORDER BY 1").arg(table))) {
        m_error = QStringLiteral("Cannot read %1: %2").arg(table, query.lastError().text());
        return false;
    }

    const QSqlRecord record = query.record();
    QStringList header;
    for (int c = 0; c < record.count(); ++c) {
        header.append(record.fieldName(c));
    }
    QString text = csv::row(header);
    while (query.next()) {
        QStringList fields;
        for (int c = 0; c < record.count(); ++c) {
            fields.append(cellText(query.value(c)));
        }
        text += csv::row(fields);
    }

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        m_error = QStringLiteral("Cannot write %1: %2").arg(filePath, file.errorString());
        return false;
    }
    file.write(text.toUtf8());
    if (!file.commit()) {
        m_error = QStringLiteral("Cannot write %1: %2").arg(filePath, file.errorString());
        return false;
    }
    return true;
}

bool CsvExporter::exportAll(const QString &directory)
{
    m_error.clear();
    QDir dir(directory);
    if (!dir.mkpath(QStringLiteral("."))) {
        m_error = QStringLiteral("Cannot create directory %1").arg(directory);
        return false;
    }
    for (const QString &table : exportformat::tables) {
        if (!exportTable(table, dir.filePath(table + QStringLiteral(".csv")))) {
            return false;
        }
    }
    return true;
}

QString CsvExporter::errorString() const
{
    return m_error;
}

} // namespace garagelog::core
