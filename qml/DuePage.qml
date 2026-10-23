// "Due" overview across all vehicles (role: qml, pure UI).
// Lists the active intervals of every non-archived vehicle from
// DueIntervals, most urgent first (SPEC §4.4); a click opens the vehicle.

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.tmx.garagelog

// ScrollablePage puts its single child (here the ListView) into a
// scrolling area that fits the page; lists get Kirigami's list styling.
Kirigami.ScrollablePage {
    id: page

    signal vehicleRequested(string vehicleId)

    title: i18n("Due")

    header: Kirigami.InlineMessage {
        position: Kirigami.InlineMessage.Position.Header
        type: Kirigami.MessageType.Error
        text: DueIntervals.errorString
        visible: text !== ""
    }

    ListView {
        id: list
        model: DueIntervals

        delegate: Controls.ItemDelegate {
            id: row

            required property string vehicleId
            required property string vehicleName
            required property string name
            required property string status
            required property var dueOdometerM
            required property string dueDate
            required property var remainingM
            required property var remainingDays

            width: ListView.view.width
            onClicked: page.vehicleRequested(vehicleId)

            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing

                StatusDot {
                    status: row.status
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    Controls.Label {
                        Layout.fillWidth: true
                        text: i18nc("%1 vehicle, %2 interval name", "%1 · %2", row.vehicleName, row.name)
                        elide: Text.ElideRight
                    }

                    Controls.Label {
                        Layout.fillWidth: true
                        text: [row.dueOdometerM === null ? "" : Format.distance(row.dueOdometerM),
                               row.dueDate === "" ? "" : Format.date(row.dueDate)]
                            .filter(part => part !== "").join(" / ")
                        visible: text !== ""
                        font: Kirigami.Theme.smallFont
                        color: Kirigami.Theme.disabledTextColor
                        elide: Text.ElideRight
                    }
                }

                Controls.Label {
                    text: Format.dueSummary(row.remainingM, row.remainingDays, row.status)
                    color: row.status === "overdue" ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
                }
            }
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            visible: list.count === 0
            icon.name: "view-calendar-upcoming-events"
            text: i18n("Nothing to remind you of")
            explanation: i18n("Intervals you add to your vehicles appear here, most urgent first.")
        }
    }
}
