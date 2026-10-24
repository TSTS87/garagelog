// Implementation of the fuel price arithmetic (role: core).

#include "fuelprice.h"

namespace garagelog::core {

namespace {

/**
 * a * b / denominator rounded half up; all must be >= 0, denominator > 0.
 * nullopt if an intermediate result does not fit into 64 bits (absurd
 * input like a total of 10^14), instead of silently wrapping negative.
 */
std::optional<qint64> mulDivRounded(qint64 a, qint64 b, qint64 denominator)
{
    qint64 product = 0;
    qint64 sum = 0;
    if (__builtin_mul_overflow(a, b, &product) || __builtin_add_overflow(product, denominator / 2, &sum)) {
        return std::nullopt;
    }
    return sum / denominator;
}

} // namespace

// Conversion factor in all three functions: cents = ml * milli / 10'000,
// because 1 l = 1000 ml and 1 cent = 10 milli (same for Wh/kWh).

std::optional<qint64> unitPriceMilli(qint64 totalCostC, qint64 quantity)
{
    if (quantity <= 0 || totalCostC < 0) {
        return std::nullopt;
    }
    return mulDivRounded(totalCostC, 10'000, quantity);
}

std::optional<qint64> totalCostFor(qint64 quantity, qint64 unitPriceMilli)
{
    if (quantity < 0 || unitPriceMilli < 0) {
        return std::nullopt;
    }
    return mulDivRounded(quantity, unitPriceMilli, 10'000);
}

std::optional<qint64> quantityFor(qint64 totalCostC, qint64 unitPriceMilli)
{
    if (unitPriceMilli <= 0 || totalCostC < 0) {
        return std::nullopt;
    }
    return mulDivRounded(totalCostC, 10'000, unitPriceMilli);
}

} // namespace garagelog::core
