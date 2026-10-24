// Tests for Vehicle and VehicleRepository (role: tests).
// Each test opens a fresh garage in a temporary directory, so tests are
// independent and never touch the user's real data.

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

#include <memory>

#include "database.h"
#include "vehicle.h"
#include "vehiclerepository.h"

using namespace garagelog::core;

namespace {

/** A vehicle with every optional field set, so roundtrips cover all columns. */
Vehicle fullVehicle(const QString &name)
{
    Vehicle v;
    v.name = name;
    v.type = VehicleType::Motorcycle;
    v.make = QStringLiteral("Honda");
    v.model = QStringLiteral("CB500F");
    v.year = 2019;
    v.plate = QStringLiteral("M-AB 123");
    v.powertrain = Powertrain::Petrol;
    v.odometerReadingM = 1500;
    v.notes = QStringLiteral("bought used");
    return v;
}

/** Inserts a fuel entry directly via SQL; the fuel repository arrives in M05. */
bool insertFuelEntry(const Database &db, const QString &vehicleId)
{
    QSqlQuery query(db.connection());
    query.prepare(QStringLiteral(
        "INSERT INTO fuel_entries (id, vehicle_id, date, odometer_m, quantity, total_cost_c, created_at, updated_at) "
        "VALUES ('f1', ?, '2026-10-23', 1000000, 40000, 7000, '2026-10-23T10:00:00Z', '2026-10-23T10:00:00Z')"));
    query.addBindValue(vehicleId);
    return query.exec();
}

} // namespace

/**
 * @brief Covers M03: vehicle CRUD, archiving and the delete restriction.
 */
class TestVehicleRepository : public QObject
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

    /** Every enum value maps to its schema text and back. */
    void enumTextRoundtrip()
    {
        for (auto t : {VehicleType::Car, VehicleType::Motorcycle, VehicleType::Scooter, VehicleType::EBike, VehicleType::Other}) {
            QCOMPARE(vehicleTypeFromDb(toDbString(t)), t);
        }
        for (auto p : {Powertrain::Petrol, Powertrain::Diesel, Powertrain::Electric, Powertrain::Hybrid, Powertrain::Other}) {
            QCOMPARE(powertrainFromDb(toDbString(p)), p);
        }
        QCOMPARE(toDbString(VehicleType::EBike), QStringLiteral("ebike"));
        QVERIFY(!vehicleTypeFromDb(QStringLiteral("truck")));
        QVERIFY(!powertrainFromDb(QStringLiteral("steam")));
    }

    /** create() assigns id and timestamps, and byId() returns exactly what was stored. */
    void createAndReadBack()
    {
        VehicleRepository repo(*m_db);
        Vehicle v = fullVehicle(QStringLiteral("  CB500F  "));
        QVERIFY2(repo.create(v), qPrintable(repo.errorString()));

        QVERIFY(!QUuid::fromString(v.id).isNull());
        QCOMPARE(v.name, QStringLiteral("CB500F"));
        QVERIFY(v.createdAt.isValid());
        QCOMPARE(v.createdAt.timeSpec(), Qt::UTC);
        QCOMPARE(v.updatedAt, v.createdAt);

        const auto loaded = repo.byId(v.id);
        QVERIFY(loaded);
        QCOMPARE(*loaded, v);
    }

    /** Empty optional fields are stored as NULL and read back as empty / nullopt. */
    void optionalFieldsStayEmpty()
    {
        VehicleRepository repo(*m_db);
        Vehicle v;
        v.name = QStringLiteral("Zoe");
        v.powertrain = Powertrain::Electric;
        QVERIFY2(repo.create(v), qPrintable(repo.errorString()));

        QSqlQuery query(m_db->connection());
        QVERIFY(query.exec(QStringLiteral("SELECT make IS NULL, year IS NULL, plate IS NULL FROM vehicles")));
        QVERIFY(query.next());
        QVERIFY(query.value(0).toBool() && query.value(1).toBool() && query.value(2).toBool());

        const auto loaded = repo.byId(v.id);
        QVERIFY(loaded);
        QVERIFY(!loaded->year);
        QVERIFY(loaded->make.isEmpty());
    }

    /** A blank name is rejected by create() and update() without touching the database. */
    void blankNameIsRejected()
    {
        VehicleRepository repo(*m_db);
        Vehicle v;
        v.name = QStringLiteral("   ");
        QVERIFY(!repo.create(v));
        QVERIFY(v.id.isEmpty());
        QVERIFY(!repo.errorString().isEmpty());

        v.name = QStringLiteral("Golf");
        QVERIFY(repo.create(v));
        v.name.clear();
        QVERIFY(!repo.update(v));
        QCOMPARE(repo.byId(v.id)->name, QStringLiteral("Golf"));
    }

    /** update() changes the editable fields and keeps id and createdAt. */
    void updateChangesFields()
    {
        VehicleRepository repo(*m_db);
        Vehicle v = fullVehicle(QStringLiteral("Bike"));
        QVERIFY(repo.create(v));
        const QDateTime created = v.createdAt;

        v.name = QStringLiteral("Bike renamed");
        v.year.reset();
        v.plate.clear();
        v.odometerReadingM.reset();
        QVERIFY2(repo.update(v), qPrintable(repo.errorString()));

        const auto loaded = repo.byId(v.id);
        QVERIFY(loaded);
        QCOMPARE(*loaded, v);
        QCOMPARE(loaded->createdAt, created);
    }

    /** Operations on an unknown id fail with a message instead of silently succeeding. */
    void unknownIdFails()
    {
        VehicleRepository repo(*m_db);
        Vehicle ghost = fullVehicle(QStringLiteral("Ghost"));
        ghost.id = QStringLiteral("no-such-id");
        QVERIFY(!repo.update(ghost));
        QVERIFY(!repo.byId(ghost.id));
        QVERIFY(!repo.archive(ghost.id));
        QVERIFY(!repo.unarchive(ghost.id));
        QVERIFY(!repo.remove(ghost.id));
        QVERIFY(!repo.errorString().isEmpty());
    }

    /** all() sorts by name case-insensitively and hides archived vehicles unless asked. */
    void allSortsAndFiltersArchived()
    {
        VehicleRepository repo(*m_db);
        Vehicle b = fullVehicle(QStringLiteral("bmw"));
        Vehicle a = fullVehicle(QStringLiteral("Audi"));
        Vehicle c = fullVehicle(QStringLiteral("Corsa"));
        QVERIFY(repo.create(b) && repo.create(a) && repo.create(c));

        QVERIFY(repo.archive(b.id));
        QVERIFY(repo.byId(b.id)->archived);

        const QList<Vehicle> active = repo.all();
        QCOMPARE(active.size(), 2);
        QCOMPARE(active.at(0).name, QStringLiteral("Audi"));
        QCOMPARE(active.at(1).name, QStringLiteral("Corsa"));

        const QList<Vehicle> everything = repo.all(true);
        QCOMPARE(everything.size(), 3);
        QCOMPARE(everything.at(1).name, QStringLiteral("bmw"));

        QVERIFY(repo.unarchive(b.id));
        QVERIFY(!repo.byId(b.id)->archived);
        QCOMPARE(repo.all().size(), 3);
    }

    /** A vehicle without entries can be deleted. */
    void removeWithoutEntries()
    {
        VehicleRepository repo(*m_db);
        Vehicle v = fullVehicle(QStringLiteral("Temp"));
        QVERIFY(repo.create(v));
        QVERIFY2(repo.remove(v.id), qPrintable(repo.errorString()));
        QVERIFY(!repo.byId(v.id));
    }

    /** Invariant 2: a vehicle with entries cannot be deleted, only archived. */
    void removeWithEntriesFails()
    {
        VehicleRepository repo(*m_db);
        Vehicle v = fullVehicle(QStringLiteral("Daily"));
        QVERIFY(repo.create(v));
        QVERIFY(insertFuelEntry(*m_db, v.id));

        QVERIFY(!repo.remove(v.id));
        QVERIFY(!repo.errorString().isEmpty());
        QVERIFY(repo.byId(v.id));

        QVERIFY(repo.archive(v.id));
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Database> m_db;
};

QTEST_GUILESS_MAIN(TestVehicleRepository)
#include "test_vehiclerepository.moc"
