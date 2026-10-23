// Implementation of the garage database connection (role: core).

#include "database.h"

#include "migrationrunner.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

namespace garagelog::core {

Database::Database()
    // Qt identifies connections by name; a UUID avoids clashes between instances.
    : m_connectionName(QStringLiteral("garagelog-") + QUuid::createUuid().toString(QUuid::WithoutBraces))
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

bool Database::open(const QString &filePath)
{
    close();
    m_error.clear();

    const QFileInfo info(filePath);
    if (!QDir().mkpath(info.absolutePath())) {
        m_error = QStringLiteral("Cannot create directory %1").arg(info.absolutePath());
        return false;
    }

    // Scoped so the local handle is gone before close() calls removeDatabase().
    bool ok = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
        db.setDatabaseName(info.absoluteFilePath());
        if (!db.open()) {
            m_error = QStringLiteral("Cannot open %1: %2").arg(filePath, db.lastError().text());
        } else {
            ok = prepare(db);
        }
    }
    if (!ok) {
        close();
        return false;
    }

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

} // namespace garagelog::core
