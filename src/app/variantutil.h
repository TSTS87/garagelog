// QVariant helpers shared by the QML-facing models (role: app).
// QML tells "no value" apart by comparing with null; these make every model
// hand out the same kind of empty value.

#pragma once

#include <QVariant>

#include <optional>

namespace garagelog::app {

/** JS null. A default QVariant() would arrive in QML as undefined instead. */
inline QVariant jsNull()
{
    return QVariant::fromValue(nullptr);
}

/** The value of @p value, or JS null for nullopt. */
template<typename T>
QVariant optionalToVariant(const std::optional<T> &value)
{
    return value ? QVariant::fromValue(*value) : jsNull();
}

} // namespace garagelog::app
