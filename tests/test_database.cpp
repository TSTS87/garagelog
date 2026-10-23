// Tests for Database and MigrationRunner (role: tests).
// Every test works on a fresh SQLite file in a temporary directory, so no
// test touches the user's real garage and tests do not influence each other.

#include <QFile>
#include <QSet>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

#include "database.h"
#include "migrationrunner.h"

using garagelog::core::Database;
using garagelog::core::MigrationRunner;

namespace {

/** Runs a single-value query and returns the first column of the first row. */
QVariant scalar(const QSqlDatabase &db, const QString &sql)
{
    QSqlQuery query(db);
    if (!query.exec(sql) || !query.next()) {
        return {};
    }
    return query.value(0);
}

/** Writes a text file, used to build custom migration directories. */
bool writeFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
}

/** Copies the embedded 0001 migration into @p dir so tests can add a fake 0002 next to it. */
bool copyInitialMigration(const QString &dir)
{
    return QFile::copy(QStringLiteral(":/migrations/0001_initial.sql"), dir + QStringLiteral("/0001_initial.sql"));
}

} // namespace

/**
 * @brief Covers M02: creation, migration, idempotency and schema contents.
 */
class TestDatabase : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    /** The statement splitter drops comments and does not split on ';' inside quotes. */
    void splitStatementsHandlesCommentsAndQuotes()
    {
        const QString sql = QStringLiteral(
            "-- header; with semicolon\n"
            "CREATE TABLE a (x TEXT); /* block; comment */\n"
            "INSERT INTO a VALUES ('semi;colon -- not a comment', 'it''s');\n"
            "\n;\n");
        const QStringList statements = MigrationRunner::splitStatements(sql);
        QCOMPARE(statements.size(), 2);
        QCOMPARE(statements.at(0), QStringLiteral("CREATE TABLE a (x TEXT)"));
        QCOMPARE(statements.at(1), QStringLiteral("INSERT INTO a VALUES ('semi;colon -- not a comment', 'it''s')"));
    }

    /** Opening a non-existent file creates it (including parent dirs) and migrates it to version 1. */
    void emptyDatabaseMigratesToVersion1()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("sub/garage.sqlite"));

        Database db;
        QVERIFY2(db.open(path), qPrintable(db.errorString()));
        QVERIFY(QFile::exists(path));
        QCOMPARE(MigrationRunner::schemaVersion(db.connection()), 1);
        QCOMPARE(MigrationRunner().latestVersion(), 1);
    }

    /** Running the migrations again, in the same or a new connection, changes nothing. */
    void secondRunIsIdempotent()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("garage.sqlite"));

        Database db;
        QVERIFY2(db.open(path), qPrintable(db.errorString()));

        QSqlDatabase conn = db.connection();
        MigrationRunner runner;
        QVERIFY2(runner.run(conn), qPrintable(runner.errorString()));
        conn = {};

        db.close();
        QVERIFY2(db.open(path), qPrintable(db.errorString()));

        QCOMPARE(MigrationRunner::schemaVersion(db.connection()), 1);
        QCOMPARE(scalar(db.connection(), QStringLiteral("SELECT COUNT(*) FROM schema_version")).toInt(), 1);
        QCOMPARE(scalar(db.connection(), QStringLiteral("SELECT COUNT(*) FROM service_categories")).toInt(), 13);
    }

    /** Every table from DATA_MODEL.md exists after migration 0001. */
    void allTablesExist()
    {
        QTemporaryDir dir;
        Database db;
        QVERIFY2(db.open(dir.filePath(QStringLiteral("garage.sqlite"))), qPrintable(db.errorString()));

        QSqlQuery query(db.connection());
        QVERIFY(query.exec(QStringLiteral("SELECT name FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%'")));
        QSet<QString> tables;
        while (query.next()) {
            tables.insert(query.value(0).toString());
        }

        const QSet<QString> expected{
            QStringLiteral("schema_version"),
            QStringLiteral("vehicles"),
            QStringLiteral("fuel_entries"),
            QStringLiteral("service_categories"),
            QStringLiteral("service_entries"),
            QStringLiteral("attachments"),
            QStringLiteral("maintenance_intervals"),
            QStringLiteral("trips"),
            QStringLiteral("settings"),
        };
        QCOMPARE(tables, expected);
    }

    /** The 13 built-in service categories are seeded with builtin = 1. */
    void builtinCategoriesSeeded()
    {
        QTemporaryDir dir;
        Database db;
        QVERIFY2(db.open(dir.filePath(QStringLiteral("garage.sqlite"))), qPrintable(db.errorString()));

        QSqlQuery query(db.connection());
        QVERIFY(query.exec(QStringLiteral("SELECT name FROM service_categories WHERE builtin = 1")));
        QSet<QString> names;
        while (query.next()) {
            names.insert(query.value(0).toString());
        }

        const QSet<QString> expected{
            QStringLiteral("oil_change"),
            QStringLiteral("tires"),
            QStringLiteral("brakes"),
            QStringLiteral("chain"),
            QStringLiteral("inspection"),
            QStringLiteral("mot"),
            QStringLiteral("insurance"),
            QStringLiteral("tax"),
            QStringLiteral("repair"),
            QStringLiteral("battery"),
            QStringLiteral("coolant"),
            QStringLiteral("filters"),
            QStringLiteral("other"),
        };
        QCOMPARE(names, expected);
    }

    /** Foreign keys are enforced: an entry for an unknown vehicle is rejected. */
    void foreignKeysEnforced()
    {
        QTemporaryDir dir;
        Database db;
        QVERIFY2(db.open(dir.filePath(QStringLiteral("garage.sqlite"))), qPrintable(db.errorString()));

        QSqlQuery query(db.connection());
        const bool inserted = query.exec(QStringLiteral(
            "INSERT INTO fuel_entries (id, vehicle_id, date, odometer_m, quantity, total_cost_c, created_at, updated_at) "
            "VALUES ('f1', 'no-such-vehicle', '2026-10-23', 1000, 40000, 7000, '2026-10-23T10:00:00Z', '2026-10-23T10:00:00Z')"));
        QVERIFY(!inserted);
    }

    /** A database at version n-1 is upgraded by exactly the missing migration. */
    void upgradesFromPreviousVersion()
    {
        QTemporaryDir dir;
        Database db;
        QVERIFY2(db.open(dir.filePath(QStringLiteral("garage.sqlite"))), qPrintable(db.errorString()));
        QCOMPARE(MigrationRunner::schemaVersion(db.connection()), 1);

        QTemporaryDir migrations;
        QVERIFY(copyInitialMigration(migrations.path()));
        QVERIFY(writeFile(migrations.filePath(QStringLiteral("0002_test.sql")), "CREATE TABLE test_upgrade (x INTEGER);\n"));

        QSqlDatabase conn = db.connection();
        MigrationRunner runner(migrations.path());
        QVERIFY2(runner.run(conn), qPrintable(runner.errorString()));
        QCOMPARE(MigrationRunner::schemaVersion(conn), 2);
        QCOMPARE(scalar(conn, QStringLiteral("SELECT COUNT(*) FROM sqlite_master WHERE name = 'test_upgrade'")).toInt(), 1);
        // 0001 must not have run a second time, otherwise the seed would be duplicated or fail.
        QCOMPARE(scalar(conn, QStringLiteral("SELECT COUNT(*) FROM service_categories")).toInt(), 13);
    }

    /** A failing migration is rolled back completely and the version stays unchanged. */
    void failedMigrationRollsBack()
    {
        QTemporaryDir dir;
        Database db;
        QVERIFY2(db.open(dir.filePath(QStringLiteral("garage.sqlite"))), qPrintable(db.errorString()));

        QTemporaryDir migrations;
        QVERIFY(copyInitialMigration(migrations.path()));
        QVERIFY(writeFile(migrations.filePath(QStringLiteral("0002_broken.sql")),
                          "CREATE TABLE half_done (x INTEGER);\nTHIS IS NOT SQL;\n"));

        QSqlDatabase conn = db.connection();
        MigrationRunner runner(migrations.path());
        QVERIFY(!runner.run(conn));
        QVERIFY(!runner.errorString().isEmpty());
        QCOMPARE(MigrationRunner::schemaVersion(conn), 1);
        QCOMPARE(scalar(conn, QStringLiteral("SELECT COUNT(*) FROM sqlite_master WHERE name = 'half_done'")).toInt(), 0);
    }

    /** A garage written by a newer GarageLog (higher schema version) is refused, not modified. */
    void newerDatabaseIsRefused()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("garage.sqlite"));
        {
            Database db;
            QVERIFY2(db.open(path), qPrintable(db.errorString()));
            QSqlQuery query(db.connection());
            QVERIFY(query.exec(QStringLiteral("UPDATE schema_version SET version = 99")));
        }

        Database db;
        QVERIFY(!db.open(path));
        QVERIFY(db.errorString().contains(QStringLiteral("newer")));
        QVERIFY(!db.isOpen());
    }

    /** fileState() reports a write-protected and a deleted garage file while it is open. */
    void fileStateFollowsTheFileOnDisk()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("garage.sqlite"));
        Database db;
        QCOMPARE(db.fileState(), Database::FileState::Closed);
        QVERIFY2(db.open(path), qPrintable(db.errorString()));
        QCOMPARE(db.fileState(), Database::FileState::Ok);

        QVERIFY(QFile::setPermissions(path, QFile::ReadOwner));
        QCOMPARE(db.fileState(), Database::FileState::ReadOnly);
        QVERIFY(QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner));

        QVERIFY(QFile::remove(path));
        QCOMPARE(db.fileState(), Database::FileState::Missing);
    }

    /** An up-to-date garage without write permission still opens, so its data can be read and copied. */
    void readOnlyGarageOpens()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("garage.sqlite"));
        {
            Database db;
            QVERIFY(db.open(path));
        }
        QVERIFY(QFile::setPermissions(path, QFile::ReadOwner));
        Database db;
        QVERIFY2(db.open(path), qPrintable(db.errorString()));
        QCOMPARE(db.fileState(), Database::FileState::ReadOnly);
        QCOMPARE(scalar(db.connection(), QStringLiteral("SELECT COUNT(*) FROM service_categories")).toInt(), 13);
        // Writing fails with an error instead of silently doing nothing.
        QSqlQuery query(db.connection());
        QVERIFY(!query.exec(QStringLiteral("DELETE FROM service_categories WHERE builtin = 0")));
        QVERIFY(QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner));
    }

    /** Misnamed .sql files are reported instead of being silently skipped. */
    void malformedMigrationNameIsRejected()
    {
        QTemporaryDir migrations;
        QVERIFY(writeFile(migrations.filePath(QStringLiteral("initial.sql")), "SELECT 1;"));
        MigrationRunner runner(migrations.path());
        QCOMPARE(runner.latestVersion(), -1);
        QVERIFY(runner.errorString().contains(QStringLiteral("NNNN")));
    }
};

QTEST_GUILESS_MAIN(TestDatabase)
#include "test_database.moc"
