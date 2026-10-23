// Page of one vehicle (role: qml, pure UI).
// Header with icon, name, details and current odometer, below it the tabs
// fuel | service | intervals | statistics (SPEC §6). Fuel and service have
// content so far; the others are filled in later milestones.

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
    /** Fuel form for an entry; entryId "" = new entry. */
    signal fuelFormRequested(string entryId)
    /** Service form for an entry; entryId "" = new entry. */
    signal serviceFormRequested(string entryId)

    function reload() {
        vehicle = Vehicles.get(vehicleId);
    }

    title: vehicle.name ?? ""

    actions: [
        Kirigami.Action {
            // One "add" button whose meaning follows the visible tab.
            text: tabBar.currentIndex === 1 ? i18n("Add Service") : i18n("Add Entry")
            icon.name: "list-add"
            visible: tabBar.currentIndex <= 1
            shortcut: StandardKey.New
            onTriggered: tabBar.currentIndex === 1 ? page.serviceFormRequested("") : page.fuelFormRequested("")
        },
        Kirigami.Action {
            text: i18n("Edit Vehicle")
            icon.name: "document-edit"
            onTriggered: page.editRequested()
        }
    ]

    // FuelEntries/ServiceEntries are singletons shared by all pages; only one
    // vehicle page exists at a time, so this page points them at its vehicle.
    onVehicleIdChanged: {
        reload();
        FuelEntries.vehicleId = vehicleId;
        ServiceEntries.vehicleId = vehicleId;
    }

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

        // TabBar (Qt Quick Controls) only draws the tab buttons; the
        // StackLayout below shows the child whose index matches currentIndex.
        Controls.TabBar {
            id: tabBar
            Layout.fillWidth: true

            Controls.TabButton {
                text: i18n("Fuel")
            }
            Controls.TabButton {
                text: i18n("Service")
            }
            Controls.TabButton {
                text: i18n("Intervals")
            }
            Controls.TabButton {
                text: i18n("Statistics")
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex

            FuelTab {
                onFormRequested: entryId => page.fuelFormRequested(entryId)
            }

            ServiceTab {
                onFormRequested: entryId => page.serviceFormRequested(entryId)
            }

            // Empty until their milestones (intervals M10, statistics M12).
            // The Item wrapper gets the full tab size from the StackLayout, so the
            // message can be centred inside it.
            Repeater {
                model: 2
                Item {
                    Kirigami.PlaceholderMessage {
                        anchors.centerIn: parent
                        width: parent.width - Kirigami.Units.gridUnit * 4
                        text: i18n("Coming soon")
                    }
                }
            }
        }
    }
}
