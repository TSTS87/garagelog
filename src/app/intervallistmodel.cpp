// Implementation of the interval list model (role: app).

#include "intervallistmodel.h"

#include "categoryrepository.h"
#include "intervalrepository.h"
#include "servicelistmodel.h"
#include "vehiclerepository.h"

#include <QDate>

#include <algorithm>

namespace garagelog::app {

using core::DueCalculator;
using core::DueResult;
using core::DueStatus;
using core::IntervalRepository;
using core::MaintenanceInterval;

namespace {

template<typename T>
QVariant optionalToVariant(const std::optional<T> &value)
{
    return value ? QVariant::fromValue(*value) : QVariant::fromValue(nullptr);
}

QString isoOrEmpty(const std::optional<QDate> &date)
{
    return date ? date->toString(Qt::ISODate) : QString();
}

} // namespace

IntervalListModel::IntervalListModel(const core::Database &database, Scope scope, QObject *parent)
    : QAbstractListModel(parent)
    , m_database(database)
    , m_scope(scope)
{
    if (m_scope == Scope::AllVehicles) {
        reload();
    }
}

int IntervalListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant IntervalListModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }
    const Row &row = m_rows.at(index.row());
    const MaintenanceInterval &i = row.interval;
    const DueResult &d = row.due;
    switch (role) {
    case IdRole:
        return i.id;
    case VehicleIdRole:
        return i.vehicleId;
    case VehicleNameRole:
        return row.vehicleName;
    case NameRole:
        return i.name;
    case CategoryIdRole:
        return i.categoryId;
    case EveryMRole:
        return optionalToVariant(i.everyM);
    case EveryMonthsRole:
        return optionalToVariant(i.everyMonths);
    case ActiveRole:
        return i.active;
    case StatusRole:
        return i.active ? core::dueStatusName(d.status) : QStringLiteral("paused");
    case LastDateRole:
        return isoOrEmpty(d.lastDate);
    case DueOdometerRole:
        return optionalToVariant(d.dueOdometerM);
    case DueDateRole:
        return isoOrEmpty(d.dueDate);
    case RemainingMRole:
        return optionalToVariant(d.remainingM);
    case RemainingDaysRole:
        return optionalToVariant(d.remainingDays);
    }
    return {};
}

QHash<int, QByteArray> IntervalListModel::roleNames() const
{
    return {
        {IdRole, "intervalId"},
        {VehicleIdRole, "vehicleId"},
        {VehicleNameRole, "vehicleName"},
        {NameRole, "name"},
        {CategoryIdRole, "categoryId"},
        {EveryMRole, "everyM"},
        {EveryMonthsRole, "everyMonths"},
        {ActiveRole, "active"},
        {StatusRole, "status"},
        {LastDateRole, "lastDate"},
        {DueOdometerRole, "dueOdometerM"},
        {DueDateRole, "dueDate"},
        {RemainingMRole, "remainingM"},
        {RemainingDaysRole, "remainingDays"},
    };
}

QString IntervalListModel::vehicleId() const
{
    return m_vehicleId;
}

void IntervalListModel::setVehicleId(const QString &id)
{
    if (m_vehicleId == id) {
        return;
    }
    m_vehicleId = id;
    Q_EMIT vehicleIdChanged();
    reload();
}

QString IntervalListModel::worstStatus() const
{
    // Rows are sorted most urgent first, so the first active row decides.
    for (const Row &row : m_rows) {
        if (row.interval.active) {
            return core::dueStatusName(row.due.status);
        }
    }
    return {};
}

int IntervalListModel::overdueCount() const
{
    return static_cast<int>(std::count_if(m_rows.cbegin(), m_rows.cend(), [](const Row &row) {
        return row.interval.active && row.due.status == DueStatus::Overdue;
    }));
}

QVariantMap IntervalListModel::nextDue() const
{
    for (const Row &row : m_rows) {
        if (row.interval.active && row.due.status != DueStatus::Unknown) {
            return toMap(row);
        }
    }
    return {};
}

QString IntervalListModel::errorString() const
{
    return m_error;
}

void IntervalListModel::reload()
{
    const int oldCount = rowCount();
    const QString oldWorst = worstStatus();
    const int oldOverdue = overdueCount();
    const QVariantMap oldNext = nextDue();

    QList<Row> rows;
    QString error;
    IntervalRepository repo(m_database);
    QList<MaintenanceInterval> intervals;
    if (m_scope == Scope::AllVehicles) {
        intervals = repo.allActive();
    } else if (!m_vehicleId.isEmpty()) {
        intervals = repo.byVehicle(m_vehicleId);
    }
    error = repo.errorString();

    // Vehicle names for the overview; one query instead of one per row.
    QHash<QString, QString> vehicleNames;
    core::VehicleRepository vehicles(m_database);
    for (const core::Vehicle &v : vehicles.all(true)) {
        vehicleNames.insert(v.id, v.name);
    }

    const QDate today = QDate::currentDate();
    DueCalculator calc(m_database);
    rows.reserve(intervals.size());
    for (const MaintenanceInterval &i : intervals) {
        rows.append({i, calc.evaluate(i, today), vehicleNames.value(i.vehicleId)});
        if (error.isEmpty()) {
            error = calc.errorString();
        }
    }
    // stable_sort keeps the name order from the repository within equal urgency.
    std::stable_sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) {
        if (a.interval.active != b.interval.active) {
            return a.interval.active;
        }
        return DueCalculator::moreUrgent(a.due, b.due);
    });

    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
    setErrorString(error);
    if (rowCount() != oldCount) {
        Q_EMIT countChanged();
    }
    if (worstStatus() != oldWorst || overdueCount() != oldOverdue || nextDue() != oldNext) {
        Q_EMIT statusChanged();
    }
}

QVariantMap IntervalListModel::toMap(const Row &row) const
{
    const MaintenanceInterval &i = row.interval;
    return {
        {QStringLiteral("id"), i.id},
        {QStringLiteral("vehicleId"), i.vehicleId},
        {QStringLiteral("vehicleName"), row.vehicleName},
        {QStringLiteral("categoryId"), i.categoryId},
        {QStringLiteral("name"), i.name},
        {QStringLiteral("everyM"), optionalToVariant(i.everyM)},
        {QStringLiteral("everyMonths"), optionalToVariant(i.everyMonths)},
        {QStringLiteral("active"), i.active},
        {QStringLiteral("notes"), i.notes},
        {QStringLiteral("status"), core::dueStatusName(row.due.status)},
        {QStringLiteral("lastDate"), isoOrEmpty(row.due.lastDate)},
        {QStringLiteral("dueOdometerM"), optionalToVariant(row.due.dueOdometerM)},
        {QStringLiteral("dueDate"), isoOrEmpty(row.due.dueDate)},
        {QStringLiteral("remainingM"), optionalToVariant(row.due.remainingM)},
        {QStringLiteral("remainingDays"), optionalToVariant(row.due.remainingDays)},
    };
}

QVariantMap IntervalListModel::get(const QString &id)
{
    IntervalRepository repo(m_database);
    const auto i = repo.byId(id);
    if (!i) {
        return {};
    }
    DueCalculator calc(m_database);
    return toMap({*i, calc.evaluate(*i, QDate::currentDate()), QString()});
}

QString IntervalListModel::save(const QVariantMap &values)
{
    IntervalRepository repo(m_database);
    const QString id = values.value(QStringLiteral("id")).toString();

    MaintenanceInterval i;
    if (id.isEmpty()) {
        i.vehicleId = m_vehicleId;
    } else {
        const auto existing = repo.byId(id);
        if (!existing) {
            setErrorString(repo.errorString());
            return {};
        }
        i = *existing;
    }

    const QVariant everyM = values.value(QStringLiteral("everyM"));
    const QVariant everyMonths = values.value(QStringLiteral("everyMonths"));
    i.categoryId = values.value(QStringLiteral("categoryId")).toString();
    i.name = values.value(QStringLiteral("name")).toString().trimmed();
    i.everyM = everyM.isNull() ? std::nullopt : std::optional<qint64>(everyM.toLongLong());
    i.everyMonths = everyMonths.isNull() ? std::nullopt : std::optional<int>(everyMonths.toInt());
    i.active = values.value(QStringLiteral("active"), true).toBool();
    i.notes = values.value(QStringLiteral("notes")).toString();
    // "Oil change" as name is what most users would type anyway.
    if (i.name.isEmpty()) {
        if (const auto category = core::CategoryRepository(m_database).byId(i.categoryId)) {
            i.name = ServiceListModel::categoryLabel(*category);
        }
    }

    const bool ok = id.isEmpty() ? repo.create(i) : repo.update(i);
    if (!ok) {
        setErrorString(repo.errorString());
        return {};
    }
    setErrorString({});
    reload();
    Q_EMIT intervalsChanged();
    return i.id;
}

bool IntervalListModel::remove(const QString &id)
{
    IntervalRepository repo(m_database);
    const bool ok = repo.remove(id);
    setErrorString(repo.errorString());
    if (ok) {
        reload();
        Q_EMIT intervalsChanged();
    }
    return ok;
}

void IntervalListModel::setErrorString(const QString &error)
{
    if (m_error != error) {
        m_error = error;
        Q_EMIT errorStringChanged();
    }
}

} // namespace garagelog::app
