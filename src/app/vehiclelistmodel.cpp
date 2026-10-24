// Implementation of the vehicle list model (role: app).

#include "vehiclelistmodel.h"

#include "odometer.h"
#include "variantutil.h"
#include "vehiclerepository.h"

#include <KLocalizedString>

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
    // Breeze only ships a "car" icon, so GarageLog brings its own line icons
    // for every type (ADR-015). They are single-colour masks: QML shows them
    // with Kirigami.Icon { isMask: true } in the colour it needs.
    static const QHash<QString, QString> icons{
        {QStringLiteral("car"), QStringLiteral("qrc:/qml/icons/vehicle-car.svg")},
        {QStringLiteral("motorcycle"), QStringLiteral("qrc:/qml/icons/vehicle-motorcycle.svg")},
        {QStringLiteral("scooter"), QStringLiteral("qrc:/qml/icons/vehicle-scooter.svg")},
        {QStringLiteral("ebike"), QStringLiteral("qrc:/qml/icons/vehicle-ebike.svg")},
        {QStringLiteral("other"), QStringLiteral("qrc:/qml/icons/vehicle-other.svg")},
    };
    return icons.value(type, QStringLiteral("qrc:/qml/icons/vehicle-other.svg"));
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
        // JS null, which the form shows as an empty field.
        {QStringLiteral("year"), optionalToVariant(v->year)},
        {QStringLiteral("plate"), v->plate},
        {QStringLiteral("powertrain"), core::toDbString(v->powertrain)},
        {QStringLiteral("notes"), v->notes},
        {QStringLiteral("archived"), v->archived},
        {QStringLiteral("icon"), iconForType(type)},
        {QStringLiteral("odometerM"), QVariant::fromValue(Odometer(m_database).current(v->id).value_or(0))},
        // A failed check counts as "has entries", so the form never offers a delete that would fail.
        {QStringLiteral("hasEntries"), repo.hasEntries(v->id).value_or(true)},
    };
}

QString VehicleListModel::save(const QVariantMap &values)
{
    VehicleRepository repo(m_database);
    const QString id = values.value(QStringLiteral("id")).toString();

    // Start from the stored row on edit, so fields the form does not show
    // (archived, odometer reading, timestamps) keep their values.
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
        setErrorString(i18n("Unknown vehicle type or powertrain"));
        return {};
    }

    v.name = values.value(QStringLiteral("name")).toString();
    v.type = *type;
    v.make = values.value(QStringLiteral("make")).toString();
    v.model = values.value(QStringLiteral("model")).toString();
    v.plate = values.value(QStringLiteral("plate")).toString();
    v.powertrain = *powertrain;
    v.notes = values.value(QStringLiteral("notes")).toString();

    // QML sends null, undefined or 0 for "no year"; only positive years count.
    const QVariant year = values.value(QStringLiteral("year"));
    v.year = (!year.isNull() && year.toInt() > 0) ? std::optional<int>(year.toInt()) : std::nullopt;

    const QVariant odometerM = values.value(QStringLiteral("odometerM"));
    // The typed odometer is a real reading (ADR-016): the vehicle shows it
    // until an entry goes beyond it.
    if (values.contains(QStringLiteral("odometerM")) && !odometerM.isNull()) {
        v.odometerReadingM = odometerM.toLongLong();
    }

    const bool ok = id.isEmpty() ? repo.create(v) : repo.update(v);
    if (!ok) {
        setErrorString(repo.errorString());
        return {};
    }
    setErrorString({});
    reload();
    Q_EMIT vehicleSaved();
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

bool VehicleListModel::remove(const QString &id)
{
    VehicleRepository repo(m_database);
    const bool ok = repo.remove(id);
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
