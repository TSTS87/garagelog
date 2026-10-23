// Due status of maintenance intervals (role: core).
// Derives when an interval is due from the latest matching service entry
// and the current odometer (DATA_MODEL.md "intervalDue"); nothing is stored.

#pragma once

#include "maintenanceinterval.h"

#include <QDate>
#include <QString>

#include <optional>

namespace garagelog::core {

class Database;

/** Traffic-light status of an interval; the order is the urgency order for sorting. */
enum class DueStatus {
    Overdue, ///< red: due distance or date reached or passed
    Soon,    ///< yellow: at most 20 % of the interval left
    Ok,      ///< green: more than 20 % left
    Unknown, ///< no matching service yet ("enter the last service")
};

/** Database-like text of @p status for QML: "overdue", "soon", "ok", "unknown". */
QString dueStatusName(DueStatus status);

/** The latest service that resets an interval: its date and, if recorded, its odometer. */
struct LastService {
    QDate date;
    std::optional<qint64> odometerM; ///< metres; nullopt if the entry has none (distance part then unknown)
};

/**
 * @brief Result of DueCalculator::evaluate(). Distances in metres, may be negative when overdue.
 *
 * Each part (distance, time) is only filled if the interval has it and the
 * last service provides its base (odometer for distance).
 */
struct DueResult {
    DueStatus status = DueStatus::Unknown;
    std::optional<QDate> lastDate;           ///< date of the last matching service
    std::optional<qint64> lastOdometerM;     ///< its odometer
    std::optional<qint64> dueOdometerM;      ///< lastOdometerM + everyM
    std::optional<QDate> dueDate;            ///< lastDate + everyMonths
    std::optional<qint64> remainingM;        ///< dueOdometerM - current odometer; <= 0 means overdue
    std::optional<qint64> remainingDays;     ///< days from today to dueDate; <= 0 means overdue
    /** Smallest remaining share of the known parts (1 = just serviced, <= 0 = overdue); for sorting. */
    std::optional<double> remainingFraction;

    bool operator==(const DueResult &) const = default;
};

/**
 * @brief Computes the due status of intervals.
 *
 * Rules (DATA_MODEL.md):
 * - last service = latest service entry of the vehicle with the interval's
 *   category and date <= today (on the same day the highest odometer),
 * - dueOdometer = lastOdometer + everyM, dueDate = lastDate + everyMonths
 *   (QDate::addMonths, so 31 Jan + 1 month = 28/29 Feb),
 * - per part: remaining <= 0 -> Overdue, remaining <= 20 % of the interval
 *   -> Soon, else Ok; the interval's status is the worst known part,
 * - no last service, or no part computable -> Unknown.
 * A due day equal to today already counts as Overdue, so the reminder is
 * red on the day the work is due.
 */
class DueCalculator
{
public:
    /** Share of the interval at or below which the status turns Soon. */
    static constexpr double soonThreshold = 0.2;

    explicit DueCalculator(const Database &database);

    /**
     * @brief Pure calculation, independent of the database.
     * @param interval The rule (everyM in metres, everyMonths).
     * @param last Latest matching service, nullopt if there is none.
     * @param currentOdometerM The vehicle's current odometer in metres.
     * @param today Reference day for the time part.
     */
    static DueResult evaluate(const MaintenanceInterval &interval, const std::optional<LastService> &last,
                              qint64 currentOdometerM, const QDate &today);

    /**
     * @brief Looks up last service and current odometer, then calls the pure evaluate().
     * @return Unknown status (and errorString set) if the lookup fails.
     */
    DueResult evaluate(const MaintenanceInterval &interval, const QDate &today);

    /** Latest service of @p vehicleId in @p categoryId on or before @p today; nullopt if none. */
    std::optional<LastService> lastService(const QString &vehicleId, const QString &categoryId, const QDate &today);

    /**
     * @brief Sort order for "most urgent first": status order, then smaller
     *        remainingFraction first. true if @p a comes before @p b.
     */
    static bool moreUrgent(const DueResult &a, const DueResult &b);

    /** Reason for the last failed lookup. */
    QString errorString() const;

private:
    const Database &m_database;
    QString m_error;
};

} // namespace garagelog::core
