// Performance test with a large garage (role: tests, M15).
// Generates 10,000 fuel entries with garagelog_sampledata and checks that
// loading them into the app-layer models stays well below the 1 s start
// budget of ROADMAP M15. Limits are generous so Debug builds pass, too.

#include <QDate>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

#include "csvfuelimporter.h"
#include "database.h"
#include "fuellistmodel.h"
#include "intervallistmodel.h"
#include "sampledata.h"
#include "servicelistmodel.h"
#include "statsmodel.h"
#include "vehiclelistmodel.h"

using namespace garagelog::app;
using garagelog::core::Database;

namespace {

/** Number of fuel entries demanded by ROADMAP M15. */
constexpr int fuelCount = 10000;

/** Start budget for opening the garage and filling every model the first page needs. */
constexpr qint64 startBudgetMs = 1000;

} // namespace

/**
 * @brief Covers M15: the app stays fast with 10,000 fuel entries in one vehicle.
 */
class TestPerformance : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        m_path = m_dir.filePath(QStringLiteral("garage.sqlite"));
        Database db;
        QVERIFY2(db.open(m_path), qPrintable(db.errorString()));
        QElapsedTimer timer;
        timer.start();
        QString error;
        const auto result = garagelog::tools::generateSampleData(db, fuelCount, QDate(2026, 10, 23), &error);
        QVERIFY2(!result.vehicleId.isEmpty(), qPrintable(error));
        QCOMPARE(result.fuelEntries, fuelCount);
        QVERIFY(result.serviceEntries > 0);
        m_vehicleId = result.vehicleId;
        qInfo("Generated %d fuel and %d service entries in %lld ms", result.fuelEntries, result.serviceEntries,
              timer.elapsed());
    }

    /** Opening the garage and loading everything the vehicle page shows fits into the start budget. */
    void startLoadWithinBudget()
    {
        QElapsedTimer timer;
        timer.start();

        Database db;
        QVERIFY2(db.open(m_path), qPrintable(db.errorString()));
        const qint64 openMs = timer.elapsed();

        // Same models and order as main.cpp plus the first vehicle page.
        VehicleListModel vehicles(db);
        vehicles.reload();
        FuelListModel fuel(db);
        fuel.setVehicleId(m_vehicleId);
        ServiceListModel services(db);
        services.setVehicleId(m_vehicleId);
        IntervalListModel intervals(db, IntervalListModel::Scope::Vehicle);
        intervals.setVehicleId(m_vehicleId);
        IntervalListModel due(db, IntervalListModel::Scope::AllVehicles);
        due.reload();
        StatsModel stats(db);
        stats.setVehicleId(m_vehicleId);
        // "All time" is the most expensive period: every entry is in range.
        stats.setPeriod(QStringLiteral("all"));
        const qint64 totalMs = timer.elapsed();

        qInfo("open %lld ms, all models %lld ms", openMs, totalMs);
        QCOMPARE(fuel.rowCount(), fuelCount);
        QVERIFY(fuel.errorString().isEmpty());
        QVERIFY(stats.errorString().isEmpty());
        QVERIFY2(totalMs < startBudgetMs, qPrintable(QStringLiteral("took %1 ms").arg(totalMs)));
    }

    /** A save reloads the fuel list; that reload must not make saving feel sluggish. */
    void fuelReloadFast()
    {
        Database db;
        QVERIFY(db.open(m_path));
        FuelListModel fuel(db);
        fuel.setVehicleId(m_vehicleId);

        QElapsedTimer timer;
        timer.start();
        fuel.reload();
        const qint64 ms = timer.elapsed();
        qInfo("fuel reload %lld ms", ms);
        QVERIFY2(ms < 500, qPrintable(QStringLiteral("took %1 ms").arg(ms)));
    }

    /** The plausibility check runs on every keystroke in the form, so it has to be instant. */
    void plausibilityPerKeystrokeFast()
    {
        Database db;
        QVERIFY(db.open(m_path));
        FuelListModel fuel(db);
        fuel.setVehicleId(m_vehicleId);

        QElapsedTimer timer;
        timer.start();
        for (int i = 0; i < 10; ++i) {
            fuel.plausibility(QStringLiteral("2026-10-23"), qint64(1'000'000) * i, QString());
        }
        const qint64 ms = timer.elapsed() / 10;
        qInfo("plausibility %lld ms", ms);
        QVERIFY2(ms < 20, qPrintable(QStringLiteral("took %1 ms").arg(ms)));
    }

    /** The CSV preview reruns on every change in the mapping dialog, so 10,000 rows must not freeze it. */
    void csvPreviewFast()
    {
        Database db;
        QVERIFY(db.open(m_path));
        QString text = QStringLiteral("date,odometer,quantity,total,full,station\n");
        for (int i = 0; i < fuelCount; ++i) {
            text += QStringLiteral("%1,%2,40.5,72.90,1,Aral\n").arg(QDate(2000, 1, 1).addDays(i).toString(Qt::ISODate)).arg(1000 + i * 500);
        }
        garagelog::core::CsvFuelImporter importer(db);
        importer.load(text);
        const auto mapping = importer.suggestMapping();

        QElapsedTimer timer;
        timer.start();
        const auto preview = importer.preview(mapping);
        const qint64 ms = timer.elapsed();
        qInfo("csv preview %lld ms", ms);
        QCOMPARE(preview.entries.size(), fuelCount);
        QVERIFY2(ms < 250, qPrintable(QStringLiteral("took %1 ms").arg(ms)));
    }

private:
    QTemporaryDir m_dir;
    QString m_path;
    QString m_vehicleId;
};

QTEST_GUILESS_MAIN(TestPerformance)
#include "test_performance.moc"
