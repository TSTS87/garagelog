// Tests for Odometer::current() (role: tests).
// Entries are inserted with plain SQL because the fuel/service/trip
// repositories come in later milestones; vehicles go through the repository.

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>
#include <QVariant>

#include <memory>

#include "database.h"
#include "odometer.h"
#include "vehiclerepository.h"

using namespace garagelog::core;

namespace {

const QString stamp = QStringLiteral("2026-10-23T10:00:00Z");

QString newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool insertFuel(const Database &db, const QString &vehicleId, qint64 odometerM)
{
    QSqlQuery q(db.connection());
    q.prepare(QStringLiteral(
        "INSERT INTO fuel_entries (id, vehicle_id, date, odometer_m, quantity, total_cost_c, created_at, updated_at) "
        "VALUES (?, ?, '2026-10-23', ?, 40000, 7000, ?, ?)"));
    for (const QVariant &v : {QVariant(newId()), QVariant(vehicleId), QVariant(odometerM), QVariant(stamp), QVariant(stamp)}) {
        q.addBindValue(v);
    }
    return q.exec();
}

/** @param odometerM null QVariant = service without odometer (e.g. tax). */
bool insertService(const Database &db, const QString &vehicleId, const QVariant &odometerM)
{
    QSqlQuery q(db.connection());
    q.prepare(QStringLiteral(
        "INSERT INTO service_entries (id, vehicle_id, category_id, date, odometer_m, description, created_at, updated_at) "
        "VALUES (?, ?, (SELECT id FROM service_categories WHERE name = 'other'), '2026-10-23', ?, 'test', ?, ?)"));
    for (const QVariant &v : {QVariant(newId()), QVariant(vehicleId), odometerM, QVariant(stamp), QVariant(stamp)}) {
        q.addBindValue(v);
    }
    return q.exec();
}

bool insertTrip(const Database &db, const QString &vehicleId, qint64 endOdometerM)
{
    QSqlQuery q(db.connection());
    q.prepare(QStringLiteral(
        "INSERT INTO trips (id, vehicle_id, date, distance_m, odometer_end_m, created_at, updated_at) "
        "VALUES (?, ?, '2026-10-23', 1000, ?, ?, ?)"));
    for (const QVariant &v : {QVariant(newId()), QVariant(vehicleId), QVariant(endOdometerM), QVariant(stamp), QVariant(stamp)}) {
        q.addBindValue(v);
    }
    return q.exec();
}

} // namespace

/**
 * @brief Covers M03: currentOdometer() with 0, 1 and n entries plus offset.
 */
class TestOdometer : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        m_db = std::make_unique<Database>();
        QVERIFY2(m_db->open(m_dir->filePath(QStringLiteral("garage.sqlite"))), qPrintable(m_db->errorString()));
    }

    void cleanup()
    {
        m_db.reset();
        m_dir.reset();
    }

    /** A vehicle without entries reports its offset (0 by default). */
    void noEntriesGivesOffset()
    {
        const QString id = createVehicle(0);
        QCOMPARE(Odometer(*m_db).current(id), qint64(0));

        const QString withOffset = createVehicle(5000);
        QCOMPARE(Odometer(*m_db).current(withOffset), qint64(5000));
    }

    /** A single fuel entry is the odometer. */
    void oneEntry()
    {
        const QString id = createVehicle(0);
        QVERIFY(insertFuel(*m_db, id, 12'345'000));
        QCOMPARE(Odometer(*m_db).current(id), qint64(12'345'000));
    }

    /** With several entries in all three tables the highest one wins, then the offset is added. */
    void manyEntriesTakeMaximumPlusOffset()
    {
        const QString id = createVehicle(700);
        QVERIFY(insertFuel(*m_db, id, 10'000'000));
        QVERIFY(insertFuel(*m_db, id, 10'500'000));
        QVERIFY(insertService(*m_db, id, qint64(10'400'000)));
        QVERIFY(insertTrip(*m_db, id, 10'800'000));
        QCOMPARE(Odometer(*m_db).current(id), qint64(10'800'700));

        // A later service with a higher reading takes over from the trip.
        QVERIFY(insertService(*m_db, id, qint64(11'000'000)));
        QCOMPARE(Odometer(*m_db).current(id), qint64(11'000'700));
    }

    /** Services without odometer value (NULL) do not count, and do not turn the result into NULL. */
    void nullOdometerIgnored()
    {
        const QString id = createVehicle(0);
        QVERIFY(insertService(*m_db, id, QVariant()));
        QCOMPARE(Odometer(*m_db).current(id), qint64(0));

        QVERIFY(insertFuel(*m_db, id, 2'000'000));
        QVERIFY(insertService(*m_db, id, QVariant()));
        QCOMPARE(Odometer(*m_db).current(id), qint64(2'000'000));
    }

    /** Entries of other vehicles never influence the result. */
    void otherVehiclesIgnored()
    {
        const QString mine = createVehicle(0);
        const QString other = createVehicle(0);
        QVERIFY(insertFuel(*m_db, mine, 1'000'000));
        QVERIFY(insertFuel(*m_db, other, 9'000'000));
        QCOMPARE(Odometer(*m_db).current(mine), qint64(1'000'000));
    }

    /** An unknown vehicle id yields nullopt with an error, not 0. */
    void unknownVehicle()
    {
        Odometer odometer(*m_db);
        QVERIFY(!odometer.current(QStringLiteral("no-such-id")));
        QVERIFY(!odometer.errorString().isEmpty());
    }

private:
    QString createVehicle(qint64 offsetM)
    {
        VehicleRepository repo(*m_db);
        Vehicle v;
        v.name = QStringLiteral("Test");
        v.odometerOffsetM = offsetM;
        if (!repo.create(v)) {
            qWarning("%s", qPrintable(repo.errorString()));
        }
        return v.id;
    }

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Database> m_db;
};

QTEST_GUILESS_MAIN(TestOdometer)
#include "test_odometer.moc"
