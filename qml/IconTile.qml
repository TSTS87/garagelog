// Rounded square with a line icon (role: qml, pure UI, ADR-015).
// Leading badge of list rows, stat tiles and the side bar: a light tint of
// the accent with the icon in the accent, or ("filled") the accent with a
// white icon.

import QtQuick
import org.kde.kirigami as Kirigami
import org.tmx.garagelog

Rectangle {
    id: tile

    /** Icon URL (qrc:/qml/icons/…) or a freedesktop icon name. */
    property string source
    /** Colour of icon and tint. */
    property color accent: Kirigami.Theme.highlightColor
    /** Accent background with a white icon instead of a light tint. */
    property bool filled: false
    /** Edge length in pixels. */
    property real size: Kirigami.Units.gridUnit * 2.2

    readonly property bool dark: Kirigami.ColorUtils.brightnessForColor(Kirigami.Theme.backgroundColor) === Kirigami.ColorUtils.Dark

    implicitWidth: size
    implicitHeight: size
    radius: Style.tileRadius
    color: filled ? accent : Qt.rgba(accent.r, accent.g, accent.b, dark ? 0.24 : 0.13)

    Kirigami.Icon {
        anchors.centerIn: parent
        width: Math.round(tile.size * 0.56)
        height: width
        // Keep the exact share of the tile; by default Kirigami snaps to
        // standard icon sizes, which makes the icon small in large tiles.
        roundToIconSize: false
        source: tile.source
        // isMask paints every opaque pixel of the icon in "color"; our SVGs
        // are black masks made for exactly that.
        isMask: true
        // A plain accent on a dark tint is too dim; lift it a little.
        color: tile.filled ? "white" : (tile.dark ? Qt.lighter(tile.accent, 1.35) : tile.accent)
    }
}
