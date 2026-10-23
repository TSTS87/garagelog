// Tests for the app-layer ServiceListModel (role: tests).
// Runs without a window; QAbstractItemModelTester checks the model contract.

#include <QAbstractItemModelTester>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <memory>

#include "categoryrepository.h"
#include "database.h"
#include "servicelistmodel.h"
#include "vehiclerepository.h"

using garagelog::app::ServiceListModel;
using namespace garagelog::core;

/**
 * @brief Covers M08 app layer: service list, save with attachments, categories.
 */
class TestServiceListModel : public QObject
{
    Q_OBJECT

private:
    /** Form values as QML would send them for a new entry. */
    QVariantMap fill(const QString &date, const QVariant &odometerM, const QString &categoryName) const
    {
        return {
            {QStringLiteral("date"), date},
            {QStringLiteral("odometerM"), odometerM},
            {QStringLiteral("categoryId"), CategoryRepository(*m_db).byName(categoryName)->id},
            {QStringLiteral("description"), QStringLiteral("Work")},
            {QStringLiteral("costC"), QVariant::fromValue(nullptr)},
            {QStringLiteral("performedBy"), QStringLiteral("self")},
            {QStringLiteral("shopName"), QStringLiteral("ignored")},
        };
    }

private Q_SLOTS:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        m_db = std::make_unique<Database>();
        QVERIFY2(m_db->open(m_dir->filePath(QStringLiteral("garage/garage.sqlite"))), qPrintable(m_db->errorString()));
        VehicleRepository vehicles(*m_db);
        Vehicle v;
        v.name = QStringLiteral("CB500F");
        QVERIFY(vehicles.create(v));
        m_vehicleId = v.id;
    }

    void cleanup()
    {
        m_db.reset();
        m_dir.reset();
    }

    /** Entries appear newest first with translated category names; null cost becomes 0 and a shop name is dropped for "self". */
    void listNewestFirst()
    {
        ServiceListModel model(*m_db);
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QSignalSpy changed(&model, &ServiceListModel::entriesChanged);
        model.setVehicleId(m_vehicleId);

        QVERIFY(!model.save(fill(QStringLiteral("2026-03-01"), 1000, QStringLiteral("oil_change"))).isEmpty());
        QVERIFY(!model.save(fill(QStringLiteral("2026-06-01"), QVariant::fromValue(nullptr), QStringLiteral("mot"))).isEmpty());
        QCOMPARE(changed.count(), 2);
        QCOMPARE(model.rowCount(), 2);

        const QModelIndex first = model.index(0);
        QCOMPARE(first.data(ServiceListModel::DateRole).toString(), QStringLiteral("2026-06-01"));
        QVERIFY(first.data(ServiceListModel::OdometerRole).isNull());
        QCOMPARE(first.data(ServiceListModel::CategoryNameRole).toString(), QStringLiteral("MOT / roadworthiness test"));
        QCOMPARE(first.data(ServiceListModel::CostRole).toLongLong(), 0);
        QCOMPARE(first.data(ServiceListModel::ShopNameRole).toString(), QString());
        QCOMPARE(model.index(1).data(ServiceListModel::OdometerRole).toLongLong(), 1000);
    }

    /** get() returns what save() stored, and an update keeps the id. */
    void getAndUpdate()
    {
        ServiceListModel model(*m_db);
        model.setVehicleId(m_vehicleId);
        QVariantMap values = fill(QStringLiteral("2026-03-01"), 1000, QStringLiteral("tires"));
        values[QStringLiteral("performedBy")] = QStringLiteral("shop");
        values[QStringLiteral("costC")] = 45000;
        const QString id = model.save(values);
        QVERIFY(!id.isEmpty());

        QVariantMap loaded = model.get(id);
        QCOMPARE(loaded.value(QStringLiteral("shopName")).toString(), QStringLiteral("ignored"));
        QCOMPARE(loaded.value(QStringLiteral("costC")).toLongLong(), 45000);
        loaded[QStringLiteral("description")] = QStringLiteral("Winter tyres");
        QCOMPARE(model.save(loaded), id);
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.index(0).data(ServiceListModel::DescriptionRole).toString(), QStringLiteral("Winter tyres"));
    }

    /** Files passed as newAttachments are copied, flagged in the list, and deleted with the entry. */
    void attachmentsOnSave()
    {
        const QString source = m_dir->filePath(QStringLiteral("receipt.jpg"));
        QFile f(source);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("jpeg");
        f.close();

        ServiceListModel model(*m_db);
        model.setVehicleId(m_vehicleId);
        QVariantMap values = fill(QStringLiteral("2026-03-01"), 1000, QStringLiteral("repair"));
        values[QStringLiteral("newAttachments")] = QVariantList{QUrl::fromLocalFile(source).toString()};
        const QString id = model.save(values);
        QVERIFY2(model.errorString().isEmpty(), qPrintable(model.errorString()));
        QVERIFY(model.index(0).data(ServiceListModel::HasAttachmentRole).toBool());

        const QVariantList list = model.attachments(id);
        QCOMPARE(list.size(), 1);
        const QVariantMap a = list.first().toMap();
        QCOMPARE(a.value(QStringLiteral("filename")).toString(), QStringLiteral("receipt.jpg"));
        const QString stored = QUrl(a.value(QStringLiteral("url")).toString()).toLocalFile();
        QVERIFY(QFile::exists(stored));

        QVERIFY(model.remove(id));
        QVERIFY(!QFile::exists(stored));
    }

    /** A failing attachment still saves the entry but reports the error. */
    void failedAttachmentReported()
    {
        ServiceListModel model(*m_db);
        model.setVehicleId(m_vehicleId);
        QVariantMap values = fill(QStringLiteral("2026-03-01"), 1000, QStringLiteral("repair"));
        values[QStringLiteral("newAttachments")] = QVariantList{m_dir->filePath(QStringLiteral("missing.pdf"))};
        QVERIFY(!model.save(values).isEmpty());
        QVERIFY(!model.errorString().isEmpty());
        QCOMPARE(model.rowCount(), 1);
    }

    /** categories() lists built-ins translated and first; addCategory() appends a user category and refuses duplicates. */
    void categories()
    {
        ServiceListModel model(*m_db);
        QCOMPARE(model.categories().first().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Oil change"));
        const QString id = model.addCategory(QStringLiteral("Detailing"));
        QVERIFY(!id.isEmpty());
        const QVariantMap last = model.categories().last().toMap();
        QCOMPARE(last.value(QStringLiteral("id")).toString(), id);
        QCOMPARE(last.value(QStringLiteral("builtin")).toBool(), false);
        QVERIFY(model.addCategory(QStringLiteral("Detailing")).isEmpty());
        QVERIFY(!model.errorString().isEmpty());
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Database> m_db;
    QString m_vehicleId;
};

QTEST_GUILESS_MAIN(TestServiceListModel)
#include "test_servicelistmodel.moc"
