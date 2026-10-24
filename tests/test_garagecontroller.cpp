// Tests for GarageController, DataTransfer, SettingsRepository and the unit conversions of Format (role: tests).
// Every garage lives in a temporary directory; the KConfig part of main.cpp
// is not involved, so the user's configuration is never touched.

#include <QDir>
#include <QFile>
#include <QLocale>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <memory>

#include "attachmentstore.h"
#include "categoryrepository.h"
#include "database.h"
#include "datatransfer.h"
#include "format.h"
#include "garagecontroller.h"
#include "servicerepository.h"
#include "settingsrepository.h"
#include "vehiclerepository.h"

using garagelog::app::DataTransfer;
using garagelog::app::Format;
using garagelog::app::GarageController;
using namespace garagelog::core;

/**
 * @brief Covers M14: settings storage, unit conversion, garage switching, save-as, export/import actions;
 * M15: watching the garage file for deletion and write protection.
 */
class TestGarageController : public QObject
{
    Q_OBJECT

private:
    QString path(const QString &name) const
    {
        return m_dir->filePath(name);
    }

    QString addVehicle(const QString &name)
    {
        Vehicle v;
        v.name = name;
        VehicleRepository(*m_db).create(v);
        return v.id;
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
        QVERIFY(m_db->open(path(QStringLiteral("home/garage.sqlite"))));
    }

    void cleanup()
    {
        m_db.reset();
        m_dir.reset();
    }

    /** Settings are stored per garage and replaced on a second write. */
    void settingsRepository()
    {
        SettingsRepository settings(*m_db);
        QCOMPARE(settings.value(SettingsRepository::currencyKey, QStringLiteral("EUR")), QStringLiteral("EUR"));
        QVERIFY(settings.setValue(SettingsRepository::currencyKey, QStringLiteral("USD")));
        QVERIFY(settings.setValue(SettingsRepository::currencyKey, QStringLiteral("GBP")));
        QCOMPARE(settings.value(SettingsRepository::currencyKey), QStringLiteral("GBP"));
    }

    /** Setting units through the controller stores them and switches Format (km -> mi, l -> US gal, EUR -> USD). */
    void unitsFollowSettings()
    {
        Format format;
        GarageController garage(*m_db, format);
        QSignalSpy units(&format, &Format::unitsChanged);
        QCOMPARE(format.distance(12'345'678), QStringLiteral("12.345 km"));

        garage.setDistanceUnit(QStringLiteral("mi"));
        garage.setVolumeUnit(QStringLiteral("gal_us"));
        garage.setCurrency(QStringLiteral("USD"));
        QCOMPARE(units.count(), 3);
        QCOMPARE(SettingsRepository(*m_db).value(SettingsRepository::volumeUnitKey), QStringLiteral("gal_us"));
        QCOMPARE(format.currencySymbol(), QStringLiteral("$"));

        // 16,093.44 m = 10 mi; 37,854.11784 ml = 10 US gal.
        QCOMPARE(format.distance(16'094), QStringLiteral("10 mi"));
        QCOMPARE(format.distance(16'093), QStringLiteral("9 mi")); // truncated like an odometer
        QCOMPARE(format.parseDistance(QStringLiteral("10")).toLongLong(), 16'093);
        QCOMPARE(format.quantity(37'854, false), QStringLiteral("10,00 gal"));
        QCOMPARE(format.parseQuantity(QStringLiteral("10"), false).toLongLong(), 37'854);
        QCOMPARE(format.quantity(37'854, true), QStringLiteral("37,85 kWh")); // electric stays kWh
        // 1.000 $/l = 3.785 $/gal and back.
        QCOMPARE(format.unitPriceInput(1'000, false), QStringLiteral("3,785"));
        QCOMPARE(format.parseUnitPrice(QStringLiteral("3,785411784"), false).toLongLong(), 1'000);
        // 7.84 l/100 km is almost exactly 30 mpg (US).
        QVERIFY(qAbs(format.consumptionValue(7'840.0, false) - 30.0) < 0.05);
        QCOMPARE(format.consumptionUnit(false), QStringLiteral("mpg"));
        QCOMPARE(format.consumptionUnit(true), QStringLiteral("kWh/100 mi"));

        // A second controller on the same garage reads the stored units.
        Format other;
        GarageController again(*m_db, other);
        QCOMPARE(again.distanceUnit(), QStringLiteral("mi"));
        QCOMPARE(other.volumeLabel(), QStringLiteral("gal"));
    }

    /** Unknown values fall back to km, l and EUR. */
    void unknownUnitsFallBack()
    {
        Format format;
        format.setUnits(QStringLiteral("parsec"), QStringLiteral("barrel"), QStringLiteral("x"));
        QCOMPARE(format.distanceUnit(), QStringLiteral("km"));
        QCOMPARE(format.volumeUnit(), QStringLiteral("l"));
        QCOMPARE(format.currency(), QStringLiteral("EUR"));
    }

    /** newGarage() creates and switches; openGarage() goes back; units are per garage. */
    void switchGarages()
    {
        Format format;
        GarageController garage(*m_db, format);
        addVehicle(QStringLiteral("Golf"));
        garage.setDistanceUnit(QStringLiteral("mi"));
        const QString home = m_db->filePath();
        QSignalSpy changed(&garage, &GarageController::garageChanged);

        const QString second = path(QStringLiteral("other/second.sqlite"));
        QVERIFY2(garage.newGarage(QUrl::fromLocalFile(second).toString()), qPrintable(garage.errorString()));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(garage.filePath(), second);
        QVERIFY(VehicleRepository(*m_db).all(true).isEmpty());
        QCOMPARE(format.distanceUnit(), QStringLiteral("km"));
        QVERIFY(!garage.newGarage(second)); // exists already

        QVERIFY2(garage.openGarage(home), qPrintable(garage.errorString()));
        QCOMPARE(VehicleRepository(*m_db).all(true).size(), 1);
        QCOMPARE(format.distanceUnit(), QStringLiteral("mi"));
        QVERIFY(!garage.openGarage(path(QStringLiteral("missing.sqlite"))));
        QCOMPARE(garage.filePath(), home);
    }

    /** A foreign SQLite file is refused untouched, and the open garage stays open. */
    void foreignDatabaseRefused()
    {
        const QString foreign = path(QStringLiteral("foreign.sqlite"));
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("foreign"));
            db.setDatabaseName(foreign);
            QVERIFY(db.open());
            QSqlQuery(db).exec(QStringLiteral("CREATE TABLE notes (x TEXT)"));
            db.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("foreign"));

        Format format;
        GarageController garage(*m_db, format);
        QVERIFY(!garage.openGarage(foreign));
        QVERIFY(garage.errorString().contains(QStringLiteral("not a GarageLog garage")));
        QVERIFY(m_db->isOpen());
        QCOMPARE(garage.filePath(), path(QStringLiteral("home/garage.sqlite")));
    }

    /** saveGarageAs() copies database and attachment files and continues in the copy. */
    void saveAs()
    {
        Format format;
        GarageController garage(*m_db, format);
        const QString vehicleId = addVehicle(QStringLiteral("Golf"));
        ServiceEntry e;
        e.vehicleId = vehicleId;
        e.categoryId = CategoryRepository(*m_db).byName(QStringLiteral("repair"))->id;
        e.date = QDate(2026, 1, 1);
        e.description = QStringLiteral("Clutch");
        QVERIFY(ServiceRepository(*m_db).create(e));
        QFile receipt(path(QStringLiteral("receipt.jpg")));
        QVERIFY(receipt.open(QIODevice::WriteOnly));
        receipt.write("jpg");
        receipt.close();
        const auto attachment = AttachmentStore(*m_db).add(e.id, receipt.fileName());
        QVERIFY(attachment);

        const QString copy = path(QStringLiteral("backup/copy.sqlite"));
        QVERIFY2(garage.saveGarageAs(copy), qPrintable(garage.errorString()));
        QCOMPARE(garage.filePath(), copy);
        QCOMPARE(VehicleRepository(*m_db).all().size(), 1);
        QVERIFY(QFile::exists(path(QStringLiteral("backup/attachments/")) + attachment->storedName));
        QVERIFY(!garage.saveGarageAs(copy)); // the open garage itself
    }

    /** saveGarageAs() copies only attachments of this garage, replaces an existing target and leaves no temporary file. */
    void saveAsCopiesOnlyOwnAttachments()
    {
        Format format;
        GarageController garage(*m_db, format);
        addVehicle(QStringLiteral("Golf"));
        // A file of another garage in the same folder must not travel along.
        QVERIFY(QDir().mkpath(path(QStringLiteral("home/attachments"))));
        QFile foreign(path(QStringLiteral("home/attachments/other-garage.pdf")));
        QVERIFY(foreign.open(QIODevice::WriteOnly));
        foreign.close();
        QFile old(path(QStringLiteral("out/copy.sqlite")));
        QVERIFY(QDir().mkpath(path(QStringLiteral("out"))));
        QVERIFY(old.open(QIODevice::WriteOnly));
        old.write("old content");
        old.close();

        QVERIFY2(garage.saveGarageAs(path(QStringLiteral("out/copy.sqlite"))), qPrintable(garage.errorString()));
        QVERIFY(!QFile::exists(path(QStringLiteral("out/attachments/other-garage.pdf"))));
        QCOMPARE(VehicleRepository(*m_db).all().size(), 1);
        QCOMPARE(QDir(path(QStringLiteral("out"))).entryList(QDir::Files), QStringList{QStringLiteral("copy.sqlite")});
    }

    /** A failed switch keeps the open garage usable, even if its file was deleted meanwhile (no silent empty garage). */
    void failedSwitchKeepsOpenGarage()
    {
        Format format;
        GarageController garage(*m_db, format);
        addVehicle(QStringLiteral("Golf"));
        const QString file = garage.filePath();
        QVERIFY(QFile::remove(file));

        QFile text(path(QStringLiteral("notes.sqlite")));
        QVERIFY(text.open(QIODevice::WriteOnly));
        text.write("just text, not a database");
        text.close();
        QVERIFY(!garage.openGarage(text.fileName()));
        QVERIFY(!garage.newGarage(path(QStringLiteral("notes.sqlite"))));
        QCOMPARE(garage.filePath(), file);
        QCOMPARE(VehicleRepository(*m_db).all().size(), 1);
        QVERIFY(!QFile::exists(file));
    }

    /** A garage file deleted while open is reported via the watcher, and "save as" still rescues its data. */
    void deletedGarageIsReportedAndRescued()
    {
        Format format;
        GarageController garage(*m_db, format);
        addVehicle(QStringLiteral("Golf"));
        QCOMPARE(garage.fileState(), QStringLiteral("ok"));

        QSignalSpy spy(&garage, &GarageController::fileStateChanged);
        QVERIFY(QFile::remove(garage.filePath()));
        // The folder watcher reports the delete through the event loop.
        QTRY_COMPARE(garage.fileState(), QStringLiteral("missing"));
        QCOMPARE(spy.count(), 1);

        const QString rescue = path(QStringLiteral("rescue/garage.sqlite"));
        QVERIFY2(garage.saveGarageAs(rescue), qPrintable(garage.errorString()));
        QCOMPARE(garage.fileState(), QStringLiteral("ok"));
        QCOMPARE(VehicleRepository(*m_db).all().size(), 1);
    }

    /** A write-protected garage is reported as read-only and can be copied to a writable place. */
    void readOnlyGarageIsReportedAndCopied()
    {
        addVehicle(QStringLiteral("Golf"));
        const QString file = m_db->filePath();
        QVERIFY(QFile::setPermissions(file, QFile::ReadOwner));
        // Reopen: a connection opened before the chmod could still write.
        QVERIFY(m_db->open(file));
        Format format;
        GarageController garage(*m_db, format);
        QCOMPARE(garage.fileState(), QStringLiteral("readOnly"));
        // Writing a setting fails visibly instead of pretending to work.
        garage.setCurrency(QStringLiteral("USD"));
        QVERIFY(!garage.errorString().isEmpty());

        QVERIFY2(garage.saveGarageAs(path(QStringLiteral("copy.sqlite"))), qPrintable(garage.errorString()));
        QCOMPARE(garage.fileState(), QStringLiteral("ok"));
        QCOMPARE(VehicleRepository(*m_db).all().size(), 1);
        QVERIFY(QFile::setPermissions(file, QFile::ReadOwner | QFile::WriteOwner));
    }

    /** JSON export into a dated subfolder, import into a new garage; CSV mapping survives the QVariantMap roundtrip. */
    void dataTransfer()
    {
        addVehicle(QStringLiteral("Golf"));
        DataTransfer transfer(*m_db);
        QVERIFY2(transfer.exportJson(path(QStringLiteral("exports"))), qPrintable(transfer.errorString()));
        QVERIFY(transfer.lastPath().contains(QStringLiteral("garagelog-export-")));
        QVERIFY(transfer.exportJson(path(QStringLiteral("exports"))));
        QVERIFY(transfer.lastPath().endsWith(QStringLiteral("-2")));
        const QString exported = transfer.lastPath();
        QVERIFY(transfer.exportCsv(path(QStringLiteral("exports"))));
        QVERIFY(QFile::exists(QDir(transfer.lastPath()).filePath(QStringLiteral("vehicles.csv"))));

        Format format;
        GarageController garage(*m_db, format);
        QVERIFY(garage.newGarage(path(QStringLiteral("restored.sqlite"))));
        QSignalSpy imported(&transfer, &DataTransfer::dataImported);
        QVERIFY2(transfer.importJson(QUrl::fromLocalFile(exported).toString()), qPrintable(transfer.errorString()));
        QCOMPARE(imported.count(), 1);
        QCOMPARE(VehicleRepository(*m_db).all().first().name, QStringLiteral("Golf"));

        CsvFuelMapping m;
        m.headerRow = 3;
        m.dateColumn = 1;
        m.quantityColumn = 2;
        m.dateFormat = QStringLiteral("dd.MM.yyyy");
        m.decimalSeparator = QLatin1Char(',');
        QCOMPARE(DataTransfer::toMapping(DataTransfer::fromMapping(m)), m);
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Database> m_db;
};

QTEST_GUILESS_MAIN(TestGarageController)
#include "test_garagecontroller.moc"
