// Intervals tab of the vehicle page (role: qml, pure UI).
// Lists the maintenance intervals of Intervals.vehicleId, most urgent
// first, in a card: status-coloured tile, rule ("every 15.000 km / 12
// months") and a status chip with what is left until due. A click asks for the edit form.

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.tmx.garagelog

ListCard {
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

    model: Intervals
    errorText: Intervals.errorString
    topMargin: Kirigami.Units.smallSpacing

    delegate: EntryRow {
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

        /** "due at 60.000 km / 01.03.2027"; empty without a recorded service. */
        readonly property string dueAt: Format.dueAt(dueOdometerM, dueDate)

        width: ListView.view.width
        onClicked: tab.formRequested(intervalId)
        opacity: active ? 1.0 : 0.6

        iconSource: "qrc:/qml/icons/interval.svg"
        // The tile takes the status colour, so urgency shows before reading.
        accent: chip.statusColor
        title: name
        subtitle: [tab.ruleText(everyM, everyMonths), dueAt === "" ? "" : i18n("due at %1", dueAt)]
            .filter(part => part !== "").join(" · ")

        StatusChip {
            id: chip
            status: row.status
            text: Format.dueSummary(row.remainingM, row.remainingDays, row.status)
        }
    }

    emptyState: EmptyState {
        iconSource: "qrc:/qml/icons/interval.svg"
        text: i18n("No intervals yet")
        explanation: i18n("Add rules like “oil change every 15,000 km or 12 months”. The due date is calculated from the last matching service entry.")
        helpfulAction: Kirigami.Action {
            text: i18n("Add Interval")
            icon.name: "list-add"
            onTriggered: tab.formRequested("")
        }
    }
}
