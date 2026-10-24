// Vehicle list for QML (role: app).
// Wraps core::VehicleRepository and core::Odometer in a QAbstractListModel
// that feeds the drawer list, and offers the load/save/archive calls the
// vehicle page and form need. Exposed to QML as singleton "Vehicles".

#pragma once

#include "vehicle.h"

#include <QAbstractListModel>
#include <QList>
#include <QVariantMap>

namespace garagelog::core {
class Database;
}

namespace garagelog::app {

/**
 * @brief List model of the garage's vehicles, sorted by name.
 *
 * Archived vehicles are only included while showArchived is true. Every
 * change reloads the whole list (a garage has a handful of vehicles, so
 * fine-grained updates are not worth the complexity). Errors are reported
 * through errorString instead of being swallowed (SPEC §3).
 */
class VehicleListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool showArchived READ showArchived WRITE setShowArchived NOTIFY showArchivedChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    /** Archived vehicles in the garage, also while they are hidden; for the empty state. */
    Q_PROPERTY(int archivedCount READ archivedCount NOTIFY archivedCountChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        NameRole,       ///< display name
        TypeRole,       ///< database text, e.g. "motorcycle"
        IconRole,       ///< freedesktop icon name for the type
        OdometerRole,   ///< current odometer in metres (qint64)
        ArchivedRole,   ///< bool
    };
    Q_ENUM(Role)

    /** @param database Must be open and outlive the model. */
    explicit VehicleListModel(const core::Database &database, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool showArchived() const;
    void setShowArchived(bool show);
    QString errorString() const;
    int archivedCount() const;

    /** qrc URL of GarageLog's own line icon for a vehicle type (database text); the van icon for unknown text. */
    static QString iconForType(const QString &type);

    /** Picker entries {value, text} of the vehicle types, in form order, with translated labels. */
    Q_INVOKABLE static QVariantList typeOptions();
    /** Picker entries {value, text} of the powertrains, in form order, with translated labels. */
    Q_INVOKABLE static QVariantList powertrainOptions();
    /** Translated label of a vehicle type (database text); "Other" for unknown text. */
    static QString typeLabel(const QString &type);

    /** Re-reads all vehicles and odometers from the database. */
    Q_INVOKABLE void reload();

    /** Id of the vehicle in @p row, empty if out of range. */
    Q_INVOKABLE QString idAt(int row) const;

    /**
     * @brief One vehicle as map for the page and the form, independent of showArchived.
     * @return Keys id, name, type, make, model, year (int or null), plate,
     *         powertrain, notes, archived, icon, typeLabel, odometerM (metres),
     *         hasEntries (true if it cannot be deleted). Empty map if the id is unknown.
     */
    Q_INVOKABLE QVariantMap get(const QString &id);

    /**
     * @brief Creates (empty "id") or updates a vehicle from form values.
     * @param values Same keys as get(); "odometerM" (metres) is optional:
     *        if present and not null, it is stored as the manual odometer
     *        reading (ADR-016); if absent, the stored reading is unchanged.
     * @return Id of the saved vehicle, empty on failure (errorString set).
     */
    Q_INVOKABLE QString save(const QVariantMap &values);

    /** Archives or restores a vehicle. @return false on failure (errorString set). */
    Q_INVOKABLE bool setArchived(const QString &id, bool archived);

    /**
     * @brief Deletes a vehicle without entries, together with its intervals (SPEC §4.1).
     * @return false if it has entries or the id is unknown (errorString set).
     */
    Q_INVOKABLE bool remove(const QString &id);

Q_SIGNALS:
    void showArchivedChanged();
    void countChanged();
    void archivedCountChanged();
    void errorStringChanged();
    /** Emitted after every reload(); pages showing one vehicle re-read it via get(). */
    void reloaded();
    /** A vehicle was created or edited; its powertrain may have switched the fuel unit (l <-> kWh). */
    void vehicleSaved();

private:
    /** A vehicle plus its derived odometer, cached for data(). */
    struct Row {
        core::Vehicle vehicle;
        qint64 odometerM = 0;
    };

    void setErrorString(const QString &error);

    const core::Database &m_database;
    QList<Row> m_rows;
    bool m_showArchived = false;
    int m_archivedCount = 0;
    QString m_error;
};

} // namespace garagelog::app
