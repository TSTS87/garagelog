// Implementation of the JSON import (role: core).

#include "jsonimporter.h"

#include "attachmentstore.h"
#include "database.h"
#include "jsonexporter.h"
#include "migrationrunner.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>

namespace garagelog::core {

namespace {

/** JSON -> SQLite value; numbers without fraction become integers again. */
QVariant toSql(const QJsonValue &value)
{
    switch (value.type()) {
    case QJsonValue::Null:
    case QJsonValue::Undefined:
        return QVariant();
    case QJsonValue::Bool:
        return value.toBool() ? 1 : 0;
    case QJsonValue::Double: {
        const double d = value.toDouble();
        const auto i = static_cast<qint64>(d);
        return static_cast<double>(i) == d ? QVariant(i) : QVariant(d);
    }
    default:
        return value.toString();
    }
}

/** Column names of @p table from SQLite itself; the only names ever put into SQL text. */
QSet<QString> columnsOf(const QSqlDatabase &db, const QString &table)
{
    QSet<QString> columns;
    QSqlQuery query(db);
    if (query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table))) {
        while (query.next()) {
            columns.insert(query.value(1).toString());
        }
    }
    return columns;
}

/** Same rule as AttachmentStore::removeFiles(): a plain file name inside attachments/. */
bool isSafeStoredName(const QString &name)
{
    return !name.isEmpty() && !name.contains(QLatin1Char('/')) && !name.contains(QLatin1Char('\\'))
        && !name.startsWith(QLatin1Char('.'));
}

/** Reads and parses <dir>/garagelog.json; sets @p error on failure. */
QJsonObject readExport(const QString &directory, QString &error)
{
    QFile file(QDir(directory).filePath(exportformat::jsonFileName));
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("Cannot read %1: %2").arg(file.fileName(), file.errorString());
        return {};
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (doc.isNull() || !doc.isObject()) {
        error = QStringLiteral("%1 is not valid JSON: %2").arg(file.fileName(), parseError.errorString());
        return {};
    }
    return doc.object();
}

} // namespace

JsonImporter::JsonImporter(const Database &database)
    : m_database(database)
{
}

bool JsonImporter::check(const QString &directory)
{
    m_error.clear();
    const QJsonObject root = readExport(directory, m_error);
    if (!m_error.isEmpty()) {
        return false;
    }
    if (root.value(QStringLiteral("format")).toString() != exportformat::formatName) {
        m_error = QStringLiteral("This is not a GarageLog export");
        return false;
    }
    const int exportVersion = root.value(QStringLiteral("schema_version")).toInt(-1);
    const int ownVersion = MigrationRunner::schemaVersion(m_database.connection());
    if (exportVersion < 1 || exportVersion > ownVersion) {
        m_error = QStringLiteral("The export has schema version %1, this garage supports up to %2; please update GarageLog")
                      .arg(exportVersion)
                      .arg(ownVersion);
        return false;
    }
    QSqlQuery query(m_database.connection());
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM vehicles")) || !query.next()) {
        m_error = QStringLiteral("Cannot check garage: %1").arg(query.lastError().text());
        return false;
    }
    if (query.value(0).toInt() > 0) {
        m_error = QStringLiteral("The garage already contains vehicles; import only works into a new, empty garage");
        return false;
    }
    return true;
}

bool JsonImporter::importFrom(const QString &directory)
{
    m_importedRows = 0;
    m_warnings.clear();
    if (!check(directory)) {
        return false;
    }
    const QJsonObject tables = readExport(directory, m_error).value(QStringLiteral("tables")).toObject();

    QSqlDatabase db = m_database.connection();
    if (!db.transaction()) {
        m_error = QStringLiteral("Cannot start transaction: %1").arg(db.lastError().text());
        return false;
    }
    QStringList copiedFiles;
    AttachmentStore store(m_database);
    const QDir sourceFiles(QDir(directory).filePath(AttachmentStore::directoryName));
    const QDir targetFiles(store.directory());

    // Undo everything: rows via rollback, files by hand.
    auto fail = [&](const QString &error) {
        db.rollback();
        for (const QString &path : std::as_const(copiedFiles)) {
            QFile::remove(path);
        }
        m_error = error;
        m_importedRows = 0;
        return false;
    };

    int rows = 0;
    for (const QString &table : exportformat::tables) {
        const QSet<QString> columns = columnsOf(db, table);
        for (const QJsonValue &rowValue : tables.value(table).toArray()) {
            const QJsonObject row = rowValue.toObject();
            QStringList names;
            QVariantList values;
            for (auto it = row.begin(); it != row.end(); ++it) {
                // Unknown keys cannot come from an older or equal schema;
                // skip them instead of building SQL from foreign text.
                if (!columns.contains(it.key())) {
                    m_warnings.append(QStringLiteral("Unknown column %1.%2 skipped").arg(table, it.key()));
                    continue;
                }
                names.append(it.key());
                values.append(toSql(it.value()));
            }
            if (names.isEmpty()) {
                continue;
            }

            QString verb = QStringLiteral("INSERT");
            if (table == QLatin1String("service_categories")) {
                // Built-ins exist in every garage with the same fixed ids.
                QSqlQuery exists(db);
                exists.prepare(QStringLiteral("SELECT 1 FROM service_categories WHERE id = ?"));
                exists.addBindValue(row.value(QStringLiteral("id")).toString());
                if (exists.exec() && exists.next()) {
                    continue;
                }
            } else if (table == QLatin1String("settings")) {
                verb = QStringLiteral("INSERT OR REPLACE");
            } else if (table == QLatin1String("attachments")) {
                const QString name = row.value(QStringLiteral("stored_name")).toString();
                if (!isSafeStoredName(name)) {
                    return fail(QStringLiteral("Invalid attachment file name %1").arg(name));
                }
                if (!sourceFiles.exists(name)) {
                    m_warnings.append(QStringLiteral("Attachment file missing in export: %1").arg(name));
                } else {
                    if (!QDir().mkpath(targetFiles.path())) {
                        return fail(QStringLiteral("Cannot create %1").arg(targetFiles.path()));
                    }
                    const QString target = targetFiles.filePath(name);
                    if (!QFile::copy(sourceFiles.filePath(name), target)) {
                        return fail(QStringLiteral("Cannot copy attachment %1").arg(name));
                    }
                    copiedFiles.append(target);
                }
            }

            QSqlQuery insert(db);
            insert.prepare(QStringLiteral("%1 INTO %2 (%3) VALUES (%4)")
                               .arg(verb, table, names.join(QStringLiteral(", ")), QStringList(names.size(), QStringLiteral("?")).join(QStringLiteral(", "))));
            for (const QVariant &v : std::as_const(values)) {
                insert.addBindValue(v);
            }
            if (!insert.exec()) {
                return fail(QStringLiteral("Cannot import into %1: %2").arg(table, insert.lastError().text()));
            }
            ++rows;
        }
    }

    if (!db.commit()) {
        return fail(QStringLiteral("Cannot commit import: %1").arg(db.lastError().text()));
    }
    m_importedRows = rows;
    return true;
}

int JsonImporter::importedRows() const
{
    return m_importedRows;
}

QStringList JsonImporter::warnings() const
{
    return m_warnings;
}

QString JsonImporter::errorString() const
{
    return m_error;
}

} // namespace garagelog::core
