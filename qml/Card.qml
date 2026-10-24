// Raised surface for grouped content (role: qml, pure UI, ADR-015).
// A rounded panel a shade lighter than the window with a soft shadow in
// light schemes; children are placed inside by the user of the card.

import QtQuick
import org.kde.kirigami as Kirigami
import org.tmx.garagelog

// ShadowedRectangle is Kirigami's GPU-drawn rectangle with a real soft
// shadow and per-corner radii, cheaper than a blur effect on top of an Item.
Kirigami.ShadowedRectangle {
    id: card

    /** True in dark colour schemes; shadows are invisible there, so the card is lifted by brightness instead. */
    readonly property bool dark: Kirigami.ColorUtils.brightnessForColor(Kirigami.Theme.backgroundColor) === Kirigami.ColorUtils.Dark

    // Lighter than the window in both schemes, so the card reads as "on top".
    color: dark ? Qt.lighter(Kirigami.Theme.backgroundColor, 1.45) : Qt.lighter(Kirigami.Theme.backgroundColor, 1.05)
    radius: Style.cardRadius
    border.width: 1
    border.color: Kirigami.ColorUtils.tintWithAlpha(color, Kirigami.Theme.textColor, dark ? 0.06 : 0.08)
    shadow.size: dark ? 0 : Kirigami.Units.gridUnit * 0.8
    shadow.yOffset: 2
    shadow.color: Qt.rgba(0, 0, 0, 0.07)
}
