// Fuel entries of one vehicle for QML (role: app).
// Wraps core::FuelRepository, core::FuelStats and the fuel price arithmetic
// in a QAbstractListModel for the fuel tab, and offers the load/save/check
// calls the fuel form needs. Exposed to QML as singleton "FuelEntries".

#pragma once

#include "fuelentry.h"
#include "fuelstats.h"

#include <QAbstractListModel>
#include <QList>
#include <QStringList>
#include <QVariantMap>

#include <optional>

namespace garagelog::core {
class Database;
}

namespace garagelog::app {

/**
 * @brief List model of the fuel entries of the vehicle in vehicleId, newest first.
 *
 * Every change reloads the whole list, because one new entry can change
 * the consumption of the next full tank. Quantities are ml or Wh (see
 * electric), money in cents; Format turns them into text. Errors are
 * reported through errorString instead of being swallowed (SPEC §3).
 */
class FuelListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString vehicleId READ vehicleId WRITE setVehicleId NOTIFY vehicleIdChanged)
    Q_PROPERTY(bool electric READ electric NOTIFY electricChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        DateRole,               ///< ISO text "YYYY-MM-DD"
        OdometerRole,           ///< metres (qint64)
        QuantityRole,           ///< ml or Wh (qint64)
        TotalCostRole,          ///< cents (qint64)
        UnitPriceRole,          ///< thousandths of the currency per l or kWh (qint64)
        FullTankRole,           ///< bool
        StationRole,            ///< text, may be empty
        NotesRole,              ///< text, may be empty
        HasConsumptionRole,     ///< bool: false for partial fills, the first full tank, odometer typos
        ConsumptionRole,        ///< ml or Wh per 100 km (double), 0 without consumption
        CostPer100KmRole,       ///< cents per 100 km (double), 0 without consumption
    };
    Q_ENUM(Role)

    /** @param database Must be open and outlive the model. */
    explicit FuelListModel(const core::Database &database, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString vehicleId() const;
    /** Shows the entries of another vehicle; "" empties the list. */
    void setVehicleId(const QString &id);
    /** true if the vehicle charges (quantities in Wh/kWh), false for fuel (ml/l). */
    bool electric() const;
    QString errorString() const;

    /** Re-reads the vehicle's powertrain and all its entries from the database. */
    Q_INVOKABLE void reload();

    /**
     * @brief One entry as map for the form.
     * @return Keys id, date (ISO), odometerM, quantity, totalCostC,
     *         fullTank, station, notes. Empty map if the id is unknown.
     */
    Q_INVOKABLE QVariantMap get(const QString &id);

    /**
     * @brief Creates (empty "id") or updates an entry of the current vehicle.
     * @param values Same keys as get(); date as ISO text, odometerM in
     *        metres, quantity in ml/Wh, totalCostC in cents. Missing or
     *        null numbers are refused.
     * @return Id of the saved entry, empty on failure (errorString set).
     */
    Q_INVOKABLE QString save(const QVariantMap &values);

    /** Deletes an entry permanently. @return false on failure (errorString set). */
    Q_INVOKABLE bool remove(const QString &id);

    /** Stations used before for this vehicle, most recent first (for autocompletion). */
    Q_INVOKABLE QStringList stationSuggestions();

    /**
     * @brief Plausibility of a new or edited entry; the result only warns, saving is still allowed.
     *
     * Compares against the other entries of the current vehicle:
     * - lowerThanPrevious: an entry on the same or an earlier date has a
     *   higher odometer (previousOdometerM = the highest such reading),
     * - duplicate: an entry with the same date and odometer exists.
     *
     * @param date ISO text.
     * @param odometerM Metres, or null while the field is empty.
     * @param excludeId Entry being edited, so it is not compared with itself.
     * @return Keys lowerThanPrevious (bool), previousOdometerM (metres),
     *         duplicate (bool); all false for an invalid date or null odometer.
     */
    Q_INVOKABLE QVariantMap plausibility(const QString &date, const QVariant &odometerM, const QString &excludeId) const;

    /** core::unitPriceMilli() for QML; null instead of nullopt. */
    Q_INVOKABLE QVariant unitPriceFor(qint64 totalCostC, qint64 quantity) const;
    /** core::totalCostFor() for QML; null instead of nullopt. */
    Q_INVOKABLE QVariant totalCostFor(qint64 quantity, qint64 unitPriceMilli) const;
    /** core::quantityFor() for QML; null instead of nullopt. */
    Q_INVOKABLE QVariant quantityFor(qint64 totalCostC, qint64 unitPriceMilli) const;

Q_SIGNALS:
    void vehicleIdChanged();
    void electricChanged();
    void countChanged();
    void errorStringChanged();
    /** Emitted after save() or remove() succeeded; the vehicle's odometer may have changed. */
    void entriesChanged();

private:
    /** An entry plus its derived consumption, cached for data(). */
    struct Row {
        core::FuelEntry entry;
        std::optional<core::Consumption> consumption;
    };

    void setErrorString(const QString &error);

    const core::Database &m_database;
    QString m_vehicleId;
    QList<Row> m_rows; ///< newest first
    bool m_electric = false;
    QString m_error;
};

} // namespace garagelog::app
