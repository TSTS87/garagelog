// Implementation of the settings access (role: core).

#include "settingsrepository.h"

#include "database.h"

#include <QSqlError>
#include <QSqlQuery>

namespace garagelog::core {

SettingsRepository::SettingsRepository(const Database &database)
    : m_database(database)
{
}

QString SettingsRepository::value(const QString &key, const QString &fallback)
{
    m_error.clear();
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral("SELECT value FROM settings WHERE key = ?"));
    query.addBindValue(key);
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot read setting %1: %2").arg(key, query.lastError().text());
        return fallback;
    }
    return query.next() ? query.value(0).toString() : fallback;
}

bool SettingsRepository::setValue(const QString &key, const QString &value)
{
    m_error.clear();
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)"));
    query.addBindValue(key);
    query.addBindValue(value);
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot save setting %1: %2").arg(key, query.lastError().text());
        return false;
    }
    return true;
}

QString SettingsRepository::errorString() const
{
    return m_error;
}

} // namespace garagelog::core
