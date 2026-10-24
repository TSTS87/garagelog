// Implementation of the vehicle-scoped list model base (role: app).

#include "vehiclescopedlistmodel.h"

namespace garagelog::app {

VehicleScopedListModel::VehicleScopedListModel(const core::Database &database, QObject *parent)
    : QAbstractListModel(parent)
    , m_database(database)
{
}

QString VehicleScopedListModel::vehicleId() const
{
    return m_vehicleId;
}

void VehicleScopedListModel::setVehicleId(const QString &id)
{
    if (m_vehicleId == id) {
        return;
    }
    m_vehicleId = id;
    Q_EMIT vehicleIdChanged();
    reload();
}

int VehicleScopedListModel::count() const
{
    return rowCount();
}

QString VehicleScopedListModel::errorString() const
{
    return m_error;
}

void VehicleScopedListModel::setErrorString(const QString &error)
{
    if (m_error != error) {
        m_error = error;
        Q_EMIT errorStringChanged();
    }
}

void VehicleScopedListModel::emitCountIfChanged(int oldCount)
{
    if (rowCount() != oldCount) {
        Q_EMIT countChanged();
    }
}

} // namespace garagelog::app
