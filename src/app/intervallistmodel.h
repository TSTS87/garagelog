// Maintenance intervals with their due status for QML (role: app).
// Wraps core::IntervalRepository and core::DueCalculator. One instance per
// vehicle feeds the intervals tab ("Intervals"), a second one across all
// vehicles feeds the due overview ("DueIntervals").

#pragma once

#include "vehiclescopedlistmodel.h"

#include "duecalculator.h"
#include "maintenanceinterval.h"

#include <QHash>
#include <QList>
#include <QVariantMap>

namespace garagelog::core {
class Database;
}

namespace garagelog::app {

/**
 * @brief List model of maintenance intervals sorted by urgency (DueCalculator::moreUrgent).
 *
 * Scope::Vehicle lists all intervals of vehicleId (paused ones last);
 * Scope::AllVehicles lists the active intervals of all non-archived
 * vehicles. The due status is evaluated against today's date on every
 * reload(), so it follows new service and fuel entries once the vehicle
 * model reloads. Distances in metres, dates as ISO text.
 */
class IntervalListModel : public VehicleScopedListModel
{
    Q_OBJECT
    /** Most urgent status of the active intervals: "overdue", "soon", "ok", "unknown", or "" without intervals. */
    Q_PROPERTY(QString worstStatus READ worstStatus NOTIFY statusChanged)
    /** Number of active intervals that are overdue (badge in the drawer). */
    Q_PROPERTY(int overdueCount READ overdueCount NOTIFY statusChanged)
    /** Most urgent active interval with a known status as map (keys like get() plus the due roles), empty if none. */
    Q_PROPERTY(QVariantMap nextDue READ nextDue NOTIFY statusChanged)

public:
    enum class Scope {
        Vehicle,     ///< intervals of vehicleId
        AllVehicles, ///< active intervals of every non-archived vehicle
    };

    enum Role {
        IdRole = Qt::UserRole + 1,
        VehicleIdRole,
        VehicleNameRole,
        NameRole,
        CategoryIdRole,
        EveryMRole,          ///< metres (qint64) or null
        EveryMonthsRole,     ///< int or null
        ActiveRole,          ///< bool
        StatusRole,          ///< "overdue", "soon", "ok", "unknown"; "paused" for inactive intervals
        LastDateRole,        ///< ISO text or "" without a last service
        DueOdometerRole,     ///< metres or null
        DueDateRole,         ///< ISO text or ""
        RemainingMRole,      ///< metres (negative = overdue) or null
        RemainingDaysRole,   ///< days (negative = overdue) or null
    };
    Q_ENUM(Role)

    /** @param database Must be open and outlive the model. */
    explicit IntervalListModel(const core::Database &database, Scope scope, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString worstStatus() const;
    int overdueCount() const;
    QVariantMap nextDue() const;

    /** Re-reads intervals and re-evaluates them for today's date. */
    Q_INVOKABLE void reload() override;

    /**
     * @brief One interval as map for the form.
     * @return Keys id, vehicleId, categoryId, name, everyM (metres or null),
     *         everyMonths (int or null), active, notes. Empty if unknown.
     */
    Q_INVOKABLE QVariantMap get(const QString &id);

    /**
     * @brief Creates (empty "id") or updates an interval of the current vehicle.
     * @param values Same keys as get(); a blank name is replaced by the
     *        display name of the category.
     * @return Id of the saved interval, empty on failure (errorString set).
     */
    Q_INVOKABLE QString save(const QVariantMap &values);

    /** Deletes an interval. @return false on failure (errorString set). */
    Q_INVOKABLE bool remove(const QString &id);

Q_SIGNALS:
    void statusChanged();
    /** Emitted after save() or remove(); the other interval model reloads on it. */
    void intervalsChanged();

private:
    /** An interval plus its due evaluation and vehicle name, cached for data(). */
    struct Row {
        core::MaintenanceInterval interval;
        core::DueResult due;
        QString vehicleName;
    };

    /** Map with the keys of get() plus status and due values, for nextDue. */
    /** Status text for QML: the due status, or "paused" for an inactive interval. */
    static QString statusName(const Row &row);
    QVariantMap toMap(const Row &row) const;
    const Scope m_scope;
    QList<Row> m_rows;
};

} // namespace garagelog::app
