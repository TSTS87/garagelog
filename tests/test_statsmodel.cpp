// Tests for the app-layer StatsModel and the statistics helpers of Format (role: tests).
// Data is created relative to today's date, because the model always
// evaluates its period against QDate::currentDate().

#include <QLocale>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

#include "categoryrepository.h"
#include "database.h"
#include "format.h"
#include "fuelrepository.h"
#include "servicerepository.h"
#include "statsmodel.h"
#include "vehiclerepository.h"

using garagelog::app::Format;
using garagelog::app::StatsModel;
using namespace garagelog::core;

/**
 * @brief Covers M12 app layer: period keys, figures as QML values, chart data in l/100 km.
 */
class TestStatsModel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
    }

    /** Period keys map to year-to-date, the last 12 months (365 days in a common year) and an open start. */
    void periodKeys()
    {
        const QDate today(2026, 10, 23);
        const Period year = StatsModel::periodFor(QStringLiteral("year"), today);
        QCOMPARE(year.from, QDate(2026, 1, 1));
        QCOMPARE(year.to, today);
        const Period last12 = StatsModel::periodFor(QStringLiteral("12months"), today);
        QCOMPARE(last12.from, QDate(2025, 10, 24));
        QCOMPARE(last12.to, today);
        const Period all = StatsModel::periodFor(QStringLiteral("all"), today);
        QVERIFY(!all.from.isValid());
        QCOMPARE(all.to, today);
    }

    /** reload() fills the figures, converts chart values to l/100 km and translates category names. */
    void figures()
    {
        QTemporaryDir dir;
        Database db;
        QVERIFY(db.open(dir.filePath(QStringLiteral("garage.sqlite"))));
        Vehicle v;
        v.name = QStringLiteral("Golf");
        QVERIFY(VehicleRepository(db).create(v));

        const QDate today = QDate::currentDate();
        FuelRepository fuel(db);
        for (const auto &[daysAgo, odoKm, ml, costC] : {std::tuple{60, 10'000, 40'000, 7'000}, std::tuple{30, 10'800, 48'000, 8'400}}) {
            FuelEntry e;
            e.vehicleId = v.id;
            e.date = today.addDays(-daysAgo);
            e.odometerM = odoKm * 1000;
            e.quantity = ml;
            e.totalCostC = costC;
            QVERIFY(fuel.create(e));
        }
        ServiceEntry s;
        s.vehicleId = v.id;
        s.categoryId = CategoryRepository(db).byName(QStringLiteral("tires"))->id;
        s.date = today.addDays(-10);
        s.description = QStringLiteral("Tyres");
        s.costC = 40'000;
        QVERIFY(ServiceRepository(db).create(s));

        StatsModel model(db);
        QSignalSpy changed(&model, &StatsModel::changed);
        model.setVehicleId(v.id);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(model.period(), QStringLiteral("12months"));
        QCOMPARE(model.totalCostC(), 55'400);
        QCOMPARE(model.distanceM().toLongLong(), 800'000);
        QCOMPARE(model.costPerKmC().toDouble(), 55'400.0 / 800.0);
        QVERIFY(!model.costPerMonthC().isNull());
        QCOMPARE(model.averageConsumption().toDouble(), 6'000.0);                  // 48 l / 800 km
        QCOMPARE(model.consumptionValues(), QVariantList{6.0});
        QCOMPARE(model.consumptionDates(), QVariantList{today.addDays(-30).toString(Qt::ISODate)});

        const QVariantList categories = model.categoryCosts();
        QCOMPARE(categories.size(), 2);
        QCOMPARE(categories.at(0).toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Tires"));
        QCOMPARE(categories.at(1).toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Fuel"));
        QVERIFY(categories.at(1).toMap().value(QStringLiteral("isFuel")).toBool());

        model.setVehicleId(QString());
        QCOMPARE(model.totalCostC(), 0);
        QVERIFY(model.distanceM().isNull());
    }

    /** Cost per km uses three decimals; number() formats plain values for axis labels. */
    void formatHelpers()
    {
        Format f;
        QCOMPARE(f.moneyPerKm(24.56), QStringLiteral("0,246\u00A0€/km")); // German locale uses a no-break space
        QCOMPARE(f.number(6.5, 1), QStringLiteral("6,5"));
    }
};

QTEST_GUILESS_MAIN(TestStatsModel)
#include "test_statsmodel.moc"
