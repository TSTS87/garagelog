// Fuel tab of the vehicle page (role: qml, pure UI).
// Lists the fuel entries of FuelEntries.vehicleId, newest first: date,
// station, odometer, quantity, price and consumption. A click asks for the
// edit form; numbers come pre-calculated from C++ and are only formatted here.

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.tmx.garagelog

ColumnLayout {
    id: tab

    /** entryId "" = new entry. Main.qml opens the form (ADR-008). */
    signal formRequested(string entryId)

    spacing: 0

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        type: Kirigami.MessageType.Error
        text: FuelEntries.errorString
        visible: text !== ""
    }

    ListView {
        id: list
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: FuelEntries
        Controls.ScrollBar.vertical: Controls.ScrollBar {}

        // ItemDelegate is the clickable list row of Qt Quick Controls; the
        // KDE style draws it like a Plasma list item. contentItem replaces
        // its single text with our two-line layout.
        delegate: Controls.ItemDelegate {
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

            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    Controls.Label {
                        Layout.fillWidth: true
                        text: [Format.date(row.date), row.station].filter(part => part !== "").join(" · ")
                        elide: Text.ElideRight
                    }

                    // Secondary line in the smaller, greyed font Kirigami uses for details.
                    Controls.Label {
                        Layout.fillWidth: true
                        text: [Format.distance(row.odometerM),
                               Format.quantity(row.quantity, FuelEntries.electric),
                               row.unitPriceMilli === null ? "" : Format.unitPrice(row.unitPriceMilli, FuelEntries.electric),
                               row.fullTank ? "" : i18n("partial")]
                            .filter(part => part !== "").join(" · ")
                        font: Kirigami.Theme.smallFont
                        color: Kirigami.Theme.disabledTextColor
                        elide: Text.ElideRight
                    }
                }

                ColumnLayout {
                    spacing: 0

                    Controls.Label {
                        Layout.alignment: Qt.AlignRight
                        text: Format.money(row.totalCostC)
                    }

                    // Only a full tank after an earlier full tank has a consumption.
                    Controls.Label {
                        Layout.alignment: Qt.AlignRight
                        text: row.hasConsumption ? Format.consumption(row.consumptionPer100Km, FuelEntries.electric) : "–"
                        font: Kirigami.Theme.smallFont
                        color: Kirigami.Theme.disabledTextColor
                    }
                }
            }
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            visible: list.count === 0
            icon.name: "car" // Breeze has no fuel pump icon
            text: FuelEntries.electric ? i18n("No charging entries yet") : i18n("No fuel entries yet")
            helpfulAction: Kirigami.Action {
                text: i18n("Add Entry")
                icon.name: "list-add"
                onTriggered: tab.formRequested("")
            }
        }
    }
}
