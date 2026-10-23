// Tests for FuelEntry and FuelRepository (role: tests).
// Each test opens a fresh garage in a temporary directory, so tests are
// independent and never touch the user's real data.

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

#include <memory>

#include "database.h"
#include "fuelentry.h"
#include "fuelrepository.h"
#include "vehiclerepository.h"

using namespace garagelog::core;

/**
 * @brief Covers M05: fuel entry CRUD, ordering and validation.
 */
class TestFuelRepository : public QObject
{
    Q_OBJECT

private:
    /** A fuel entry for the test vehicle with every optional field set. */
    FuelEntry entry(const QDate &date, qint64 odometerM) const
    {
        FuelEntry e;
        e.vehicleId = m_vehicleId;
        e.date = date;
        e.odometerM = odometerM;
        e.quantity = 42500;
        e.totalCostC = 7225;
        e.fullTank = true;
        e.station = QStringLiteral("Aral Hauptstraße");
        e.notes = QStringLiteral("before holiday");
        return e;
    }

private Q_SLOTS:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        m_db = std::make_unique<Database>();
        QVERIFY2(m_db->open(m_dir->filePath(QStringLiteral("garage.sqlite"))), qPrintable(m_db->errorString()));

        VehicleRepository vehicles(*m_db);
        Vehicle v;
        v.name = QStringLiteral("Golf");
        QVERIFY2(vehicles.create(v), qPrintable(vehicles.errorString()));
        m_vehicleId = v.id;
    }

    void cleanup()
    {
        m_db.reset();
        m_dir.reset();
    }

    /** Wh is used only for electric vehicles; hybrids are refuelled in ml. */
    void quantityUnitFollowsPowertrain()
    {
        QCOMPARE(quantityUnitFor(Powertrain::Electric), QuantityUnit::WattHour);
        for (auto p : {Powertrain::Petrol, Powertrain::Diesel, Powertrain::Hybrid, Powertrain::Other}) {
            QCOMPARE(quantityUnitFor(p), QuantityUnit::Millilitre);
        }
    }

    /** create() assigns id and timestamps, trims text, and byId() returns exactly what was stored. */
    void createAndReadBack()
    {
        FuelRepository repo(*m_db);
        FuelEntry e = entry(QDate(2026, 10, 1), 12'345'000);
        e.station = QStringLiteral("  Aral Hauptstraße ");
        e.fullTank = false;
        QVERIFY2(repo.create(e), qPrintable(repo.errorString()));

        QVERIFY(!QUuid::fromString(e.id).isNull());
        QCOMPARE(e.station, QStringLiteral("Aral Hauptstraße"));
        QCOMPARE(e.createdAt.timeSpec(), Qt::UTC);
        QCOMPARE(e.updatedAt, e.createdAt);

        const auto loaded = repo.byId(e.id);
        QVERIFY(loaded);
        QCOMPARE(*loaded, e);
    }

    /** Dates are stored as ISO text and empty station/notes as NULL. */
    void storageFormat()
    {
        FuelRepository repo(*m_db);
        FuelEntry e = entry(QDate(2026, 3, 7), 1000);
        e.station.clear();
        e.notes = QStringLiteral("   ");
        QVERIFY2(repo.create(e), qPrintable(repo.errorString()));

        QSqlQuery query(m_db->connection());
        QVERIFY(query.exec(QStringLiteral("SELECT date, station IS NULL, notes IS NULL FROM fuel_entries")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toString(), QStringLiteral("2026-03-07"));
        QVERIFY(query.value(1).toBool());
        QVERIFY(query.value(2).toBool());
    }

    /** update() changes the stored fields and updatedAt, but not createdAt. */
    void updateChangesFields()
    {
        FuelRepository repo(*m_db);
        FuelEntry e = entry(QDate(2026, 10, 1), 5000);
        QVERIFY(repo.create(e));
        const QDateTime created = e.createdAt;

        e.quantity = 30000;
        e.fullTank = false;
        e.station = QStringLiteral("Shell");
        QVERIFY2(repo.update(e), qPrintable(repo.errorString()));
        QCOMPARE(e.createdAt, created);

        const auto loaded = repo.byId(e.id);
        QVERIFY(loaded);
        QCOMPARE(*loaded, e);
    }

    /** update() and remove() on an unknown id fail with an error. */
    void unknownIdFails()
    {
        FuelRepository repo(*m_db);
        FuelEntry e = entry(QDate(2026, 10, 1), 5000);
        e.id = QStringLiteral("does-not-exist");
        QVERIFY(!repo.update(e));
        QVERIFY(!repo.errorString().isEmpty());
        QVERIFY(!repo.remove(e.id));
        QVERIFY(!repo.byId(e.id));
    }

    /** remove() deletes the entry permanently. */
    void removeDeletes()
    {
        FuelRepository repo(*m_db);
        FuelEntry e = entry(QDate(2026, 10, 1), 5000);
        QVERIFY(repo.create(e));
        QVERIFY2(repo.remove(e.id), qPrintable(repo.errorString()));
        QVERIFY(!repo.byId(e.id));
        QVERIFY(repo.byVehicle(m_vehicleId).isEmpty());
    }

    /** byVehicle() sorts by date, then odometer, and leaves out other vehicles' entries. */
    void byVehicleSortsChronologically()
    {
        FuelRepository repo(*m_db);
        FuelEntry late = entry(QDate(2026, 10, 20), 3000);
        FuelEntry sameDayHigh = entry(QDate(2026, 10, 10), 2000);
        FuelEntry sameDayLow = entry(QDate(2026, 10, 10), 1000);
        for (FuelEntry *e : {&late, &sameDayHigh, &sameDayLow}) {
            QVERIFY(repo.create(*e));
        }

        VehicleRepository vehicles(*m_db);
        Vehicle other;
        other.name = QStringLiteral("Zoe");
        QVERIFY(vehicles.create(other));
        FuelEntry foreign = entry(QDate(2026, 10, 15), 1500);
        foreign.vehicleId = other.id;
        QVERIFY(repo.create(foreign));

        const QList<FuelEntry> list = repo.byVehicle(m_vehicleId);
        QCOMPARE(list.size(), 3);
        QCOMPARE(list.at(0).id, sameDayLow.id);
        QCOMPARE(list.at(1).id, sameDayHigh.id);
        QCOMPARE(list.at(2).id, late.id);
    }

    /** Invalid fields are rejected before anything is written. */
    void invalidFieldsAreRejected_data()
    {
        QTest::addColumn<int>("field");
        QTest::newRow("no vehicle") << 0;
        QTest::newRow("invalid date") << 1;
        QTest::newRow("negative odometer") << 2;
        QTest::newRow("zero quantity") << 3;
        QTest::newRow("negative cost") << 4;
    }

    void invalidFieldsAreRejected()
    {
        QFETCH(int, field);
        FuelRepository repo(*m_db);
        FuelEntry e = entry(QDate(2026, 10, 1), 5000);
        switch (field) {
        case 0: e.vehicleId.clear(); break;
        case 1: e.date = QDate(); break;
        case 2: e.odometerM = -1; break;
        case 3: e.quantity = 0; break;
        case 4: e.totalCostC = -1; break;
        }
        QVERIFY(!repo.create(e));
        QVERIFY(e.id.isEmpty());
        QVERIFY(!repo.errorString().isEmpty());
        QVERIFY(repo.byVehicle(m_vehicleId).isEmpty());
    }

    /** An entry for a vehicle that does not exist is refused by the foreign key. */
    void unknownVehicleIsRejected()
    {
        FuelRepository repo(*m_db);
        FuelEntry e = entry(QDate(2026, 10, 1), 5000);
        e.vehicleId = QStringLiteral("no-such-vehicle");
        QVERIFY(!repo.create(e));
        QVERIFY(!repo.errorString().isEmpty());
    }

    /** Invariant 1: two entries with the same date and odometer are both stored (the UI only warns). */
    void duplicatesAreAllowed()
    {
        FuelRepository repo(*m_db);
        FuelEntry a = entry(QDate(2026, 10, 1), 5000);
        FuelEntry b = entry(QDate(2026, 10, 1), 5000);
        QVERIFY(repo.create(a));
        QVERIFY2(repo.create(b), qPrintable(repo.errorString()));
        QCOMPARE(repo.byVehicle(m_vehicleId).size(), 2);
    }

    /** Invariant 2: a vehicle with a fuel entry cannot be deleted, only archived. */
    void vehicleWithEntryCannotBeDeleted()
    {
        FuelRepository repo(*m_db);
        FuelEntry e = entry(QDate(2026, 10, 1), 5000);
        QVERIFY(repo.create(e));
        VehicleRepository vehicles(*m_db);
        QVERIFY(!vehicles.remove(m_vehicleId));
        QVERIFY(vehicles.byId(m_vehicleId));
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Database> m_db;
    QString m_vehicleId;
};

QTEST_GUILESS_MAIN(TestFuelRepository)
#include "test_fuelrepository.moc"
