// Fuel tab of the vehicle page (role: qml, pure UI).
// Lists the fuel entries of FuelEntries.vehicleId, newest first and grouped
// by month, in a card: station, date, odometer, quantity, price, consumption. A click asks for the
// edit form; numbers come pre-calculated from C++ and are only formatted here.

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.tmx.garagelog

ColumnLayout {
    id: tab

    /** Accent of the vehicle type, for the icon tiles. */
    property color accent: Kirigami.Theme.highlightColor

    /** entryId "" = new entry. Main.qml opens the form (ADR-008). */
    signal formRequested(string entryId)

    spacing: Kirigami.Units.largeSpacing

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        type: Kirigami.MessageType.Error
        text: FuelEntries.errorString
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
            model: FuelEntries
            // Room below the last row, so the floating button never covers it.
            bottomMargin: Kirigami.Units.gridUnit * 4
            Controls.ScrollBar.vertical: Controls.ScrollBar {}

            // Sections group consecutive rows with the same "month" role
            // (YYYY-MM from C++) under one heading; the list is sorted by date.
            section.property: "month"
            section.delegate: SectionHeader {
                required property string section
                text: Format.month(section)
            }

            delegate: EntryRow {
                id: row

                // "required property" pulls the model roles into the delegate by
                // name; QML then checks at load time that the roles exist.
                required property string entryId
                required property string date
                required property var odometerM
                required property var quantity
                required property var totalCostC
                required property var unitPriceMilli
                required property bool fullTank
                required property string station
                required property bool hasConsumption
                required property real consumptionPer100Km

                width: ListView.view.width
                onClicked: tab.formRequested(entryId)

                iconSource: FuelEntries.electric ? "qrc:/qml/icons/charge.svg" : "qrc:/qml/icons/fuel.svg"
                accent: tab.accent
                // A partial fill has no consumption of its own; the weaker tile says so at a glance.
                iconMuted: !fullTank
                title: station !== "" ? station : (FuelEntries.electric ? i18n("Charge") : i18n("Fuel"))
                subtitle: [Format.date(date), Format.distance(odometerM),
                           Format.quantity(quantity, FuelEntries.electric),
                           unitPriceMilli === null ? "" : Format.unitPrice(unitPriceMilli, FuelEntries.electric),
                           fullTank ? "" : i18n("partial")]
                    .filter(part => part !== "").join(" · ")
                value: Format.money(totalCostC)
                // Only a full tank after an earlier full tank has a consumption.
                detail: hasConsumption ? Format.consumption(consumptionPer100Km, FuelEntries.electric) : ""
            }

            EmptyState {
                anchors.centerIn: parent
                width: parent.width - Kirigami.Units.gridUnit * 4
                visible: list.count === 0
                accent: tab.accent
                iconSource: FuelEntries.electric ? "qrc:/qml/icons/charge.svg" : "qrc:/qml/icons/fuel.svg"
                text: FuelEntries.electric ? i18n("No charging entries yet") : i18n("No fuel entries yet")
                explanation: i18n("Record every refuelling or charge; consumption is calculated between two full tanks.")
                helpfulAction: Kirigami.Action {
                    text: i18n("Add Entry")
                    icon.name: "list-add"
                    onTriggered: tab.formRequested("")
                }
            }
        }
    }
}
