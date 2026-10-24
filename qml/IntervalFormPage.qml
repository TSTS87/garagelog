// Form to add, edit and delete a maintenance interval (role: qml, pure UI).
// Opened as its own page next to the vehicle page (ADR-008). A rule is
// "every X km and/or every Y months" for one service category; the due
// status itself is calculated in C++ (DueCalculator).

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.tmx.garagelog

Kirigami.ScrollablePage {
    id: form

    /** Interval to edit; "" = add a new one to Intervals.vehicleId. Set by Main.qml. */
    property string intervalId: ""
    readonly property bool isNew: intervalId === ""

    /** Stored values (plus due status) when editing, read once. */
    readonly property var original: isNew ? ({}) : Intervals.get(intervalId)

    readonly property var categories: ServiceEntries.categories()
    property string categoryId: original.categoryId ?? (categories.length > 0 ? categories[0].id : "")
    readonly property string categoryName: categories.find(c => c.id === categoryId)?.name ?? ""

    // Both parts are optional, but at least one is needed; text that does
    // not parse blocks saving.
    readonly property var everyM: Format.parseDistance(distanceField.text)
    readonly property var everyMonths: Format.parseMonths(monthsField.text)
    readonly property bool distanceEmpty: distanceField.text.trim() === ""
    readonly property bool monthsEmpty: monthsField.text.trim() === ""
    readonly property bool partsValid: (distanceEmpty || (everyM !== null && everyM > 0))
        && (monthsEmpty || everyMonths !== null) && !(distanceEmpty && monthsEmpty)
    readonly property bool canSave: categoryId !== "" && partsValid

    property string errorText: ""

    signal saved(string intervalId)
    signal canceled()
    signal removed()

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
        const savedId = Intervals.save({
            id: intervalId,
            categoryId: categoryId,
            name: nameField.text,
            everyM: distanceEmpty ? null : everyM,
            everyMonths: monthsEmpty ? null : everyMonths,
            active: activeSwitch.checked,
            notes: notesField.text
        });
        if (savedId === "") {
            errorText = Intervals.errorString;
        } else {
            closing = true;
            saved(savedId);
        }
    }

    title: isNew ? i18n("Add Interval") : i18n("Edit Interval")

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
            text: i18n("Delete")
            icon.name: "edit-delete"
            onTriggered: deleteDialog.open()
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

    Kirigami.PromptDialog {
        id: deleteDialog
        title: i18n("Delete Interval?")
        subtitle: i18n("The rule is removed. Service entries are kept.")
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                text: i18n("Delete")
                icon.name: "edit-delete"
                onTriggered: {
                    deleteDialog.close();
                    if (Intervals.remove(form.intervalId)) {
                        form.closing = true;
                        form.removed();
                    } else {
                        form.errorText = Intervals.errorString;
                    }
                }
            }
        ]
    }

    Kirigami.FormLayout {
        // Key events nobody handled bubble up to the parent item; text fields
        // save via onAccepted, this catches Enter on switches and combo boxes.
        Keys.onReturnPressed: form.save()
        Keys.onEnterPressed: form.save()

        // Status of an existing interval, so the form also answers "when is it due?".
        RowLayout {
            Kirigami.FormData.label: i18n("Status:")
            visible: !form.isNew
            StatusDot {
                status: form.original.status ?? "unknown"
            }
            Controls.Label {
                text: {
                    const summary = Format.dueSummary(form.original.remainingM ?? null, form.original.remainingDays ?? null,
                                                      form.original.status ?? "unknown");
                    const last = form.original.lastDate ? i18n("last service %1", Format.date(form.original.lastDate)) : "";
                    return [summary, last].filter(part => part !== "").join(" · ");
                }
            }
        }

        FormComboBox {
            id: categoryBox
            onSubmitted: form.save()
            Kirigami.FormData.label: i18n("Service category:")
            model: form.categories
            textRole: "name"
            currentIndex: form.categories.findIndex(c => c.id === form.categoryId)
            onActivated: index => form.categoryId = form.categories[index].id
        }

        Controls.Label {
            Layout.maximumWidth: Kirigami.Units.gridUnit * 20
            text: i18n("Service entries of this category reset the interval.")
            wrapMode: Text.WordWrap
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
        }

        Controls.TextField {
            id: nameField
            Kirigami.FormData.label: i18n("Name:")
            text: form.original.name ?? ""
            // Empty name = category name, done by Intervals.save().
            placeholderText: form.categoryName
            onAccepted: form.save()
        }

        Controls.TextField {
            id: distanceField
            Kirigami.FormData.label: i18n("Every (%1):", Format.distanceLabel)
            text: form.original.everyM === undefined || form.original.everyM === null ? "" : Format.distanceInput(form.original.everyM)
            placeholderText: i18n("e.g. 15000")
            inputMethodHints: Qt.ImhDigitsOnly
            focus: true
            onAccepted: form.save()
        }

        Controls.TextField {
            id: monthsField
            Kirigami.FormData.label: i18n("Every (months):")
            text: form.original.everyMonths === undefined || form.original.everyMonths === null ? "" : String(form.original.everyMonths)
            placeholderText: i18n("e.g. 12")
            inputMethodHints: Qt.ImhDigitsOnly
            onAccepted: form.save()
        }

        Controls.Label {
            visible: !form.partsValid
            text: form.distanceEmpty && form.monthsEmpty
                ? i18n("Enter a distance, a number of months, or both.")
                : i18n("Distance must be a positive number, months a whole number from 1 to 1200.")
            color: Kirigami.Theme.negativeTextColor
        }

        Controls.Switch {
            id: activeSwitch
            Kirigami.FormData.label: i18n("Active:")
            checked: form.original.active ?? true
        }

        NotesArea {
            id: notesField
            Kirigami.FormData.label: i18n("Notes:")
            text: form.original.notes ?? ""
        }
    }
}
