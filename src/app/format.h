// Unit conversion and number formatting for QML (role: app).
// The database stores metres (ADR-003); this is the one place that turns
// them into display text and parses user input back. km only until the
// unit settings arrive (M14).

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
     * @return Metres as qint64 in a QVariant; a null QVariant (JS null) for
     *         empty, non-numeric or negative input.
     */
    Q_INVOKABLE QVariant parseDistance(const QString &text) const;
};

} // namespace garagelog::app
