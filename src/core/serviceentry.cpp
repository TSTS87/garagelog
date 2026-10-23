// Enum <-> database text mapping for ServiceEntry (role: core).

#include "serviceentry.h"

namespace garagelog::core {

QString toDbString(PerformedBy who)
{
    // Must match the CHECK constraint in migrations/0001_initial.sql.
    switch (who) {
    case PerformedBy::Self:
        return QStringLiteral("self");
    case PerformedBy::Shop:
        return QStringLiteral("shop");
    case PerformedBy::Unknown:
        break;
    }
    return {};
}

std::optional<PerformedBy> performedByFromDb(const QString &text)
{
    if (text.isEmpty()) {
        return PerformedBy::Unknown;
    }
    if (text == QLatin1String("self")) {
        return PerformedBy::Self;
    }
    if (text == QLatin1String("shop")) {
        return PerformedBy::Shop;
    }
    return std::nullopt;
}

} // namespace garagelog::core
