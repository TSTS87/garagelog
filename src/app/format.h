// Unit conversion and number formatting for QML (role: app).
// The database stores metres, cents, ml and Wh (ADR-003) and ISO dates; this
// is the one place that turns them into display text in the garage's units
// (km/mi, l/US gal/imp. gal, currency) and parses user input back.

#pragma once

#include <QObject>
#include <QString>
#include <QVariant>

namespace garagelog::app {

/**
 * @brief Formatting helpers, exposed to QML as singleton "Format".
 *
 * Uses the default QLocale for decimal and group separators. The units
 * are set by GarageController from the garage settings; the label
 * properties notify on change so form labels follow, while lists refresh
 * because the models reload after a settings change. Electric quantities
 * always stay kWh, only fuel volumes switch to gallons.
 */
class Format : public QObject
{
    Q_OBJECT
    /** "km" or "mi" (settings value). */
    Q_PROPERTY(QString distanceUnit READ distanceUnit NOTIFY unitsChanged)
    /** "l", "gal_us" or "gal_uk" (settings value). */
    Q_PROPERTY(QString volumeUnit READ volumeUnit NOTIFY unitsChanged)
    /** ISO 4217 code, e.g. "EUR". */
    Q_PROPERTY(QString currency READ currency NOTIFY unitsChanged)
    /** Short labels for form fields: "km"/"mi", "l"/"gal", "€"/"$"/... */
    Q_PROPERTY(QString distanceLabel READ distanceLabel NOTIFY unitsChanged)
    Q_PROPERTY(QString volumeLabel READ volumeLabel NOTIFY unitsChanged)
    Q_PROPERTY(QString currencySymbol READ currencySymbol NOTIFY unitsChanged)

public:
    using QObject::QObject;

    /** Metres in one mile (international mile). */
    static constexpr double metresPerMile = 1609.344;
    /** Millilitres in one US liquid gallon and one imperial gallon. */
    static constexpr double mlPerUsGallon = 3785.411784;
    static constexpr double mlPerImperialGallon = 4546.09;

    QString distanceUnit() const;
    QString volumeUnit() const;
    QString currency() const;
    QString distanceLabel() const;
    QString volumeLabel() const;
    QString currencySymbol() const;

    /**
     * @brief Switches the display units; unknown values fall back to km, l and EUR.
     * @param distanceUnit "km" | "mi"; @param volumeUnit "l" | "gal_us" | "gal_uk";
     * @param currency ISO 4217 code.
     */
    void setUnits(const QString &distanceUnit, const QString &volumeUnit, const QString &currency);

    /** Symbol for an ISO 4217 code from Qt's locale data ("EUR" -> "€"); the code itself if unknown. */
    static QString symbolForCurrency(const QString &isoCode);

    /**
     * @brief Odometer/distance for display, e.g. "12.345 km" (German locale) or "7.671 mi".
     * @param metres Distance in metres; truncated to whole units like a real odometer.
     */
    Q_INVOKABLE QString distance(qint64 metres) const;

    /**
     * @brief Distance as editable text in km or mi without group separators, e.g. "12345".
     * @param metres Distance in metres; up to one decimal so 12345.6 survives an edit.
     */
    Q_INVOKABLE QString distanceInput(qint64 metres) const;

    /**
     * @brief Parses user input in km or mi (locale decimal separator) to metres.
     * @return Metres as qint64 in a QVariant; QVariant(nullptr), i.e. JS null, for
     *         empty, non-numeric or negative input.
     */
    Q_INVOKABLE QVariant parseDistance(const QString &text) const;

    /** Amount for display with currency, e.g. "72,50 €". @param cents Amount in cents (hundredths of the currency). */
    Q_INVOKABLE QString money(qint64 cents) const;

    /** Cost per distance unit with three decimals, e.g. "0,246 €/km" or "0,396 €/mi". @param centsPerKm Cents per km. */
    Q_INVOKABLE QString moneyPerKm(double centsPerKm) const;

    /** Plain locale number with @p decimals decimals, e.g. for chart axis labels ("6,5"). */
    Q_INVOKABLE QString number(double value, int decimals) const;

    /** Amount as editable text without currency, e.g. "72,50". @param cents Amount in cents. */
    Q_INVOKABLE QString moneyInput(qint64 cents) const;

    /** Parses an amount in currency units to cents; null for empty, non-numeric or negative input. */
    Q_INVOKABLE QVariant parseMoney(const QString &text) const;

    /**
     * @brief Fuel or charge quantity for display, e.g. "40,30 l", "10,65 gal" or "32,50 kWh".
     * @param amount ml (fuel) or Wh (electric).
     * @param electric true if @p amount is Wh (core::quantityUnitFor()).
     */
    Q_INVOKABLE QString quantity(qint64 amount, bool electric) const;

    /** Quantity as editable text in l/gal or kWh, as short as exact, e.g. "40,3". @param amount ml or Wh. */
    Q_INVOKABLE QString quantityInput(qint64 amount, bool electric = false) const;

    /** Parses l/gal or kWh to ml or Wh; null for empty, non-numeric or negative input. */
    Q_INVOKABLE QVariant parseQuantity(const QString &text, bool electric = false) const;

    /**
     * @brief Unit price for display, e.g. "1,799 €/l" or "6,810 $/gal".
     * @param milli Thousandths of the currency per l or kWh (see fuelprice.h).
     * @param electric true for per kWh, false for per l/gal.
     */
    Q_INVOKABLE QString unitPrice(qint64 milli, bool electric) const;

    /** Unit price per l/gal or kWh as editable text with three decimals. @param milli See unitPrice(). */
    Q_INVOKABLE QString unitPriceInput(qint64 milli, bool electric = false) const;

    /** Parses a unit price per l/gal or kWh to thousandths of the currency per l or kWh; null for invalid input. */
    Q_INVOKABLE QVariant parseUnitPrice(const QString &text, bool electric = false) const;

    /**
     * @brief Consumption for display, e.g. "6,42 l/100 km", "36,6 mpg" or "16,50 kWh/100 km".
     * @param per100Km ml or Wh per 100 km, as in core::Consumption.
     * @param electric true for kWh, false for fuel.
     */
    Q_INVOKABLE QString consumption(double per100Km, bool electric) const;

    /** consumption() as plain number in the display unit, for charts. */
    Q_INVOKABLE double consumptionValue(double per100Km, bool electric) const;

    /** Unit text of consumption(): "l/100 km", "mpg", "kWh/100 km" or "kWh/100 mi". */
    Q_INVOKABLE QString consumptionUnit(bool electric) const;

    /** Date for display and input in the locale's short format with four-digit year. @param iso "YYYY-MM-DD". */
    Q_INVOKABLE QString date(const QString &iso) const;

    /** Month heading like "October 2026" in the locale's language. @param isoMonth "YYYY-MM"; "" for an invalid value. */
    Q_INVOKABLE QString month(const QString &isoMonth) const;

    /**
     * @brief Parses a date typed by the user.
     *
     * Accepts the locale format that date() shows and ISO "YYYY-MM-DD".
     * Dates travel between QML and C++ as ISO text, because a JS Date
     * carries a time zone and can shift the day.
     *
     * @return ISO text; null for invalid input or years before 1900.
     */
    Q_INVOKABLE QVariant parseDate(const QString &text) const;

    /**
     * @brief Short due text for an interval, e.g. "1.500 km left · 47 days left" or "3 days overdue".
     * @param remainingM Metres to the due odometer (negative = overdue) or null.
     * @param remainingDays Days to the due date (negative = overdue) or null.
     * @param status Status text from IntervalListModel ("unknown" and "paused" get their own text).
     */
    Q_INVOKABLE QString dueSummary(const QVariant &remainingM, const QVariant &remainingDays, const QString &status) const;

    /**
     * @brief Parses a whole positive number such as a count of months.
     * @return int in a QVariant; null for empty, non-integer, zero or negative input.
     */
    Q_INVOKABLE QVariant parseCount(const QString &text) const;

    /** Today's date as ISO text, the default for new entries. */
    Q_INVOKABLE QString today() const;

Q_SIGNALS:
    void unitsChanged();

private:
    /**
     * @brief Parses a non-negative locale number and scales it to an integer unit.
     * @param factor e.g. 100 for currency to cents, 1000 for l to ml.
     * @return qint64 in a QVariant, or QVariant(nullptr) (JS null) for invalid input.
     */
    static QVariant parseScaled(const QString &text, double factor);

    /** Metres per displayed distance unit (1000 or 1609.344). */
    double metresPerUnit() const;
    /** Millilitres per displayed fuel volume unit (1000 for l, or a gallon). */
    double mlPerVolumeUnit() const;

    QString m_distanceUnit = QStringLiteral("km");
    QString m_volumeUnit = QStringLiteral("l");
    QString m_currency = QStringLiteral("EUR");
    QString m_currencySymbol = QStringLiteral("€");
};

} // namespace garagelog::app
