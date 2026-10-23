// Tests for ServiceEntry and ServiceRepository (role: tests).
// Each test opens a fresh garage in a temporary directory with one vehicle.

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

#include <memory>

#include "categoryrepository.h"
#include "database.h"
#include "odometer.h"
#include "servicerepository.h"
#include "vehiclerepository.h"

using namespace garagelog::core;

/**
 * @brief Covers M07: service entry CRUD, ordering, NULL handling and validation.
 */
class TestServiceRepository : public QObject
{
    Q_OBJECT

private:
    /** An oil change for the test vehicle with every optional field set. */
    ServiceEntry entry(const QDate &date, std::optional<qint64> odometerM) const
    {
        ServiceEntry e;
        e.vehicleId = m_vehicleId;
        e.categoryId = m_oilId;
        e.date = date;
        e.odometerM = odometerM;
        e.description = QStringLiteral("Oil and filter");
        e.costC = 8950;
        e.performedBy = PerformedBy::Shop;
        e.shopName = QStringLiteral("Meier KFZ");
        e.notes = QStringLiteral("5W-30");
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
        QVERIFY(vehicles.create(v));
        m_vehicleId = v.id;
        m_oilId = CategoryRepository(*m_db).byName(QStringLiteral("oil_change"))->id;
    }

    void cleanup()
    {
        m_db.reset();
        m_dir.reset();
    }

    /** PerformedBy maps to 'self'/'shop'/NULL and rejects other texts. */
    void performedByMapping()
    {
        QCOMPARE(toDbString(PerformedBy::Self), QStringLiteral("self"));
        QCOMPARE(toDbString(PerformedBy::Shop), QStringLiteral("shop"));
        QVERIFY(toDbString(PerformedBy::Unknown).isEmpty());
        QCOMPARE(performedByFromDb(QString()), PerformedBy::Unknown);
        QCOMPARE(performedByFromDb(QStringLiteral("shop")), PerformedBy::Shop);
        QVERIFY(!performedByFromDb(QStringLiteral("garage")));
    }

    /** create() assigns id and timestamps, trims text, and byId() returns exactly what was stored. */
    void createAndReadBack()
    {
        ServiceRepository repo(*m_db);
        ServiceEntry e = entry(QDate(2026, 9, 1), 50'000'000);
        e.description = QStringLiteral("  Oil and filter ");
        QVERIFY2(repo.create(e), qPrintable(repo.errorString()));
        QVERIFY(!QUuid::fromString(e.id).isNull());
        QCOMPARE(e.description, QStringLiteral("Oil and filter"));
        QCOMPARE(e.updatedAt, e.createdAt);
        QCOMPARE(*repo.byId(e.id), e);
    }

    /** Missing odometer, unknown performer and empty texts are stored as NULL and read back unchanged. */
    void nullColumns()
    {
        ServiceRepository repo(*m_db);
        ServiceEntry e = entry(QDate(2026, 1, 15), std::nullopt);
        e.performedBy = PerformedBy::Unknown;
        e.shopName.clear();
        e.notes.clear();
        QVERIFY(repo.create(e));

        QSqlQuery q(m_db->connection());
        QVERIFY(q.exec(QStringLiteral("SELECT odometer_m IS NULL, performed_by IS NULL, shop_name IS NULL FROM service_entries")));
        QVERIFY(q.next());
        QVERIFY(q.value(0).toBool() && q.value(1).toBool() && q.value(2).toBool());
        QCOMPARE(*repo.byId(e.id), e);
    }

    /** update() writes the fields and updatedAt but keeps createdAt; unknown ids fail. */
    void updateEntry()
    {
        ServiceRepository repo(*m_db);
        ServiceEntry e = entry(QDate(2026, 9, 1), 1000);
        QVERIFY(repo.create(e));
        const QDateTime created = e.createdAt;
        e.performedBy = PerformedBy::Self;
        e.costC = 0;
        e.odometerM.reset();
        QVERIFY2(repo.update(e), qPrintable(repo.errorString()));
        QCOMPARE(e.createdAt, created);
        QCOMPARE(*repo.byId(e.id), e);

        ServiceEntry unknown = e;
        unknown.id = QStringLiteral("nope");
        QVERIFY(!repo.update(unknown));
    }

    /** byVehicle() sorts by date, then odometer, with odometer-less entries first on the same day. */
    void chronologicalOrder()
    {
        ServiceRepository repo(*m_db);
        ServiceEntry late = entry(QDate(2026, 5, 1), 3000);
        ServiceEntry early = entry(QDate(2026, 2, 1), 2000);
        ServiceEntry sameDayOdo = entry(QDate(2026, 5, 1), 2500);
        ServiceEntry sameDayNoOdo = entry(QDate(2026, 5, 1), std::nullopt);
        for (ServiceEntry *e : {&late, &early, &sameDayOdo, &sameDayNoOdo}) {
            QVERIFY(repo.create(*e));
        }
        const auto list = repo.byVehicle(m_vehicleId);
        QCOMPARE(list.size(), 4);
        QCOMPARE(list.at(0).id, early.id);
        QCOMPARE(list.at(1).id, sameDayNoOdo.id);
        QCOMPARE(list.at(2).id, sameDayOdo.id);
        QCOMPARE(list.at(3).id, late.id);
    }

    /** Invalid fields, unknown vehicles and unknown categories are refused. */
    void validation()
    {
        ServiceRepository repo(*m_db);
        ServiceEntry noDesc = entry(QDate(2026, 1, 1), 1);
        noDesc.description = QStringLiteral("  ");
        QVERIFY(!repo.create(noDesc));
        ServiceEntry negative = entry(QDate(2026, 1, 1), 1);
        negative.costC = -1;
        QVERIFY(!repo.create(negative));
        ServiceEntry noDate = entry(QDate(), 1);
        QVERIFY(!repo.create(noDate));
        ServiceEntry badCategory = entry(QDate(2026, 1, 1), 1);
        badCategory.categoryId = QStringLiteral("missing");
        QVERIFY(!repo.create(badCategory));
        ServiceEntry badVehicle = entry(QDate(2026, 1, 1), 1);
        badVehicle.vehicleId = QStringLiteral("missing");
        QVERIFY(!repo.create(badVehicle));
        QVERIFY(repo.byVehicle(m_vehicleId).isEmpty());
    }

    /** remove() deletes the entry; a second remove() of the same id fails. */
    void removeEntry()
    {
        ServiceRepository repo(*m_db);
        ServiceEntry e = entry(QDate(2026, 1, 1), 1);
        QVERIFY(repo.create(e));
        QVERIFY2(repo.remove(e.id), qPrintable(repo.errorString()));
        QVERIFY(!repo.byId(e.id));
        QVERIFY(!repo.remove(e.id));
    }

    /** Service odometers count for the vehicle's current odometer (DATA_MODEL derived values). */
    void serviceRaisesOdometer()
    {
        ServiceRepository repo(*m_db);
        ServiceEntry e = entry(QDate(2026, 1, 1), 77'000'000);
        QVERIFY(repo.create(e));
        QCOMPARE(Odometer(*m_db).current(m_vehicleId), 77'000'000);
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Database> m_db;
    QString m_vehicleId;
    QString m_oilId;
};

QTEST_GUILESS_MAIN(TestServiceRepository)
#include "test_servicerepository.moc"
