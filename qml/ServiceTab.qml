// Service tab of the vehicle page (role: qml, pure UI).
// Lists the service entries of ServiceEntries.vehicleId, newest first:
// date, category, description, cost and a paperclip if files are attached.
// A click asks for the edit form.

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
        text: ServiceEntries.errorString
        visible: text !== ""
    }

    ListView {
        id: list
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: ServiceEntries
        Controls.ScrollBar.vertical: Controls.ScrollBar {}

        delegate: Controls.ItemDelegate {
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

            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    Controls.Label {
                        Layout.fillWidth: true
                        text: [Format.date(row.date), row.categoryName].join(" · ")
                        elide: Text.ElideRight
                    }

                    Controls.Label {
                        Layout.fillWidth: true
                        // odometerM is null for odometer-independent costs like tax.
                        text: [row.description,
                               row.odometerM === null ? "" : Format.distance(row.odometerM),
                               row.performedBy === "shop" ? (row.shopName !== "" ? row.shopName : i18n("workshop"))
                                   : row.performedBy === "self" ? i18n("self") : ""]
                            .filter(part => part !== "").join(" · ")
                        font: Kirigami.Theme.smallFont
                        color: Kirigami.Theme.disabledTextColor
                        elide: Text.ElideRight
                    }
                }

                Kirigami.Icon {
                    visible: row.hasAttachment
                    source: "mail-attachment"
                    implicitWidth: Kirigami.Units.iconSizes.small
                    implicitHeight: Kirigami.Units.iconSizes.small
                    Controls.ToolTip.text: i18n("Has attachments")
                }

                Controls.Label {
                    text: Format.money(row.costC)
                }
            }
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            visible: list.count === 0
            icon.name: "tools"
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
