// Implementation of the fuel entry list model (role: app).

#include "fuellistmodel.h"

#include "fuelprice.h"
#include "fuelrepository.h"
#include "vehiclerepository.h"

#include <algorithm>

#include <KLocalizedString>

namespace garagelog::app {

using core::FuelEntry;
using core::FuelRepository;
using core::FuelStats;

namespace {

/** nullopt becomes QVariant(nullptr), which arrives in JS as null. */
QVariant toVariant(const std::optional<qint64> &value)
{
    return value ? QVariant::fromValue(*value) : QVariant::fromValue(nullptr);
}

} // namespace

FuelListModel::FuelListModel(const core::Database &database, QObject *parent)
    : QAbstractListModel(parent)
    , m_database(database)
{
}

int FuelListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant FuelListModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }
    const Row &row = m_rows.at(index.row());
    const FuelEntry &e = row.entry;
    switch (role) {
    case IdRole:
        return e.id;
    case DateRole:
        return e.date.toString(Qt::ISODate);
    case OdometerRole:
        return QVariant::fromValue(e.odometerM);
    case QuantityRole:
        return QVariant::fromValue(e.quantity);
    case TotalCostRole:
        return QVariant::fromValue(e.totalCostC);
    case UnitPriceRole:
        // Stored quantities are > 0, so this is never null for a saved entry.
        return toVariant(core::unitPriceMilli(e.totalCostC, e.quantity));
    case FullTankRole:
        return e.fullTank;
    case StationRole:
        return e.station;
    case NotesRole:
        return e.notes;
    case HasConsumptionRole:
        return row.consumption.has_value();
    case ConsumptionRole:
        return row.consumption ? row.consumption->quantityPer100Km : 0.0;
    case CostPer100KmRole:
        return row.consumption ? row.consumption->costPer100KmC : 0.0;
    }
    return {};
}

QHash<int, QByteArray> FuelListModel::roleNames() const
{
    return {
        {IdRole, "entryId"},
        {DateRole, "date"},
        {OdometerRole, "odometerM"},
        {QuantityRole, "quantity"},
        {TotalCostRole, "totalCostC"},
        {UnitPriceRole, "unitPriceMilli"},
        {FullTankRole, "fullTank"},
        {StationRole, "station"},
        {NotesRole, "notes"},
        {HasConsumptionRole, "hasConsumption"},
        {ConsumptionRole, "consumptionPer100Km"},
        {CostPer100KmRole, "costPer100KmC"},
    };
}

QString FuelListModel::vehicleId() const
{
    return m_vehicleId;
}

void FuelListModel::setVehicleId(const QString &id)
{
    if (m_vehicleId == id) {
        return;
    }
    m_vehicleId = id;
    Q_EMIT vehicleIdChanged();
    reload();
}

bool FuelListModel::electric() const
{
    return m_electric;
}

QString FuelListModel::errorString() const
{
    return m_error;
}

void FuelListModel::reload()
{
    const int oldCount = rowCount();
    QList<Row> rows;
    bool electric = false;
    QString error;

    if (!m_vehicleId.isEmpty()) {
        core::VehicleRepository vehicles(m_database);
        if (const auto vehicle = vehicles.byId(m_vehicleId)) {
            electric = core::quantityUnitFor(vehicle->powertrain) == core::QuantityUnit::WattHour;
        }
        FuelRepository repo(m_database);
        const QList<FuelEntry> entries = repo.byVehicle(m_vehicleId);
        error = repo.errorString();
        // consumption() needs chronological order; the list shows newest first.
        const auto consumption = FuelStats::consumption(entries);
        rows.reserve(entries.size());
        for (qsizetype i = entries.size() - 1; i >= 0; --i) {
            rows.append({entries.at(i), consumption.at(i)});
        }
    }

    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
    setErrorString(error);
    if (m_electric != electric) {
        m_electric = electric;
        Q_EMIT electricChanged();
    }
    if (rowCount() != oldCount) {
        Q_EMIT countChanged();
    }
}

QVariantMap FuelListModel::get(const QString &id)
{
    FuelRepository repo(m_database);
    const auto e = repo.byId(id);
    if (!e) {
        return {};
    }
    return {
        {QStringLiteral("id"), e->id},
        {QStringLiteral("date"), e->date.toString(Qt::ISODate)},
        {QStringLiteral("odometerM"), QVariant::fromValue(e->odometerM)},
        {QStringLiteral("quantity"), QVariant::fromValue(e->quantity)},
        {QStringLiteral("totalCostC"), QVariant::fromValue(e->totalCostC)},
        {QStringLiteral("fullTank"), e->fullTank},
        {QStringLiteral("station"), e->station},
        {QStringLiteral("notes"), e->notes},
    };
}

QString FuelListModel::save(const QVariantMap &values)
{
    FuelRepository repo(m_database);
    const QString id = values.value(QStringLiteral("id")).toString();

    // Start from the stored row on edit, so vehicle and timestamps stay.
    FuelEntry e;
    if (id.isEmpty()) {
        e.vehicleId = m_vehicleId;
    } else {
        const auto existing = repo.byId(id);
        if (!existing) {
            setErrorString(repo.errorString().isEmpty() ? i18n("Fuel entry not found") : repo.errorString());
            return {};
        }
        e = *existing;
    }

    // null from a field that did not parse would silently become 0 with toLongLong().
    const QVariant odometerM = values.value(QStringLiteral("odometerM"));
    const QVariant quantity = values.value(QStringLiteral("quantity"));
    const QVariant totalCostC = values.value(QStringLiteral("totalCostC"));
    if (odometerM.isNull() || quantity.isNull() || totalCostC.isNull()) {
        setErrorString(i18n("Odometer, quantity and total price are required"));
        return {};
    }

    e.date = QDate::fromString(values.value(QStringLiteral("date")).toString(), Qt::ISODate);
    e.odometerM = odometerM.toLongLong();
    e.quantity = quantity.toLongLong();
    e.totalCostC = totalCostC.toLongLong();
    e.fullTank = values.value(QStringLiteral("fullTank"), true).toBool();
    e.station = values.value(QStringLiteral("station")).toString().trimmed();
    e.notes = values.value(QStringLiteral("notes")).toString().trimmed();

    const bool ok = id.isEmpty() ? repo.create(e) : repo.update(e);
    if (!ok) {
        setErrorString(repo.errorString());
        return {};
    }
    setErrorString({});
    reload();
    Q_EMIT entriesChanged();
    return e.id;
}

bool FuelListModel::remove(const QString &id)
{
    FuelRepository repo(m_database);
    const bool ok = repo.remove(id);
    setErrorString(repo.errorString());
    if (ok) {
        reload();
        Q_EMIT entriesChanged();
    }
    return ok;
}

QStringList FuelListModel::stationSuggestions()
{
    FuelStats stats(m_database);
    const QStringList result = stats.stationSuggestions(m_vehicleId);
    if (!stats.errorString().isEmpty()) {
        setErrorString(stats.errorString());
    }
    return result;
}

QVariantMap FuelListModel::plausibility(const QString &date, const QVariant &odometerM, const QString &excludeId) const
{
    bool lowerThanPrevious = false;
    bool duplicate = false;
    qint64 previousOdometerM = 0;

    const QDate d = QDate::fromString(date, Qt::ISODate);
    if (d.isValid() && !odometerM.isNull()) {
        const qint64 odo = odometerM.toLongLong();
        for (const Row &row : m_rows) {
            const FuelEntry &e = row.entry;
            // Later entries are not "previous"; a too high value there is not
            // part of the SPEC check.
            if (e.id == excludeId || e.date > d) {
                continue;
            }
            previousOdometerM = std::max(previousOdometerM, e.odometerM);
            duplicate = duplicate || (e.date == d && e.odometerM == odo);
        }
        lowerThanPrevious = odo < previousOdometerM;
    }

    return {
        {QStringLiteral("lowerThanPrevious"), lowerThanPrevious},
        {QStringLiteral("previousOdometerM"), QVariant::fromValue(previousOdometerM)},
        {QStringLiteral("duplicate"), duplicate},
    };
}

QVariant FuelListModel::unitPriceFor(qint64 totalCostC, qint64 quantity) const
{
    return toVariant(core::unitPriceMilli(totalCostC, quantity));
}

QVariant FuelListModel::totalCostFor(qint64 quantity, qint64 unitPriceMilli) const
{
    return toVariant(core::totalCostFor(quantity, unitPriceMilli));
}

QVariant FuelListModel::quantityFor(qint64 totalCostC, qint64 unitPriceMilli) const
{
    return toVariant(core::quantityFor(totalCostC, unitPriceMilli));
}

void FuelListModel::setErrorString(const QString &error)
{
    if (m_error != error) {
        m_error = error;
        Q_EMIT errorStringChanged();
    }
}

} // namespace garagelog::app
