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
            fields.append(query.value(c).isNull() ? QString() : query.value(c).toString());
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
