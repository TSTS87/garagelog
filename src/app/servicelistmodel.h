// Service entries of one vehicle for QML (role: app).
// Wraps core::ServiceRepository, core::CategoryRepository and
// core::AttachmentStore in a QAbstractListModel for the service tab, and
// offers the load/save calls of the service form. Exposed as singleton "ServiceEntries".

#pragma once

#include "serviceentry.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>

namespace garagelog::core {
class Database;
}

namespace garagelog::app {

/**
 * @brief List model of the service entries of the vehicle in vehicleId, newest first.
 *
 * Money is in cents, odometers in metres (null if the entry has none);
 * Format turns them into text. Category names of built-ins are translated
 * here (categoryLabel()), because the database stores only their keys.
 * Errors are reported through errorString (SPEC §3).
 */
class ServiceListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString vehicleId READ vehicleId WRITE setVehicleId NOTIFY vehicleIdChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        DateRole,           ///< ISO text "YYYY-MM-DD"
        OdometerRole,       ///< metres (qint64) or null
        CategoryIdRole,
        CategoryNameRole,   ///< translated display name
        DescriptionRole,
        CostRole,           ///< cents (qint64)
        PerformedByRole,    ///< "self", "shop" or "" (unknown)
        ShopNameRole,       ///< text, may be empty
        NotesRole,          ///< text, may be empty
        HasAttachmentRole,  ///< bool: at least one attached file
        MonthRole,          ///< ISO text "YYYY-MM", groups the list into month sections
    };
    Q_ENUM(Role)

    /** @param database Must be open and outlive the model. */
    explicit ServiceListModel(const core::Database &database, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString vehicleId() const;
    /** Shows the entries of another vehicle; "" empties the list. */
    void setVehicleId(const QString &id);
    QString errorString() const;

    /**
     * @brief Display name of a category: built-in keys translated (e.g. "mot" -> "MOT / inspection"), user names unchanged.
     */
    static QString categoryLabel(const core::ServiceCategory &category);

    /** Re-reads categories, entries and attachment flags from the database. */
    Q_INVOKABLE void reload();

    /**
     * @brief One entry as map for the form.
     * @return Keys id, date (ISO), odometerM (metres or null), categoryId,
     *         description, costC, performedBy ("self"/"shop"/""), shopName,
     *         notes. Empty map if the id is unknown.
     */
    Q_INVOKABLE QVariantMap get(const QString &id);

    /**
     * @brief Creates (empty "id") or updates an entry of the current vehicle.
     * @param values Same keys as get(); odometerM may be null, costC null
     *        counts as 0. Optional "newAttachments": list of file URLs or
     *        paths to copy into the garage after the entry is saved.
     * @return Id of the saved entry, empty if the entry could not be saved.
     *         If the entry was saved but an attachment failed, the id is
     *         returned and errorString says which file failed.
     */
    Q_INVOKABLE QString save(const QVariantMap &values);

    /** Deletes an entry and its attachment files. @return false on failure (errorString set). */
    Q_INVOKABLE bool remove(const QString &id);

    /** All categories for the picker: list of maps {id, name (translated), builtin}, built-ins first. */
    Q_INVOKABLE QVariantList categories() const;

    /** Adds a user category. @return Its id, empty on failure (errorString set, e.g. duplicate name). */
    Q_INVOKABLE QString addCategory(const QString &name);

    /** Attachments of an entry: list of maps {id, filename, url (file://), sizeBytes, mime}. */
    Q_INVOKABLE QVariantList attachments(const QString &entryId);

    /** Deletes one attachment (row and file). @return false on failure (errorString set). */
    Q_INVOKABLE bool removeAttachment(const QString &id);

Q_SIGNALS:
    void vehicleIdChanged();
    void countChanged();
    void errorStringChanged();
    /** Emitted after save()/remove()/attachment changes; the odometer may have changed. */
    void entriesChanged();

private:
    void setErrorString(const QString &error);

    const core::Database &m_database;
    QString m_vehicleId;
    QList<core::ServiceEntry> m_entries; ///< newest first
    QHash<QString, core::ServiceCategory> m_categories; ///< by id, refreshed in reload()
    QSet<QString> m_withAttachments;     ///< entry ids
    QString m_error;
};

} // namespace garagelog::app
