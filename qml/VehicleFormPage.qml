// Form to add, edit and archive a vehicle (role: qml, pure UI).
// Opened as its own page next to the vehicle page (ADR-008). Collects the
// values, hands them to Vehicles.save() and reports the result via signals;
// validation of stored data happens in C++.

import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.tmx.garagelog

// ScrollablePage is a Kirigami.Page whose content scrolls, which a form
// needs as soon as the window is small.
Kirigami.ScrollablePage {
    id: form

    /** Vehicle to edit; "" = add a new vehicle. Set by Main.qml. */
    property string vehicleId: ""
    readonly property bool isNew: vehicleId === ""

    /** Stored values when editing, read once when the page is created. */
    readonly property var original: isNew ? ({}) : Vehicles.get(vehicleId)

    /** Odometer text as first shown, to detect whether the user changed it. */
    readonly property string initialOdometerText: isNew ? "" : Format.distanceInput(original.odometerM ?? 0)
    readonly property bool odometerChanged: odometerField.text !== initialOdometerText
    // Unchanged is always fine; a changed value must parse (an empty field
    // on a new vehicle simply means "start at 0").
    readonly property bool odometerValid: !odometerChanged || (isNew && odometerField.text === "")
        || Format.parseDistance(odometerField.text) !== null
    readonly property bool canSave: nameField.text.trim() !== "" && odometerValid
        && (yearField.text === "" || yearField.acceptableInput)

    property string errorText: ""

    // Options for the two combo boxes: database key + translated label.
    readonly property var typeOptions: [
        { value: "car", text: i18n("Car") },
        { value: "motorcycle", text: i18n("Motorcycle") },
        { value: "scooter", text: i18n("Scooter") },
        { value: "ebike", text: i18n("E-bike") },
        { value: "other", text: i18n("Other") }
    ]
    readonly property var powertrainOptions: [
        { value: "petrol", text: i18n("Petrol") },
        { value: "diesel", text: i18n("Diesel") },
        { value: "electric", text: i18n("Electric") },
        { value: "hybrid", text: i18n("Hybrid") },
        { value: "other", text: i18n("Other") }
    ]

    signal saved(string vehicleId)
    signal canceled()

    /**
     * True once the form has emitted saved, removed or canceled. Enter in a
     * text field reaches both onAccepted and the FormLayout key handler, and
     * the page's shortcuts stay active during the pop animation, so without
     * this a second trigger would save twice or pop the page below.
     */
    property bool closing: false

    /** Leaves the form without saving; ignored while it is already closing. */
    function cancel() {
        if (!closing) {
            closing = true;
            canceled();
        }
    }

    function save() {
        if (closing || !canSave) {
            return;
        }
        const values = {
            id: vehicleId,
            name: nameField.text,
            type: typeBox.currentValue,
            powertrain: powertrainBox.currentValue,
            make: makeField.text,
            model: modelField.text,
            year: yearField.text === "" ? null : parseInt(yearField.text),
            plate: plateField.text,
            notes: notesField.text
        };
        // Only a changed odometer is sent, so an untouched field never
        // rewrites the offset (see VehicleListModel::save).
        if (odometerChanged && odometerField.text !== "") {
            values.odometerM = Format.parseDistance(odometerField.text);
        }
        const savedId = Vehicles.save(values);
        if (savedId === "") {
            errorText = Vehicles.errorString;
        } else {
            closing = true;
            saved(savedId);
        }
    }

    title: isNew ? i18n("Add Vehicle") : i18n("Edit Vehicle")

    actions: [
        Kirigami.Action {
            text: i18n("Save")
            icon.name: "document-save"
            enabled: form.canSave
            shortcut: StandardKey.Save
            onTriggered: form.save()
        },
        Kirigami.Action {
            visible: !form.isNew
            text: form.original.archived ? i18n("Restore") : i18n("Archive")
            icon.name: form.original.archived ? "archive-extract" : "archive-insert"
            onTriggered: {
                if (Vehicles.setArchived(form.vehicleId, !form.original.archived)) {
                    form.cancel();
                } else {
                    form.errorText = Vehicles.errorString;
                }
            }
        },
        Kirigami.Action {
            text: i18n("Cancel")
            icon.name: "dialog-cancel"
            shortcut: "Escape"
            onTriggered: form.cancel()
        }
    ]

    header: Kirigami.InlineMessage {
        position: Kirigami.InlineMessage.Position.Header
        type: Kirigami.MessageType.Error
        text: form.errorText
        visible: text !== ""
    }

    // FormLayout is Kirigami's two-column form: labels come from the
    // attached Kirigami.FormData.label of each field and align
    // automatically; on narrow windows it switches to labels above fields.
    Kirigami.FormLayout {
        // Key events nobody handled bubble up to the parent item; text fields
        // save via onAccepted, this catches Enter on switches and combo boxes.
        Keys.onReturnPressed: form.save()
        Keys.onEnterPressed: form.save()

        Controls.TextField {
            id: nameField
            Kirigami.FormData.label: i18n("Name:")
            placeholderText: i18n("e.g. Daily driver")
            text: form.original.name ?? ""
            focus: true
            // Enter in any single-line field saves (SPEC §3).
            onAccepted: form.save()
        }

        // ComboBox with textRole/valueRole: shows the translated text but
        // currentValue yields the database key the C++ side expects.
        FormComboBox {
            id: typeBox
            onSubmitted: form.save()
            Kirigami.FormData.label: i18n("Type:")
            textRole: "text"
            valueRole: "value"
            model: form.typeOptions
            // Looked up in the plain array, not via indexOfValue(): that one
            // depends on the combo's internal model, which may not exist yet
            // when the binding first runs (it then picked "Car" for every vehicle).
            // Picking an entry replaces the binding, as intended.
            currentIndex: Math.max(0, form.typeOptions.findIndex(option => option.value === (form.original.type ?? "car")))
        }

        FormComboBox {
            id: powertrainBox
            onSubmitted: form.save()
            Kirigami.FormData.label: i18n("Powertrain:")
            textRole: "text"
            valueRole: "value"
            model: form.powertrainOptions
            currentIndex: Math.max(0, form.powertrainOptions.findIndex(option => option.value === (form.original.powertrain ?? "petrol")))
        }

        Controls.TextField {
            id: odometerField
            Kirigami.FormData.label: i18n("Odometer (%1):", Format.distanceLabel)
            placeholderText: form.isNew ? i18n("Current reading, optional") : ""
            text: form.initialOdometerText
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            onAccepted: form.save()
        }

        Controls.Label {
            visible: !form.odometerValid
            text: i18n("Please enter a number of kilometres.")
            color: Kirigami.Theme.negativeTextColor
        }

        // A Separator with isSection becomes a section heading inside the FormLayout.
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Details")
        }

        Controls.TextField {
            id: makeField
            Kirigami.FormData.label: i18n("Make:")
            text: form.original.make ?? ""
            onAccepted: form.save()
        }

        Controls.TextField {
            id: modelField
            Kirigami.FormData.label: i18n("Model:")
            text: form.original.model ?? ""
            onAccepted: form.save()
        }

        Controls.TextField {
            id: yearField
            Kirigami.FormData.label: i18n("Year:")
            text: form.original.year ? String(form.original.year) : ""
            // Year of the first car (1886) up to a generous future limit.
            validator: IntValidator { bottom: 1886; top: 2100 }
            inputMethodHints: Qt.ImhDigitsOnly
            onAccepted: form.save()
        }

        Controls.TextField {
            id: plateField
            Kirigami.FormData.label: i18n("Licence plate:")
            text: form.original.plate ?? ""
            onAccepted: form.save()
        }

        NotesArea {
            id: notesField
            Kirigami.FormData.label: i18n("Notes:")
            text: form.original.notes ?? ""
            implicitHeight: Kirigami.Units.gridUnit * 5
        }
    }
}
