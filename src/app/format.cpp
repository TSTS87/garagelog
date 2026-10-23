// Implementation of the display formatting helpers (role: app).

#include "format.h"

#include <QLocale>

#include <cmath>

namespace garagelog::app {

QString Format::distance(qint64 metres) const
{
    // Integer division truncates, matching how an odometer never rounds up.
    return QStringLiteral("%1 km").arg(QLocale().toString(metres / 1000));
}

QString Format::distanceInput(qint64 metres) const
{
    QLocale locale;
    // Group separators would make the text fail to parse again in some locales.
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    if (metres % 1000 == 0) {
        return locale.toString(metres / 1000);
    }
    return locale.toString(metres / 1000.0, 'f', 1);
}

QVariant Format::parseDistance(const QString &text) const
{
    bool ok = false;
    const double km = QLocale().toDouble(text.trimmed(), &ok);
    if (!ok || km < 0 || !std::isfinite(km)) {
        return {};
    }
    return QVariant::fromValue<qint64>(std::llround(km * 1000.0));
}

} // namespace garagelog::app
