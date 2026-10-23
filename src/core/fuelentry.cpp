// Quantity unit mapping for FuelEntry (role: core).

#include "fuelentry.h"

namespace garagelog::core {

QuantityUnit quantityUnitFor(Powertrain powertrain)
{
    // Hybrids are refuelled at the pump, so only pure electric means Wh.
    return powertrain == Powertrain::Electric ? QuantityUnit::WattHour : QuantityUnit::Millilitre;
}

} // namespace garagelog::core
