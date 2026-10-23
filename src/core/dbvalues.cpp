// Implementation of the SQLite value conversions (role: core).

#include "dbvalues.h"

#include <QTimeZone>

namespace garagelog::core::db {

QDateTime nowUtc()
{
    return QDateTime::fromSecsSinceEpoch(QDateTime::currentSecsSinceEpoch(), QTimeZone::UTC);
}

QString toDbTimestamp(const QDateTime &time)
{
    return time.toUTC().toString(Qt::ISODate);
}

QDateTime fromDbTimestamp(const QVariant &value)
{
    return QDateTime::fromString(value.toString(), Qt::ISODate).toUTC();
}

QString toDbDate(const QDate &date)
{
    return date.toString(Qt::ISODate);
}

QDate fromDbDate(const QVariant &value)
{
    return QDate::fromString(value.toString(), Qt::ISODate);
}

QVariant nullIfEmpty(const QString &text)
{
    return text.isEmpty() ? QVariant() : QVariant(text);
}

} // namespace garagelog::core::db
