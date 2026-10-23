// Side bar with the vehicle list (role: qml, pure UI).
// Lists the vehicles from the Vehicles model and offers "Due", "Add Vehicle",
// "Show Archived", the "Data" menu and "Settings". Navigation and dialogs
// are left to Main.qml via signals.

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigami.delegates as Delegates
import org.tmx.garagelog

// GlobalDrawer is Kirigami's application side bar. On wide windows it is
// docked permanently (modal: false), on narrow ones it slides in over the
// content and is opened via the hamburger handle.
Kirigami.GlobalDrawer {
    id: drawer

    /** Highlighted vehicle, set by Main.qml. */
    property string currentVehicleId: ""

    signal vehicleSelected(string vehicleId)
    signal addRequested()
    signal dueRequested()
    signal settingsRequested()
    /** One of "exportJson", "importJson", "exportCsv", "importCsv". */
    signal dataRequested(string what)

    /** Runs a drawer entry and closes the drawer on narrow windows. */
    function trigger(callback) {
        callback();
        if (modal) {
            close();
        }
    }

    // applicationWindow() is a Kirigami helper returning the ApplicationWindow
    // this item lives in; wideScreen flips when the window gets narrow.
    modal: !applicationWindow().wideScreen
    handleVisible: modal

    // Kirigami.Action is a QtQuick Action with icon, text and extras like
    // checkable/visible; the drawer renders its actions as menu entries.
    actions: [
        Kirigami.Action {
            // The overdue count makes reminders visible without opening the page.
            text: DueIntervals.overdueCount > 0 ? i18n("Due (%1 overdue)", DueIntervals.overdueCount) : i18n("Due")
            icon.name: "view-calendar-upcoming-events"
            onTriggered: {
                drawer.dueRequested();
                if (drawer.modal) {
                    drawer.close();
                }
            }
        },
        Kirigami.Action {
            text: i18n("Add Vehicle")
            icon.name: "list-add"
            onTriggered: drawer.addRequested()
        },
        Kirigami.Action {
            text: i18n("Show Archived")
            icon.name: "archive-extract"
            checkable: true
            checked: Vehicles.showArchived
            onToggled: Vehicles.showArchived = checked
        },
        // An action with children becomes a submenu in the drawer.
        Kirigami.Action {
            text: i18n("Data")
            icon.name: "document-export"
            Kirigami.Action {
                text: i18n("Export Everything (JSON)…")
                icon.name: "document-export"
                onTriggered: drawer.trigger(() => drawer.dataRequested("exportJson"))
            }
            Kirigami.Action {
                text: i18n("Import Export Folder (JSON)…")
                icon.name: "document-import"
                onTriggered: drawer.trigger(() => drawer.dataRequested("importJson"))
            }
            Kirigami.Action {
                text: i18n("Export Tables (CSV)…")
                icon.name: "text-csv"
                onTriggered: drawer.trigger(() => drawer.dataRequested("exportCsv"))
            }
            Kirigami.Action {
                text: i18n("Import Fuel Entries (CSV)…")
                icon.name: "text-csv"
                onTriggered: drawer.trigger(() => drawer.dataRequested("importCsv"))
            }
        },
        Kirigami.Action {
            text: i18n("Settings")
            icon.name: "settings-configure"
            onTriggered: drawer.trigger(() => drawer.settingsRequested())
        }
    ]

    // topContent is placed above the actions, so the vehicles come first.
    topContent: [
        ListView {
            id: list
            Layout.fillWidth: true
            // The drawer scrolls as a whole; the list itself shows all rows.
            implicitHeight: contentHeight
            interactive: false
            model: Vehicles

            // SubtitleDelegate is a Kirigami list row with icon, title and a
            // smaller subtitle line underneath, here the odometer.
            delegate: Delegates.SubtitleDelegate {
                // "required property" pulls the matching model role into the
                // delegate explicitly (Qt 6 style) instead of the implicit model.xxx.
                required property string vehicleId
                required property string name
                required property string iconName
                required property var odometerM
                required property bool archived

                width: ListView.view.width
                text: name
                subtitle: archived ? i18n("%1 · archived", Format.distance(odometerM)) : Format.distance(odometerM)
                icon.name: iconName
                highlighted: vehicleId === drawer.currentVehicleId
                opacity: archived ? 0.6 : 1.0

                onClicked: {
                    drawer.vehicleSelected(vehicleId);
                    if (drawer.modal) {
                        drawer.close();
                    }
                }
            }
        }
    ]
}
