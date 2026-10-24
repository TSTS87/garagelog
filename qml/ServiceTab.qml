// Service tab of the vehicle page (role: qml, pure UI).
// Lists the service entries of ServiceEntries.vehicleId, newest first and
// grouped by month, in a card: category, date, description, cost and a
// paperclip if files are attached.
// A click asks for the edit form.

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
        text: ServiceEntries.errorString
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
            model: ServiceEntries
            bottomMargin: Kirigami.Units.gridUnit * 4
            Controls.ScrollBar.vertical: Controls.ScrollBar {}

            section.property: "month"
            section.delegate: SectionHeader {
                required property string section
                text: Format.month(section)
            }

            delegate: EntryRow {
                id: row

                required property string entryId
                required property string date
                required property var odometerM
                required property string categoryName
                required property string description
                required property var costC
                required property string performedBy
                required property string shopName
                required property bool hasAttachment

                width: ListView.view.width
                onClicked: tab.formRequested(entryId)

                iconSource: "qrc:/qml/icons/service.svg"
                accent: tab.accent
                title: categoryName
                // odometerM is null for odometer-independent costs like tax.
                subtitle: [Format.date(date), description,
                           odometerM === null ? "" : Format.distance(odometerM),
                           performedBy === "shop" ? (shopName !== "" ? shopName : i18n("workshop"))
                               : performedBy === "self" ? i18n("self") : ""]
                    .filter(part => part !== "").join(" · ")
                value: Format.money(costC)

                Kirigami.Icon {
                    visible: row.hasAttachment
                    source: "mail-attachment"
                    implicitWidth: Kirigami.Units.iconSizes.small
                    implicitHeight: Kirigami.Units.iconSizes.small
                    Controls.ToolTip.text: i18n("Has attachments")
                }
            }

            EmptyState {
                anchors.centerIn: parent
                width: parent.width - Kirigami.Units.gridUnit * 4
                visible: list.count === 0
                accent: tab.accent
                iconSource: "qrc:/qml/icons/service.svg"
                text: i18n("No service entries yet")
                explanation: i18n("Record oil changes, repairs, inspections and recurring costs like insurance.")
                helpfulAction: Kirigami.Action {
                    text: i18n("Add Service")
                    icon.name: "list-add"
                    onTriggered: tab.formRequested("")
                }
            }
        }
    }
}
