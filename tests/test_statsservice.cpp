// Tests for StatsService (role: tests).
// One garage with a known history across the turn of the year 2025/2026;
// every expected value is worked out by hand in the comments.

#include <QTemporaryDir>
#include <QTest>

#include <memory>

#include "categoryrepository.h"
#include "database.h"
#include "fuelrepository.h"
#include "servicerepository.h"
#include "statsservice.h"
#include "vehiclerepository.h"

using namespace garagelog::core;

namespace {
constexpr qint64 km = 1000;
}

/**
 * @brief Covers M11: the key figures total cost, cost per km, cost per month, by category, consumption.
 */
class TestStatsService : public QObject
{
    Q_OBJECT

private:
    void fuel(const QDate &date, qint64 odoKm, qint64 ml, qint64 costC, bool full)
    {
        FuelEntry e;
        e.vehicleId = m_vehicleId;
        e.date = date;
        e.odometerM = odoKm * km;
        e.quantity = ml;
        e.totalCostC = costC;
        e.fullTank = full;
        QVERIFY(FuelRepository(*m_db).create(e));
    }

    void service(const QDate &date, std::optional<qint64> odoKm, const QString &category, qint64 costC)
    {
        ServiceEntry e;
        e.vehicleId = m_vehicleId;
        e.categoryId = CategoryRepository(*m_db).byName(category)->id;
        e.date = date;
        if (odoKm) {
            e.odometerM = *odoKm * km;
        }
        e.description = category;
        e.costC = costC;
        QVERIFY(ServiceRepository(*m_db).create(e));
    }

    const Period year2026{QDate(2026, 1, 1), QDate(2026, 12, 31)};

private Q_SLOTS:
    void initTestCase()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        m_db = std::make_unique<Database>();
        QVERIFY2(m_db->open(m_dir->filePath(QStringLiteral("garage.sqlite"))), qPrintable(m_db->errorString()));
        Vehicle v;
        v.name = QStringLiteral("Golf");
        QVERIFY(VehicleRepository(*m_db).create(v));
        m_vehicleId = v.id;

        service(QDate(2025, 6, 1), 5'000, QStringLiteral("repair"), 30'000);       // before 2026
        fuel(QDate(2025, 12, 15), 9'500, 30'000, 5'000, true);                     // before 2026, segment start
        fuel(QDate(2026, 1, 10), 10'000, 40'000, 7'000, true);                     // 40 l / 500 km = 8 l/100 km
        service(QDate(2026, 2, 1), 10'300, QStringLiteral("oil_change"), 8'950);
        fuel(QDate(2026, 2, 10), 10'600, 20'000, 3'500, false);                    // partial
        fuel(QDate(2026, 3, 10), 11'000, 30'000, 5'400, true);                     // 50 l / 1000 km = 5 l/100 km
        service(QDate(2026, 4, 1), std::nullopt, QStringLiteral("tax"), 12'000);
    }

    void cleanupTestCase()
    {
        m_db.reset();
        m_dir.reset();
    }

    /** Totals: 2026 fuel 70 + 35 + 54 = 159 €, service 89.50 + 120 = 209.50 €; whole time adds 50 € fuel and 300 € repair. */
    void totalCost()
    {
        StatsService stats(*m_db);
        const CostTotals year = stats.totalCost(m_vehicleId, year2026);
        QCOMPARE(year.fuelC, 15'900);
        QCOMPARE(year.serviceC, 20'950);
        QCOMPARE(year.totalC(), 36'850);
        QCOMPARE(stats.totalCost(m_vehicleId, Period{}).totalC(), 71'850);
    }

    /** Distance 2026 = 11,000 km - 9,500 km (last reading before) = 1,500 km; cost/km = 36,850 c / 1,500 km. */
    void costPerKm()
    {
        StatsService stats(*m_db);
        QCOMPARE(stats.distance(m_vehicleId, year2026), 1'500 * km);
        QCOMPARE(*stats.costPerKm(m_vehicleId, year2026), 36'850.0 / 1'500.0);
        // Whole time: 11,000 - 5,000 km = 6,000 km for 718.50 €.
        QCOMPARE(stats.distance(m_vehicleId, Period{}), 6'000 * km);
        QCOMPARE(*stats.costPerKm(m_vehicleId, Period{}), 71'850.0 / 6'000.0);
    }

    /** Cost/month = total / (days / 30.4375): 181 days in H1 2026; whole time starts at the first entry (1 Jun 2025). */
    void costPerMonth()
    {
        StatsService stats(*m_db);
        const Period h1{QDate(2026, 1, 1), QDate(2026, 6, 30)};
        QCOMPARE(*stats.costPerMonth(m_vehicleId, h1), 36'850.0 / (181.0 / (365.25 / 12.0)));
        const Period all{QDate(), QDate(2026, 6, 30)};
        QCOMPARE(*stats.costPerMonth(m_vehicleId, all), 71'850.0 / (395.0 / (365.25 / 12.0)));
        QVERIFY(!stats.costPerMonth(m_vehicleId, Period{}));                       // open end
        const Period shortPeriod{QDate(2026, 4, 1), QDate(2026, 4, 1)};
        QCOMPARE(*stats.costPerMonth(m_vehicleId, shortPeriod), 12'000.0);         // at least one month
    }

    /** By category 2026: fuel 159 €, tax 120 €, oil change 89.50 €, highest first. */
    void costByCategory()
    {
        StatsService stats(*m_db);
        const auto list = stats.costByCategory(m_vehicleId, year2026);
        QCOMPARE(list.size(), 3);
        QVERIFY(list.at(0).isFuel);
        QCOMPARE(list.at(0).costC, 15'900);
        QCOMPARE(list.at(1).category.name, QStringLiteral("tax"));
        QCOMPARE(list.at(1).costC, 12'000);
        QCOMPARE(list.at(2).category.name, QStringLiteral("oil_change"));
        QCOMPARE(list.at(2).costC, 8'950);
    }

    /** Series 2026 has the two closing full tanks (8 and 5 l/100 km); the average weights by distance: 90 l / 1,500 km = 6 l/100 km. */
    void consumption()
    {
        StatsService stats(*m_db);
        const auto series = stats.consumptionSeries(m_vehicleId, year2026);
        QCOMPARE(series.size(), 2);
        QCOMPARE(series.at(0).date, QDate(2026, 1, 10));
        QCOMPARE(series.at(0).quantityPer100Km, 8'000.0);
        QCOMPARE(series.at(1).quantityPer100Km, 5'000.0);
        QCOMPARE(series.at(1).costPer100KmC, 890.0);                               // (35 + 54) € / 1,000 km
        QCOMPARE(*stats.averageConsumption(m_vehicleId, year2026), 6'000.0);
    }

    /** A period without entries gives zero costs and no distance, cost/km, series or average. */
    void emptyPeriod()
    {
        StatsService stats(*m_db);
        const Period future{QDate(2030, 1, 1), QDate(2030, 12, 31)};
        QCOMPARE(stats.totalCost(m_vehicleId, future).totalC(), 0);
        QVERIFY(!stats.distance(m_vehicleId, future));
        QVERIFY(!stats.costPerKm(m_vehicleId, future));
        QVERIFY(stats.costByCategory(m_vehicleId, future).isEmpty());
        QVERIFY(stats.consumptionSeries(m_vehicleId, future).isEmpty());
        QVERIFY(!stats.averageConsumption(m_vehicleId, future));
        QVERIFY(stats.errorString().isEmpty());
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Database> m_db;
    QString m_vehicleId;
};

QTEST_GUILESS_MAIN(TestStatsService)
#include "test_statsservice.moc"
