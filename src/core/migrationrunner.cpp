// Implementation of the schema migration runner (role: core).
// See migrationrunner.h for the file naming contract.

#include "migrationrunner.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>

#include <algorithm>
#include <utility>

namespace garagelog::core {

MigrationRunner::MigrationRunner(QString directory)
    : m_directory(std::move(directory))
{
}

QString MigrationRunner::errorString() const
{
    return m_error;
}

int MigrationRunner::schemaVersion(const QSqlDatabase &db)
{
    QSqlQuery query(db);
    // An empty database has no tables at all; that is version 0, not an error.
    if (!query.exec(QStringLiteral("SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = 'schema_version'"))) {
        return -1;
    }
    if (!query.next()) {
        return 0;
    }
    // MAX() instead of plain SELECT so a stray second row cannot hide the real version.
    if (!query.exec(QStringLiteral("SELECT MAX(version) FROM schema_version")) || !query.next()) {
        return -1;
    }
    return query.value(0).toInt();
}

bool MigrationRunner::scan(QList<Migration> &out)
{
    out.clear();
    static const QRegularExpression pattern(QStringLiteral("^(\\d{4})_.+\\.sql$"));

    const QDir dir(m_directory);
    if (!dir.exists()) {
        m_error = QStringLiteral("Migration directory %1 does not exist").arg(m_directory);
        return false;
    }

    const QStringList files = dir.entryList({QStringLiteral("*.sql")}, QDir::Files);
    for (const QString &name : files) {
        const QRegularExpressionMatch match = pattern.match(name);
        // A misnamed file would silently never run; refuse instead.
        if (!match.hasMatch()) {
            m_error = QStringLiteral("Migration file %1 does not match NNNN_*.sql").arg(name);
            return false;
        }
        out.append({match.captured(1).toInt(), dir.filePath(name)});
    }

    std::sort(out.begin(), out.end(), [](const Migration &a, const Migration &b) {
        return a.version < b.version;
    });
    for (qsizetype i = 1; i < out.size(); ++i) {
        if (out[i].version == out[i - 1].version) {
            m_error = QStringLiteral("Duplicate migration number %1").arg(out[i].version);
            return false;
        }
    }
    return true;
}

int MigrationRunner::latestVersion()
{
    QList<Migration> migrations;
    if (!scan(migrations)) {
        return -1;
    }
    return migrations.isEmpty() ? 0 : migrations.last().version;
}

bool MigrationRunner::run(QSqlDatabase &db)
{
    m_error.clear();

    QList<Migration> migrations;
    if (!scan(migrations)) {
        return false;
    }

    const int current = schemaVersion(db);
    if (current < 0) {
        m_error = QStringLiteral("Cannot read schema version: %1").arg(db.lastError().text());
        return false;
    }

    const int latest = migrations.isEmpty() ? 0 : migrations.last().version;
    // Opening a newer garage file with an old build could corrupt it on write.
    if (current > latest) {
        m_error = QStringLiteral("Database schema version %1 is newer than this GarageLog build supports (%2)")
                      .arg(current)
                      .arg(latest);
        return false;
    }

    for (const Migration &migration : std::as_const(migrations)) {
        if (migration.version <= current) {
            continue;
        }
        if (!apply(db, migration)) {
            return false;
        }
    }
    return true;
}

bool MigrationRunner::apply(QSqlDatabase &db, const Migration &migration)
{
    QFile file(migration.filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_error = QStringLiteral("Cannot read %1: %2").arg(migration.filePath, file.errorString());
        return false;
    }
    const QStringList statements = splitStatements(QString::fromUtf8(file.readAll()));

    // SQLite DDL is transactional, so a failing statement leaves no half-built schema.
    if (!db.transaction()) {
        m_error = QStringLiteral("Cannot start transaction: %1").arg(db.lastError().text());
        return false;
    }

    QSqlQuery query(db);
    for (const QString &statement : statements) {
        if (!query.exec(statement)) {
            m_error = QStringLiteral("Migration %1 failed: %2\nStatement: %3")
                          .arg(QFileInfo(migration.filePath).fileName(), query.lastError().text(), statement);
            db.rollback();
            return false;
        }
    }

    // The runner owns the version bookkeeping so later migrations need not
    // repeat it; 0001 also inserts the initial row, which this then confirms.
    query.prepare(QStringLiteral("UPDATE schema_version SET version = ?"));
    query.addBindValue(migration.version);
    if (!query.exec() || query.numRowsAffected() != 1) {
        m_error = QStringLiteral("Cannot update schema_version to %1: %2").arg(migration.version).arg(query.lastError().text());
        db.rollback();
        return false;
    }

    if (!db.commit()) {
        m_error = QStringLiteral("Cannot commit migration %1: %2").arg(migration.version).arg(db.lastError().text());
        db.rollback();
        return false;
    }
    return true;
}

QStringList MigrationRunner::splitStatements(const QString &sql)
{
    QStringList statements;
    QString current;

    enum class State { Normal, SingleQuote, DoubleQuote, LineComment, BlockComment };
    State state = State::Normal;

    for (qsizetype i = 0; i < sql.size(); ++i) {
        const QChar c = sql.at(i);
        const QChar next = i + 1 < sql.size() ? sql.at(i + 1) : QChar();

        switch (state) {
        case State::Normal:
            if (c == u'-' && next == u'-') {
                state = State::LineComment;
                ++i;
            } else if (c == u'/' && next == u'*') {
                state = State::BlockComment;
                ++i;
            } else if (c == u';') {
                statements.append(current.trimmed());
                current.clear();
            } else {
                if (c == u'\'') {
                    state = State::SingleQuote;
                } else if (c == u'"') {
                    state = State::DoubleQuote;
                }
                current.append(c);
            }
            break;
        case State::SingleQuote:
            // SQL escapes a quote by doubling it (''), which this handles for
            // free: the second quote simply re-enters the string state.
            current.append(c);
            if (c == u'\'') {
                state = State::Normal;
            }
            break;
        case State::DoubleQuote:
            current.append(c);
            if (c == u'"') {
                state = State::Normal;
            }
            break;
        case State::LineComment:
            if (c == u'\n') {
                // Keep the line break so tokens around the comment stay separated.
                current.append(c);
                state = State::Normal;
            }
            break;
        case State::BlockComment:
            if (c == u'*' && next == u'/') {
                current.append(u' ');
                state = State::Normal;
                ++i;
            }
            break;
        }
    }
    statements.append(current.trimmed());

    statements.removeIf([](const QString &s) {
        return s.isEmpty();
    });
    return statements;
}

} // namespace garagelog::core
