// Tests for the app-layer FuelListModel and the fuel helpers of Format (role: tests).
// Runs without a window: the model is a plain QAbstractListModel, checked
// for model-contract violations with QAbstractItemModelTester.

#include <QAbstractItemModelTester>
#include <QLocale>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

#include "database.h"
#include "format.h"
#include "fuellistmodel.h"
#include "vehicle.h"
#include "vehiclerepository.h"

using garagelog::app::Format;
using garagelog::app::FuelListModel;
using namespace garagelog::core;

namespace {

/** One kilometre in metres, to keep odometer values readable. */
constexpr qint64 km = 1000;

/** Form values as QML would send them for a new entry. */
QVariantMap fill(const QString &date, qint64 odometerM, qint64 quantity, qint64 costC, bool fullTank = true)
{
    return {
        {QStringLiteral("date"), date},
        {QStringLiteral("odometerM"), odometerM},
        {QStringLiteral("quantity"), quantity},
        {QStringLiteral("totalCostC"), costC},
        {QStringLiteral("fullTank"), fullTank},
        {QStringLiteral("station"), QStringLiteral(" Aral ")},
    };
}

} // namespace

/**
 * @brief Covers M06 app layer: fuel list, save/remove, plausibility, fuel formatting.
 */
class TestFuelListModel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // Fixed locale so the expected separators do not depend on the machine.
        QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
    }

    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        m_db = std::make_unique<Database>();
        QVERIFY2(m_db->open(m_dir->filePath(QStringLiteral("garage.sqlite"))), qPrintable(m_db->errorString()));
        m_vehicleId = createVehicle(Powertrain::Petrol);
    }

    void cleanup()
    {
        m_db.reset();
        m_dir.reset();
    }

    /** Saved entries appear newest first; the later full tank carries the consumption. */
    void listNewestFirstWithConsumption()
    {
        FuelListModel model(*m_db);
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QSignalSpy changedSpy(&model, &FuelListModel::entriesChanged);
        model.setVehicleId(m_vehicleId);
        QCOMPARE(model.rowCount(), 0);

        QVERIFY2(!model.save(fill(QStringLiteral("2026-10-01"), 10'000 * km, 40'000, 7000)).isEmpty(), qPrintable(model.errorString()));
        const QString second = model.save(fill(QStringLiteral("2026-10-15"), 10'500 * km, 30'000, 5400));
        QVERIFY(!second.isEmpty());
        QCOMPARE(changedSpy.count(), 2);
        QCOMPARE(model.rowCount(), 2);

        const QModelIndex newest = model.index(0);
        QCOMPARE(newest.data(FuelListModel::IdRole).toString(), second);
        QCOMPARE(newest.data(FuelListModel::DateRole).toString(), QStringLiteral("2026-10-15"));
        QVERIFY(newest.data(FuelListModel::HasConsumptionRole).toBool());
        // 30 l on 500 km = 6 l/100 km = 6000 ml/100 km; 54 EUR on 500 km = 1080 cents/100 km
        QCOMPARE(newest.data(FuelListModel::ConsumptionRole).toDouble(), 6000.0);
        QCOMPARE(newest.data(FuelListModel::CostPer100KmRole).toDouble(), 1080.0);
        QCOMPARE(newest.data(FuelListModel::UnitPriceRole).toLongLong(), qint64(1800));
        QCOMPARE(newest.data(FuelListModel::StationRole).toString(), QStringLiteral("Aral"));
        QVERIFY(!model.index(1).data(FuelListModel::HasConsumptionRole).toBool());

        QCOMPARE(model.stationSuggestions(), QStringList{QStringLiteral("Aral")});
    }

    /** Editing keeps the vehicle; get() returns the form values; remove() deletes. */
    void editAndRemove()
    {
        FuelListModel model(*m_db);
        model.setVehicleId(m_vehicleId);
        const QString id = model.save(fill(QStringLiteral("2026-10-01"), 10'000 * km, 40'000, 7000));

        QVariantMap values = model.get(id);
        QCOMPARE(values.value(QStringLiteral("quantity")).toLongLong(), qint64(40'000));
        values.insert(QStringLiteral("fullTank"), false);
        values.insert(QStringLiteral("totalCostC"), qint64(7100));
        QCOMPARE(model.save(values), id);
        QCOMPARE(model.rowCount(), 1);
        QVERIFY(!model.index(0).data(FuelListModel::FullTankRole).toBool());
        QCOMPARE(model.index(0).data(FuelListModel::TotalCostRole).toLongLong(), qint64(7100));

        QVERIFY(model.remove(id));
        QCOMPARE(model.rowCount(), 0);
        QVERIFY(!model.remove(id));
        QVERIFY(!model.errorString().isEmpty());
    }

    /** Fields that did not parse (null) and invalid values are refused, nothing is stored. */
    void invalidValuesRejected()
    {
        FuelListModel model(*m_db);
        model.setVehicleId(m_vehicleId);
        QVariantMap values = fill(QStringLiteral("2026-10-01"), 10'000 * km, 40'000, 7000);
        values.insert(QStringLiteral("quantity"), QVariant::fromValue(nullptr));
        QVERIFY(model.save(values).isEmpty());
        QVERIFY(!model.errorString().isEmpty());

        QVERIFY(model.save(fill(QStringLiteral("not a date"), 10'000 * km, 40'000, 7000)).isEmpty());
        QVERIFY(model.save(fill(QStringLiteral("2026-10-01"), 10'000 * km, 0, 7000)).isEmpty());
        QCOMPARE(model.rowCount(), 0);
    }

    /** A lower odometer than an earlier entry and an exact duplicate are flagged; later entries and the edited entry itself are not compared. */
    void plausibility()
    {
        FuelListModel model(*m_db);
        model.setVehicleId(m_vehicleId);
        const QString first = model.save(fill(QStringLiteral("2026-10-01"), 10'000 * km, 40'000, 7000));
        model.save(fill(QStringLiteral("2026-10-15"), 10'500 * km, 30'000, 5400));

        QVariantMap p = model.plausibility(QStringLiteral("2026-10-20"), 10'400 * km, QString());
        QVERIFY(p.value(QStringLiteral("lowerThanPrevious")).toBool());
        QCOMPARE(p.value(QStringLiteral("previousOdometerM")).toLongLong(), 10'500 * km);
        QVERIFY(!p.value(QStringLiteral("duplicate")).toBool());

        p = model.plausibility(QStringLiteral("2026-10-15"), 10'500 * km, QString());
        QVERIFY(p.value(QStringLiteral("duplicate")).toBool());
        QVERIFY(!p.value(QStringLiteral("lowerThanPrevious")).toBool());

        // Before the second entry only the first one counts.
        p = model.plausibility(QStringLiteral("2026-10-10"), 10'200 * km, QString());
        QVERIFY(!p.value(QStringLiteral("lowerThanPrevious")).toBool());

        p = model.plausibility(QStringLiteral("2026-10-01"), 10'000 * km, first);
        QVERIFY(!p.value(QStringLiteral("duplicate")).toBool());

        p = model.plausibility(QStringLiteral("2026-10-20"), QVariant::fromValue(nullptr), QString());
        QVERIFY(!p.value(QStringLiteral("lowerThanPrevious")).toBool());
    }

    /** An electric vehicle switches the model to Wh; another vehicle's entries are not shown. */
    void electricAndVehicleSwitch()
    {
        FuelListModel model(*m_db);
        model.setVehicleId(m_vehicleId);
        model.save(fill(QStringLiteral("2026-10-01"), 10'000 * km, 40'000, 7000));
        QVERIFY(!model.electric());

        QSignalSpy electricSpy(&model, &FuelListModel::electricChanged);
        model.setVehicleId(createVehicle(Powertrain::Electric));
        QVERIFY(model.electric());
        QCOMPARE(electricSpy.count(), 1);
        QCOMPARE(model.rowCount(), 0);
    }

    /** The QML wrappers of the price arithmetic return null instead of dividing by zero. */
    void priceWrappers()
    {
        FuelListModel model(*m_db);
        QCOMPARE(model.unitPriceFor(7250, 40'300).toLongLong(), qint64(1799));
        QCOMPARE(model.totalCostFor(40'300, 1799).toLongLong(), qint64(7250));
        QCOMPARE(model.quantityFor(7250, 1799).toLongLong(), qint64(40'300));
        QCOMPARE(model.unitPriceFor(7250, 0).metaType(), QMetaType::fromType<std::nullptr_t>());
        QCOMPARE(model.quantityFor(7250, 0).metaType(), QMetaType::fromType<std::nullptr_t>());
    }

    /** Money, quantity, unit price and consumption are shown and parsed with locale separators. */
    void formatFuelValues()
    {
        Format f;
        const QString nbsp(QChar(0x00A0));
        QCOMPARE(f.money(7250), QStringLiteral("72,50") + nbsp + QStringLiteral("€"));
        QCOMPARE(f.moneyInput(7250), QStringLiteral("72,50"));
        QCOMPARE(f.parseMoney(QStringLiteral("72,5")).toLongLong(), qint64(7250));
        QCOMPARE(f.quantity(40'300, false), QStringLiteral("40,30 l"));
        QCOMPARE(f.quantity(32'500, true), QStringLiteral("32,50 kWh"));
        QCOMPARE(f.quantityInput(40'300), QStringLiteral("40,3"));
        QCOMPARE(f.quantityInput(40'000), QStringLiteral("40"));
        QCOMPARE(f.parseQuantity(QStringLiteral("40,305")).toLongLong(), qint64(40'305));
        QCOMPARE(f.unitPrice(1799, false), QStringLiteral("1,799") + nbsp + QStringLiteral("€/l"));
        QCOMPARE(f.unitPriceInput(1799), QStringLiteral("1,799"));
        QCOMPARE(f.parseUnitPrice(QStringLiteral("1,799")).toLongLong(), qint64(1799));
        QCOMPARE(f.consumption(6420.0, false), QStringLiteral("6,42 l/100 km"));
        QVERIFY(f.parseMoney(QStringLiteral("abc")).isNull());
        QVERIFY(f.parseQuantity(QStringLiteral("-1")).isNull());
    }

    /** Dates show in the locale format with four-digit year and parse back from it or from ISO. */
    void formatDates()
    {
        Format f;
        QCOMPARE(f.date(QStringLiteral("2026-10-23")), QStringLiteral("23.10.2026"));
        QCOMPARE(f.parseDate(QStringLiteral("23.10.2026")).toString(), QStringLiteral("2026-10-23"));
        QCOMPARE(f.parseDate(QStringLiteral(" 2026-10-23 ")).toString(), QStringLiteral("2026-10-23"));
        QVERIFY(f.parseDate(QStringLiteral("23.10.26")).isNull());
        QVERIFY(f.parseDate(QStringLiteral("31.02.2026")).isNull());
        QVERIFY(f.parseDate(QString()).isNull());
        QCOMPARE(f.parseDate(f.date(f.today())).toString(), f.today());
    }

private:
    /** Creates a vehicle with @p powertrain and returns its id. */
    QString createVehicle(Powertrain powertrain)
    {
        VehicleRepository repo(*m_db);
        Vehicle v;
        v.name = QStringLiteral("Test");
        v.powertrain = powertrain;
        if (!repo.create(v)) {
            qWarning() << repo.errorString();
        }
        return v.id;
    }

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Database> m_db;
    QString m_vehicleId;
};

QTEST_GUILESS_MAIN(TestFuelListModel)
#include "test_fuellistmodel.moc"
