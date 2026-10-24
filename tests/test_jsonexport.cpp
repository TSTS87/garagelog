// Tests for JsonExporter, JsonImporter and CsvExporter (role: tests).
// Builds a garage with rows in every table plus an attachment, exports it
// and imports it into a second, empty garage (DATA_MODEL invariant 3).

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

#include "attachmentstore.h"
#include "categoryrepository.h"
#include "csv.h"
#include "csvexporter.h"
#include "database.h"
#include "fuelrepository.h"
#include "intervalrepository.h"
#include "jsonexporter.h"
#include "jsonimporter.h"
#include "servicerepository.h"
#include "vehiclerepository.h"

using namespace garagelog::core;

namespace {

/** Every row of every exported table as text, for comparing two garages. */
QStringList dump(const Database &db)
{
    QStringList lines;
    for (const QString &table : exportformat::tables) {
        QSqlQuery q(db.connection());
        q.exec(QStringLiteral("SELECT * FROM %1 ORDER BY 1").arg(table));
        while (q.next()) {
            QStringList values;
            for (int c = 0; c < q.record().count(); ++c) {
                values << (q.value(c).isNull() ? QStringLiteral("<NULL>") : q.value(c).toString());
            }
            lines << table + QLatin1Char(':') + values.join(QLatin1Char('|'));
        }
    }
    return lines;
}

} // namespace

/**
 * @brief Covers M13: complete JSON roundtrip with attachments, import guards, CSV table export.
 */
class TestJsonExport : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        m_source = std::make_unique<Database>();
        QVERIFY(m_source->open(m_dir->filePath(QStringLiteral("a/garage.sqlite"))));

        Vehicle v;
        v.name = QStringLiteral("Golf \"GTI\"");
        v.year = 2019;
        v.notes = QStringLiteral("line1\nline2");
        QVERIFY(VehicleRepository(*m_source).create(v));
        Vehicle archived;
        archived.name = QStringLiteral("Old");
        archived.archived = true;
        QVERIFY(VehicleRepository(*m_source).create(archived));

        CategoryRepository categories(*m_source);
        ServiceCategory own;
        own.name = QStringLiteral("Detailing");
        QVERIFY(categories.create(own));

        FuelEntry f;
        f.vehicleId = v.id;
        f.date = QDate(2026, 1, 5);
        f.odometerM = 12'000'000;
        f.quantity = 40'500;
        f.totalCostC = 7'250;
        f.station = QStringLiteral("Aral");
        QVERIFY(FuelRepository(*m_source).create(f));

        ServiceEntry s;
        s.vehicleId = v.id;
        s.categoryId = own.id;
        s.date = QDate(2026, 2, 1);
        s.description = QStringLiteral("Wash");
        s.costC = 2'500;
        s.performedBy = PerformedBy::Shop;
        QVERIFY(ServiceRepository(*m_source).create(s));

        const QString receipt = m_dir->filePath(QStringLiteral("receipt.pdf"));
        QFile file(receipt);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("%PDF receipt");
        file.close();
        QVERIFY(AttachmentStore(*m_source).add(s.id, receipt));

        MaintenanceInterval i;
        i.vehicleId = v.id;
        i.categoryId = categories.byName(QStringLiteral("oil_change"))->id;
        i.name = QStringLiteral("Oil");
        i.everyMonths = 12;
        QVERIFY(IntervalRepository(*m_source).create(i));

        QSqlQuery q(m_source->connection());
        QVERIFY(q.exec(QStringLiteral("INSERT INTO settings (key, value) VALUES ('currency', 'EUR')")));
    }

    void cleanup()
    {
        m_source.reset();
        m_dir.reset();
    }

    /** Export -> import into an empty garage gives identical rows and attachment files (invariant 3). */
    void roundtrip()
    {
        const QString exportDir = m_dir->filePath(QStringLiteral("export"));
        JsonExporter exporter(*m_source);
        QVERIFY2(exporter.exportTo(exportDir), qPrintable(exporter.errorString()));
        QVERIFY(exporter.warnings().isEmpty());

        Database target;
        QVERIFY(target.open(m_dir->filePath(QStringLiteral("b/garage.sqlite"))));
        JsonImporter importer(target);
        QVERIFY2(importer.importFrom(exportDir), qPrintable(importer.errorString()));
        QVERIFY2(importer.warnings().isEmpty(), qPrintable(importer.warnings().join(QLatin1Char('\n'))));
        QCOMPARE(dump(target), dump(*m_source));

        const auto attachments = AttachmentStore(target).byServiceEntry(ServiceRepository(target).byVehicle(VehicleRepository(target).all().first().id).first().id);
        QCOMPARE(attachments.size(), 1);
        QFile copy(AttachmentStore(target).filePath(attachments.first()));
        QVERIFY(copy.open(QIODevice::ReadOnly));
        QCOMPARE(copy.readAll(), QByteArray("%PDF receipt"));
    }

    /** The JSON header carries format, schema version and the tables; a second export into the same folder is refused. */
    void header()
    {
        const QString exportDir = m_dir->filePath(QStringLiteral("export"));
        JsonExporter exporter(*m_source);
        QVERIFY(exporter.exportTo(exportDir));
        QFile file(QDir(exportDir).filePath(exportformat::jsonFileName));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(root.value(QStringLiteral("format")).toString(), QStringLiteral("garagelog-export"));
        QCOMPARE(root.value(QStringLiteral("schema_version")).toInt(), 2);
        QCOMPARE(root.value(QStringLiteral("tables")).toObject().keys().size(), exportformat::tables.size());
        QVERIFY(!exporter.exportTo(exportDir));
    }

    /** Import refuses garages with vehicles, foreign files and newer schemas, and leaves the garage untouched. */
    void importGuards()
    {
        const QString exportDir = m_dir->filePath(QStringLiteral("export"));
        QVERIFY(JsonExporter(*m_source).exportTo(exportDir));

        JsonImporter intoSelf(*m_source);
        QVERIFY(!intoSelf.importFrom(exportDir));
        QVERIFY(intoSelf.errorString().contains(QStringLiteral("already contains vehicles")));

        Database target;
        QVERIFY(target.open(m_dir->filePath(QStringLiteral("c/garage.sqlite"))));
        const QString jsonPath = QDir(exportDir).filePath(exportformat::jsonFileName);
        QFile file(jsonPath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        root.insert(QStringLiteral("schema_version"), 99);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(QJsonDocument(root).toJson());
        file.close();
        JsonImporter newer(target);
        QVERIFY(!newer.importFrom(exportDir));
        QVERIFY(newer.errorString().contains(QStringLiteral("99")));
        QVERIFY(VehicleRepository(target).all(true).isEmpty());

        QVERIFY(!JsonImporter(target).check(m_dir->path())); // no export there
    }

    /** Import refuses rows the repositories would never write (negative odometer, broken date, text in a number column). */
    void importRejectsInvalidValues_data()
    {
        QTest::addColumn<QString>("table");
        QTest::addColumn<QString>("column");
        QTest::addColumn<QJsonValue>("value");
        QTest::newRow("negative odometer") << "fuel_entries" << "odometer_m" << QJsonValue(-5);
        QTest::newRow("broken date") << "fuel_entries" << "date" << QJsonValue(QStringLiteral("kaputt"));
        QTest::newRow("impossible date") << "service_entries" << "date" << QJsonValue(QStringLiteral("2025-02-30"));
        QTest::newRow("text quantity") << "fuel_entries" << "quantity" << QJsonValue(QStringLiteral("lots"));
        QTest::newRow("huge cost") << "fuel_entries" << "total_cost_c" << QJsonValue(1.5e300);
        QTest::newRow("huge offset") << "vehicles" << "odometer_offset_m" << QJsonValue(1e30);
        QTest::newRow("negative interval") << "maintenance_intervals" << "every_months" << QJsonValue(-5);
    }

    void importRejectsInvalidValues()
    {
        QFETCH(QString, table);
        QFETCH(QString, column);
        QFETCH(QJsonValue, value);
        const QString exportDir = m_dir->filePath(QStringLiteral("export"));
        QVERIFY(JsonExporter(*m_source).exportTo(exportDir));
        QFile file(QDir(exportDir).filePath(exportformat::jsonFileName));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        QJsonObject tables = root.value(QStringLiteral("tables")).toObject();
        QJsonArray rows = tables.value(table).toArray();
        QJsonObject row = rows.at(0).toObject();
        row.insert(column, value);
        rows.replace(0, row);
        tables.insert(table, rows);
        root.insert(QStringLiteral("tables"), tables);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(QJsonDocument(root).toJson());
        file.close();

        Database target;
        QVERIFY(target.open(m_dir->filePath(QStringLiteral("e/garage.sqlite"))));
        JsonImporter importer(target);
        QVERIFY(!importer.importFrom(exportDir));
        QVERIFY2(importer.errorString().contains(QStringLiteral("Invalid values in ") + table), qPrintable(importer.errorString()));
        QVERIFY(VehicleRepository(target).all(true).isEmpty());
    }

    /** An export of schema 1 is imported with its odometer offset converted like migration 0002 does (ADR-016). */
    void importConvertsSchema1Offsets()
    {
        const QString exportDir = m_dir->filePath(QStringLiteral("export"));
        QVERIFY(JsonExporter(*m_source).exportTo(exportDir));
        QFile file(QDir(exportDir).filePath(exportformat::jsonFileName));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        root.insert(QStringLiteral("schema_version"), 1);
        QJsonObject tables = root.value(QStringLiteral("tables")).toObject();
        QJsonArray vehicles = tables.value(QStringLiteral("vehicles")).toArray();
        for (int i = 0; i < vehicles.size(); ++i) {
            QJsonObject v = vehicles.at(i).toObject();
            v.remove(QStringLiteral("odometer_reading_m"));
            // The Golf has a fuel entry at 12,000 km; +300 km was a correction.
            v.insert(QStringLiteral("odometer_offset_m"), v.value(QStringLiteral("name")).toString().startsWith(QStringLiteral("Golf")) ? 300'000 : 0);
            vehicles.replace(i, v);
        }
        tables.insert(QStringLiteral("vehicles"), vehicles);
        root.insert(QStringLiteral("tables"), tables);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(QJsonDocument(root).toJson());
        file.close();

        Database target;
        QVERIFY(target.open(m_dir->filePath(QStringLiteral("f/garage.sqlite"))));
        JsonImporter importer(target);
        QVERIFY2(importer.importFrom(exportDir), qPrintable(importer.errorString()));
        const auto golf = VehicleRepository(target).all().first();
        QCOMPARE(golf.odometerReadingM, std::optional<qint64>(12'300'000));
    }

    /** A crafted stored_name in the garage never makes the export write outside the export folder. */
    void exportSkipsUnsafeStoredName()
    {
        QSqlQuery q(m_source->connection());
        QVERIFY(q.exec(QStringLiteral("UPDATE attachments SET stored_name = '../../planted.txt'")));
        // The file the traversal would copy: <garage dir>/attachments/../../planted.txt
        QFile bait(m_dir->filePath(QStringLiteral("planted.txt")));
        QVERIFY(bait.open(QIODevice::WriteOnly));
        bait.close();

        const QString exportDir = m_dir->filePath(QStringLiteral("x/y/export"));
        JsonExporter exporter(*m_source);
        QVERIFY2(exporter.exportTo(exportDir), qPrintable(exporter.errorString()));
        QCOMPARE(exporter.warnings().size(), 1);
        QVERIFY(exporter.warnings().first().contains(QStringLiteral("not allowed")));
        QVERIFY(!QFile::exists(m_dir->filePath(QStringLiteral("x/planted.txt"))));
    }

    /** Import refuses an attachment that is a symlink, so it cannot pull other files of the user into the garage. */
    void importRefusesSymlink()
    {
        const QString exportDir = m_dir->filePath(QStringLiteral("export"));
        QVERIFY(JsonExporter(*m_source).exportTo(exportDir));
        const QDir files(QDir(exportDir).filePath(AttachmentStore::directoryName));
        const QString name = files.entryList(QDir::Files).first();
        QVERIFY(QFile::remove(files.filePath(name)));
        QVERIFY(QFile::link(m_dir->filePath(QStringLiteral("receipt.pdf")), files.filePath(name)));

        Database target;
        QVERIFY(target.open(m_dir->filePath(QStringLiteral("d/garage.sqlite"))));
        JsonImporter importer(target);
        QVERIFY(!importer.importFrom(exportDir));
        QVERIFY(importer.errorString().contains(QStringLiteral("not a regular file")));
        QVERIFY(VehicleRepository(target).all(true).isEmpty());
    }

    /** CSV export writes one file per table with header and RFC 4180 quoting; NULL becomes empty. */
    void csvExport()
    {
        const QString dir = m_dir->filePath(QStringLiteral("csv"));
        CsvExporter exporter(*m_source);
        QVERIFY2(exporter.exportAll(dir), qPrintable(exporter.errorString()));
        for (const QString &table : exportformat::tables) {
            QVERIFY2(QFile::exists(QDir(dir).filePath(table + QStringLiteral(".csv"))), qPrintable(table));
        }
        QFile file(QDir(dir).filePath(QStringLiteral("vehicles.csv")));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto rows = csv::parse(QString::fromUtf8(file.readAll()), QLatin1Char(','));
        QCOMPARE(rows.size(), 3);
        QCOMPARE(rows.at(0).first(), QStringLiteral("id"));
        const int nameColumn = static_cast<int>(rows.at(0).indexOf(QStringLiteral("name")));
        const int makeColumn = static_cast<int>(rows.at(0).indexOf(QStringLiteral("make")));
        QStringList names{rows.at(1).at(nameColumn), rows.at(2).at(nameColumn)};
        QVERIFY(names.contains(QStringLiteral("Golf \"GTI\"")));
        QCOMPARE(rows.at(1).at(makeColumn), QString());
        QVERIFY(!exporter.exportTable(QStringLiteral("schema_version; DROP TABLE vehicles"), QDir(dir).filePath(QStringLiteral("x.csv"))));
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Database> m_source;
};

QTEST_GUILESS_MAIN(TestJsonExport)
#include "test_jsonexport.moc"
