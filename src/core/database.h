// SQLite connection owner for one garage file (role: core).
// Opens or creates the file, enables foreign keys and runs pending
// migrations; repositories (from M03 on) get their QSqlDatabase from here.

#pragma once

#include <QSqlDatabase>
#include <QString>

namespace garagelog::core {

/**
 * @brief Owns one named QSqlDatabase connection to a garage file.
 *
 * Each instance registers its own uniquely named Qt SQL connection, so
 * several garages (or tests) can be open at the same time. Not copyable;
 * the connection is removed from Qt's registry in close()/the destructor.
 */
class Database
{
public:
    /** Result of fileState(): can the open garage file still be written? */
    enum class FileState {
        Ok,       ///< file exists and file plus folder are writable
        Missing,  ///< deleted or moved while open; writes would go to an unlinked file and be lost on exit
        ReadOnly, ///< file or its folder is write-protected; SQLite can read but every write fails
        Closed,   ///< no garage is open
    };

    /** What open() may do with a path that has no file yet. */
    enum class OpenMode {
        CreateIfMissing, ///< create file and parent folders (new garage, default garage)
        MustExist,       ///< fail instead: a remembered or chosen garage that is gone must not come back empty
    };

    /** Result of probe(): what kind of file a path is, without changing it. */
    enum class Probe {
        Missing,   ///< no file at the path
        Empty,     ///< 0-byte file, can become a new garage
        Garage,    ///< SQLite file with a schema_version table
        Foreign,   ///< SQLite file of another application
        NotSqlite, ///< not readable as SQLite (text, binary, corrupt, folder)
    };

    /**
     * @brief Looks at @p filePath on a separate read-only connection.
     *
     * Our migrations must never run on a foreign SQLite file, so open()
     * and the settings page check a file with this first.
     * @param detail Receives the SQLite error for NotSqlite (may be nullptr).
     */
    static Probe probe(const QString &filePath, QString *detail = nullptr);

    /** File name of a garage inside the app data directory (SPEC: garage.sqlite). */
    static inline const QString defaultFileName = QStringLiteral("garage.sqlite");

    Database();
    ~Database();
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    /**
     * @brief Default garage path: QStandardPaths::AppDataLocation + "/garage.sqlite".
     *
     * Resolves to ~/.local/share/garagelog/garage.sqlite as long as no
     * organization name is set on QCoreApplication (only the domain is).
     */
    static QString defaultFilePath();

    /**
     * @brief Opens (or creates) the SQLite file and migrates it to the newest schema.
     * @param filePath Path to the .sqlite file; with CreateIfMissing missing
     *        parent directories are created.
     * @param mode Whether a missing file is created or is an error.
     * @return true on success. false if the file is missing (MustExist), is
     *         not a garage (see probe()), cannot be opened, foreign keys
     *         cannot be enabled or a migration fails; errorString() says why.
     *         The new file is opened on its own connection first: if that
     *         fails, an already open garage stays open and usable.
     */
    bool open(const QString &filePath, OpenMode mode = OpenMode::CreateIfMissing);

    /** Closes the connection; no-op if not open. QSqlDatabase copies obtained via connection() become invalid. */
    void close();

    /** True between a successful open() and close(). */
    bool isOpen() const;

    /**
     * @brief Handle to the open connection, for repositories and QSqlQuery.
     * @return Invalid QSqlDatabase if not open. Do not keep the copy beyond
     *         the lifetime of this Database (Qt warns on removal otherwise).
     */
    QSqlDatabase connection() const;

    /** Path given to the last successful open(), empty if not open. */
    QString filePath() const;

    /** Reason for the last failed open(). */
    QString errorString() const;

    /**
     * @brief Checks the open file on disk (cheap: two stat() calls, no SQL).
     *
     * The folder counts as well, because SQLite creates its journal file next
     * to the database while writing. On Linux an open file that is deleted
     * stays readable and writable through the connection, so a missing file
     * is not an error SQLite would ever report by itself.
     */
    FileState fileState() const;

private:
    /** Applies connection settings and migrations; returns false with m_error set. */
    bool prepare(QSqlDatabase &db);

    QString m_connectionName;
    QString m_filePath;
    QString m_error;
};

} // namespace garagelog::core
