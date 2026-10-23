// Page of one vehicle (role: qml, pure UI).
// M04: header with icon, name, details and current odometer. The tabs
// (fuel | service | intervals | statistics) follow in M06.

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.tmx.garagelog

Kirigami.Page {
    id: page

    /** Id of the shown vehicle; set by Main.qml when pushing the page. */
    property string vehicleId: ""

    /** Values from Vehicles.get(); re-read after every model change. */
    property var vehicle: ({})

    signal editRequested()

    function reload() {
        vehicle = Vehicles.get(vehicleId);
    }

    title: vehicle.name ?? ""

    actions: [
        Kirigami.Action {
            text: i18n("Edit")
            icon.name: "document-edit"
            onTriggered: page.editRequested()
        }
    ]

    onVehicleIdChanged: reload()

    // Connections listens to signals of an object defined elsewhere, here the
    // C++ model: any save/archive reloads it, and this page follows along.
    Connections {
        target: Vehicles
        function onReloaded() {
            page.reload();
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.largeSpacing

        // InlineMessage is Kirigami's in-page banner for errors and hints
        // (SPEC §3: show errors, do not swallow them); it can carry actions.
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: page.vehicle.archived === true
            type: Kirigami.MessageType.Information
            text: i18n("This vehicle is archived. It is hidden from the vehicle list, its entries are kept.")
            actions: [
                Kirigami.Action {
                    text: i18n("Restore")
                    icon.name: "archive-extract"
                    onTriggered: Vehicles.setArchived(page.vehicleId, false)
                }
            ]
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            // Kirigami.Icon renders a freedesktop icon by name in any size,
            // following the colour scheme; Units.iconSizes keeps sizes consistent.
            Kirigami.Icon {
                source: page.vehicle.icon ?? "car"
                implicitWidth: Kirigami.Units.iconSizes.huge
                implicitHeight: Kirigami.Units.iconSizes.huge
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Heading {
                    Layout.fillWidth: true
                    level: 1
                    text: page.vehicle.name ?? ""
                    elide: Text.ElideRight
                }

                // "Honda CB500F 2019 · M-AB 123"; empty parts are left out.
                Controls.Label {
                    Layout.fillWidth: true
                    readonly property string makeModelYear: [page.vehicle.make, page.vehicle.model, page.vehicle.year]
                        .filter(part => part !== undefined && part !== null && part !== "").join(" ")
                    text: [makeModelYear, page.vehicle.plate ?? ""].filter(part => part !== "").join(" · ")
                    visible: text !== ""
                    elide: Text.ElideRight
                }

                Controls.Label {
                    text: i18n("Odometer: %1", Format.distance(page.vehicle.odometerM ?? 0))
                }
            }
        }

        // Pushes the header to the top until the tabs fill this space (M06).
        Item {
            Layout.fillHeight: true
        }
    }
}
