// Tests for IntervalRepository (role: tests).
// Each test opens a fresh garage in a temporary directory with one vehicle.

#include <QTemporaryDir>
#include <QTest>

#include <memory>

#include "categoryrepository.h"
#include "database.h"
#include "intervalrepository.h"
#include "vehiclerepository.h"

using namespace garagelog::core;

/**
 * @brief Covers M09: interval CRUD, validation and the active list for the due overview.
 */
class TestIntervalRepository : public QObject
{
    Q_OBJECT

private:
    MaintenanceInterval interval(const QString &name, std::optional<qint64> everyM, std::optional<int> everyMonths) const
    {
        MaintenanceInterval i;
        i.vehicleId = m_vehicleId;
        i.categoryId = m_oilId;
        i.name = name;
        i.everyM = everyM;
        i.everyMonths = everyMonths;
        return i;
    }

private Q_SLOTS:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        m_db = std::make_unique<Database>();
        QVERIFY2(m_db->open(m_dir->filePath(QStringLiteral("garage.sqlite"))), qPrintable(m_db->errorString()));
        Vehicle v;
        v.name = QStringLiteral("Golf");
        QVERIFY(VehicleRepository(*m_db).create(v));
        m_vehicleId = v.id;
        m_oilId = CategoryRepository(*m_db).byName(QStringLiteral("oil_change"))->id;
    }

    void cleanup()
    {
        m_db.reset();
        m_dir.reset();
    }

    /** create() fills id and timestamps; byId() returns the stored interval including NULL parts. */
    void createAndReadBack()
    {
        IntervalRepository repo(*m_db);
        MaintenanceInterval i = interval(QStringLiteral(" Oil change "), 15'000'000, std::nullopt);
        QVERIFY2(repo.create(i), qPrintable(repo.errorString()));
        QCOMPARE(i.name, QStringLiteral("Oil change"));
        QCOMPARE(*repo.byId(i.id), i);
        QVERIFY(!repo.byId(i.id)->everyMonths);
    }

    /** Intervals need a name and at least one positive part. */
    void validation()
    {
        IntervalRepository repo(*m_db);
        MaintenanceInterval none = interval(QStringLiteral("x"), std::nullopt, std::nullopt);
        QVERIFY(!repo.create(none));
        MaintenanceInterval zero = interval(QStringLiteral("x"), 0, 12);
        QVERIFY(!repo.create(zero));
        MaintenanceInterval noName = interval(QStringLiteral(" "), std::nullopt, 12);
        QVERIFY(!repo.create(noName));
        MaintenanceInterval badCategory = interval(QStringLiteral("x"), std::nullopt, 12);
        badCategory.categoryId = QStringLiteral("missing");
        QVERIFY(!repo.create(badCategory));
    }

    /** update() changes fields and pauses; byVehicle() lists by name; remove() deletes. */
    void updateListRemove()
    {
        IntervalRepository repo(*m_db);
        MaintenanceInterval b = interval(QStringLiteral("brakes"), 30'000'000, 24);
        MaintenanceInterval a = interval(QStringLiteral("Air filter"), std::nullopt, 24);
        QVERIFY(repo.create(b));
        QVERIFY(repo.create(a));
        b.active = false;
        b.everyM.reset();
        QVERIFY2(repo.update(b), qPrintable(repo.errorString()));
        QCOMPARE(*repo.byId(b.id), b);

        const auto list = repo.byVehicle(m_vehicleId);
        QCOMPARE(list.size(), 2);
        QCOMPARE(list.first().id, a.id);

        QVERIFY(repo.remove(a.id));
        QVERIFY(!repo.remove(a.id));
        QCOMPARE(repo.byVehicle(m_vehicleId).size(), 1);
    }

    /** allActive() skips paused intervals and intervals of archived vehicles. */
    void allActiveSkipsPausedAndArchived()
    {
        IntervalRepository repo(*m_db);
        MaintenanceInterval active = interval(QStringLiteral("Oil"), std::nullopt, 12);
        MaintenanceInterval paused = interval(QStringLiteral("Paused"), std::nullopt, 12);
        paused.active = false;
        QVERIFY(repo.create(active));
        QVERIFY(repo.create(paused));

        VehicleRepository vehicles(*m_db);
        Vehicle old;
        old.name = QStringLiteral("Old bike");
        old.archived = true;
        QVERIFY(vehicles.create(old));
        MaintenanceInterval archived = interval(QStringLiteral("Archived"), std::nullopt, 12);
        archived.vehicleId = old.id;
        QVERIFY(repo.create(archived));

        const auto list = repo.allActive();
        QCOMPARE(list.size(), 1);
        QCOMPARE(list.first().id, active.id);
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Database> m_db;
    QString m_vehicleId;
    QString m_oilId;
};

QTEST_GUILESS_MAIN(TestIntervalRepository)
#include "test_intervalrepository.moc"
