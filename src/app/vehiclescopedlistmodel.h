// Common base of the list models that show the entries of one vehicle (role: app).
// Holds what fuel, service and interval lists share: the vehicleId QML sets,
// the count and errorString properties and the full-reload pattern.

#pragma once

#include <QAbstractListModel>
#include <QString>

namespace garagelog::core {
class Database;
}

namespace garagelog::app {

/**
 * @brief QAbstractListModel with vehicleId, count and errorString for QML.
 *
 * Setting vehicleId reloads the list. Subclasses implement reload() and
 * rowCount(); they wrap their reset in beginResetModel()/endResetModel()
 * and call emitCountIfChanged() afterwards.
 */
class VehicleScopedListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString vehicleId READ vehicleId WRITE setVehicleId NOTIFY vehicleIdChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

public:
    QString vehicleId() const;
    /** Shows the entries of @p id; "" empties the list (no vehicle shown). */
    void setVehicleId(const QString &id);
    int count() const;
    /** Reason for the last failed operation, "" after a successful one. */
    QString errorString() const;

    /** Re-reads the list from the database. */
    Q_INVOKABLE virtual void reload() = 0;

Q_SIGNALS:
    void vehicleIdChanged();
    void countChanged();
    void errorStringChanged();

protected:
    explicit VehicleScopedListModel(const core::Database &database, QObject *parent = nullptr);

    void setErrorString(const QString &error);
    /** Emits countChanged() if rowCount() differs from @p oldCount (taken before the reset). */
    void emitCountIfChanged(int oldCount);

    const core::Database &m_database;

private:
    QString m_vehicleId;
    QString m_error;
};

} // namespace garagelog::app
