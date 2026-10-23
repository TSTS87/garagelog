// Schema migration runner (role: core).
// Applies the numbered SQL files NNNN_*.sql (embedded at :/migrations) that a
// database has not seen yet, each in its own transaction, and keeps
// schema_version in sync. Used by Database::open(); tests drive it directly.

#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QStringList>

namespace garagelog::core {

/**
 * @brief Brings an SQLite database up to the newest embedded schema version.
 *
 * Migration files are named `NNNN_description.sql` with a four-digit,
 * strictly increasing number; the number is the schema version the file
 * produces. Migrations are additive and never rolled back (see CLAUDE.md).
 */
class MigrationRunner
{
public:
    /** Resource directory holding the migrations compiled into garagelog_core. */
    static inline const QString defaultDirectory = QStringLiteral(":/migrations");

    /**
     * @param directory Directory (file system or Qt resource) to read the
     *        `NNNN_*.sql` files from. Tests pass a temporary directory.
     */
    explicit MigrationRunner(QString directory = defaultDirectory);

    /**
     * @brief Applies all migrations with a number above the current schema version.
     * @param db Open SQLite connection.
     * @return true if the database is at latestVersion() afterwards (also when
     *         nothing had to be done). false on SQL errors, on a malformed
     *         migration directory, or if the database is newer than this
     *         build knows; errorString() then says why. A failed migration is
     *         rolled back, earlier successful ones stay applied.
     */
    bool run(QSqlDatabase &db);

    /**
     * @brief Reads the schema version stored in the database.
     * @return 0 for an empty database (no schema_version table), -1 on SQL error.
     */
    static int schemaVersion(const QSqlDatabase &db);

    /**
     * @brief Highest migration number found in the directory.
     * @return 0 if there are no migrations, -1 if the directory is malformed
     *         (duplicate numbers or a .sql file not matching `NNNN_*.sql`).
     */
    int latestVersion();

    /** Human-readable reason for the last failure of run() or latestVersion(). */
    QString errorString() const;

    /**
     * @brief Splits an SQL script into single statements.
     *
     * Needed because QSqlQuery::exec() runs only one statement per call with
     * the SQLite driver. Removes `--` and `/ * * /` comments and splits on `;`
     * outside of quoted text. Not supported: statements that themselves
     * contain `;`, i.e. CREATE TRIGGER ... BEGIN ...; END.
     *
     * @param sql Complete script text.
     * @return Trimmed, non-empty statements without the trailing `;`.
     */
    static QStringList splitStatements(const QString &sql);

private:
    /** One migration file with its parsed version number. */
    struct Migration {
        int version;
        QString filePath;
    };

    /** Scans the directory; on failure sets m_error and returns false. */
    bool scan(QList<Migration> &out);

    /** Executes one file and bumps schema_version, all in one transaction. */
    bool apply(QSqlDatabase &db, const Migration &migration);

    QString m_directory;
    QString m_error;
};

} // namespace garagelog::core
