// Implementation of the display formatting helpers (role: app).

#include "format.h"

#include "valuelimits.h"

#include <KLocalizedString>

#include <QDate>
#include <QHash>
#include <QLocale>

#include <cmath>

namespace garagelog::app {

namespace {

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

QString Format::distanceUnit() const
{
    return m_distanceUnit;
}

QString Format::volumeUnit() const
{
    return m_volumeUnit;
}

QString Format::currency() const
{
    return m_currency;
}

QString Format::distanceLabel() const
{
    return m_distanceUnit == QLatin1String("mi") ? QStringLiteral("mi") : QStringLiteral("km");
}

QString Format::volumeLabel() const
{
    return m_volumeUnit == QLatin1String("l") ? QStringLiteral("l") : QStringLiteral("gal");
}

QString Format::currencySymbol() const
{
    return m_currencySymbol;
}

void Format::setUnits(const QString &distanceUnit, const QString &volumeUnit, const QString &currency)
{
    const QString d = distanceUnit == QLatin1String("mi") ? distanceUnit : QStringLiteral("km");
    const QString v = (volumeUnit == QLatin1String("gal_us") || volumeUnit == QLatin1String("gal_uk")) ? volumeUnit : QStringLiteral("l");
    const QString c = currency.size() == 3 ? currency.toUpper() : QStringLiteral("EUR");
    if (d == m_distanceUnit && v == m_volumeUnit && c == m_currency) {
        return;
    }
    m_distanceUnit = d;
    m_volumeUnit = v;
    m_currency = c;
    m_currencySymbol = symbolForCurrency(c);
    Q_EMIT unitsChanged();
}

QString Format::symbolForCurrency(const QString &isoCode)
{
    // Qt knows symbols only per locale; the first locale using the code
    // gives it (e.g. de_DE for EUR, en_US for USD). Cached, as the scan
    // over all locales is not free.
    static QHash<QString, QString> cache;
    const auto it = cache.constFind(isoCode);
    if (it != cache.cend()) {
        return *it;
    }
    QString symbol = isoCode;
    for (const QLocale &locale : QLocale::matchingLocales(QLocale::AnyLanguage, QLocale::AnyScript, QLocale::AnyTerritory)) {
        if (locale.currencySymbol(QLocale::CurrencyIsoCode) == isoCode) {
            symbol = locale.currencySymbol(QLocale::CurrencySymbol);
            break;
        }
    }
    cache.insert(isoCode, symbol);
    return symbol;
}

double Format::metresPerUnit() const
{
    return m_distanceUnit == QLatin1String("mi") ? metresPerMile : 1000.0;
}

double Format::mlPerVolumeUnit() const
{
    if (m_volumeUnit == QLatin1String("gal_us")) {
        return mlPerUsGallon;
    }
    if (m_volumeUnit == QLatin1String("gal_uk")) {
        return mlPerImperialGallon;
    }
    return 1000.0;
}

QString Format::distance(qint64 metres) const
{
    // Truncated, matching how an odometer never rounds up.
    const auto units = static_cast<qint64>(std::trunc(static_cast<double>(metres) / metresPerUnit()));
    return QStringLiteral("%1 %2").arg(QLocale().toString(units), distanceLabel());
}

QString Format::distanceInput(qint64 metres) const
{
    const QLocale locale = inputLocale();
    const double units = static_cast<double>(metres) / metresPerUnit();
    if (units == std::trunc(units)) {
        return locale.toString(static_cast<qint64>(units));
    }
    return locale.toString(units, 'f', 1);
}

QString Format::dueSummary(const QVariant &remainingM, const QVariant &remainingDays, const QString &status) const
{
    if (status == QLatin1String("paused")) {
        return i18n("Paused");
    }
    if (status == QLatin1String("unknown")) {
        return i18n("No matching service recorded yet");
    }
    QStringList parts;
    if (!remainingM.isNull()) {
        const qint64 m = remainingM.toLongLong();
        parts.append(m > 0 ? i18nc("distance until due", "%1 left", distance(m)) : i18nc("distance past due", "%1 overdue", distance(-m)));
    }
    if (!remainingDays.isNull()) {
        const qint64 d = remainingDays.toLongLong();
        if (d > 0) {
            parts.append(i18np("%1 day left", "%1 days left", d));
        } else if (d == 0) {
            parts.append(i18n("due today"));
        } else {
            parts.append(i18np("%1 day overdue", "%1 days overdue", -d));
        }
    }
    return parts.join(QStringLiteral(" · "));
}

QVariant Format::parseMonths(const QString &text) const
{
    bool ok = false;
    const int value = QLocale().toInt(text.trimmed(), &ok);
    return ok && value > 0 && value <= core::limits::maxIntervalMonths ? QVariant(value) : QVariant::fromValue(nullptr);
}

QVariant Format::parseDistance(const QString &text) const
{
    return parseScaled(text, metresPerUnit(), core::limits::maxDistanceM);
}

QString Format::money(qint64 cents) const
{
    return QLocale().toCurrencyString(cents / 100.0, m_currencySymbol, 2);
}

QString Format::moneyPerKm(double centsPerKm) const
{
    const double perUnit = centsPerKm * metresPerUnit() / 1000.0;
    return QStringLiteral("%1/%2").arg(QLocale().toCurrencyString(perUnit / 100.0, m_currencySymbol, 3), distanceLabel());
}

QString Format::number(double value, int decimals) const
{
    return QLocale().toString(value, 'f', decimals);
}

QString Format::moneyInput(qint64 cents) const
{
    return inputLocale().toString(cents / 100.0, 'f', 2);
}

QVariant Format::parseMoney(const QString &text) const
{
    return parseScaled(text, 100.0, core::limits::maxMoneyC);
}

QString Format::quantity(qint64 amount, bool electric) const
{
    const double perUnit = electric ? 1000.0 : mlPerVolumeUnit();
    return QStringLiteral("%1 %2").arg(QLocale().toString(amount / perUnit, 'f', 2), electric ? QStringLiteral("kWh") : volumeLabel());
}

QString Format::quantityInput(qint64 amount, bool electric) const
{
    const double perUnit = electric ? 1000.0 : mlPerVolumeUnit();
    // Litres/kWh: shortest exact form (40300 ml -> "40,3"). Gallons are
    // never exact, so they get three decimals (0.3 ml precision).
    if (perUnit == 1000.0) {
        return inputLocale().toString(amount / perUnit, 'f', QLocale::FloatingPointShortest);
    }
    return inputLocale().toString(amount / perUnit, 'f', 3);
}

QVariant Format::parseQuantity(const QString &text, bool electric) const
{
    return parseScaled(text, electric ? 1000.0 : mlPerVolumeUnit(), core::limits::maxQuantity);
}

QString Format::unitPrice(qint64 milli, bool electric) const
{
    // milli is per l; per gallon it is that many litres more expensive.
    const double perUnit = electric ? milli : milli * mlPerVolumeUnit() / 1000.0;
    return QStringLiteral("%1/%2").arg(QLocale().toCurrencyString(perUnit / 1000.0, m_currencySymbol, 3),
                                       electric ? QStringLiteral("kWh") : volumeLabel());
}

QString Format::unitPriceInput(qint64 milli, bool electric) const
{
    const double perUnit = electric ? milli : milli * mlPerVolumeUnit() / 1000.0;
    return inputLocale().toString(perUnit / 1000.0, 'f', 3);
}

QVariant Format::parseUnitPrice(const QString &text, bool electric) const
{
    // Typed per gallon, stored per litre (see fuelprice.h).
    return parseScaled(text, electric ? 1000.0 : 1000.0 * 1000.0 / mlPerVolumeUnit(), core::limits::maxMoneyC * 10);
}

double Format::consumptionValue(double per100Km, bool electric) const
{
    if (electric) {
        // Wh per 100 km -> kWh per 100 of the display unit.
        return per100Km / 1000.0 * metresPerUnit() / 1000.0;
    }
    if (m_volumeUnit == QLatin1String("l")) {
        return per100Km / 1000.0;
    }
    // Miles per gallon is the inverse: distance per volume.
    if (per100Km <= 0) {
        return 0;
    }
    return (100'000.0 / metresPerMile) / (per100Km / mlPerVolumeUnit());
}

QString Format::consumptionUnit(bool electric) const
{
    if (electric) {
        return QStringLiteral("kWh/100 %1").arg(distanceLabel());
    }
    return m_volumeUnit == QLatin1String("l") ? QStringLiteral("l/100 km") : QStringLiteral("mpg");
}

QString Format::consumption(double per100Km, bool electric) const
{
    const int decimals = (!electric && m_volumeUnit != QLatin1String("l")) ? 1 : 2;
    return QStringLiteral("%1 %2").arg(QLocale().toString(consumptionValue(per100Km, electric), 'f', decimals), consumptionUnit(electric));
}

QString Format::date(const QString &iso) const
{
    return QLocale().toString(QDate::fromString(iso, Qt::ISODate), dateFormat());
}

QString Format::month(const QString &isoMonth) const
{
    const QDate d = QDate::fromString(isoMonth + QStringLiteral("-01"), Qt::ISODate);
    if (!d.isValid()) {
        return {};
    }
    // Standalone form: some languages inflect the month inside a full date
    // ("Oktobra") but use the nominative in a heading ("Oktober").
    return QStringLiteral("%1 %2").arg(QLocale().standaloneMonthName(d.month(), QLocale::LongFormat)).arg(d.year());
}

QVariant Format::parseDate(const QString &text) const
{
    const QString trimmed = text.trimmed();
    QDate d = QLocale().toDate(trimmed, dateFormat());
    if (!d.isValid()) {
        d = QDate::fromString(trimmed, Qt::ISODate);
    }
    // A two-digit year typed into the four-digit format parses as year 0026.
    if (!core::limits::isStorableDate(d)) {
        return QVariant::fromValue(nullptr);
    }
    return d.toString(Qt::ISODate);
}

QString Format::today() const
{
    return QDate::currentDate().toString(Qt::ISODate);
}

QVariant Format::parseScaled(const QString &text, double factor, qint64 max)
{
    bool ok = false;
    const double value = QLocale().toDouble(text.trimmed(), &ok);
    // The limit is checked on the scaled double: llround() of a value beyond
    // 64 bits returns LLONG_MIN, which would turn "1e16 km" into a negative odometer.
    if (!ok || value < 0 || !std::isfinite(value) || value * factor > double(max)) {
        // An empty QVariant would arrive in JS as undefined; nullptr arrives as null,
        // which is what QML compares against.
        return QVariant::fromValue(nullptr);
    }
    return QVariant::fromValue<qint64>(std::llround(value * factor));
}

} // namespace garagelog::app
