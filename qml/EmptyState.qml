// Empty state of pages and lists (role: qml, pure UI, ADR-015).
// A large tinted icon tile above Kirigami's PlaceholderMessage. The tile
// recolours GarageLog's own mask icons, which PlaceholderMessage would
// draw black (invisible in dark schemes).

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: empty

    property string iconSource
    property color accent: Kirigami.Theme.highlightColor
    property string text
    property string explanation
    /** Button for the obvious next step; null hides it. */
    property Kirigami.Action helpfulAction: null

    spacing: Kirigami.Units.largeSpacing

    IconTile {
        Layout.alignment: Qt.AlignHCenter
        source: empty.iconSource
        accent: empty.accent
        size: Kirigami.Units.gridUnit * 4.5
        radius: Kirigami.Units.gridUnit * 1.2
    }

    // PlaceholderMessage is Kirigami's standard "empty state": text,
    // explanation and an optional button; its own icon is left empty here.
    Kirigami.PlaceholderMessage {
        Layout.fillWidth: true
        text: empty.text
        explanation: empty.explanation
        helpfulAction: empty.helpfulAction
    }
}
