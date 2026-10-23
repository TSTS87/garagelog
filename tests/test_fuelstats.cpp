// Tests for FuelStats (role: tests).
// consumption() is tested on in-memory lists, stationSuggestions() on a
// fresh garage in a temporary directory.

#include <QTemporaryDir>
#include <QTest>

#include <memory>

#include "database.h"
#include "fuelrepository.h"
#include "fuelstats.h"
#include "vehiclerepository.h"

using namespace garagelog::core;

namespace {

/** One kilometre in metres, to keep odometer values readable. */
constexpr qint64 km = 1000;

/** Entry with the fields consumption() looks at; quantity in ml or Wh, cost in cents. */
FuelEntry fill(qint64 odometerM, qint64 quantity, qint64 costC, bool fullTank = true)
{
    FuelEntry e;
    e.odometerM = odometerM;
    e.quantity = quantity;
    e.totalCostC = costC;
    e.fullTank = fullTank;
    return e;
}

} // namespace

/**
 * @brief Covers M05: consumption between full tanks and station suggestions.
 */
class TestFuelStats : public QObject
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

    /** Two full tanks: the second one's quantity and cost divided by the distance between them. */
    void consumptionNormal()
    {
        const auto result = FuelStats::consumption({
            fill(10'000 * km, 40'000, 7000),
            fill(10'500 * km, 35'000, 6125),
        });
        QCOMPARE(result.size(), 2);
        QVERIFY(!result.at(0));
        QVERIFY(result.at(1));
        QCOMPARE(result.at(1)->distanceM, 500 * km);
        QCOMPARE(result.at(1)->quantity, 35'000);
        QCOMPARE(result.at(1)->costC, 6125);
        QCOMPARE(result.at(1)->quantityPer100Km, 7000.0);  // 7.0 l/100 km
        QCOMPARE(result.at(1)->costPer100KmC, 1225.0);     // 12.25 per 100 km
    }

    /** A partial fill gets no value itself; its quantity and cost count towards the next full tank. */
    void consumptionWithPartialFill()
    {
        const auto result = FuelStats::consumption({
            fill(0, 40'000, 7000),
            fill(300 * km, 20'000, 3500, false),
            fill(600 * km, 22'000, 3850),
        });
        QCOMPARE(result.size(), 3);
        QVERIFY(!result.at(1));
        QVERIFY(result.at(2));
        QCOMPARE(result.at(2)->distanceM, 600 * km);
        QCOMPARE(result.at(2)->quantity, 42'000);
        QCOMPARE(result.at(2)->costC, 7350);
        QCOMPARE(result.at(2)->quantityPer100Km, 7000.0);
        QCOMPARE(result.at(2)->costPer100KmC, 1225.0);
    }

    /** The first full tank has no value, and partial fills before it are not counted later. */
    void consumptionFirstEntry()
    {
        QVERIFY(FuelStats::consumption({}).isEmpty());

        const auto single = FuelStats::consumption({fill(1000 * km, 40'000, 7000)});
        QCOMPARE(single.size(), 1);
        QVERIFY(!single.at(0));

        const auto result = FuelStats::consumption({
            fill(900 * km, 10'000, 1750, false),
            fill(1000 * km, 40'000, 7000),
            fill(1500 * km, 35'000, 6125),
        });
        QVERIFY(!result.at(0));
        QVERIFY(!result.at(1));
        QVERIFY(result.at(2));
        QCOMPARE(result.at(2)->quantity, 35'000);
    }

    /** Charging an electric vehicle: the same rule, quantity in Wh gives Wh/100 km. */
    void consumptionElectric()
    {
        QCOMPARE(quantityUnitFor(Powertrain::Electric), QuantityUnit::WattHour);
        const auto result = FuelStats::consumption({
            fill(5000 * km, 45'000, 1500),
            fill(5200 * km, 30'000, 1080),
        });
        QVERIFY(result.at(1));
        QCOMPARE(result.at(1)->quantityPer100Km, 15'000.0);  // 15 kWh/100 km
        QCOMPARE(result.at(1)->costPer100KmC, 540.0);
    }

    /** An odometer at or below the previous full tank gives no value, but becomes the new start. */
    void consumptionOdometerBackwards()
    {
        const auto result = FuelStats::consumption({
            fill(1000 * km, 40'000, 7000),
            fill(900 * km, 30'000, 5000),   // typo: odometer went backwards
            fill(900 * km, 5'000, 900),     // same odometer: no distance
            fill(1300 * km, 28'000, 4900),
        });
        QCOMPARE(result.size(), 4);
        QVERIFY(!result.at(1));
        QVERIFY(!result.at(2));
        QVERIFY(result.at(3));
        QCOMPARE(result.at(3)->distanceM, 400 * km);
        QCOMPARE(result.at(3)->quantityPer100Km, 7000.0);
    }

    /** Suggestions are distinct, most recent first, without empty names or other vehicles' stations. */
    void stationSuggestions()
    {
        VehicleRepository vehicles(*m_db);
        Vehicle golf;
        golf.name = QStringLiteral("Golf");
        Vehicle zoe;
        zoe.name = QStringLiteral("Zoe");
        QVERIFY(vehicles.create(golf));
        QVERIFY(vehicles.create(zoe));

        FuelRepository repo(*m_db);
        auto add = [&](const QString &vehicleId, const QDate &date, const QString &station) {
            FuelEntry e = fill(1000, 1000, 100);
            e.vehicleId = vehicleId;
            e.date = date;
            e.station = station;
            return repo.create(e);
        };
        QVERIFY(add(golf.id, QDate(2026, 1, 1), QStringLiteral("Aral")));
        QVERIFY(add(golf.id, QDate(2026, 2, 1), QStringLiteral("Shell")));
        QVERIFY(add(golf.id, QDate(2026, 3, 1), QStringLiteral(" Aral ")));
        QVERIFY(add(golf.id, QDate(2026, 4, 1), QString()));
        QVERIFY(add(zoe.id, QDate(2026, 5, 1), QStringLiteral("Ionity")));

        FuelStats stats(*m_db);
        QCOMPARE(stats.stationSuggestions(golf.id), (QStringList{QStringLiteral("Aral"), QStringLiteral("Shell")}));
        QCOMPARE(stats.stationSuggestions(zoe.id), QStringList{QStringLiteral("Ionity")});
        QVERIFY(stats.stationSuggestions(QStringLiteral("unknown")).isEmpty());
        QVERIFY(stats.errorString().isEmpty());
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Database> m_db;
};

QTEST_GUILESS_MAIN(TestFuelStats)
#include "test_fuelstats.moc"
