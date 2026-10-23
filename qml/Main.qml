// Root QML file of GarageLog (role: qml, pure UI).
// Main window with the vehicle drawer and the page stack. Owns navigation:
// which page is shown (vehicle, due overview, settings) and when a form
// opens (SPEC §6), plus the folder dialogs of the "Data" menu.

import QtQuick
import QtQuick.Dialogs
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
        onDueRequested: root.showDue()
        onSettingsRequested: root.showSettings()
        onDataRequested: what => {
            if (what === "importCsv") {
                root.openCsvImport();
            } else {
                folderDialog.purpose = what;
                folderDialog.open();
            }
        }
    }

    /** First vehicle, or the placeholder if the garage has none. */
    function showHome() {
        if (Vehicles.count > 0) {
            showVehicle(Vehicles.idAt(0));
        } else {
            showPlaceholder();
        }
    }

    function showSettings() {
        currentVehicleId = "";
        pageStack.clear();
        pageStack.push(settingsComponent);
    }

    /** CSV import as a column next to the current page, preselecting the shown vehicle. */
    function openCsvImport() {
        if (Vehicles.count === 0) {
            showPassiveNotification(i18n("Add a vehicle first; fuel entries are imported into a vehicle."));
            return;
        }
        pushForm(csvImportComponent, { vehicleId: currentVehicleId !== "" ? currentVehicleId : Vehicles.idAt(0) });
    }

    /** Shows the outcome of an export/import from DataTransfer as a passive notification. */
    function reportTransfer(ok, successText) {
        if (!ok) {
            showPassiveNotification(DataTransfer.errorString, "long");
            return;
        }
        const warnings = DataTransfer.warnings.length > 0 ? " " + i18np("(%1 warning)", "(%1 warnings)", DataTransfer.warnings.length) : "";
        showPassiveNotification(successText + warnings, "long");
    }

    // One folder dialog for the three folder-based actions of the Data menu.
    FolderDialog {
        id: folderDialog
        property string purpose: ""
        title: purpose === "importJson" ? i18n("Choose Export Folder to Import") : i18n("Choose Folder for the Export")
        currentFolder: Garage.folderUrl
        onAccepted: {
            if (purpose === "exportJson") {
                root.reportTransfer(DataTransfer.exportJson(selectedFolder), i18n("Exported to %1", DataTransfer.lastPath));
            } else if (purpose === "exportCsv") {
                root.reportTransfer(DataTransfer.exportCsv(selectedFolder), i18n("Exported to %1", DataTransfer.lastPath));
            } else if (purpose === "importJson") {
                const ok = DataTransfer.importJson(selectedFolder);
                root.reportTransfer(ok, i18n("Import finished"));
                if (ok) {
                    root.showHome();
                }
            }
        }
    }

    // After switching the garage the shown vehicle no longer exists; the
    // settings page stays open because the switch was made there.
    Connections {
        target: Garage
        function onGarageChanged() {
            if (root.pageStack.currentItem?.objectName !== "settingsPage") {
                root.showHome();
            }
        }
    }

    /** Replaces everything in the page stack with the page of one vehicle. */
    function showVehicle(vehicleId) {
        currentVehicleId = vehicleId;
        pageStack.clear();
        pageStack.push(vehiclePageComponent, { vehicleId: vehicleId });
    }

    /** Due overview of all vehicles; no vehicle is highlighted while it is shown. */
    function showDue() {
        currentVehicleId = "";
        pageStack.clear();
        pageStack.push(duePageComponent);
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
        pushForm(formComponent, { vehicleId: vehicleId });
    }

    /** Opens the fuel form for an entry of the shown vehicle; entryId "" = new entry. */
    function openFuelForm(entryId) {
        pushForm(fuelFormComponent, { entryId: entryId });
    }

    /** Opens the service form for an entry of the shown vehicle; entryId "" = new entry. */
    function openServiceForm(entryId) {
        pushForm(serviceFormComponent, { entryId: entryId });
    }

    /** Opens the interval form for the shown vehicle; intervalId "" = new interval. */
    function openIntervalForm(intervalId) {
        pushForm(intervalFormComponent, { intervalId: intervalId });
    }

    function pushForm(component, properties) {
        // At most one form at a time: drop an already open one first.
        while (pageStack.depth > 1) {
            pageStack.pop();
        }
        pageStack.push(component, properties);
    }

    // Pages are created from Components on demand, so each push gets a fresh
    // page and the PageRow destroys it again when it is popped or cleared.
    Component {
        id: vehiclePageComponent
        VehiclePage {
            onEditRequested: root.openForm(vehicleId)
            onFuelFormRequested: entryId => root.openFuelForm(entryId)
            onServiceFormRequested: entryId => root.openServiceForm(entryId)
            onIntervalFormRequested: intervalId => root.openIntervalForm(intervalId)
        }
    }

    Component {
        id: intervalFormComponent
        IntervalFormPage {
            onSaved: root.pageStack.pop()
            onRemoved: root.pageStack.pop()
            onCanceled: root.pageStack.pop()
        }
    }

    Component {
        id: settingsComponent
        SettingsPage {}
    }

    Component {
        id: csvImportComponent
        CsvImportPage {
            onImported: root.pageStack.pop()
            onCanceled: root.pageStack.pop()
        }
    }

    Component {
        id: duePageComponent
        DuePage {
            onVehicleRequested: vehicleId => root.showVehicle(vehicleId)
        }
    }

    Component {
        id: serviceFormComponent
        ServiceFormPage {
            onSaved: root.pageStack.pop()
            onRemoved: root.pageStack.pop()
            onCanceled: root.pageStack.pop()
        }
    }

    // The list updates itself through FuelEntries, so closing the form is enough.
    Component {
        id: fuelFormComponent
        FuelFormPage {
            onSaved: root.pageStack.pop()
            onRemoved: root.pageStack.pop()
            onCanceled: root.pageStack.pop()
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
                // Only archived vehicles left: the garage is not empty, they are just hidden.
                readonly property bool onlyArchived: Vehicles.archivedCount > 0

                anchors.centerIn: parent
                width: parent.width - Kirigami.Units.gridUnit * 4
                icon.name: "car"
                text: onlyArchived ? i18n("All vehicles are archived") : i18n("No vehicles yet")
                explanation: onlyArchived
                    ? i18n("Show the archived vehicles to look at their entries, or add a new vehicle from the side bar.")
                    : i18n("Add your car, motorcycle or scooter to start tracking fuel and service.")
                helpfulAction: onlyArchived ? showArchivedAction : addVehicleAction

                Kirigami.Action {
                    id: addVehicleAction
                    text: i18n("Add Vehicle")
                    icon.name: "list-add"
                    onTriggered: root.openForm("")
                }
                Kirigami.Action {
                    id: showArchivedAction
                    text: i18n("Show Archived")
                    icon.name: "archive-extract"
                    onTriggered: {
                        Vehicles.showArchived = true;
                        root.showHome();
                    }
                }
            }
        }
    }

    Component.onCompleted: showHome()
}
