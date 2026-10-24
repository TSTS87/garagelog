// Tests for the app-layer IntervalListModel and Format::dueSummary (role: tests).
// The model evaluates against today's real date, so service dates are
// built relative to QDate::currentDate().

#include <QAbstractItemModelTester>
#include <QLocale>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

#include "categoryrepository.h"
#include "database.h"
#include "format.h"
#include "intervallistmodel.h"
#include "servicerepository.h"
#include "vehiclerepository.h"

using garagelog::app::Format;
using garagelog::app::IntervalListModel;
using namespace garagelog::core;

/**
 * @brief Covers M10 app layer: urgency order, worst status, defaults, overview across vehicles.
 */
class TestIntervalListModel : public QObject
{
    Q_OBJECT

private:
    QString createVehicle(const QString &name, bool archived = false)
    {
        Vehicle v;
        v.name = name;
        v.archived = archived;
        VehicleRepository(*m_db).create(v);
        return v.id;
    }

    void addService(const QString &vehicleId, const QString &categoryKey, const QDate &date)
    {
        ServiceEntry e;
        e.vehicleId = vehicleId;
        e.categoryId = CategoryRepository(*m_db).byName(categoryKey)->id;
        e.date = date;
        e.description = QStringLiteral("x");
        QVERIFY(ServiceRepository(*m_db).create(e));
    }

    QVariantMap monthly(const QString &categoryKey, int months, const QString &name = {}) const
    {
        return {
            {QStringLiteral("categoryId"), CategoryRepository(*m_db).byName(categoryKey)->id},
            {QStringLiteral("name"), name},
            {QStringLiteral("everyM"), QVariant::fromValue(nullptr)},
            {QStringLiteral("everyMonths"), months},
            {QStringLiteral("active"), true},
        };
    }

private Q_SLOTS:
    void initTestCase()
    {
        QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
    }

    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        m_db = std::make_unique<Database>();
        QVERIFY2(m_db->open(m_dir->filePath(QStringLiteral("garage.sqlite"))), qPrintable(m_db->errorString()));
        m_vehicleId = createVehicle(QStringLiteral("Golf"));
    }

    void cleanup()
    {
        m_db.reset();
        m_dir.reset();
    }

    /** Rows are sorted overdue, soon, ok, unknown, paused; worstStatus and nextDue follow the first rows. */
    void urgencyOrder()
    {
        const QDate today = QDate::currentDate();
        addService(m_vehicleId, QStringLiteral("oil_change"), today.addMonths(-6));   // 12 months -> ok
        addService(m_vehicleId, QStringLiteral("mot"), today.addMonths(-23));         // 24 months -> soon
        addService(m_vehicleId, QStringLiteral("brakes"), today.addMonths(-13));      // 12 months -> overdue

        IntervalListModel model(*m_db, IntervalListModel::Scope::Vehicle);
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.setVehicleId(m_vehicleId);
        QCOMPARE(model.worstStatus(), QString());
        QVERIFY(!model.save(monthly(QStringLiteral("oil_change"), 12)).isEmpty());
        QVERIFY(!model.save(monthly(QStringLiteral("mot"), 24)).isEmpty());
        QVERIFY(!model.save(monthly(QStringLiteral("brakes"), 12)).isEmpty());
        QVERIFY(!model.save(monthly(QStringLiteral("tires"), 36)).isEmpty());   // never serviced -> unknown
        QVariantMap paused = monthly(QStringLiteral("coolant"), 1);
        paused[QStringLiteral("active")] = false;
        QVERIFY(!model.save(paused).isEmpty());

        QCOMPARE(model.rowCount(), 5);
        QStringList statuses;
        for (int r = 0; r < model.rowCount(); ++r) {
            statuses << model.index(r).data(IntervalListModel::StatusRole).toString();
        }
        QCOMPARE(statuses, (QStringList{QStringLiteral("overdue"), QStringLiteral("soon"), QStringLiteral("ok"),
                                        QStringLiteral("unknown"), QStringLiteral("paused")}));
        QCOMPARE(model.worstStatus(), QStringLiteral("overdue"));
        QCOMPARE(model.overdueCount(), 1);
        QCOMPARE(model.nextDue().value(QStringLiteral("name")).toString(), QStringLiteral("Brakes"));
    }

    /** A blank name is replaced by the translated category name; get() returns the stored values. */
    void defaultNameAndGet()
    {
        IntervalListModel model(*m_db, IntervalListModel::Scope::Vehicle);
        model.setVehicleId(m_vehicleId);
        const QString id = model.save(monthly(QStringLiteral("oil_change"), 12, QStringLiteral("  ")));
        QVERIFY(!id.isEmpty());
        const QVariantMap m = model.get(id);
        QCOMPARE(m.value(QStringLiteral("name")).toString(), QStringLiteral("Oil change"));
        QVERIFY(m.value(QStringLiteral("everyM")).isNull());
        QCOMPARE(m.value(QStringLiteral("everyMonths")).toInt(), 12);
        QCOMPARE(m.value(QStringLiteral("status")).toString(), QStringLiteral("unknown"));

        QVariantMap invalid = monthly(QStringLiteral("oil_change"), 12);
        invalid[QStringLiteral("everyMonths")] = QVariant::fromValue(nullptr);
        QVERIFY(model.save(invalid).isEmpty());
        QVERIFY(!model.errorString().isEmpty());
    }

    /** The overview lists active intervals of all non-archived vehicles with the vehicle name. */
    void allVehiclesScope()
    {
        const QString bike = createVehicle(QStringLiteral("CB500F"));
        const QString old = createVehicle(QStringLiteral("Old"), true);
        IntervalListModel perVehicle(*m_db, IntervalListModel::Scope::Vehicle);
        for (const QString &v : {m_vehicleId, bike, old}) {
            perVehicle.setVehicleId(v);
            QVERIFY(!perVehicle.save(monthly(QStringLiteral("mot"), 24)).isEmpty());
        }

        IntervalListModel all(*m_db, IntervalListModel::Scope::AllVehicles);
        QCOMPARE(all.rowCount(), 2);
        QStringList names{all.index(0).data(IntervalListModel::VehicleNameRole).toString(),
                          all.index(1).data(IntervalListModel::VehicleNameRole).toString()};
        names.sort();
        QCOMPARE(names, (QStringList{QStringLiteral("CB500F"), QStringLiteral("Golf")}));
    }

    /** month() names the month in the locale's language for list section headings; invalid input gives "". */
    void monthHeading()
    {
        Format f;
        QCOMPARE(f.month(QStringLiteral("2026-10")), QStringLiteral("Oktober 2026"));
        QCOMPARE(f.month(QStringLiteral("2026-13")), QString());
    }

    /** dueSummary() combines distance and days, with own texts for overdue, today, unknown and paused. */
    void dueSummary()
    {
        Format f;
        const QVariant null = QVariant::fromValue(nullptr);
        QCOMPARE(f.dueSummary(qint64(1'500'000), 47, QStringLiteral("soon")), QStringLiteral("1.500 km left · 47 days left"));
        QCOMPARE(f.dueSummary(qint64(-500'000), null, QStringLiteral("overdue")), QStringLiteral("500 km overdue"));
        QCOMPARE(f.dueSummary(null, 0, QStringLiteral("overdue")), QStringLiteral("due today"));
        QCOMPARE(f.dueSummary(null, -1, QStringLiteral("overdue")), QStringLiteral("1 day overdue"));
        QCOMPARE(f.dueSummary(null, null, QStringLiteral("unknown")), QStringLiteral("No matching service recorded yet"));
        QCOMPARE(f.dueSummary(null, null, QStringLiteral("paused")), QStringLiteral("Paused"));
        QCOMPARE(f.parseMonths(QStringLiteral(" 12 ")), QVariant(12));
        QVERIFY(f.parseMonths(QStringLiteral("0")).isNull());
        QVERIFY(f.parseMonths(QStringLiteral("1,5")).isNull());
        QCOMPARE(f.parseMonths(QStringLiteral("1200")), QVariant(1200));
        // Where an interval falls due shows only the known parts.
        QCOMPARE(f.dueAt(qint64(60'000'000), QStringLiteral("2027-03-01")), QStringLiteral("60.000 km / 01.03.2027"));
        QCOMPARE(f.dueAt(null, QStringLiteral("2027-03-01")), QStringLiteral("01.03.2027"));
        QCOMPARE(f.dueAt(null, QString()), QString());
        QCOMPARE(f.fileName(QStringLiteral("file:///tmp/a%20b/r%C3%A9%23.pdf")), QStringLiteral("ré#.pdf"));
        QCOMPARE(f.fileName(QStringLiteral("/home/x/garage.sqlite")), QStringLiteral("garage.sqlite"));
        QVERIFY(f.parseMonths(QStringLiteral("2147483647")).isNull());
        // An odometer beyond 64 bits must not wrap into a negative distance.
        QVERIFY(f.parseDistance(QStringLiteral("9999999999999999")).isNull());
        QVERIFY(f.parseMoney(QStringLiteral("99999999999999")).isNull());
        QCOMPARE(f.parseDistance(QStringLiteral("10000000")), QVariant::fromValue<qint64>(10'000'000'000));
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Database> m_db;
    QString m_vehicleId;
};

QTEST_GUILESS_MAIN(TestIntervalListModel)
#include "test_intervallistmodel.moc"
