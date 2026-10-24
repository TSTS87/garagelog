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

/** Copies the embedded migrations into @p dir so tests can add a fake next one; @p upTo limits the copy to 0001..upTo. */
bool copyEmbeddedMigrations(const QString &dir, int upTo = 999)
{
    const QStringList names{QStringLiteral("0001_initial.sql"), QStringLiteral("0002_odometer_reading.sql")};
    for (int i = 0; i < names.size() && i < upTo; ++i) {
        if (!QFile::copy(QStringLiteral(":/migrations/") + names.at(i), dir + QLatin1Char('/') + names.at(i))) {
            return false;
        }
    }
    return true;
}

/** Schema version of a freshly created garage. */
constexpr int latest = 2;

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

    /** Opening a non-existent file creates it (including parent dirs) and migrates it to the latest version. */
    void emptyDatabaseMigratesToLatest()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("sub/garage.sqlite"));

        Database db;
        QVERIFY2(db.open(path), qPrintable(db.errorString()));
        QVERIFY(QFile::exists(path));
        QCOMPARE(MigrationRunner::schemaVersion(db.connection()), latest);
        QCOMPARE(MigrationRunner().latestVersion(), latest);
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

        QCOMPARE(MigrationRunner::schemaVersion(db.connection()), latest);
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
        QCOMPARE(MigrationRunner::schemaVersion(db.connection()), latest);

        QTemporaryDir migrations;
        QVERIFY(copyEmbeddedMigrations(migrations.path()));
        QVERIFY(writeFile(migrations.filePath(QStringLiteral("0003_test.sql")), "CREATE TABLE test_upgrade (x INTEGER);\n"));

        QSqlDatabase conn = db.connection();
        MigrationRunner runner(migrations.path());
        QVERIFY2(runner.run(conn), qPrintable(runner.errorString()));
        QCOMPARE(MigrationRunner::schemaVersion(conn), latest + 1);
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
        QVERIFY(copyEmbeddedMigrations(migrations.path()));
        QVERIFY(writeFile(migrations.filePath(QStringLiteral("0003_broken.sql")),
                          "CREATE TABLE half_done (x INTEGER);\nTHIS IS NOT SQL;\n"));

        QSqlDatabase conn = db.connection();
        MigrationRunner runner(migrations.path());
        QVERIFY(!runner.run(conn));
        QVERIFY(!runner.errorString().isEmpty());
        QCOMPARE(MigrationRunner::schemaVersion(conn), latest);
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

    /** Migration 0002 turns a start reading stored as offset into a reading and keeps small corrections as shown (ADR-016). */
    void migration0002ConvertsOffsets()
    {
        QTemporaryDir dir;
        QTemporaryDir onlyFirst;
        QVERIFY(copyEmbeddedMigrations(onlyFirst.path(), 1));
        {
            QSqlDatabase conn = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("v1"));
            conn.setDatabaseName(dir.filePath(QStringLiteral("garage.sqlite")));
            QVERIFY(conn.open());
            QVERIFY(MigrationRunner(onlyFirst.path()).run(conn));
            QSqlQuery q(conn);
            const QString stamp = QStringLiteral("'2026-10-23T10:00:00Z'");
            // start: offset 50,000 km, entry at 50,500 km (old form, doubled to 100,500)
            // fix: offset +300 km on top of 20,000 km (a later correction)
            // none: no offset; fresh: offset 7,000 km and no entries
            const QList<std::pair<QString, qint64>> vehicles{
                {QStringLiteral("start"), 50'000'000}, {QStringLiteral("fix"), 300'000}, {QStringLiteral("none"), 0}, {QStringLiteral("fresh"), 7'000'000}};
            for (const auto &[id, offset] : vehicles) {
                QVERIFY(q.exec(QStringLiteral("INSERT INTO vehicles (id, name, type, powertrain, odometer_offset_m, created_at, updated_at) "
                                              "VALUES ('%1', 'x', 'car', 'petrol', %2, %3, %3)")
                                   .arg(id)
                                   .arg(offset)
                                   .arg(stamp)));
            }
            for (const QString &entry : {QStringLiteral("'f1', 'start', 50500000"), QStringLiteral("'f2', 'fix', 20000000"), QStringLiteral("'f3', 'none', 1000000")}) {
                QVERIFY(q.exec(QStringLiteral("INSERT INTO fuel_entries (id, vehicle_id, odometer_m, date, quantity, total_cost_c, created_at, updated_at) "
                                              "VALUES (%1, '2026-10-23', 40000, 7000, %2, %2)").arg(entry, stamp)));
            }
            QVERIFY2(MigrationRunner().run(conn), "migration 0002 failed");
            auto reading = [&](const QString &id) { return scalar(conn, QStringLiteral("SELECT odometer_reading_m FROM vehicles WHERE id = '%1'").arg(id)); };
            QCOMPARE(reading(QStringLiteral("start")).toLongLong(), 50'000'000);
            QCOMPARE(reading(QStringLiteral("fix")).toLongLong(), 20'300'000);
            QVERIFY(reading(QStringLiteral("none")).isNull());
            QCOMPARE(reading(QStringLiteral("fresh")).toLongLong(), 7'000'000);
            QCOMPARE(scalar(conn, QStringLiteral("SELECT SUM(odometer_offset_m) FROM vehicles")).toLongLong(), 0);
            conn.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("v1"));
    }

    /** A new garage is readable by its owner only and deletes content for real (secure_delete). */
    void newGarageIsPrivate()
    {
        QTemporaryDir dir;
        Database db;
        const QString path = dir.filePath(QStringLiteral("garage.sqlite"));
        QVERIFY(db.open(path));
        QCOMPARE(QFile::permissions(path) & (QFile::ReadGroup | QFile::ReadOther | QFile::WriteGroup | QFile::WriteOther), QFile::Permissions());
        QCOMPARE(scalar(db.connection(), QStringLiteral("PRAGMA secure_delete")).toInt(), 1);
    }

    /** MustExist refuses a missing file instead of creating an empty garage in its place. */
    void mustExistDoesNotCreate()
    {
        QTemporaryDir dir;
        Database db;
        const QString file = dir.filePath(QStringLiteral("gone/garage.sqlite"));
        QVERIFY(!db.open(file, Database::OpenMode::MustExist));
        QVERIFY(!QFile::exists(file));
        QCOMPARE(Database::probe(file), Database::Probe::Missing);
    }

    /** open() never migrates a foreign SQLite or a non-SQLite file, and a failed open keeps the previous garage open. */
    void foreignFilesAreRefusedAndKeepPrevious()
    {
        QTemporaryDir dir;
        Database db;
        const QString garage = dir.filePath(QStringLiteral("garage.sqlite"));
        QVERIFY(db.open(garage));
        QCOMPARE(Database::probe(garage), Database::Probe::Garage);

        const QString foreign = dir.filePath(QStringLiteral("other.sqlite"));
        {
            QSqlDatabase raw = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("raw"));
            raw.setDatabaseName(foreign);
            QVERIFY(raw.open());
            QSqlQuery q(raw);
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE foo (x)")));
            raw.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("raw"));
        QCOMPARE(Database::probe(foreign), Database::Probe::Foreign);
        QVERIFY(!db.open(foreign));

        const QString text = dir.filePath(QStringLiteral("text.sqlite"));
        QFile f(text);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("not a database at all, just some text that is long enough");
        f.close();
        QCOMPARE(Database::probe(text), Database::Probe::NotSqlite);
        QVERIFY(!db.open(text));

        QCOMPARE(db.filePath(), garage);
        QVERIFY(db.isOpen());
        QSqlQuery check(db.connection());
        QVERIFY(check.exec(QStringLiteral("SELECT COUNT(*) FROM vehicles")));
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
