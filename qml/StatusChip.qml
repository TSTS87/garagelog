// Pill with the due status of an interval (role: qml, pure UI, ADR-015).
// Tinted in the status colour (red overdue, yellow soon, green ok, grey
// unknown/paused) with a dot and a short text like "402 km left".

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Rectangle {
    id: chip

    /** Status text from IntervalListModel: "overdue", "soon", "ok", "unknown" or "paused". */
    property string status: "unknown"
    property string text

    /** Same mapping as StatusDot, so dot and chip always agree. */
    readonly property color statusColor: status === "overdue" ? Kirigami.Theme.negativeTextColor
        : status === "soon" ? Kirigami.Theme.neutralTextColor
        : status === "ok" ? Kirigami.Theme.positiveTextColor
        : Kirigami.Theme.disabledTextColor

    implicitWidth: content.implicitWidth + Kirigami.Units.largeSpacing * 2
    implicitHeight: content.implicitHeight + Kirigami.Units.smallSpacing * 2
    radius: height / 2
    color: Qt.rgba(statusColor.r, statusColor.g, statusColor.b, 0.15)

    RowLayout {
        id: content
        anchors.centerIn: parent
        spacing: Kirigami.Units.smallSpacing

        Rectangle {
            implicitWidth: Kirigami.Units.smallSpacing * 2
            implicitHeight: implicitWidth
            radius: width / 2
            color: chip.statusColor
        }
        Controls.Label {
            text: chip.text
            font: Kirigami.Theme.smallFont
            color: chip.status === "overdue" ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
        }
    }
}
