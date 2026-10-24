// "Due" overview across all vehicles (role: qml, pure UI).
// Lists the active intervals of every non-archived vehicle from
// DueIntervals in a card, grouped by status and most urgent first
// (SPEC §4.4); a click opens the vehicle.

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.tmx.garagelog

Kirigami.Page {
    id: page

    signal vehicleRequested(string vehicleId)

    /** Section heading for a status; the list is sorted by urgency, so each status forms one block. */
    function statusHeading(status) {
        return ({ overdue: i18n("Overdue"), soon: i18n("Due soon"), ok: i18n("OK") })[status]
            ?? i18n("No matching service recorded yet");
    }

    title: i18n("Due")

    ColumnLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.largeSpacing

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Error
            text: DueIntervals.errorString
            visible: text !== ""
        }

        Card {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: list
                anchors.fill: parent
                anchors.margins: Kirigami.Units.smallSpacing
                clip: true
                model: DueIntervals
                bottomMargin: Kirigami.Units.largeSpacing
                Controls.ScrollBar.vertical: Controls.ScrollBar {}

                section.property: "status"
                section.delegate: SectionHeader {
                    required property string section
                    text: page.statusHeading(section)
                }

                delegate: EntryRow {
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

                    iconSource: "qrc:/qml/icons/interval.svg"
                    accent: chip.statusColor
                    title: i18nc("%1 vehicle, %2 interval name", "%1 · %2", vehicleName, name)
                    subtitle: [dueOdometerM === null ? "" : Format.distance(dueOdometerM),
                               dueDate === "" ? "" : Format.date(dueDate)]
                        .filter(part => part !== "").join(" / ")

                    StatusChip {
                        id: chip
                        status: row.status
                        text: Format.dueSummary(row.remainingM, row.remainingDays, row.status)
                    }
                }

                Kirigami.PlaceholderMessage {
                    anchors.centerIn: parent
                    width: parent.width - Kirigami.Units.gridUnit * 4
                    visible: list.count === 0
                    icon.source: "qrc:/qml/icons/due.svg"
                    text: i18n("Nothing to remind you of")
                    explanation: i18n("Intervals you add to your vehicles appear here, most urgent first.")
                }
            }
        }
    }
}
