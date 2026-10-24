// Implementation of the JSON export (role: core).

#include "jsonexporter.h"

#include "attachmentstore.h"
#include "database.h"
#include "dbvalues.h"
#include "migrationrunner.h"
#include "version.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

namespace garagelog::core {

namespace {

/** SQLite value -> JSON: integers stay integers, NULL becomes null, the rest text. */
QJsonValue toJson(const QVariant &value)
{
    if (value.isNull()) {
        return QJsonValue::Null;
    }
    switch (value.typeId()) {
    case QMetaType::Int:
    case QMetaType::LongLong:
    case QMetaType::UInt:
    case QMetaType::ULongLong:
        // Fits exactly: double holds integers up to 2^53, far beyond any
        // odometer in metres or amount in cents.
        return QJsonValue(value.toLongLong());
    case QMetaType::Double:
        return QJsonValue(value.toDouble());
    default:
        return QJsonValue(value.toString());
    }
}

} // namespace

JsonExporter::JsonExporter(const Database &database)
    : m_database(database)
{
}

bool JsonExporter::exportTo(const QString &directory)
{
    m_error.clear();
    m_warnings.clear();
    QDir dir(directory);
    if (!dir.mkpath(QStringLiteral("."))) {
        m_error = QStringLiteral("Cannot create directory %1").arg(directory);
        return false;
    }
    // Never mix two exports in one folder; the attachments would get mixed up.
    if (dir.exists(exportformat::jsonFileName) || dir.exists(AttachmentStore::directoryName)) {
        m_error = QStringLiteral("%1 already contains an export").arg(directory);
        return false;
    }

    QJsonObject tables;
    QStringList storedNames;
    for (const QString &table : exportformat::tables) {
        QSqlQuery query(m_database.connection());
        // The table names come from the fixed list above, never from input.
        if (!query.exec(QStringLiteral("SELECT * FROM %1 ORDER BY 1").arg(table))) {
            m_error = QStringLiteral("Cannot read %1: %2").arg(table, query.lastError().text());
            return false;
        }
        QJsonArray rows;
        while (query.next()) {
            const QSqlRecord record = query.record();
            QJsonObject row;
            for (int c = 0; c < record.count(); ++c) {
                row.insert(record.fieldName(c), toJson(query.value(c)));
            }
            if (table == QLatin1String("attachments")) {
                storedNames.append(row.value(QStringLiteral("stored_name")).toString());
            }
            rows.append(row);
        }
        tables.insert(table, rows);
    }

    QJsonObject root{
        {QStringLiteral("format"), exportformat::formatName},
        {QStringLiteral("schema_version"), MigrationRunner::schemaVersion(m_database.connection())},
        {QStringLiteral("app_version"), version()},
        {QStringLiteral("exported_at"), db::toDbTimestamp(db::nowUtc())},
        {QStringLiteral("tables"), tables},
    };

    // Copy the files first: a JSON file next to an incomplete attachments
    // folder would look like a finished export.
    AttachmentStore store(m_database);
    if (!storedNames.isEmpty() && !dir.mkpath(AttachmentStore::directoryName)) {
        m_error = QStringLiteral("Cannot create %1").arg(dir.filePath(AttachmentStore::directoryName));
        return false;
    }
    for (const QString &name : storedNames) {
        // The names come from the garage file; a crafted one like
        // "../../.config/autostart/x.desktop" must not become a path.
        const QString from = store.filePath(name);
        if (from.isEmpty()) {
            m_warnings.append(QStringLiteral("Attachment file name not allowed, skipped: %1").arg(name));
            continue;
        }
        const QString to = dir.filePath(AttachmentStore::directoryName + QLatin1Char('/') + name);
        if (!QFile::exists(from)) {
            m_warnings.append(QStringLiteral("Attachment file missing: %1").arg(from));
        } else if (!QFile::copy(from, to)) {
            m_error = QStringLiteral("Cannot copy %1 to %2").arg(from, to);
            return false;
        }
    }

    // QSaveFile writes to a temporary file and renames on commit, so a
    // crash never leaves a half-written JSON behind.
    QSaveFile file(dir.filePath(exportformat::jsonFileName));
    if (!file.open(QIODevice::WriteOnly)) {
        m_error = QStringLiteral("Cannot write %1: %2").arg(file.fileName(), file.errorString());
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        m_error = QStringLiteral("Cannot write %1: %2").arg(file.fileName(), file.errorString());
        return false;
    }
    return true;
}

QStringList JsonExporter::warnings() const
{
    return m_warnings;
}

QString JsonExporter::errorString() const
{
    return m_error;
}

} // namespace garagelog::core
