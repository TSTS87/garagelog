// Root QML file of GarageLog (role: qml, pure UI).
// Main window with the vehicle drawer and the page stack. Owns navigation:
// which vehicle page is shown and when the vehicle form opens (SPEC §6).

import QtQuick
import org.kde.kirigami as Kirigami
import org.tmx.garagelog

// Kirigami.ApplicationWindow is the Kirigami top-level window: it brings a
// pageStack (PageRow) plus slots for globalDrawer/contextDrawer.
Kirigami.ApplicationWindow {
    id: root

    /** Vehicle whose page is shown; empty while the placeholder is shown. */
    property string currentVehicleId: ""

    title: i18n("GarageLog")
    width: Kirigami.Units.gridUnit * 50
    height: Kirigami.Units.gridUnit * 35

    globalDrawer: VehicleDrawer {
        currentVehicleId: root.currentVehicleId
        onVehicleSelected: vehicleId => root.showVehicle(vehicleId)
        onAddRequested: root.openForm("")
    }

    /** Replaces everything in the page stack with the page of one vehicle. */
    function showVehicle(vehicleId) {
        currentVehicleId = vehicleId;
        pageStack.clear();
        pageStack.push(vehiclePageComponent, { vehicleId: vehicleId });
    }

    /** Placeholder shown when the garage has no (active) vehicle yet. */
    function showPlaceholder() {
        currentVehicleId = "";
        pageStack.clear();
        pageStack.push(placeholderComponent);
    }

    /**
     * Opens the vehicle form next to the current page; on wide windows the
     * PageRow shows both as columns. vehicleId "" = add a new vehicle.
     */
    function openForm(vehicleId) {
        // At most one form at a time: drop an already open one first.
        while (pageStack.depth > 1) {
            pageStack.pop();
        }
        pageStack.push(formComponent, { vehicleId: vehicleId });
    }

    // Pages are created from Components on demand, so each push gets a fresh
    // page and the PageRow destroys it again when it is popped or cleared.
    Component {
        id: vehiclePageComponent
        VehiclePage {
            onEditRequested: root.openForm(vehicleId)
        }
    }

    Component {
        id: formComponent
        VehicleFormPage {
            onSaved: vehicleId => root.showVehicle(vehicleId)
            onCanceled: root.pageStack.pop()
        }
    }

    Component {
        id: placeholderComponent
        // Kirigami.Page is the basic page type of a PageRow: it has a title
        // shown in the toolbar and an actions list rendered as buttons there.
        Kirigami.Page {
            title: i18n("GarageLog")

            // PlaceholderMessage is Kirigami's standard "empty state": icon,
            // text, explanation and an optional button for the obvious next step.
            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                width: parent.width - Kirigami.Units.gridUnit * 4
                icon.name: "car"
                text: i18n("No vehicles yet")
                explanation: i18n("Add your car, motorcycle or scooter to start tracking fuel and service.")
                helpfulAction: Kirigami.Action {
                    text: i18n("Add Vehicle")
                    icon.name: "list-add"
                    onTriggered: root.openForm("")
                }
            }
        }
    }

    Component.onCompleted: {
        if (Vehicles.count > 0) {
            showVehicle(Vehicles.idAt(0));
        } else {
            showPlaceholder();
        }
    }
}
