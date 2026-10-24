// Implementation of the JSON import (role: core).

#include "jsonimporter.h"

#include "attachmentstore.h"
#include "database.h"
#include "jsonexporter.h"
#include "migrationrunner.h"
#include "odometer.h"
#include "valuelimits.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>

#include <cmath>
#include <limits>

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
        // Casting a double outside the 64-bit range is undefined behaviour;
        // such values stay REAL and are rejected by validationError().
        if (!std::isfinite(d) || std::fabs(d) >= 9.0e18) {
            return QVariant(d);
        }
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

/** SQL condition: @p column holds an integer in [@p low, @p high]. */
QString integerIn(const QString &column, qint64 low, qint64 high)
{
    return QStringLiteral("(typeof(%1) = 'integer' AND %1 BETWEEN %2 AND %3)").arg(column).arg(low).arg(high);
}

/** Same as integerIn(), NULL allowed. */
QString optionalIntegerIn(const QString &column, qint64 low, qint64 high)
{
    return QStringLiteral("(%1 IS NULL OR %2)").arg(column, integerIn(column, low, high));
}

/** SQL condition: @p column is a real date written as YYYY-MM-DD from limits::minYear on. */
QString isoDate(const QString &column)
{
    // date() normalises or rejects anything else ("2025-02-30", "kaputt"),
    // so only a canonical, existing date compares equal to itself.
    return QStringLiteral("(typeof(%1) = 'text' AND date(%1) IS %1 AND %1 >= '%2-01-01')").arg(column).arg(limits::minYear);
}

/**
 * @brief Checks the imported rows against the same rules as the repositories.
 *
 * The importer inserts rows as they are, so a hand-edited or foreign file
 * could otherwise bring negative odometers, text in number columns or
 * impossible dates into statistics and due dates. Runs inside the import
 * transaction, before the commit.
 * @return "" if every row is valid, otherwise which table and row is not.
 */
QString validationError(QSqlDatabase &db)
{
    using namespace limits;
    const QString flag = QStringLiteral("(typeof(%1) = 'integer' AND %1 IN (0, 1))");
    const QList<std::pair<QString, QString>> rules{
        {QStringLiteral("vehicles"),
         QStringList{optionalIntegerIn(QStringLiteral("year"), 1885, maxYear),
                     integerIn(QStringLiteral("odometer_offset_m"), -maxDistanceM, maxDistanceM),
                     optionalIntegerIn(QStringLiteral("odometer_reading_m"), 0, maxDistanceM), flag.arg(QStringLiteral("archived"))}
             .join(QStringLiteral(" AND "))},
        {QStringLiteral("fuel_entries"),
         QStringList{isoDate(QStringLiteral("date")), integerIn(QStringLiteral("odometer_m"), 0, maxDistanceM),
                     integerIn(QStringLiteral("quantity"), 1, maxQuantity), integerIn(QStringLiteral("total_cost_c"), 0, maxMoneyC),
                     flag.arg(QStringLiteral("full_tank"))}
             .join(QStringLiteral(" AND "))},
        {QStringLiteral("service_entries"),
         QStringList{isoDate(QStringLiteral("date")), optionalIntegerIn(QStringLiteral("odometer_m"), 0, maxDistanceM),
                     integerIn(QStringLiteral("cost_c"), 0, maxMoneyC)}
             .join(QStringLiteral(" AND "))},
        {QStringLiteral("maintenance_intervals"),
         QStringList{optionalIntegerIn(QStringLiteral("every_m"), 1, maxDistanceM), optionalIntegerIn(QStringLiteral("every_months"), 1, maxIntervalMonths),
                     QStringLiteral("(every_m IS NOT NULL OR every_months IS NOT NULL)"), flag.arg(QStringLiteral("active"))}
             .join(QStringLiteral(" AND "))},
        {QStringLiteral("attachments"), optionalIntegerIn(QStringLiteral("size_bytes"), 0, std::numeric_limits<qint64>::max())},
        {QStringLiteral("trips"),
         QStringList{isoDate(QStringLiteral("date")), integerIn(QStringLiteral("distance_m"), 0, maxDistanceM),
                     optionalIntegerIn(QStringLiteral("odometer_start_m"), 0, maxDistanceM),
                     optionalIntegerIn(QStringLiteral("odometer_end_m"), 0, maxDistanceM)}
             .join(QStringLiteral(" AND "))},
    };
    for (const auto &[table, condition] : rules) {
        QSqlQuery query(db);
        // COALESCE: a NULL condition (e.g. NULL in a NOT NULL-free column) counts as invalid.
        if (!query.exec(QStringLiteral("SELECT id FROM %1 WHERE NOT COALESCE(%2, 0) LIMIT 1").arg(table, condition))) {
            return QStringLiteral("Cannot check %1: %2").arg(table, query.lastError().text());
        }
        if (query.next()) {
            return QStringLiteral("Invalid values in %1, row %2").arg(table, query.value(0).toString());
        }
    }
    return {};
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
    const QJsonObject root = readExport(directory, m_error);
    const QJsonObject tables = root.value(QStringLiteral("tables")).toObject();
    const int exportVersion = root.value(QStringLiteral("schema_version")).toInt();

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
                if (!AttachmentStore::isSafeStoredName(name)) {
                    return fail(QStringLiteral("Invalid attachment file name %1").arg(name));
                }
                const QFileInfo sourceFile(sourceFiles.filePath(name));
                if (!sourceFile.exists() && !sourceFile.isSymLink()) {
                    m_warnings.append(QStringLiteral("Attachment file missing in export: %1").arg(name));
                } else if (sourceFile.isSymLink() || !sourceFile.isFile()) {
                    // A symlink in a received export could pull any file of
                    // the user (e.g. ~/.ssh/id_rsa) into the garage.
                    return fail(QStringLiteral("Attachment %1 is not a regular file").arg(name));
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

    // Exports of schema 1 carry the old odometer offset; convert it the
    // same way migration 0002 converted the garages of that time.
    if (exportVersion < 2) {
        QSqlQuery convert(db);
        if (!convert.exec(Odometer::convertOffsetsSql)) {
            return fail(QStringLiteral("Cannot convert odometer offsets: %1").arg(convert.lastError().text()));
        }
    }
    if (const QString invalid = validationError(db); !invalid.isEmpty()) {
        return fail(invalid);
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
