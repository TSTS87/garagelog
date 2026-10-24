// Form to add, edit and delete a fuel or charging entry (role: qml, pure UI).
// Opened as its own page next to the vehicle page (ADR-008). Of quantity,
// total and unit price the user enters two, the third is calculated by
// FuelEntries (C++); a lower odometer than before only warns (SPEC §4.2).

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.tmx.garagelog

FormPage {
    id: form

    /** Entry to edit; "" = add a new entry to FuelEntries.vehicleId. Set by Main.qml. */
    property string entryId: ""
    isNew: entryId === ""
    readonly property bool electric: FuelEntries.electric

    /** Stored values when editing, read once when the page is created. */
    readonly property var original: isNew ? ({}) : FuelEntries.get(entryId)

    // Parsed field values: numbers in the database units (metres, ml/Wh,
    // cents) or null while a field is empty or not a number.
    readonly property var dateIso: Format.parseDate(dateField.text)
    readonly property var odometerM: Format.parseDistance(odometerField.text)
    readonly property var quantity: Format.parseQuantity(quantityField.text, electric)
    readonly property var totalCostC: Format.parseMoney(totalField.text)

    canSave: dateIso !== null && odometerM !== null && quantity !== null && quantity > 0
        && totalCostC !== null

    /**
     * The two price fields the user typed into last, most recent first; the
     * third one is calculated. Pump displays show quantity and total, so the
     * unit price is calculated until the user types into it.
     */
    property var priceSources: ["quantity", "total"]
    readonly property string calculatedField: ["quantity", "total", "unitPrice"].find(key => !priceSources.includes(key))

    /** Re-evaluated whenever date or odometer change; only warns, never blocks saving. */
    readonly property var plausibility: FuelEntries.plausibility(dateIso ?? "", odometerM, entryId)

    /** Called on user edits of a price field: it becomes a source, the oldest source gets calculated. */
    function priceEdited(key) {
        priceSources = [key, priceSources[0] === key ? priceSources[1] : priceSources[0]];
        recalculate();
    }

    /** Fills the calculated price field from the other two, or empties it if they are incomplete. */
    function recalculate() {
        // Parse the texts here instead of reading the quantity/totalCostC
        // bindings: textEdited fires before textChanged, so the bindings
        // still hold the value from before the current key press.
        const quantity = Format.parseQuantity(quantityField.text, electric);
        const totalCostC = Format.parseMoney(totalField.text);
        const unitPrice = Format.parseUnitPrice(unitPriceField.text, electric);
        let result = null;
        if (calculatedField === "unitPrice") {
            if (quantity !== null && totalCostC !== null) {
                result = FuelEntries.unitPriceFor(totalCostC, quantity);
            }
            unitPriceField.text = result === null ? "" : Format.unitPriceInput(result, electric);
        } else if (calculatedField === "total") {
            if (quantity !== null && unitPrice !== null) {
                result = FuelEntries.totalCostFor(quantity, unitPrice);
            }
            totalField.text = result === null ? "" : Format.moneyInput(result);
        } else {
            if (totalCostC !== null && unitPrice !== null) {
                result = FuelEntries.quantityFor(totalCostC, unitPrice);
            }
            quantityField.text = result === null ? "" : Format.quantityInput(result, electric);
        }
    }

    onSaveRequested: {
        const savedId = FuelEntries.save({
            id: entryId,
            date: dateIso,
            odometerM: odometerM,
            quantity: quantity,
            totalCostC: totalCostC,
            fullTank: fullTankSwitch.checked,
            station: stationBox.editText,
            notes: notesField.text
        });
        if (savedId === "") {
            errorText = FuelEntries.errorString;
        } else {
            finishSave(savedId);
        }
    }

    title: isNew ? i18n("Add Entry") : i18n("Edit Entry")

    deleteTitle: i18n("Delete Entry?")
    deleteSubtitle: i18n("This entry is removed permanently. Consumption values are recalculated.")

    onRemoveRequested: {
        if (FuelEntries.remove(entryId)) {
            finishRemove();
        } else {
            errorText = FuelEntries.errorString;
        }
    }

    header: ColumnLayout {
        spacing: 0

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            position: Kirigami.InlineMessage.Position.Header
            type: Kirigami.MessageType.Error
            text: form.errorText
            visible: text !== ""
        }

        // Warning, not error: typos and odometer swaps happen, the user decides.
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            position: Kirigami.InlineMessage.Position.Header
            type: Kirigami.MessageType.Warning
            text: form.plausibility.lowerThanPrevious
                ? i18n("The odometer is lower than in an earlier entry (%1). Please check the reading; you can still save.",
                       Format.distance(form.plausibility.previousOdometerM))
                : form.plausibility.duplicate ? i18n("An entry with the same date and odometer already exists.") : ""
            visible: text !== ""
        }
    }

    Kirigami.FormLayout {
        // Key events nobody handled bubble up to the parent item; text fields
        // save via onAccepted, this catches Enter on switches and combo boxes.
        Keys.onReturnPressed: form.save()
        Keys.onEnterPressed: form.save()

        Controls.TextField {
            id: dateField
            Kirigami.FormData.label: i18n("Date:")
            text: Format.date(form.original.date ?? Format.today())
            onAccepted: form.save()
        }

        Controls.Label {
            visible: form.dateIso === null
            text: i18n("Please enter a date like %1.", Format.date(Format.today()))
            color: Kirigami.Theme.negativeTextColor
        }

        Controls.TextField {
            id: odometerField
            Kirigami.FormData.label: i18n("Odometer (%1):", Format.distanceLabel)
            // The current reading as hint, so the user sees roughly what to expect.
            placeholderText: i18n("Currently %1", Format.distance(Vehicles.get(FuelEntries.vehicleId).odometerM ?? 0))
            text: form.isNew ? "" : Format.distanceInput(form.original.odometerM)
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            // New entries usually keep today's date, so typing starts here.
            focus: true
            onAccepted: form.save()
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: form.electric ? i18n("Charge") : i18n("Fuel")
        }

        // textEdited (not textChanged) fires only for user input, so writing
        // the calculated value into a field does not count as an edit.
        Controls.TextField {
            id: quantityField
            Kirigami.FormData.label: form.electric ? i18n("Energy (kWh):") : i18n("Quantity (%1):", Format.volumeLabel)
            text: form.isNew ? "" : Format.quantityInput(form.original.quantity, form.electric)
            font.italic: form.calculatedField === "quantity"
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            onTextEdited: form.priceEdited("quantity")
            onAccepted: form.save()
        }

        Controls.TextField {
            id: totalField
            Kirigami.FormData.label: i18n("Total price (%1):", Format.currencySymbol)
            text: form.isNew ? "" : Format.moneyInput(form.original.totalCostC)
            font.italic: form.calculatedField === "total"
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            onTextEdited: form.priceEdited("total")
            onAccepted: form.save()
        }

        Controls.TextField {
            id: unitPriceField
            Kirigami.FormData.label: form.electric ? i18n("Price per kWh (%1):", Format.currencySymbol)
                : Format.volumeUnit === "l" ? i18n("Price per litre (%1):", Format.currencySymbol)
                : i18n("Price per gallon (%1):", Format.currencySymbol)
            text: {
                const milli = form.isNew ? null : FuelEntries.unitPriceFor(form.original.totalCostC, form.original.quantity);
                return milli === null ? "" : Format.unitPriceInput(milli, form.electric);
            }
            font.italic: form.calculatedField === "unitPrice"
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            onTextEdited: form.priceEdited("unitPrice")
            onAccepted: form.save()
        }

        Controls.Label {
            Layout.maximumWidth: Kirigami.Units.gridUnit * 20
            text: {
                const names = {
                    quantity: form.electric ? i18n("Energy") : i18n("Quantity"),
                    total: i18n("Total price"),
                    unitPrice: form.electric ? i18n("Price per kWh") : Format.volumeUnit === "l" ? i18n("Price per litre") : i18n("Price per gallon")
                };
                return i18n("Enter two of the three values. Calculated: %1", names[form.calculatedField]);
            }
            wrapMode: Text.WordWrap
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
        }

        // Partial fills count towards the next full tank's consumption.
        Controls.Switch {
            id: fullTankSwitch
            Kirigami.FormData.label: form.electric ? i18n("Charged to full:") : i18n("Full tank:")
            checked: form.original.fullTank ?? true
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Details")
        }

        // An editable ComboBox completes the typed text inline from its
        // model and still accepts any new name, which is exactly the
        // free-text-with-suggestions the station field needs.
        Controls.ComboBox {
            id: stationBox
            Kirigami.FormData.label: form.electric ? i18n("Charging station:") : i18n("Station:")
            editable: true
            model: FuelEntries.stationSuggestions()
            // Without this the first suggestion would be preselected silently.
            currentIndex: -1
            Component.onCompleted: editText = form.original.station ?? ""
            onAccepted: form.save()
        }

        NotesArea {
            id: notesField
            Kirigami.FormData.label: i18n("Notes:")
            text: form.original.notes ?? ""
        }
    }
}
