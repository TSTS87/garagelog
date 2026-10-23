// Unit conversion and number formatting for QML (role: app).
// The database stores metres, cents, ml and Wh (ADR-003) and ISO dates; this
// is the one place that turns them into display text and parses user input
// back. km, l/kWh and EUR only until the unit settings arrive (M14).

#pragma once

#include <QObject>
#include <QString>
#include <QVariant>

namespace garagelog::app {

/**
 * @brief Stateless formatting helpers, exposed to QML as singleton "Format".
 *
 * Uses the default QLocale, so decimal and group separators follow the
 * user's language settings.
 */
class Format : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    /**
     * @brief Odometer/distance for display, e.g. "12.345 km" (German locale).
     * @param metres Distance in metres; truncated to whole km like a real odometer.
     */
    Q_INVOKABLE QString distance(qint64 metres) const;

    /**
     * @brief Distance as editable text without unit or group separators, e.g. "12345".
     * @param metres Distance in metres; shown with up to one decimal so a
     *        reading like 12345.6 km survives an edit unchanged.
     */
    Q_INVOKABLE QString distanceInput(qint64 metres) const;

    /**
     * @brief Parses user input in km (locale decimal separator) to metres.
     * @return Metres as qint64 in a QVariant; QVariant(nullptr), i.e. JS null, for
     *         empty, non-numeric or negative input.
     */
    Q_INVOKABLE QVariant parseDistance(const QString &text) const;

    /** Amount for display with currency, e.g. "72,50 €". @param cents Amount in cents. */
    Q_INVOKABLE QString money(qint64 cents) const;

    /** Amount as editable text without currency, e.g. "72,50". @param cents Amount in cents. */
    Q_INVOKABLE QString moneyInput(qint64 cents) const;

    /** Parses an amount in currency units to cents; null for empty, non-numeric or negative input. */
    Q_INVOKABLE QVariant parseMoney(const QString &text) const;

    /**
     * @brief Fuel or charge quantity for display, e.g. "40,30 l" or "32,50 kWh".
     * @param amount ml (fuel) or Wh (electric).
     * @param electric true if @p amount is Wh (core::quantityUnitFor()).
     */
    Q_INVOKABLE QString quantity(qint64 amount, bool electric) const;

    /** Quantity as editable text in l or kWh, as short as exact, e.g. "40,3". @param amount ml or Wh. */
    Q_INVOKABLE QString quantityInput(qint64 amount) const;

    /** Parses l or kWh to ml or Wh; null for empty, non-numeric or negative input. */
    Q_INVOKABLE QVariant parseQuantity(const QString &text) const;

    /**
     * @brief Unit price for display, e.g. "1,799 €/l".
     * @param milli Thousandths of the currency per l or kWh (see fuelprice.h).
     * @param electric true for per kWh, false for per l.
     */
    Q_INVOKABLE QString unitPrice(qint64 milli, bool electric) const;

    /** Unit price as editable text with three decimals, e.g. "1,799". @param milli See unitPrice(). */
    Q_INVOKABLE QString unitPriceInput(qint64 milli) const;

    /** Parses a unit price per l or kWh to thousandths of the currency; null for invalid input. */
    Q_INVOKABLE QVariant parseUnitPrice(const QString &text) const;

    /**
     * @brief Consumption for display, e.g. "6,42 l/100 km".
     * @param per100Km ml or Wh per 100 km, as in core::Consumption.
     * @param electric true for kWh/100 km, false for l/100 km.
     */
    Q_INVOKABLE QString consumption(double per100Km, bool electric) const;

    /** Date for display and input in the locale's short format with four-digit year. @param iso "YYYY-MM-DD". */
    Q_INVOKABLE QString date(const QString &iso) const;

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

private:
    /**
     * @brief Parses a non-negative locale number and scales it to an integer unit.
     * @param factor e.g. 100 for currency to cents, 1000 for l to ml.
     * @return qint64 in a QVariant, or QVariant(nullptr) (JS null) for invalid input.
     */
    static QVariant parseScaled(const QString &text, double factor);
};

} // namespace garagelog::app
