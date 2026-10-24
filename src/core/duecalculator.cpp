// Implementation of the due status calculation (role: core).

#include "duecalculator.h"

#include "database.h"
#include "dbvalues.h"
#include "odometer.h"

#include <QSqlError>
#include <QSqlQuery>

#include <algorithm>

namespace garagelog::core {

using namespace db;

namespace {

/** Status of one part from what is left and the full interval length (same unit). */
DueStatus partStatus(qint64 remaining, qint64 length)
{
    if (remaining <= 0) {
        return DueStatus::Overdue;
    }
    if (static_cast<double>(remaining) <= DueCalculator::soonThreshold * static_cast<double>(length)) {
        return DueStatus::Soon;
    }
    return DueStatus::Ok;
}

/** The more urgent of two known statuses (enum order is urgency order). */
DueStatus worse(DueStatus a, DueStatus b)
{
    return std::min(a, b);
}

} // namespace

QString dueStatusName(DueStatus status)
{
    switch (status) {
    case DueStatus::Overdue:
        return QStringLiteral("overdue");
    case DueStatus::Soon:
        return QStringLiteral("soon");
    case DueStatus::Ok:
        return QStringLiteral("ok");
    case DueStatus::Unknown:
        break;
    }
    return QStringLiteral("unknown");
}

DueCalculator::DueCalculator(const Database &database)
    : m_database(database)
{
}

DueResult DueCalculator::evaluate(const MaintenanceInterval &interval, const std::optional<LastService> &last,
                                  qint64 currentOdometerM, const QDate &today)
{
    DueResult r;
    if (!last) {
        return r;
    }
    r.lastDate = last->date;
    r.lastOdometerM = last->odometerM;

    std::optional<DueStatus> status;
    auto addPart = [&](DueStatus part, double fraction) {
        status = status ? worse(*status, part) : part;
        r.remainingFraction = r.remainingFraction ? std::min(*r.remainingFraction, fraction) : fraction;
    };

    // Distance part needs the odometer of the last service; tax-like entries
    // without one cannot anchor a km interval.
    if (interval.everyM && last->odometerM) {
        r.dueOdometerM = *last->odometerM + *interval.everyM;
        r.remainingM = *r.dueOdometerM - currentOdometerM;
        addPart(partStatus(*r.remainingM, *interval.everyM), double(*r.remainingM) / double(*interval.everyM));
    }
    // addMonths() returns an invalid date when the result leaves QDate's
    // range; the time part is then unknown instead of "billions of days".
    const QDate dueDate = interval.everyMonths ? last->date.addMonths(*interval.everyMonths) : QDate();
    if (dueDate.isValid()) {
        r.dueDate = dueDate;
        r.remainingDays = today.daysTo(*r.dueDate);
        const qint64 lengthDays = std::max<qint64>(1, last->date.daysTo(*r.dueDate));
        addPart(partStatus(*r.remainingDays, lengthDays), double(*r.remainingDays) / double(lengthDays));
    }

    r.status = status.value_or(DueStatus::Unknown);
    return r;
}

std::optional<LastService> DueCalculator::lastService(const QString &vehicleId, const QString &categoryId, const QDate &today)
{
    m_error.clear();
    QSqlQuery query(m_database.connection());
    // Future-dated entries (planned appointments) must not reset the interval.
    // DESC puts NULL odometers last, so on the same day the reading wins.
    query.prepare(QStringLiteral(
        "SELECT date, odometer_m FROM service_entries WHERE vehicle_id = ? AND category_id = ? AND date <= ? "
        "ORDER BY date DESC, odometer_m DESC LIMIT 1"));
    query.addBindValue(vehicleId);
    query.addBindValue(categoryId);
    query.addBindValue(toDbDate(today));
    if (!query.exec()) {
        m_error = QStringLiteral("Cannot read last service: %1").arg(query.lastError().text());
        return std::nullopt;
    }
    if (!query.next()) {
        return std::nullopt;
    }
    LastService last;
    last.date = fromDbDate(query.value(0));
    if (!query.value(1).isNull()) {
        last.odometerM = query.value(1).toLongLong();
    }
    return last;
}

DueResult DueCalculator::evaluate(const MaintenanceInterval &interval, const QDate &today)
{
    const auto last = lastService(interval.vehicleId, interval.categoryId, today);
    if (!m_error.isEmpty()) {
        return {};
    }
    Odometer odometer(m_database);
    const auto current = odometer.current(interval.vehicleId);
    if (!current) {
        m_error = odometer.errorString();
        return {};
    }
    return evaluate(interval, last, *current, today);
}

bool DueCalculator::moreUrgent(const DueResult &a, const DueResult &b)
{
    if (a.status != b.status) {
        return a.status < b.status;
    }
    // Within one status the interval with less left comes first; results
    // without a fraction (Unknown) keep their relative order.
    return a.remainingFraction.value_or(0.0) < b.remainingFraction.value_or(0.0);
}

QString DueCalculator::errorString() const
{
    return m_error;
}

} // namespace garagelog::core
