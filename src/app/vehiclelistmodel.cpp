// Implementation of the vehicle list model (role: app).

#include "vehiclelistmodel.h"

#include "odometer.h"
#include "vehiclerepository.h"

namespace garagelog::app {

using core::Odometer;
using core::Vehicle;
using core::VehicleRepository;

VehicleListModel::VehicleListModel(const core::Database &database, QObject *parent)
    : QAbstractListModel(parent)
    , m_database(database)
{
    reload();
}

int VehicleListModel::rowCount(const QModelIndex &parent) const
{
    // A list model has no children; Qt asks with a valid parent to find that out.
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant VehicleListModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }
    const Row &row = m_rows.at(index.row());
    switch (role) {
    case IdRole:
        return row.vehicle.id;
    case Qt::DisplayRole:
    case NameRole:
        return row.vehicle.name;
    case TypeRole:
        return core::toDbString(row.vehicle.type);
    case Qt::DecorationRole:
    case IconRole:
        return iconForType(core::toDbString(row.vehicle.type));
    case OdometerRole:
        return QVariant::fromValue(row.odometerM);
    case ArchivedRole:
        return row.vehicle.archived;
    }
    return {};
}

QHash<int, QByteArray> VehicleListModel::roleNames() const
{
    // These names are what QML delegates use as properties (model.name, ...).
    return {
        {IdRole, "vehicleId"},
        {NameRole, "name"},
        {TypeRole, "type"},
        {IconRole, "iconName"},
        {OdometerRole, "odometerM"},
        {ArchivedRole, "archived"},
    };
}

bool VehicleListModel::showArchived() const
{
    return m_showArchived;
}

void VehicleListModel::setShowArchived(bool show)
{
    if (m_showArchived == show) {
        return;
    }
    m_showArchived = show;
    Q_EMIT showArchivedChanged();
    reload();
}

QString VehicleListModel::errorString() const
{
    return m_error;
}

int VehicleListModel::archivedCount() const
{
    return m_archivedCount;
}

QString VehicleListModel::iconForType(const QString &type)
{
    // Breeze only ships a "car" icon; motorcycle, scooter and e-bike icons
    // do not exist there. Kept as a table so own icons can replace the
    // fallback per type later without touching QML.
    static const QHash<QString, QString> icons{
        {QStringLiteral("car"), QStringLiteral("car")},
        {QStringLiteral("motorcycle"), QStringLiteral("car")},
        {QStringLiteral("scooter"), QStringLiteral("car")},
        {QStringLiteral("ebike"), QStringLiteral("car")},
        {QStringLiteral("other"), QStringLiteral("car")},
    };
    return icons.value(type, QStringLiteral("car"));
}

void VehicleListModel::reload()
{
    VehicleRepository repo(m_database);
    Odometer odometer(m_database);
    const int oldCount = rowCount();

    QList<Row> rows;
    int archivedCount = 0;
    // Always read archived ones too: the empty state needs to know whether
    // the garage is really empty or only shows no archived vehicles.
    for (const Vehicle &v : repo.all(true)) {
        archivedCount += v.archived ? 1 : 0;
        if (v.archived && !m_showArchived) {
            continue;
        }
        // A failed odometer query shows 0 instead of hiding the vehicle.
        rows.append({v, odometer.current(v.id).value_or(0)});
    }
    setErrorString(repo.errorString());

    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
    if (rowCount() != oldCount) {
        Q_EMIT countChanged();
    }
    if (m_archivedCount != archivedCount) {
        m_archivedCount = archivedCount;
        Q_EMIT archivedCountChanged();
    }
    Q_EMIT reloaded();
}

QString VehicleListModel::idAt(int row) const
{
    return row >= 0 && row < rowCount() ? m_rows.at(row).vehicle.id : QString();
}

QVariantMap VehicleListModel::get(const QString &id)
{
    VehicleRepository repo(m_database);
    const auto v = repo.byId(id);
    if (!v) {
        return {};
    }
    const QString type = core::toDbString(v->type);
    return {
        {QStringLiteral("id"), v->id},
        {QStringLiteral("name"), v->name},
        {QStringLiteral("type"), type},
        {QStringLiteral("make"), v->make},
        {QStringLiteral("model"), v->model},
        // QVariant() arrives in QML as null, which the form shows as an empty field.
        {QStringLiteral("year"), v->year ? QVariant(*v->year) : QVariant()},
        {QStringLiteral("plate"), v->plate},
        {QStringLiteral("powertrain"), core::toDbString(v->powertrain)},
        {QStringLiteral("notes"), v->notes},
        {QStringLiteral("archived"), v->archived},
        {QStringLiteral("icon"), iconForType(type)},
        {QStringLiteral("odometerM"), QVariant::fromValue(Odometer(m_database).current(v->id).value_or(0))},
    };
}

QString VehicleListModel::save(const QVariantMap &values)
{
    VehicleRepository repo(m_database);
    const QString id = values.value(QStringLiteral("id")).toString();

    // Start from the stored row on edit, so fields the form does not show
    // (archived, offset, timestamps) keep their values.
    Vehicle v;
    if (!id.isEmpty()) {
        const auto existing = repo.byId(id);
        if (!existing) {
            setErrorString(repo.errorString());
            return {};
        }
        v = *existing;
    }

    const auto type = core::vehicleTypeFromDb(values.value(QStringLiteral("type")).toString());
    const auto powertrain = core::powertrainFromDb(values.value(QStringLiteral("powertrain")).toString());
    if (!type || !powertrain) {
        setErrorString(QStringLiteral("Unknown vehicle type or powertrain"));
        return {};
    }

    v.name = values.value(QStringLiteral("name")).toString();
    v.type = *type;
    v.make = values.value(QStringLiteral("make")).toString().trimmed();
    v.model = values.value(QStringLiteral("model")).toString().trimmed();
    v.plate = values.value(QStringLiteral("plate")).toString().trimmed();
    v.powertrain = *powertrain;
    v.notes = values.value(QStringLiteral("notes")).toString().trimmed();

    // QML sends null, undefined or 0 for "no year"; only positive years count.
    const QVariant year = values.value(QStringLiteral("year"));
    v.year = (!year.isNull() && year.toInt() > 0) ? std::optional<int>(year.toInt()) : std::nullopt;

    const QVariant odometerM = values.value(QStringLiteral("odometerM"));
    if (values.contains(QStringLiteral("odometerM")) && !odometerM.isNull()) {
        Odometer odometer(m_database);
        const auto offset = odometer.offsetFor(id, odometerM.toLongLong());
        if (!offset) {
            setErrorString(odometer.errorString());
            return {};
        }
        v.odometerOffsetM = *offset;
    }

    const bool ok = id.isEmpty() ? repo.create(v) : repo.update(v);
    if (!ok) {
        setErrorString(repo.errorString());
        return {};
    }
    setErrorString({});
    reload();
    return v.id;
}

bool VehicleListModel::setArchived(const QString &id, bool archived)
{
    VehicleRepository repo(m_database);
    const bool ok = archived ? repo.archive(id) : repo.unarchive(id);
    setErrorString(repo.errorString());
    if (ok) {
        reload();
    }
    return ok;
}

void VehicleListModel::setErrorString(const QString &error)
{
    if (m_error != error) {
        m_error = error;
        Q_EMIT errorStringChanged();
    }
}

} // namespace garagelog::app
