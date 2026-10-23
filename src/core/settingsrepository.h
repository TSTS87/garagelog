// Per-garage preferences in the settings table (role: core).
// Key/value text pairs; the keys are listed in DATA_MODEL.md
// (distance_unit, volume_unit, currency, locale).

#pragma once

#include <QString>

namespace garagelog::core {

class Database;

/**
 * @brief Reads and writes settings rows of the open garage.
 *
 * Settings live in the garage file, so a garage copied to another
 * computer keeps its units and currency.
 */
class SettingsRepository
{
public:
    /** Known keys and their allowed values (DATA_MODEL.md). */
    static inline const QString distanceUnitKey = QStringLiteral("distance_unit"); ///< "km" | "mi"
    static inline const QString volumeUnitKey = QStringLiteral("volume_unit");     ///< "l" | "gal_us" | "gal_uk"
    static inline const QString currencyKey = QStringLiteral("currency");          ///< ISO 4217 code, e.g. "EUR"

    explicit SettingsRepository(const Database &database);

    /** Stored value of @p key, or @p fallback if the key is not set (or on a read error). */
    QString value(const QString &key, const QString &fallback = {});

    /** Inserts or replaces @p key. @return false on a write error (errorString set). */
    bool setValue(const QString &key, const QString &value);

    /** Reason for the last failure. */
    QString errorString() const;

private:
    const Database &m_database;
    QString m_error;
};

} // namespace garagelog::core
