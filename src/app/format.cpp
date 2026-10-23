// Implementation of the display formatting helpers (role: app).

#include "format.h"

#include <QDate>
#include <QLocale>

#include <cmath>

namespace garagelog::app {

namespace {

/** Fixed until the currency setting arrives (M14). */
const QString currencySymbol = QStringLiteral("€");

/** Locale short date format, widened to a four-digit year so input is unambiguous. */
QString dateFormat()
{
    QString format = QLocale().dateFormat(QLocale::ShortFormat);
    if (!format.contains(QStringLiteral("yyyy"))) {
        format.replace(QStringLiteral("yy"), QStringLiteral("yyyy"));
    }
    return format;
}

/** Locale for editable numbers: group separators would not parse back in some locales. */
QLocale inputLocale()
{
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    return locale;
}

} // namespace

QString Format::distance(qint64 metres) const
{
    // Integer division truncates, matching how an odometer never rounds up.
    return QStringLiteral("%1 km").arg(QLocale().toString(metres / 1000));
}

QString Format::distanceInput(qint64 metres) const
{
    const QLocale locale = inputLocale();
    if (metres % 1000 == 0) {
        return locale.toString(metres / 1000);
    }
    return locale.toString(metres / 1000.0, 'f', 1);
}

QVariant Format::parseDistance(const QString &text) const
{
    return parseScaled(text, 1000.0);
}

QString Format::money(qint64 cents) const
{
    return QLocale().toCurrencyString(cents / 100.0, currencySymbol, 2);
}

QString Format::moneyInput(qint64 cents) const
{
    return inputLocale().toString(cents / 100.0, 'f', 2);
}

QVariant Format::parseMoney(const QString &text) const
{
    return parseScaled(text, 100.0);
}

QString Format::quantity(qint64 amount, bool electric) const
{
    return QStringLiteral("%1 %2").arg(QLocale().toString(amount / 1000.0, 'f', 2), electric ? QStringLiteral("kWh") : QStringLiteral("l"));
}

QString Format::quantityInput(qint64 amount) const
{
    // Shortest exact form: 40300 ml -> "40,3", 40305 ml -> "40,305".
    return inputLocale().toString(amount / 1000.0, 'f', QLocale::FloatingPointShortest);
}

QVariant Format::parseQuantity(const QString &text) const
{
    return parseScaled(text, 1000.0);
}

QString Format::unitPrice(qint64 milli, bool electric) const
{
    return QStringLiteral("%1/%2").arg(QLocale().toCurrencyString(milli / 1000.0, currencySymbol, 3),
                                       electric ? QStringLiteral("kWh") : QStringLiteral("l"));
}

QString Format::unitPriceInput(qint64 milli) const
{
    return inputLocale().toString(milli / 1000.0, 'f', 3);
}

QVariant Format::parseUnitPrice(const QString &text) const
{
    return parseScaled(text, 1000.0);
}

QString Format::consumption(double per100Km, bool electric) const
{
    return QStringLiteral("%1 %2").arg(QLocale().toString(per100Km / 1000.0, 'f', 2),
                                       electric ? QStringLiteral("kWh/100 km") : QStringLiteral("l/100 km"));
}

QString Format::date(const QString &iso) const
{
    return QLocale().toString(QDate::fromString(iso, Qt::ISODate), dateFormat());
}

QVariant Format::parseDate(const QString &text) const
{
    const QString trimmed = text.trimmed();
    QDate d = QLocale().toDate(trimmed, dateFormat());
    if (!d.isValid()) {
        d = QDate::fromString(trimmed, Qt::ISODate);
    }
    // A two-digit year typed into the four-digit format parses as year 0026.
    if (!d.isValid() || d.year() < 1900) {
        return QVariant::fromValue(nullptr);
    }
    return d.toString(Qt::ISODate);
}

QString Format::today() const
{
    return QDate::currentDate().toString(Qt::ISODate);
}

QVariant Format::parseScaled(const QString &text, double factor)
{
    bool ok = false;
    const double value = QLocale().toDouble(text.trimmed(), &ok);
    if (!ok || value < 0 || !std::isfinite(value)) {
        // An empty QVariant would arrive in JS as undefined; nullptr arrives as null,
        // which is what QML compares against.
        return QVariant::fromValue(nullptr);
    }
    return QVariant::fromValue<qint64>(std::llround(value * factor));
}

} // namespace garagelog::app
