// Tests for the CSV helpers and CsvFuelImporter (role: tests).
// Uses tests/data/fuelio_sample.csv, a hand-written file in Fuelio's export
// layout (sections "## Vehicle", "## Log", "## CostCategories").

#include <QTemporaryDir>
#include <QTest>

#include <memory>

#include "csv.h"
#include "csvfuelimporter.h"
#include "database.h"
#include "fuelrepository.h"
#include "vehiclerepository.h"

using namespace garagelog::core;

/**
 * @brief Covers M13: CSV parsing/escaping, column mapping, dry run and import of fuel entries.
 */
class TestCsvFuelImporter : public QObject
{
    Q_OBJECT

private:
    QString samplePath() const
    {
        return QStringLiteral(GARAGELOG_TEST_DATA_DIR "/fuelio_sample.csv");
    }

private Q_SLOTS:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        m_db = std::make_unique<Database>();
        QVERIFY2(m_db->open(m_dir->filePath(QStringLiteral("garage.sqlite"))), qPrintable(m_db->errorString()));
        Vehicle v;
        v.name = QStringLiteral("Golf");
        QVERIFY(VehicleRepository(*m_db).create(v));
        m_vehicleId = v.id;
    }

    void cleanup()
    {
        m_db.reset();
        m_dir.reset();
    }

    /** Quoted fields keep delimiters, line breaks and doubled quotes; escape() and parse() are inverse. */
    void csvRoundtrip()
    {
        const QStringList fields{QStringLiteral("plain"), QStringLiteral("a,b"), QStringLiteral("say \"hi\""), QStringLiteral("two\nlines"), QString()};
        const QString text = csv::row(fields) + csv::row({QStringLiteral("x")});
        const auto rows = csv::parse(text, QLatin1Char(','));
        QCOMPARE(rows.size(), 2);
        QCOMPARE(rows.at(0), fields);
        QCOMPARE(rows.at(1), QStringList{QStringLiteral("x")});
    }

    /** The delimiter is detected from the text; a byte order mark is ignored. */
    void delimiterDetection()
    {
        QCOMPARE(csv::detectDelimiter(QStringLiteral("a;b;c\n1;2,5;3\n")), QLatin1Char(';'));
        QCOMPARE(csv::detectDelimiter(QStringLiteral("\"a,b\"\tc\n")), QLatin1Char('\t'));
        QCOMPARE(csv::parse(QStringLiteral("﻿a,b"), QLatin1Char(',')).first().first(), QStringLiteral("a"));
    }

    /** suggestMapping() finds Fuelio's "## Log" section and all its columns. */
    void fuelioMapping()
    {
        CsvFuelImporter importer(*m_db);
        QVERIFY(importer.loadFile(samplePath()));
        const CsvFuelMapping m = importer.suggestMapping();
        QCOMPARE(m.headerRow, 4);
        QCOMPARE(m.dateColumn, 0);
        QCOMPARE(m.odometerColumn, 1);
        QCOMPARE(m.quantityColumn, 2);
        QCOMPARE(m.fullTankColumn, 3);
        QCOMPARE(m.totalCostColumn, 4);
        QCOMPARE(m.stationColumn, 8);
        QCOMPARE(m.notesColumn, 9);
        QCOMPARE(m.unitPriceColumn, 13);
        QCOMPARE(m.dateFormat, QStringLiteral("yyyy-MM-dd"));
        QCOMPARE(m.decimalSeparator, QLatin1Char('.'));
    }

    /** The dry run converts units, computes a missing total from the unit price and reports the invalid date with its row. */
    void fuelioPreview()
    {
        CsvFuelImporter importer(*m_db);
        QVERIFY(importer.loadFile(samplePath()));
        const CsvFuelPreview p = importer.preview(importer.suggestMapping());
        QCOMPARE(p.entries.size(), 4);
        QCOMPARE(p.errors, QStringList{QStringLiteral("Row 9: invalid date \"2026-02-31\"")});
        QCOMPARE(p.lines, (QList<int>{6, 7, 8, 10}));

        const FuelEntry &first = p.entries.at(0);
        QCOMPARE(first.date, QDate(2026, 1, 5));
        QCOMPARE(first.odometerM, 12'000'000);
        QCOMPARE(first.quantity, 40'500);
        QCOMPARE(first.totalCostC, 7'250);
        QCOMPARE(first.station, QStringLiteral("Aral Hauptstr."));

        const FuelEntry &partial = p.entries.at(2);
        QVERIFY(!partial.fullTank);
        QCOMPARE(partial.totalCostC, 2'700); // 15 l x 1.80 (price column is 0)
        QCOMPARE(partial.notes, QStringLiteral("partial, \"quick\" stop"));

        // Nothing is written by a dry run.
        QVERIFY(FuelRepository(*m_db).byVehicle(m_vehicleId).isEmpty());
    }

    /** importInto() stores the valid rows for the vehicle. */
    void fuelioImport()
    {
        CsvFuelImporter importer(*m_db);
        QVERIFY(importer.loadFile(samplePath()));
        QCOMPARE(importer.importInto(m_vehicleId, importer.suggestMapping()), 4);
        const auto stored = FuelRepository(*m_db).byVehicle(m_vehicleId);
        QCOMPARE(stored.size(), 4);
        QCOMPARE(stored.last().odometerM, 13'350'000);
    }

    /** German-style files (semicolon, decimal comma, dd.MM.yyyy, "Ja"/"Nein") map and parse as well. */
    void germanFile()
    {
        CsvFuelImporter importer(*m_db);
        importer.load(QStringLiteral("Datum;Kilometerstand;Menge;Betrag;Volltank;Tankstelle\n"
                                     "03.01.2026;12.345;40,5;72,90;Ja;Aral\n"
                                     "04.02.2026;12.900;20,0;36,00;Nein;\n"));
        const CsvFuelMapping m = importer.suggestMapping();
        QCOMPARE(m.decimalSeparator, QLatin1Char(','));
        QCOMPARE(m.dateFormat, QStringLiteral("dd.MM.yyyy"));
        const CsvFuelPreview p = importer.preview(m);
        QVERIFY2(p.errors.isEmpty(), qPrintable(p.errors.join(QLatin1Char('\n'))));
        QCOMPARE(p.entries.size(), 2);
        QCOMPARE(p.entries.at(0).odometerM, 12'345'000);
        QCOMPARE(p.entries.at(0).quantity, 40'500);
        QCOMPARE(p.entries.at(0).totalCostC, 7'290);
        QVERIFY(!p.entries.at(1).fullTank);
    }

    /** Numbers beyond the limits and impossible years are rejected in the preview, so preview and import agree. */
    void absurdValuesAreRejected()
    {
        CsvFuelImporter importer(*m_db);
        importer.load(QStringLiteral("date,odometer,quantity,total\n"
                                     "2026-01-01,1000,40,70\n"
                                     "2026-01-02,1e300,40,70\n"
                                     "2026-01-03,99999999999999999999,40,70\n"
                                     "2026-01-04,1100,40,99999999999999999999\n"
                                     "0001-01-01,1200,40,70\n"));
        const CsvFuelMapping m = importer.suggestMapping();
        const CsvFuelPreview p = importer.preview(m);
        QCOMPARE(p.entries.size(), 1);
        QCOMPARE(p.errors.size(), 4);
        QVERIFY2(importer.importInto(m_vehicleId, m), qPrintable(importer.errorString()));
    }

    /** A negative header row (only reachable through the API) is clamped instead of reading before the first row. */
    void negativeHeaderRowIsClamped()
    {
        CsvFuelImporter importer(*m_db);
        importer.load(QStringLiteral("date,odometer,quantity,total\n2026-01-01,1000,40,70\n"));
        CsvFuelMapping m = importer.suggestMapping();
        m.headerRow = -5;
        const CsvFuelPreview p = importer.preview(m);
        QCOMPARE(p.entries.size(), 1);
    }

    /** Without the required columns the preview explains what is missing instead of guessing. */
    void incompleteMapping()
    {
        CsvFuelImporter importer(*m_db);
        importer.load(QStringLiteral("a,b\n1,2\n"));
        const CsvFuelPreview p = importer.preview(CsvFuelMapping());
        QVERIFY(p.entries.isEmpty());
        QCOMPARE(p.errors.size(), 1);
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Database> m_db;
    QString m_vehicleId;
};

QTEST_GUILESS_MAIN(TestCsvFuelImporter)
#include "test_csvfuelimporter.moc"
