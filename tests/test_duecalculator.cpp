// Tests for DueCalculator (role: tests).
// The 12 roadmap cases run against the pure evaluate(); the database
// lookup of the last service is tested separately in a temporary garage.

#include <QTemporaryDir>
#include <QTest>

#include <memory>

#include "categoryrepository.h"
#include "database.h"
#include "duecalculator.h"
#include "servicerepository.h"
#include "vehiclerepository.h"

using namespace garagelog::core;

Q_DECLARE_METATYPE(DueStatus)

namespace {

constexpr qint64 km = 1000;

/** Last service of all table cases: 1 Jan 2026 at 10,000 km. */
const LastService baseService{QDate(2026, 1, 1), 10'000 * km};

/** An interval of every @p everyKm km and/or @p everyMonths months; 0 = part not set. */
MaintenanceInterval interval(int everyKm, int everyMonths)
{
    MaintenanceInterval i;
    i.name = QStringLiteral("Oil");
    if (everyKm > 0) {
        i.everyM = everyKm * km;
    }
    if (everyMonths > 0) {
        i.everyMonths = everyMonths;
    }
    return i;
}

} // namespace

/**
 * @brief Covers M09: due status for km-only, months-only and combined intervals.
 */
class TestDueCalculator : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void statusTable_data()
    {
        QTest::addColumn<int>("everyKm");
        QTest::addColumn<int>("everyMonths");
        QTest::addColumn<bool>("hasLast");
        QTest::addColumn<qint64>("currentKm");
        QTest::addColumn<QDate>("today");
        QTest::addColumn<DueStatus>("expected");

        const QDate early(2026, 6, 1);   // 5 of 12 months used
        const QDate late(2026, 11, 15);  // 47 days left = 13 % of the year
        const QDate past(2027, 1, 2);    // one day after the due date

        // km only: 50 % left / 15 % left / 500 km over / never serviced
        QTest::newRow("km ok") << 10'000 << 0 << true << qint64(15'000) << early << DueStatus::Ok;
        QTest::newRow("km soon") << 10'000 << 0 << true << qint64(18'500) << early << DueStatus::Soon;
        QTest::newRow("km overdue") << 10'000 << 0 << true << qint64(20'500) << early << DueStatus::Overdue;
        QTest::newRow("km unknown") << 10'000 << 0 << false << qint64(15'000) << early << DueStatus::Unknown;

        // months only: the odometer does not matter
        QTest::newRow("months ok") << 0 << 12 << true << qint64(99'000) << early << DueStatus::Ok;
        QTest::newRow("months soon") << 0 << 12 << true << qint64(10'000) << late << DueStatus::Soon;
        QTest::newRow("months overdue") << 0 << 12 << true << qint64(10'000) << past << DueStatus::Overdue;
        QTest::newRow("months unknown") << 0 << 12 << false << qint64(10'000) << early << DueStatus::Unknown;

        // both: the worse part decides
        QTest::newRow("both ok") << 10'000 << 12 << true << qint64(15'000) << early << DueStatus::Ok;
        QTest::newRow("both soon by time") << 10'000 << 12 << true << qint64(15'000) << late << DueStatus::Soon;
        QTest::newRow("both overdue by km") << 10'000 << 12 << true << qint64(20'500) << early << DueStatus::Overdue;
        QTest::newRow("both unknown") << 10'000 << 12 << false << qint64(15'000) << early << DueStatus::Unknown;
    }

    /** The 12 roadmap combinations (km / months / both) x (ok / soon / overdue / unknown). */
    void statusTable()
    {
        QFETCH(int, everyKm);
        QFETCH(int, everyMonths);
        QFETCH(bool, hasLast);
        QFETCH(qint64, currentKm);
        QFETCH(QDate, today);
        QFETCH(DueStatus, expected);

        const auto last = hasLast ? std::optional<LastService>(baseService) : std::nullopt;
        const DueResult r = DueCalculator::evaluate(interval(everyKm, everyMonths), last, currentKm * km, today);
        QCOMPARE(r.status, expected);
    }

    /** The result carries due odometer/date and the remaining distance and days. */
    void dueValues()
    {
        const DueResult r = DueCalculator::evaluate(interval(10'000, 12), baseService, 18'500 * km, QDate(2026, 11, 15));
        QCOMPARE(r.lastDate, QDate(2026, 1, 1));
        QCOMPARE(r.dueOdometerM, 20'000 * km);
        QCOMPARE(r.remainingM, 1'500 * km);
        QCOMPARE(r.dueDate, QDate(2027, 1, 1));
        QCOMPARE(r.remainingDays, 47);
        QVERIFY(r.remainingFraction);
        QVERIFY(qAbs(*r.remainingFraction - 47.0 / 365.0) < 1e-9); // time part is tighter than 15 % km
    }

    /** On the due day itself (0 days / 0 km left) the status is already Overdue. */
    void dueTodayIsOverdue()
    {
        QCOMPARE(DueCalculator::evaluate(interval(0, 12), baseService, 0, QDate(2027, 1, 1)).status, DueStatus::Overdue);
        QCOMPARE(DueCalculator::evaluate(interval(10'000, 0), baseService, 20'000 * km, QDate(2026, 2, 1)).status, DueStatus::Overdue);
    }

    /** A last service without odometer leaves the km part unknown; the time part still counts. */
    void lastServiceWithoutOdometer()
    {
        const LastService noOdo{QDate(2026, 1, 1), std::nullopt};
        const DueResult both = DueCalculator::evaluate(interval(10'000, 12), noOdo, 50'000 * km, QDate(2026, 6, 1));
        QCOMPARE(both.status, DueStatus::Ok);
        QVERIFY(!both.dueOdometerM);
        QCOMPARE(DueCalculator::evaluate(interval(10'000, 0), noOdo, 50'000 * km, QDate(2026, 6, 1)).status, DueStatus::Unknown);
    }

    /** Month arithmetic clamps to the end of shorter months (31 Jan + 1 month = 28 Feb). */
    void monthEnd()
    {
        const LastService jan31{QDate(2026, 1, 31), std::nullopt};
        QCOMPARE(DueCalculator::evaluate(interval(0, 1), jan31, 0, QDate(2026, 2, 1)).dueDate, QDate(2026, 2, 28));
    }

    /** moreUrgent() orders overdue < soon < ok < unknown, and less remaining first within a status. */
    void urgencyOrder()
    {
        auto result = [](DueStatus status, double fraction) {
            DueResult r;
            r.status = status;
            r.remainingFraction = fraction;
            return r;
        };
        const DueResult overdue = result(DueStatus::Overdue, -0.1);
        const DueResult soonTight = result(DueStatus::Soon, 0.05);
        const DueResult soonLoose = result(DueStatus::Soon, 0.15);
        const DueResult ok = result(DueStatus::Ok, 0.9);
        const DueResult unknown;
        QVERIFY(DueCalculator::moreUrgent(overdue, soonTight));
        QVERIFY(DueCalculator::moreUrgent(soonTight, soonLoose));
        QVERIFY(!DueCalculator::moreUrgent(soonLoose, soonTight));
        QVERIFY(DueCalculator::moreUrgent(soonLoose, ok));
        QVERIFY(DueCalculator::moreUrgent(ok, unknown));
        QCOMPARE(dueStatusName(DueStatus::Soon), QStringLiteral("soon"));
    }

    /** The database lookup uses the latest matching service on or before today and the vehicle's odometer. */
    void evaluateFromDatabase()
    {
        QTemporaryDir dir;
        Database db;
        QVERIFY(db.open(dir.filePath(QStringLiteral("garage.sqlite"))));
        Vehicle v;
        v.name = QStringLiteral("Golf");
        QVERIFY(VehicleRepository(db).create(v));
        CategoryRepository categories(db);
        const QString oil = categories.byName(QStringLiteral("oil_change"))->id;
        const QString tires = categories.byName(QStringLiteral("tires"))->id;

        ServiceRepository services(db);
        auto add = [&](const QString &category, const QDate &date, std::optional<qint64> odo) {
            ServiceEntry e;
            e.vehicleId = v.id;
            e.categoryId = category;
            e.date = date;
            e.odometerM = odo;
            e.description = QStringLiteral("x");
            QVERIFY(services.create(e));
        };
        add(oil, QDate(2025, 1, 1), 5'000 * km);
        add(oil, QDate(2026, 1, 1), 10'000 * km);     // the one that counts
        add(oil, QDate(2026, 12, 1), std::nullopt);   // planned, in the future
        add(tires, QDate(2026, 3, 1), 19'000 * km);   // other category, raises the odometer

        MaintenanceInterval i = interval(10'000, 12);
        i.vehicleId = v.id;
        i.categoryId = oil;
        DueCalculator calc(db);
        const DueResult r = calc.evaluate(i, QDate(2026, 6, 1));
        QVERIFY2(calc.errorString().isEmpty(), qPrintable(calc.errorString()));
        QCOMPARE(r.lastDate, QDate(2026, 1, 1));
        // Current odometer 19,000 km (tires entry), due at 20,000 km.
        QCOMPARE(r.remainingM, 1'000 * km);
        QCOMPARE(r.status, DueStatus::Soon);
    }
};

QTEST_GUILESS_MAIN(TestDueCalculator)
#include "test_duecalculator.moc"
