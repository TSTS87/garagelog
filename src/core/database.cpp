// Implementation of the garage database connection (role: core).

#include "database.h"

#include "migrationrunner.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

namespace garagelog::core {

namespace {

/** Qt identifies connections by name; a UUID avoids clashes between instances. */
QString newConnectionName(const QString &prefix)
{
    return prefix + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

} // namespace

Database::Database()
    : m_connectionName(newConnectionName(QStringLiteral("garagelog-")))
{
}

Database::~Database()
{
    close();
}

QString Database::defaultFilePath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(defaultFileName);
}

Database::Probe Database::probe(const QString &filePath, QString *detail)
{
    const QFileInfo info(filePath);
    if (!info.exists()) {
        return Probe::Missing;
    }
    if (!info.isFile()) {
        return Probe::NotSqlite;
    }
    if (info.size() == 0) {
        return Probe::Empty;
    }
    const QString name = newConnectionName(QStringLiteral("garagelog-probe-"));
    Probe result = Probe::NotSqlite;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        db.setDatabaseName(info.absoluteFilePath());
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (db.open()) {
            QSqlQuery q(db);
            // SQLite opens anything lazily; the first read of sqlite_master
            // is what fails for text or binary files.
            if (q.exec(QStringLiteral("SELECT COUNT(*), SUM(name = 'schema_version') FROM sqlite_master WHERE type = 'table'")) && q.next()) {
                result = q.value(0).toInt() > 0 && q.value(1).toInt() == 0 ? Probe::Foreign : Probe::Garage;
            } else if (detail) {
                *detail = q.lastError().text();
            }
        } else if (detail) {
            *detail = db.lastError().text();
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(name);
    return result;
}

bool Database::open(const QString &filePath, OpenMode mode)
{
    m_error.clear();

    const QFileInfo info(filePath);
    QString detail;
    const Probe kind = probe(filePath, &detail);
    switch (kind) {
    case Probe::Missing:
        if (mode == OpenMode::MustExist) {
            m_error = QStringLiteral("%1 does not exist").arg(filePath);
            return false;
        }
        if (!QDir().mkpath(info.absolutePath())) {
            m_error = QStringLiteral("Cannot create directory %1").arg(info.absolutePath());
            return false;
        }
        break;
    case Probe::Foreign:
        m_error = QStringLiteral("%1 is not a GarageLog garage").arg(filePath);
        return false;
    case Probe::NotSqlite:
        m_error = QStringLiteral("%1 is not an SQLite database: %2").arg(filePath, detail);
        return false;
    case Probe::Empty:
    case Probe::Garage:
        break;
    }

    // Open on a new connection and only replace the current one on success,
    // so a failed switch never leaves the app without its open garage.
    const QString name = newConnectionName(QStringLiteral("garagelog-"));
    bool ok = false;
    {
        // Scoped so the local handle is gone before removeDatabase().
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        db.setDatabaseName(info.absoluteFilePath());
        if (!db.open()) {
            m_error = QStringLiteral("Cannot open %1: %2").arg(filePath, db.lastError().text());
        } else {
            ok = prepare(db);
        }
        if (!ok) {
            db.close();
        }
    }
    if (!ok) {
        QSqlDatabase::removeDatabase(name);
        return false;
    }

    // A new garage holds plates, notes and costs: readable by the owner only
    // (SQLite gives its journal the same mode). Existing files keep theirs.
    if (kind == Probe::Missing) {
        QFile::setPermissions(info.absoluteFilePath(), QFile::ReadOwner | QFile::WriteOwner);
    }

    close();
    m_connectionName = name;
    m_filePath = filePath;
    return true;
}

bool Database::prepare(QSqlDatabase &db)
{
    QSqlQuery query(db);
    // SQLite ships with foreign keys off and the setting is per connection,
    // so the RESTRICT/CASCADE rules of the schema only work after this.
    if (!query.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        m_error = QStringLiteral("Cannot enable foreign keys: %1").arg(query.lastError().text());
        return false;
    }
    // The PRAGMA silently does nothing if SQLite was built without FK support; verify.
    if (!query.exec(QStringLiteral("PRAGMA foreign_keys")) || !query.next() || query.value(0).toInt() != 1) {
        m_error = QStringLiteral("SQLite does not support foreign keys");
        return false;
    }

    // Deleted notes, plates and receipts would otherwise stay readable in
    // free pages of the file, also in copies the user hands on.
    // trusted_schema off: functions with side effects may not run from
    // views or triggers of an opened file someone else prepared.
    for (const QString &pragma : {QStringLiteral("PRAGMA secure_delete = ON"), QStringLiteral("PRAGMA trusted_schema = OFF")}) {
        if (!query.exec(pragma)) {
            m_error = QStringLiteral("Cannot configure database: %1").arg(query.lastError().text());
            return false;
        }
    }

    MigrationRunner runner;
    if (!runner.run(db)) {
        m_error = runner.errorString();
        return false;
    }
    return true;
}

void Database::close()
{
    if (!QSqlDatabase::contains(m_connectionName)) {
        return;
    }
    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        db.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
    m_filePath.clear();
}

bool Database::isOpen() const
{
    return QSqlDatabase::contains(m_connectionName) && QSqlDatabase::database(m_connectionName, false).isOpen();
}

QSqlDatabase Database::connection() const
{
    if (!QSqlDatabase::contains(m_connectionName)) {
        return {};
    }
    return QSqlDatabase::database(m_connectionName, false);
}

QString Database::filePath() const
{
    return m_filePath;
}

QString Database::errorString() const
{
    return m_error;
}

Database::FileState Database::fileState() const
{
    if (m_filePath.isEmpty()) {
        return FileState::Closed;
    }
    const QFileInfo file(m_filePath);
    if (!file.exists()) {
        return FileState::Missing;
    }
    // isWritable() asks the OS (access()), so it also sees ownership, not only the mode bits.
    if (!file.isWritable() || !QFileInfo(file.absolutePath()).isWritable()) {
        return FileState::ReadOnly;
    }
    return FileState::Ok;
}

} // namespace garagelog::core
