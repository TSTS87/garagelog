// Root QML file of GarageLog (role: qml, pure UI).
// Main window with the vehicle drawer and the page stack. Owns navigation:
// which page is shown (vehicle, due overview, settings) and when a form
// opens (SPEC §6), plus the folder dialogs of the "Data" menu and the
// warning when the garage file is deleted or write-protected (M15).

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
    /** Page family shown in the main column: "vehicle", "due", "settings" or "" (placeholder); the drawer highlights it. */
    property string currentSection: ""

    title: i18n("GarageLog")
    width: Kirigami.Units.gridUnit * 50
    height: Kirigami.Units.gridUnit * 35

    // The watcher may miss changes (network shares), so look again whenever
    // the user comes back to the window.
    onActiveChanged: if (active) Garage.checkFile()

    // header is the slot of the ApplicationWindow above all pages: the
    // warning stays visible whatever page is open, and disappears by itself
    // once Garage.fileState is "ok" again (file restored or copy saved).
    header: Item {
        visible: Garage.fileState !== "ok"
        implicitHeight: fileMessage.implicitHeight

        // Kirigami shifts the header left by the width of the docked drawer;
        // undoing that offset here keeps the message in the window's full width.
        Kirigami.InlineMessage {
            id: fileMessage

            readonly property bool missing: Garage.fileState === "missing"

            x: -parent.x
            width: root.width
            visible: true
            position: Kirigami.InlineMessage.Position.Header
            type: missing ? Kirigami.MessageType.Error : Kirigami.MessageType.Warning
            text: missing
                ? i18n("The garage file %1 was deleted or moved. GarageLog still has your data open, but changes are lost when it closes. Save a copy to keep them.", Garage.filePath)
                : i18n("The garage file %1 is write-protected. You can look at your data, but changes cannot be saved. Save a copy to keep working.", Garage.filePath)
            actions: [
                Kirigami.Action {
                    text: i18n("Save Copy…")
                    icon.name: "document-save-as"
                    onTriggered: rescueDialog.open()
                }
            ]
        }
    }

    // Copies the open garage (still readable even if deleted, Linux keeps
    // the open file alive) and continues in the copy.
    FileDialog {
        id: rescueDialog
        title: i18n("Save Garage As")
        fileMode: FileDialog.SaveFile
        currentFolder: Garage.folderUrl
        defaultSuffix: "sqlite"
        nameFilters: [i18n("GarageLog garages (*.sqlite)")]
        onAccepted: root.showPassiveNotification(Garage.saveGarageAs(selectedFile)
            ? i18n("Garage saved; you are now working in the copy") : Garage.errorString, "long")
    }

    globalDrawer: VehicleDrawer {
        currentVehicleId: root.currentVehicleId
        dueShown: root.currentSection === "due"
        settingsShown: root.currentSection === "settings"
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
        currentSection = "settings";
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
        currentSection = "vehicle";
        pageStack.clear();
        pageStack.push(vehiclePageComponent, { vehicleId: vehicleId });
    }

    /** Due overview of all vehicles; no vehicle is highlighted while it is shown. */
    function showDue() {
        currentVehicleId = "";
        currentSection = "due";
        pageStack.clear();
        pageStack.push(duePageComponent);
    }

    /** Placeholder shown when the garage has no (active) vehicle yet. */
    function showPlaceholder() {
        currentVehicleId = "";
        currentSection = "";
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

    /**
     * Pops a form page, but only while it is still the last page: a late
     * signal from a page that is already animating out must not pop the
     * vehicle page below it.
     */
    function closeForm(page) {
        if (pageStack.lastItem === page) {
            pageStack.pop();
        }
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
            id: intervalForm
            onSaved: root.closeForm(intervalForm)
            onRemoved: root.closeForm(intervalForm)
            onCanceled: root.closeForm(intervalForm)
        }
    }

    Component {
        id: settingsComponent
        SettingsPage {}
    }

    Component {
        id: csvImportComponent
        CsvImportPage {
            id: csvImport
            onImported: root.closeForm(csvImport)
            onCanceled: root.closeForm(csvImport)
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
            id: serviceForm
            onSaved: root.closeForm(serviceForm)
            onRemoved: root.closeForm(serviceForm)
            onCanceled: root.closeForm(serviceForm)
        }
    }

    // The list updates itself through FuelEntries, so closing the form is enough.
    Component {
        id: fuelFormComponent
        FuelFormPage {
            id: fuelForm
            onSaved: root.closeForm(fuelForm)
            onRemoved: root.closeForm(fuelForm)
            onCanceled: root.closeForm(fuelForm)
        }
    }

    Component {
        id: formComponent
        VehicleFormPage {
            id: vehicleForm
            onSaved: vehicleId => root.showVehicle(vehicleId)
            onCanceled: root.closeForm(vehicleForm)
        }
    }

    Component {
        id: placeholderComponent
        // Kirigami.Page is the basic page type of a PageRow: it has a title
        // shown in the toolbar and an actions list rendered as buttons there.
        Kirigami.Page {
            title: i18n("GarageLog")

            // EmptyState: large icon tile plus Kirigami's PlaceholderMessage
            // with text, explanation and a button for the obvious next step.
            EmptyState {
                // Only archived vehicles left: the garage is not empty, they are just hidden.
                readonly property bool onlyArchived: Vehicles.archivedCount > 0

                anchors.centerIn: parent
                width: parent.width - Kirigami.Units.gridUnit * 4
                iconSource: "qrc:/qml/icons/vehicle-car.svg"
                accent: Style.accent("car")
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
