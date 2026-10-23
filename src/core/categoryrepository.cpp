// Implementation of the service_categories data access (role: core).

#include "categoryrepository.h"

#include "database.h"
#include "dbvalues.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace garagelog::core {

using namespace db;

namespace {

/**
 * Column list shared by all SELECTs, matching readCategory(). Built-ins
 * keep the order of the seed INSERT (rowid), which groups related items
 * (oil, tires, brakes, ...); user categories follow alphabetically.
 */
const QString selectColumns = QStringLiteral("SELECT id, name, builtin, icon FROM service_categories");
const QString orderBy = QStringLiteral(" ORDER BY builtin DESC, CASE WHEN builtin = 1 THEN rowid END, name COLLATE NOCASE");

ServiceCategory readCategory(const QSqlQuery &query)
{
    ServiceCategory c;
    c.id = query.value(0).toString();
    c.name = query.value(1).toString();
    c.builtin = query.value(2).toInt() != 0;
    c.icon = query.value(3).toString();
    return c;
}

} // namespace

CategoryRepository::CategoryRepository(const Database &database)
    : m_database(database)
{
}

QList<ServiceCategory> CategoryRepository::all()
{
    m_error.clear();
    QSqlQuery query(m_database.connection());
    if (!query.exec(selectColumns + orderBy)) {
        m_error = QStringLiteral("Cannot list categories: %1").arg(query.lastError().text());
        return {};
    }
    QList<ServiceCategory> result;
    while (query.next()) {
        result.append(readCategory(query));
    }
    return result;
}

std::optional<ServiceCategory> CategoryRepository::byId(const QString &id)
{
    m_error.clear();
    QSqlQuery query(m_database.connection());
    query.prepare(selectColumns + QStringLiteral(" WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot read category: %1").arg(query.lastError().text());
        return std::nullopt;
    }
    if (!query.next()) {
        m_error = QStringLiteral("No category with id %1").arg(id);
        return std::nullopt;
    }
    return readCategory(query);
}

std::optional<ServiceCategory> CategoryRepository::byName(const QString &name)
{
    m_error.clear();
    QSqlQuery query(m_database.connection());
    query.prepare(selectColumns + QStringLiteral(" WHERE name = ?"));
    query.addBindValue(name);
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot read category: %1").arg(query.lastError().text());
        return std::nullopt;
    }
    if (!query.next()) {
        return std::nullopt;
    }
    return readCategory(query);
}

bool CategoryRepository::validateName(const QString &name, const QString &ownId)
{
    if (name.isEmpty()) {
        m_error = QStringLiteral("Category needs a name");
        return false;
    }
    // The UNIQUE constraint would catch this too, but only with an SQL
    // message the user cannot act on.
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral("SELECT 1 FROM service_categories WHERE name = ? AND id IS NOT ?"));
    query.addBindValue(name);
    query.addBindValue(ownId);
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot check category name: %1").arg(query.lastError().text());
        return false;
    }
    if (query.next()) {
        m_error = QStringLiteral("A category named \"%1\" already exists").arg(name);
        return false;
    }
    return true;
}

bool CategoryRepository::create(ServiceCategory &category)
{
    m_error.clear();
    ServiceCategory c = category;
    c.name = c.name.trimmed();
    c.builtin = false;
    if (!validateName(c.name, QString())) {
        return false;
    }
    c.id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral("INSERT INTO service_categories (id, name, builtin, icon) VALUES (?, ?, 0, ?)"));
    query.addBindValue(c.id);
    query.addBindValue(c.name);
    query.addBindValue(nullIfEmpty(c.icon));
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot create category: %1").arg(query.lastError().text());
        return false;
    }
    category = c;
    return true;
}

bool CategoryRepository::update(ServiceCategory &category)
{
    m_error.clear();
    const auto stored = byId(category.id);
    if (!stored) {
        return false;
    }
    // Built-in names are translation keys; renaming would break the UI text.
    if (stored->builtin) {
        m_error = QStringLiteral("Built-in categories cannot be changed");
        return false;
    }
    ServiceCategory c = category;
    c.name = c.name.trimmed();
    c.builtin = false;
    if (!validateName(c.name, c.id)) {
        return false;
    }

    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral("UPDATE service_categories SET name = ?, icon = ? WHERE id = ?"));
    query.addBindValue(c.name);
    query.addBindValue(nullIfEmpty(c.icon));
    query.addBindValue(c.id);
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot update category: %1").arg(query.lastError().text());
        return false;
    }
    category = c;
    return true;
}

bool CategoryRepository::remove(const QString &id)
{
    m_error.clear();
    const auto stored = byId(id);
    if (!stored) {
        return false;
    }
    if (stored->builtin) {
        m_error = QStringLiteral("Built-in categories cannot be deleted");
        return false;
    }

    // Checked explicitly for a clear message; the foreign keys would only
    // report "FOREIGN KEY constraint failed".
    QSqlQuery used(m_database.connection());
    used.prepare(QStringLiteral(
        "SELECT EXISTS (SELECT 1 FROM service_entries WHERE category_id = ?) "
        "    OR EXISTS (SELECT 1 FROM maintenance_intervals WHERE category_id = ?)"));
    used.addBindValue(id);
    used.addBindValue(id);
    if (!used.exec() || !used.next()) {
        m_error = QStringLiteral("Cannot check category usage: %1").arg(used.lastError().text());
        return false;
    }
    if (used.value(0).toInt() != 0) {
        m_error = QStringLiteral("Category \"%1\" is still used by service entries or intervals").arg(stored->name);
        return false;
    }

    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral("DELETE FROM service_categories WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot delete category: %1").arg(query.lastError().text());
        return false;
    }
    return true;
}

QString CategoryRepository::errorString() const
{
    return m_error;
}

} // namespace garagelog::core
