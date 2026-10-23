// Implementation of the fuel price arithmetic (role: core).

#include "fuelprice.h"

namespace garagelog::core {

namespace {

/** numerator / denominator rounded half up; both must be >= 0, denominator > 0. */
qint64 divideRounded(qint64 numerator, qint64 denominator)
{
    return (numerator + denominator / 2) / denominator;
}

} // namespace

// Conversion factor in all three functions: cents = ml * milli / 10'000,
// because 1 l = 1000 ml and 1 cent = 10 milli (same for Wh/kWh).

std::optional<qint64> unitPriceMilli(qint64 totalCostC, qint64 quantity)
{
    if (quantity <= 0 || totalCostC < 0) {
        return std::nullopt;
    }
    return divideRounded(totalCostC * 10'000, quantity);
}

std::optional<qint64> totalCostFor(qint64 quantity, qint64 unitPriceMilli)
{
    if (quantity < 0 || unitPriceMilli < 0) {
        return std::nullopt;
    }
    return divideRounded(quantity * unitPriceMilli, 10'000);
}

std::optional<qint64> quantityFor(qint64 totalCostC, qint64 unitPriceMilli)
{
    if (unitPriceMilli <= 0 || totalCostC < 0) {
        return std::nullopt;
    }
    return divideRounded(totalCostC * 10'000, unitPriceMilli);
}

} // namespace garagelog::core
