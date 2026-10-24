// Page of one vehicle (role: qml, pure UI).
// Hero card with name, plate, odometer and the next due interval, below it
// a pill switch for fuel | service | intervals | statistics (SPEC §6,
// look from ADR-015) and a floating "add" button.

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
    /** Interval form; intervalId "" = new interval. */
    signal intervalFormRequested(string intervalId)

    function reload() {
        vehicle = Vehicles.get(vehicleId);
    }

    // The hero card shows the name in large; the toolbar names the section.
    title: sections[tabBar.currentIndex]?.text ?? ""

    actions: [
        Kirigami.Action {
            id: addAction
            // One "add" button whose meaning follows the visible tab.
            text: [i18n("Add Entry"), i18n("Add Service"), i18n("Add Interval")][tabBar.currentIndex] ?? ""
            icon.name: "list-add"
            visible: tabBar.currentIndex <= 2
            shortcut: StandardKey.New
            onTriggered: {
                if (tabBar.currentIndex === 0) {
                    page.fuelFormRequested("");
                } else if (tabBar.currentIndex === 1) {
                    page.serviceFormRequested("");
                } else {
                    page.intervalFormRequested("");
                }
            }
        },
        Kirigami.Action {
            text: i18n("Edit Vehicle")
            icon.name: "document-edit"
            onTriggered: page.editRequested()
        }
    ]

    // FuelEntries/ServiceEntries/Intervals/Stats are singletons shared by all pages;
    // only one vehicle page exists at a time, so it points them at its vehicle.
    onVehicleIdChanged: {
        reload();
        FuelEntries.vehicleId = vehicleId;
        ServiceEntries.vehicleId = vehicleId;
        Intervals.vehicleId = vehicleId;
        Stats.vehicleId = vehicleId;
    }

    // Connections listens to signals of an object defined elsewhere, here the
    // C++ model: any save/archive reloads it, and this page follows along.
    Connections {
        target: Vehicles
        function onReloaded() {
            page.reload();
        }
    }

    /** Accent of this vehicle's type; the tabs colour their icon tiles with it. */
    readonly property color accent: Style.accent(vehicle.type ?? "other")
    /** Sections of the pill switch, in the order of the StackLayout children. */
    readonly property var sections: [
        { text: i18n("Fuel"), icon: FuelEntries.electric ? "qrc:/qml/icons/charge.svg" : "qrc:/qml/icons/fuel.svg" },
        { text: i18n("Service"), icon: "qrc:/qml/icons/service.svg" },
        { text: i18n("Intervals"), icon: "qrc:/qml/icons/interval.svg" },
        { text: i18n("Statistics"), icon: "qrc:/qml/icons/stats.svg" }
    ]

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

        HeroCard {
            Layout.fillWidth: true
            vehicle: page.vehicle
            // Low windows and the statistics (chart and tiles need the height)
            // get the slim variant.
            compact: page.height < Kirigami.Units.gridUnit * 40 || tabBar.currentIndex === 3
        }

        PillBar {
            id: tabBar
            objectName: "sectionBar"
            Layout.fillWidth: true
            model: page.sections
            accent: page.accent
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex

            FuelTab {
                accent: page.accent
                onFormRequested: entryId => page.fuelFormRequested(entryId)
            }

            ServiceTab {
                accent: page.accent
                onFormRequested: entryId => page.serviceFormRequested(entryId)
            }

            IntervalTab {
                onFormRequested: intervalId => page.intervalFormRequested(intervalId)
            }

            StatsTab {
                accent: page.accent
            }
        }
    }

    // Floating action button: the most frequent action (a new entry) one
    // click away in the corner, like in current mobile and desktop apps.
    // It runs the same toolbar action, so Ctrl+N and the button agree.
    Kirigami.ShadowedRectangle {
        id: fab
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Kirigami.Units.largeSpacing
        visible: addAction.visible
        width: Kirigami.Units.gridUnit * 3.2
        height: width
        radius: Kirigami.Units.gridUnit
        color: fabArea.pressed ? Qt.darker(page.accent, 1.2) : fabArea.containsMouse ? Qt.lighter(page.accent, 1.1) : page.accent
        shadow.size: Kirigami.Units.gridUnit
        shadow.yOffset: 3
        shadow.color: Qt.rgba(0, 0, 0, 0.25)

        Kirigami.Icon {
            anchors.centerIn: parent
            width: Kirigami.Units.iconSizes.smallMedium
            height: width
            source: "list-add"
            isMask: true
            color: "white"
        }
        MouseArea {
            id: fabArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: addAction.trigger()
        }
        Controls.ToolTip.visible: fabArea.containsMouse
        Controls.ToolTip.text: addAction.text
    }
}
