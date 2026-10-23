// Intervals tab of the vehicle page (role: qml, pure UI).
// Lists the maintenance intervals of Intervals.vehicleId, most urgent
// first, with status dot, rule ("every 15.000 km / 12 months") and what is
// left until due. A click asks for the edit form.

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.tmx.garagelog

ColumnLayout {
    id: tab

    /** intervalId "" = new interval. Main.qml opens the form (ADR-008). */
    signal formRequested(string intervalId)

    /** "every 15.000 km / 12 months" from the two optional parts. */
    function ruleText(everyM, everyMonths) {
        const parts = [];
        if (everyM !== null) {
            parts.push(Format.distance(everyM));
        }
        if (everyMonths !== null) {
            parts.push(i18np("%1 month", "%1 months", everyMonths));
        }
        return i18nc("interval rule, %1 = '15.000 km / 12 months'", "every %1", parts.join(" / "));
    }

    spacing: 0

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        type: Kirigami.MessageType.Error
        text: Intervals.errorString
        visible: text !== ""
    }

    ListView {
        id: list
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: Intervals
        Controls.ScrollBar.vertical: Controls.ScrollBar {}

        delegate: Controls.ItemDelegate {
            id: row

            required property string intervalId
            required property string name
            required property var everyM
            required property var everyMonths
            required property bool active
            required property string status
            required property var dueOdometerM
            required property string dueDate
            required property var remainingM
            required property var remainingDays

            width: ListView.view.width
            onClicked: tab.formRequested(intervalId)
            opacity: active ? 1.0 : 0.6

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
                        text: row.name
                        elide: Text.ElideRight
                    }

                    Controls.Label {
                        Layout.fillWidth: true
                        // "every 15.000 km / 12 months · due at 60.000 km / 01.03.2027"
                        readonly property string dueAt: [row.dueOdometerM === null ? "" : Format.distance(row.dueOdometerM),
                                                         row.dueDate === "" ? "" : Format.date(row.dueDate)]
                            .filter(part => part !== "").join(" / ")
                        text: [tab.ruleText(row.everyM, row.everyMonths), dueAt === "" ? "" : i18n("due at %1", dueAt)]
                            .filter(part => part !== "").join(" · ")
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
            text: i18n("No intervals yet")
            explanation: i18n("Add rules like “oil change every 15,000 km or 12 months”. The due date is calculated from the last matching service entry.")
            helpfulAction: Kirigami.Action {
                text: i18n("Add Interval")
                icon.name: "list-add"
                onTriggered: tab.formRequested("")
            }
        }
    }
}
