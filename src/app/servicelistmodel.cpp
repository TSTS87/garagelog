// Implementation of the service entry list model (role: app).

#include "servicelistmodel.h"

#include "attachmentstore.h"
#include "categoryrepository.h"
#include "garagecontroller.h"
#include "servicerepository.h"
#include "variantutil.h"

#include <KLocalizedString>

#include <QUrl>

#include <algorithm>

namespace garagelog::app {

using core::AttachmentStore;
using core::CategoryRepository;
using core::ServiceCategory;
using core::ServiceEntry;
using core::ServiceRepository;

ServiceListModel::ServiceListModel(const core::Database &database, QObject *parent)
    : VehicleScopedListModel(database, parent)
{
}

QString ServiceListModel::categoryLabel(const ServiceCategory &category)
{
    if (!category.builtin) {
        return category.name;
    }
    // Keys as seeded in migration 0001; i18nc context keeps them apart from
    // equal words elsewhere in the UI for translators.
    static const QHash<QString, KLocalizedString> labels{
        {QStringLiteral("oil_change"), ki18nc("service category", "Oil change")},
        {QStringLiteral("tires"), ki18nc("service category", "Tires")},
        {QStringLiteral("brakes"), ki18nc("service category", "Brakes")},
        {QStringLiteral("chain"), ki18nc("service category", "Chain")},
        {QStringLiteral("inspection"), ki18nc("service category", "Inspection")},
        {QStringLiteral("mot"), ki18nc("service category, statutory test (TÜV/HU)", "MOT / roadworthiness test")},
        {QStringLiteral("insurance"), ki18nc("service category", "Insurance")},
        {QStringLiteral("tax"), ki18nc("service category", "Vehicle tax")},
        {QStringLiteral("repair"), ki18nc("service category", "Repair")},
        {QStringLiteral("battery"), ki18nc("service category", "Battery")},
        {QStringLiteral("coolant"), ki18nc("service category", "Coolant")},
        {QStringLiteral("filters"), ki18nc("service category", "Filters")},
        {QStringLiteral("other"), ki18nc("service category", "Other")},
    };
    const auto it = labels.constFind(category.name);
    return it == labels.cend() ? category.name : it->toString();
}

int ServiceListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant ServiceListModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }
    const ServiceEntry &e = m_entries.at(index.row());
    switch (role) {
    case IdRole:
        return e.id;
    case DateRole:
        return e.date.toString(Qt::ISODate);
    case MonthRole:
        return e.date.toString(QStringLiteral("yyyy-MM"));
    case OdometerRole:
        return optionalToVariant(e.odometerM);
    case CategoryIdRole:
        return e.categoryId;
    case CategoryNameRole: {
        const auto it = m_categories.constFind(e.categoryId);
        return it == m_categories.cend() ? QString() : categoryLabel(*it);
    }
    case DescriptionRole:
        return e.description;
    case CostRole:
        return QVariant::fromValue(e.costC);
    case PerformedByRole:
        return core::toDbString(e.performedBy);
    case ShopNameRole:
        return e.shopName;
    case NotesRole:
        return e.notes;
    case HasAttachmentRole:
        return m_withAttachments.contains(e.id);
    }
    return {};
}

QHash<int, QByteArray> ServiceListModel::roleNames() const
{
    return {
        {IdRole, "entryId"},
        {DateRole, "date"},
        {OdometerRole, "odometerM"},
        {CategoryIdRole, "categoryId"},
        {CategoryNameRole, "categoryName"},
        {DescriptionRole, "description"},
        {CostRole, "costC"},
        {PerformedByRole, "performedBy"},
        {ShopNameRole, "shopName"},
        {NotesRole, "notes"},
        {HasAttachmentRole, "hasAttachment"},
        {MonthRole, "month"},
    };
}

void ServiceListModel::reload()
{
    const int oldCount = rowCount();
    QList<ServiceEntry> entries;
    QHash<QString, ServiceCategory> categories;
    QSet<QString> withAttachments;
    QString error;

    CategoryRepository categoryRepo(m_database);
    for (const ServiceCategory &c : categoryRepo.all()) {
        categories.insert(c.id, c);
    }
    error = categoryRepo.errorString();

    if (!vehicleId().isEmpty()) {
        ServiceRepository repo(m_database);
        entries = repo.byVehicle(vehicleId());
        std::reverse(entries.begin(), entries.end());
        AttachmentStore store(m_database);
        withAttachments = store.entriesWithAttachments(vehicleId());
        // Keep the first problem; later ones are usually follow-up errors.
        for (const QString &e : {repo.errorString(), store.errorString()}) {
            if (error.isEmpty()) {
                error = e;
            }
        }
    }

    beginResetModel();
    m_entries = std::move(entries);
    m_categories = std::move(categories);
    m_withAttachments = std::move(withAttachments);
    endResetModel();
    setErrorString(error);
    emitCountIfChanged(oldCount);
}

QVariantMap ServiceListModel::get(const QString &id)
{
    ServiceRepository repo(m_database);
    const auto e = repo.byId(id);
    if (!e) {
        return {};
    }
    return {
        {QStringLiteral("id"), e->id},
        {QStringLiteral("date"), e->date.toString(Qt::ISODate)},
        {QStringLiteral("odometerM"), optionalToVariant(e->odometerM)},
        {QStringLiteral("categoryId"), e->categoryId},
        {QStringLiteral("description"), e->description},
        {QStringLiteral("costC"), QVariant::fromValue(e->costC)},
        {QStringLiteral("performedBy"), core::toDbString(e->performedBy)},
        {QStringLiteral("shopName"), e->shopName},
        {QStringLiteral("notes"), e->notes},
    };
}

QString ServiceListModel::save(const QVariantMap &values)
{
    ServiceRepository repo(m_database);
    const QString id = values.value(QStringLiteral("id")).toString();

    // Start from the stored row on edit, so vehicle and timestamps stay.
    ServiceEntry e;
    if (id.isEmpty()) {
        e.vehicleId = vehicleId();
    } else {
        const auto existing = repo.byId(id);
        if (!existing) {
            setErrorString(repo.errorString());
            return {};
        }
        e = *existing;
    }

    const QVariant odometerM = values.value(QStringLiteral("odometerM"));
    const QVariant costC = values.value(QStringLiteral("costC"));
    e.date = QDate::fromString(values.value(QStringLiteral("date")).toString(), Qt::ISODate);
    e.odometerM = odometerM.isNull() ? std::nullopt : std::optional<qint64>(odometerM.toLongLong());
    e.categoryId = values.value(QStringLiteral("categoryId")).toString();
    e.description = values.value(QStringLiteral("description")).toString();
    e.costC = costC.isNull() ? 0 : costC.toLongLong();
    e.performedBy = core::performedByFromDb(values.value(QStringLiteral("performedBy")).toString()).value_or(core::PerformedBy::Unknown);
    // A shop name only makes sense for work done in a shop; drop leftovers
    // from switching the selection back and forth.
    e.shopName = e.performedBy == core::PerformedBy::Shop ? values.value(QStringLiteral("shopName")).toString() : QString();
    e.notes = values.value(QStringLiteral("notes")).toString();

    const bool ok = id.isEmpty() ? repo.create(e) : repo.update(e);
    if (!ok) {
        setErrorString(repo.errorString());
        return {};
    }

    QString error;
    AttachmentStore store(m_database);
    for (const QVariant &file : values.value(QStringLiteral("newAttachments")).toList()) {
        if (!store.add(e.id, localPath(file.toString()))) {
            error = store.errorString();
        }
    }
    reload();
    setErrorString(error);
    Q_EMIT entriesChanged();
    return e.id;
}

bool ServiceListModel::remove(const QString &id)
{
    ServiceRepository repo(m_database);
    const bool ok = repo.remove(id);
    if (ok) {
        reload();
        Q_EMIT entriesChanged();
    }
    // Set after reload(), which would otherwise clear an orphan-file note.
    setErrorString(repo.errorString());
    return ok;
}

QVariantList ServiceListModel::categories() const
{
    CategoryRepository repo(m_database);
    QVariantList result;
    for (const ServiceCategory &c : repo.all()) {
        result.append(QVariantMap{
            {QStringLiteral("id"), c.id},
            {QStringLiteral("name"), categoryLabel(c)},
            {QStringLiteral("builtin"), c.builtin},
        });
    }
    return result;
}

QString ServiceListModel::addCategory(const QString &name)
{
    CategoryRepository repo(m_database);
    ServiceCategory c;
    c.name = name;
    if (!repo.create(c)) {
        setErrorString(repo.errorString());
        return {};
    }
    setErrorString({});
    reload();
    return c.id;
}

QVariantList ServiceListModel::attachments(const QString &entryId)
{
    AttachmentStore store(m_database);
    QVariantList result;
    for (const core::Attachment &a : store.byServiceEntry(entryId)) {
        // "" for a stored name that would leave attachments/: nothing to open.
        const QString path = store.filePath(a);
        result.append(QVariantMap{
            {QStringLiteral("id"), a.id},
            {QStringLiteral("filename"), a.filename},
            {QStringLiteral("url"), path.isEmpty() ? QString() : QUrl::fromLocalFile(path).toString()},
            {QStringLiteral("sizeBytes"), QVariant::fromValue(a.sizeBytes)},
            {QStringLiteral("mime"), a.mime},
        });
    }
    if (!store.errorString().isEmpty()) {
        setErrorString(store.errorString());
    }
    return result;
}

bool ServiceListModel::removeAttachment(const QString &id)
{
    AttachmentStore store(m_database);
    const bool ok = store.remove(id);
    if (ok) {
        reload();
        Q_EMIT entriesChanged();
    }
    setErrorString(store.errorString());
    return ok;
}

} // namespace garagelog::app
