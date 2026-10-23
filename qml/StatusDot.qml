// Coloured dot for the due status of an interval (role: qml, pure UI).
// Green ok, yellow soon, red overdue, grey unknown/paused (SPEC §4.4).
// Used in the intervals tab, the due page and the vehicle page header.

import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami

Rectangle {
    id: dot

    /** Status text from IntervalListModel: "overdue", "soon", "ok", "unknown" or "paused". */
    property string status: "unknown"

    // Kirigami.Theme's semantic colours follow the user's colour scheme,
    // so the dot stays readable in dark themes as well.
    readonly property color statusColor: status === "overdue" ? Kirigami.Theme.negativeTextColor
        : status === "soon" ? Kirigami.Theme.neutralTextColor
        : status === "ok" ? Kirigami.Theme.positiveTextColor
        : Kirigami.Theme.disabledTextColor

    implicitWidth: Kirigami.Units.iconSizes.small
    implicitHeight: implicitWidth
    radius: width / 2
    color: statusColor

    // Colour alone is not accessible; the tooltip names the status.
    HoverHandler {
        id: hover
    }
    Controls.ToolTip.visible: hover.hovered
    Controls.ToolTip.text: status === "overdue" ? i18n("Overdue")
        : status === "soon" ? i18n("Due soon")
        : status === "ok" ? i18n("OK")
        : status === "paused" ? i18n("Paused")
        : i18n("Unknown – enter the last service")
}
