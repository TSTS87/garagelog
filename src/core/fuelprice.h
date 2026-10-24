// Price arithmetic of a fuel entry: total, quantity and unit price (role: core).
// The fuel form lets the user enter two of the three values and derives the
// third; this is the integer arithmetic behind it, so the UI never calculates.

#pragma once

#include <QtGlobal>

#include <optional>

namespace garagelog::core {

/*
 * Units: quantity in ml or Wh (see quantityUnitFor()), total in cents and
 * unit price in thousandths of the currency per litre or kWh ("milli"),
 * because pump prices have three decimals (1.799 EUR/l = 1799).
 * The unit price is never stored; fuel_entries keeps quantity and total.
 */

/**
 * @brief Unit price of a fill-up.
 * @param totalCostC Total price in cents, >= 0.
 * @param quantity ml or Wh.
 * @return Thousandths of the currency per l or kWh, rounded half up;
 *         nullopt if @p quantity <= 0 or @p totalCostC < 0, or on 64-bit overflow.
 */
std::optional<qint64> unitPriceMilli(qint64 totalCostC, qint64 quantity);

/**
 * @brief Total price for a quantity at a unit price.
 * @param quantity ml or Wh.
 * @param unitPriceMilli Thousandths of the currency per l or kWh.
 * @return Cents, rounded half up; nullopt if an argument is negative or on 64-bit overflow.
 */
std::optional<qint64> totalCostFor(qint64 quantity, qint64 unitPriceMilli);

/**
 * @brief Quantity bought for a total price at a unit price.
 * @param totalCostC Total price in cents.
 * @param unitPriceMilli Thousandths of the currency per l or kWh.
 * @return ml or Wh, rounded half up; nullopt if @p unitPriceMilli <= 0,
 *         @p totalCostC < 0 or on 64-bit overflow.
 */
std::optional<qint64> quantityFor(qint64 totalCostC, qint64 unitPriceMilli);

} // namespace garagelog::core
