// Conversions between C++ values and their SQLite text/NULL form (role: core).
// Shared by all repositories so every table stores dates, timestamps and
// optional text the same way (DATA_MODEL.md: ISO dates, UTC with 'Z').

#pragma once

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QVariant>

namespace garagelog::core::db {

/** Current time in UTC, truncated to seconds so it survives the ISO text roundtrip unchanged. */
QDateTime nowUtc();

/** UTC timestamp as stored in SQLite: YYYY-MM-DDTHH:MM:SSZ. */
QString toDbTimestamp(const QDateTime &time);

/** Parses a timestamp column; invalid QDateTime for malformed text. */
QDateTime fromDbTimestamp(const QVariant &value);

/** Calendar date as stored in SQLite: YYYY-MM-DD. */
QString toDbDate(const QDate &date);

/** Parses a date column; invalid QDate for malformed text. */
QDate fromDbDate(const QVariant &value);

/** Empty QString -> SQL NULL, so optional text columns do not fill up with ''. */
QVariant nullIfEmpty(const QString &text);

} // namespace garagelog::core::db
