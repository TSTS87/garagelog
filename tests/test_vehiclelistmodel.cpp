// Tests for the app-layer VehicleListModel and Format helpers (role: tests).
// Runs without a window: the model is a plain QAbstractListModel, checked
// for model-contract violations with QAbstractItemModelTester.

#include <QAbstractItemModelTester>
#include <QLocale>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

#include "database.h"
#include "format.h"
#include "vehiclelistmodel.h"

using garagelog::app::Format;
using garagelog::app::VehicleListModel;
using garagelog::core::Database;

namespace {

/** Form values as QML would send them for a new vehicle. */
QVariantMap newVehicle(const QString &name)
{
    return {
        {QStringLiteral("name"), name},
        {QStringLiteral("type"), QStringLiteral("motorcycle")},
        {QStringLiteral("powertrain"), QStringLiteral("petrol")},
        {QStringLiteral("year"), 2019},
    };
}

} // namespace

/**
 * @brief Covers M04 app layer: list contents, save/archive through the model, km formatting.
 */
class TestVehicleListModel : public QObject
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
    }

    void cleanup()
    {
        m_db.reset();
        m_dir.reset();
    }

    /** save() creates a vehicle that appears in the list with its roles, and get() returns the form values. */
    void saveNewAppearsInList()
    {
        VehicleListModel model(*m_db);
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QSignalSpy countSpy(&model, &VehicleListModel::countChanged);
        QCOMPARE(model.rowCount(), 0);

        QVariantMap values = newVehicle(QStringLiteral("CB500F"));
        values.insert(QStringLiteral("odometerM"), qint64(12'345'000));
        const QString id = model.save(values);
        QVERIFY2(!id.isEmpty(), qPrintable(model.errorString()));
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(countSpy.count(), 1);

        const QModelIndex index = model.index(0);
        QCOMPARE(index.data(VehicleListModel::IdRole).toString(), id);
        QCOMPARE(index.data(VehicleListModel::NameRole).toString(), QStringLiteral("CB500F"));
        QCOMPARE(index.data(VehicleListModel::TypeRole).toString(), QStringLiteral("motorcycle"));
        QCOMPARE(index.data(VehicleListModel::OdometerRole).toLongLong(), qint64(12'345'000));
        // Own line icon per type (ADR-015), not the Breeze "car" for everything.
        QCOMPARE(index.data(VehicleListModel::IconRole).toString(), QStringLiteral("qrc:/qml/icons/vehicle-motorcycle.svg"));

        const QVariantMap loaded = model.get(id);
        QCOMPARE(loaded.value(QStringLiteral("year")).toInt(), 2019);
        QCOMPARE(loaded.value(QStringLiteral("odometerM")).toLongLong(), qint64(12'345'000));
    }

    /** Editing keeps the offset unless odometerM is sent; a sent odometerM corrects it. */
    void editKeepsOrCorrectsOdometer()
    {
        VehicleListModel model(*m_db);
        QVariantMap values = newVehicle(QStringLiteral("Bike"));
        values.insert(QStringLiteral("odometerM"), qint64(1'000'000));
        const QString id = model.save(values);
        QVERIFY(!id.isEmpty());

        values = model.get(id);
        values.remove(QStringLiteral("odometerM"));
        values.insert(QStringLiteral("name"), QStringLiteral("Bike 2"));
        values.insert(QStringLiteral("year"), QVariant());
        QCOMPARE(model.save(values), id);
        QCOMPARE(model.get(id).value(QStringLiteral("odometerM")).toLongLong(), qint64(1'000'000));
        QVERIFY(model.get(id).value(QStringLiteral("year")).isNull());

        values.insert(QStringLiteral("odometerM"), qint64(1'500'000));
        QCOMPARE(model.save(values), id);
        QCOMPARE(model.get(id).value(QStringLiteral("odometerM")).toLongLong(), qint64(1'500'000));
        QCOMPARE(model.get(id).value(QStringLiteral("name")).toString(), QStringLiteral("Bike 2"));
    }

    /** A vehicle without entries can be deleted; one with a fuel entry cannot and reports hasEntries (SPEC §4.1). */
    void removeOnlyWithoutEntries()
    {
        VehicleListModel model(*m_db);
        const QString empty = model.save(newVehicle(QStringLiteral("Typo")));
        const QString used = model.save(newVehicle(QStringLiteral("Golf")));
        QSqlQuery q(m_db->connection());
        QVERIFY(q.exec(QStringLiteral("INSERT INTO fuel_entries (id, vehicle_id, date, odometer_m, quantity, total_cost_c, created_at, updated_at) "
                                      "VALUES ('f1', '%1', '2026-10-23', 1000, 40000, 7000, '2026-10-23T10:00:00Z', '2026-10-23T10:00:00Z')")
                           .arg(used)));
        QVERIFY(!model.get(empty).value(QStringLiteral("hasEntries")).toBool());
        QVERIFY(model.get(used).value(QStringLiteral("hasEntries")).toBool());

        QVERIFY(!model.remove(used));
        QVERIFY(!model.errorString().isEmpty());
        QVERIFY(model.remove(empty));
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.idAt(0), used);
    }

    /** Invalid form values are refused with an error message and nothing is stored. */
    void invalidValuesRejected()
    {
        VehicleListModel model(*m_db);
        QVERIFY(model.save(newVehicle(QStringLiteral(" "))).isEmpty());
        QVERIFY(!model.errorString().isEmpty());

        QVariantMap values = newVehicle(QStringLiteral("Truck"));
        values.insert(QStringLiteral("type"), QStringLiteral("truck"));
        QVERIFY(model.save(values).isEmpty());
        QCOMPARE(model.rowCount(), 0);
    }

    /** Archived vehicles leave the list and come back with showArchived or after restoring; archivedCount counts them while hidden. */
    void archiveFiltersList()
    {
        VehicleListModel model(*m_db);
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        const QString a = model.save(newVehicle(QStringLiteral("Audi")));
        const QString b = model.save(newVehicle(QStringLiteral("BMW")));
        QCOMPARE(model.rowCount(), 2);

        QCOMPARE(model.archivedCount(), 0);
        QVERIFY(model.setArchived(a, true));
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.idAt(0), b);
        // Counted while hidden, so the empty state can point to "Show Archived".
        QCOMPARE(model.archivedCount(), 1);
        // The page still needs the archived vehicle's data.
        QVERIFY(model.get(a).value(QStringLiteral("archived")).toBool());

        model.setShowArchived(true);
        QCOMPARE(model.rowCount(), 2);
        QVERIFY(model.index(0).data(VehicleListModel::ArchivedRole).toBool());

        model.setShowArchived(false);
        QVERIFY(model.setArchived(a, false));
        QCOMPARE(model.rowCount(), 2);
        QVERIFY(!model.setArchived(QStringLiteral("no-such-id"), true));
        QCOMPARE(model.idAt(5), QString());
    }

    /** Distances are shown as truncated whole km with locale separators. */
    void formatDistance()
    {
        Format f;
        QCOMPARE(f.distance(12'345'999), QStringLiteral("12.345 km"));
        QCOMPARE(f.distance(0), QStringLiteral("0 km"));
    }

    /** Input text roundtrips: metres -> editable km text -> metres, invalid input gives null. */
    void parseDistanceInput()
    {
        Format f;
        QCOMPARE(f.distanceInput(12'345'000), QStringLiteral("12345"));
        QCOMPARE(f.distanceInput(12'345'600), QStringLiteral("12345,6"));
        QCOMPARE(f.parseDistance(QStringLiteral("12345,6")).toLongLong(), qint64(12'345'600));
        QCOMPARE(f.parseDistance(QStringLiteral(" 800 ")).toLongLong(), qint64(800'000));
        QVERIFY(f.parseDistance(QString()).isNull());
        QVERIFY(f.parseDistance(QStringLiteral("abc")).isNull());
        QVERIFY(f.parseDistance(QStringLiteral("-5")).isNull());
        // QML compares against null, so the invalid result must be nullptr, not an empty QVariant (= undefined).
        QCOMPARE(f.parseDistance(QStringLiteral("abc")).metaType(), QMetaType::fromType<std::nullptr_t>());
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Database> m_db;
};

QTEST_GUILESS_MAIN(TestVehicleListModel)
#include "test_vehiclelistmodel.moc"
