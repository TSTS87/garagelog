// Upper bounds for stored numbers (role: core).
// Shared by input parsing (app), repositories, CSV and JSON import, so a
// value accepted in one place is accepted everywhere and no calculation on
// it can overflow 64-bit integers.

#pragma once

#include <QDate>
#include <QtGlobal>

namespace garagelog::core::limits {

/** Odometer readings and interval distances in metres: 10 million km. */
inline constexpr qint64 maxDistanceM = 10'000'000'000;

/** Fuel quantity in ml or Wh: 10,000 l or 10 MWh per entry. */
inline constexpr qint64 maxQuantity = 10'000'000;

/** Money in cents: one billion in any currency. */
inline constexpr qint64 maxMoneyC = 100'000'000'000;

/** Interval length in months: 100 years. */
inline constexpr int maxIntervalMonths = 1200;

/** Earliest and latest year of a stored date; QDate::toString(Qt::ISODate) is empty beyond 9999. */
inline constexpr int minYear = 1900;
inline constexpr int maxYear = 9999;

/** True if @p date can be stored and round-trips as YYYY-MM-DD. */
inline bool isStorableDate(const QDate &date)
{
    return date.isValid() && date.year() >= minYear && date.year() <= maxYear;
}

} // namespace garagelog::core::limits
