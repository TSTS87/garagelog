// Implementation of the export/import actions (role: app).

#include "datatransfer.h"

#include "csvexporter.h"
#include "garagecontroller.h"
#include "jsonexporter.h"
#include "jsonimporter.h"

#include <QDate>
#include <QDir>

#include <KLocalizedString>

namespace garagelog::app {

DataTransfer::DataTransfer(const core::Database &database, QObject *parent)
    : QObject(parent)
    , m_database(database)
{
}

QString DataTransfer::errorString() const
{
    return m_error;
}

QStringList DataTransfer::warnings() const
{
    return m_warnings;
}

QString DataTransfer::lastPath() const
{
    return m_lastPath;
}

void DataTransfer::setResult(const QString &error, const QStringList &warnings, const QString &path)
{
    m_error = error;
    m_warnings = warnings;
    m_lastPath = path;
    Q_EMIT resultChanged();
}

QString DataTransfer::newSubfolder(const QString &folder, const QString &base)
{
    const QDir parent(localPath(folder));
    const QString stem = QStringLiteral("%1-%2").arg(base, QDate::currentDate().toString(Qt::ISODate));
    QString name = stem;
    for (int n = 2; parent.exists(name); ++n) {
        name = QStringLiteral("%1-%2").arg(stem).arg(n);
    }
    if (!parent.mkpath(name)) {
        setResult(i18n("Cannot create %1", parent.filePath(name)));
        return {};
    }
    return parent.filePath(name);
}

bool DataTransfer::exportJson(const QString &folder)
{
    const QString target = newSubfolder(folder, QStringLiteral("garagelog-export"));
    if (target.isEmpty()) {
        return false;
    }
    core::JsonExporter exporter(m_database);
    if (!exporter.exportTo(target)) {
        setResult(exporter.errorString());
        return false;
    }
    setResult({}, exporter.warnings(), target);
    return true;
}

bool DataTransfer::importJson(const QString &folder)
{
    core::JsonImporter importer(m_database);
    if (!importer.importFrom(localPath(folder))) {
        setResult(importer.errorString());
        return false;
    }
    setResult({}, importer.warnings(), localPath(folder));
    Q_EMIT dataImported();
    return true;
}

bool DataTransfer::exportCsv(const QString &folder)
{
    const QString target = newSubfolder(folder, QStringLiteral("garagelog-csv"));
    if (target.isEmpty()) {
        return false;
    }
    core::CsvExporter exporter(m_database);
    if (!exporter.exportAll(target)) {
        setResult(exporter.errorString());
        return false;
    }
    setResult({}, {}, target);
    return true;
}

bool DataTransfer::loadCsv(const QString &file)
{
    auto importer = std::make_unique<core::CsvFuelImporter>(m_database);
    if (!importer->loadFile(localPath(file))) {
        setResult(importer->errorString());
        return false;
    }
    m_csv = std::move(importer);
    setResult({});
    return true;
}

QStringList DataTransfer::csvHeader(int headerRow) const
{
    if (!m_csv) {
        return {};
    }
    core::CsvFuelMapping m;
    m.headerRow = headerRow;
    return m_csv->header(m);
}

QVariantMap DataTransfer::suggestCsvMapping() const
{
    return m_csv ? fromMapping(m_csv->suggestMapping()) : QVariantMap();
}

QVariantMap DataTransfer::previewCsv(const QVariantMap &mapping) const
{
    if (!m_csv) {
        return {};
    }
    const core::CsvFuelPreview p = m_csv->preview(toMapping(mapping));
    QVariantList entries;
    for (qsizetype i = 0; i < p.entries.size() && i < previewLimit; ++i) {
        const core::FuelEntry &e = p.entries.at(i);
        entries.append(QVariantMap{
            {QStringLiteral("line"), p.lines.at(i)},
            {QStringLiteral("date"), e.date.toString(Qt::ISODate)},
            {QStringLiteral("odometerM"), QVariant::fromValue(e.odometerM)},
            {QStringLiteral("quantity"), QVariant::fromValue(e.quantity)},
            {QStringLiteral("totalCostC"), QVariant::fromValue(e.totalCostC)},
            {QStringLiteral("fullTank"), e.fullTank},
            {QStringLiteral("station"), e.station},
        });
    }
    return {
        {QStringLiteral("count"), static_cast<int>(p.entries.size())},
        {QStringLiteral("entries"), entries},
        {QStringLiteral("errors"), p.errors},
    };
}

int DataTransfer::importCsv(const QString &vehicleId, const QVariantMap &mapping)
{
    if (!m_csv) {
        setResult(i18n("No CSV file loaded"));
        return -1;
    }
    const int count = m_csv->importInto(vehicleId, toMapping(mapping));
    if (count < 0) {
        setResult(m_csv->errorString());
        return -1;
    }
    setResult({});
    Q_EMIT dataImported();
    return count;
}

core::CsvFuelMapping DataTransfer::toMapping(const QVariantMap &map)
{
    core::CsvFuelMapping m;
    auto column = [&](const char *key, int fallback) {
        return map.value(QLatin1String(key), fallback).toInt();
    };
    m.headerRow = column("headerRow", 0);
    m.dateColumn = column("dateColumn", -1);
    m.odometerColumn = column("odometerColumn", -1);
    m.quantityColumn = column("quantityColumn", -1);
    m.totalCostColumn = column("totalCostColumn", -1);
    m.unitPriceColumn = column("unitPriceColumn", -1);
    m.fullTankColumn = column("fullTankColumn", -1);
    m.stationColumn = column("stationColumn", -1);
    m.notesColumn = column("notesColumn", -1);
    m.dateFormat = map.value(QStringLiteral("dateFormat"), m.dateFormat).toString();
    const QString separator = map.value(QStringLiteral("decimalSeparator"), QStringLiteral(".")).toString();
    m.decimalSeparator = separator == QLatin1String(",") ? QLatin1Char(',') : QLatin1Char('.');
    return m;
}

QVariantMap DataTransfer::fromMapping(const core::CsvFuelMapping &m)
{
    return {
        {QStringLiteral("headerRow"), m.headerRow},
        {QStringLiteral("dateColumn"), m.dateColumn},
        {QStringLiteral("odometerColumn"), m.odometerColumn},
        {QStringLiteral("quantityColumn"), m.quantityColumn},
        {QStringLiteral("totalCostColumn"), m.totalCostColumn},
        {QStringLiteral("unitPriceColumn"), m.unitPriceColumn},
        {QStringLiteral("fullTankColumn"), m.fullTankColumn},
        {QStringLiteral("stationColumn"), m.stationColumn},
        {QStringLiteral("notesColumn"), m.notesColumn},
        {QStringLiteral("dateFormat"), m.dateFormat},
        {QStringLiteral("decimalSeparator"), QString(m.decimalSeparator)},
    };
}

} // namespace garagelog::app
