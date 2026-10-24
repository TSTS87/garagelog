// Form to add, edit and delete a service entry (role: qml, pure UI).
// Opened as its own page next to the vehicle page (ADR-008). Category
// picker with "New Category…", self/workshop, and file attachments that
// are copied into the garage by ServiceEntries (C++) on save.

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.tmx.garagelog

Kirigami.ScrollablePage {
    id: form

    /** Entry to edit; "" = add a new entry to ServiceEntries.vehicleId. Set by Main.qml. */
    property string entryId: ""
    readonly property bool isNew: entryId === ""

    /** Stored values when editing, read once when the page is created. */
    readonly property var original: isNew ? ({}) : ServiceEntries.get(entryId)

    /** Picker entries {id, name, builtin}; re-read after "New Category…". */
    property var categories: ServiceEntries.categories()
    property string categoryId: original.categoryId ?? (categories.length > 0 ? categories[0].id : "")

    /** Already stored attachments of an edited entry: [{id, filename, url, ...}]. */
    property var storedAttachments: isNew ? [] : ServiceEntries.attachments(entryId)
    /** Files chosen in this form session (file:// URLs), copied on save. */
    property var pendingFiles: []

    // Parsed field values in database units, or null. Odometer and cost
    // are optional; only text that does not parse blocks saving.
    readonly property var dateIso: Format.parseDate(dateField.text)
    readonly property var odometerM: Format.parseDistance(odometerField.text)
    readonly property var costC: Format.parseMoney(costField.text)
    readonly property bool odometerOk: odometerField.text.trim() === "" || odometerM !== null
    readonly property bool costOk: costField.text.trim() === "" || costC !== null

    readonly property bool canSave: dateIso !== null && odometerOk && costOk && categoryId !== ""
        && descriptionField.text.trim() !== ""

    readonly property var performedByOptions: [
        { value: "", text: i18n("Not specified") },
        { value: "self", text: i18n("Myself") },
        { value: "shop", text: i18n("Workshop") }
    ]

    property string errorText: ""

    signal saved(string entryId)
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
        const savedId = ServiceEntries.save({
            id: entryId,
            date: dateIso,
            odometerM: odometerField.text.trim() === "" ? null : odometerM,
            categoryId: categoryId,
            description: descriptionField.text,
            costC: costC,
            performedBy: performedByOptions[performedByBox.currentIndex].value,
            shopName: shopField.text,
            notes: notesField.text,
            newAttachments: pendingFiles
        });
        if (savedId === "") {
            errorText = ServiceEntries.errorString;
        } else if (ServiceEntries.errorString !== "") {
            // Entry saved, but a file could not be copied: stay here as edit
            // form so the user sees what is attached and can retry.
            pendingFiles = [];
            entryId = savedId;
            storedAttachments = ServiceEntries.attachments(savedId);
            errorText = ServiceEntries.errorString;
        } else {
            closing = true;
            saved(savedId);
        }
    }

    /** File name part of a file:// URL, for showing pending files. */
    function fileName(url) {
        return decodeURIComponent(url.toString().split("/").pop());
    }

    title: isNew ? i18n("Add Service") : i18n("Edit Service")

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
        title: i18n("Delete Service Entry?")
        subtitle: i18n("The entry and its attached files are removed permanently.")
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                text: i18n("Delete")
                icon.name: "edit-delete"
                onTriggered: {
                    deleteDialog.close();
                    if (ServiceEntries.remove(form.entryId)) {
                        form.closing = true;
                        form.removed();
                    } else {
                        form.errorText = ServiceEntries.errorString;
                    }
                }
            }
        ]
    }

    // A PromptDialog can hold any content item; here one text field for
    // the name of the new category.
    Kirigami.PromptDialog {
        id: categoryDialog
        title: i18n("New Category")
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
        onOpened: {
            categoryNameField.text = "";
            categoryNameField.forceActiveFocus();
        }
        onAccepted: {
            const id = ServiceEntries.addCategory(categoryNameField.text);
            if (id === "") {
                form.errorText = ServiceEntries.errorString;
                return;
            }
            form.errorText = "";
            form.categories = ServiceEntries.categories();
            form.categoryId = id;
            // A new model resets the ComboBox selection, so set it explicitly.
            categoryBox.currentIndex = form.categories.findIndex(c => c.id === id);
        }

        Controls.TextField {
            id: categoryNameField
            placeholderText: i18n("e.g. Car wash")
            onAccepted: categoryDialog.accept()
        }
    }

    Kirigami.PromptDialog {
        id: removeAttachmentDialog
        property string attachmentId: ""
        property string filename: ""
        title: i18n("Remove Attachment?")
        subtitle: i18n("“%1” is deleted from the garage folder.", filename)
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                text: i18n("Remove")
                icon.name: "edit-delete"
                onTriggered: {
                    removeAttachmentDialog.close();
                    if (!ServiceEntries.removeAttachment(removeAttachmentDialog.attachmentId)) {
                        form.errorText = ServiceEntries.errorString;
                    }
                    form.storedAttachments = ServiceEntries.attachments(form.entryId);
                }
            }
        ]
    }

    // FileDialog from QtQuick.Dialogs uses the platform's native dialog
    // (the KDE/portal one on Plasma and in Flatpak) when one is available.
    FileDialog {
        id: fileDialog
        title: i18n("Attach Files")
        fileMode: FileDialog.OpenFiles
        nameFilters: [i18n("Documents and images (*.pdf *.jpg *.jpeg *.png *.webp *.heic)"), i18n("All files (*)")]
        onAccepted: form.pendingFiles = form.pendingFiles.concat(selectedFiles.map(url => url.toString()))
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

        // The last entry is the "New Category…" command, not a category;
        // choosing it opens the dialog and keeps the previous selection.
        FormComboBox {
            id: categoryBox
            onSubmitted: form.save()
            Kirigami.FormData.label: i18n("Category:")
            readonly property var entries: form.categories.concat([{ id: "", name: i18n("New Category…") }])
            model: entries
            textRole: "name"
            currentIndex: form.categories.findIndex(c => c.id === form.categoryId)
            onActivated: index => {
                if (index === form.categories.length) {
                    // Restore the binding target; the dialog sets a new id on success.
                    currentIndex = Qt.binding(() => form.categories.findIndex(c => c.id === form.categoryId));
                    categoryDialog.open();
                } else {
                    form.categoryId = form.categories[index].id;
                }
            }
        }

        Controls.TextField {
            id: descriptionField
            Kirigami.FormData.label: i18n("Description:")
            text: form.original.description ?? ""
            placeholderText: i18n("e.g. Oil and filter, 5W-30")
            focus: true
            onAccepted: form.save()
        }

        Controls.TextField {
            id: odometerField
            Kirigami.FormData.label: i18n("Odometer (%1):", Format.distanceLabel)
            placeholderText: i18n("Optional, currently %1", Format.distance(Vehicles.get(ServiceEntries.vehicleId).odometerM ?? 0))
            text: form.original.odometerM === undefined || form.original.odometerM === null ? "" : Format.distanceInput(form.original.odometerM)
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            onAccepted: form.save()
        }

        Controls.TextField {
            id: costField
            Kirigami.FormData.label: i18n("Cost (%1):", Format.currencySymbol)
            text: form.isNew ? "" : Format.moneyInput(form.original.costC)
            placeholderText: "0"
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            onAccepted: form.save()
        }

        Controls.Label {
            visible: !form.odometerOk || !form.costOk
            text: i18n("Odometer and cost must be numbers (or empty).")
            color: Kirigami.Theme.negativeTextColor
        }

        FormComboBox {
            id: performedByBox
            onSubmitted: form.save()
            Kirigami.FormData.label: i18n("Done by:")
            model: form.performedByOptions
            textRole: "text"
            currentIndex: Math.max(0, form.performedByOptions.findIndex(o => o.value === (form.original.performedBy ?? "")))
        }

        Controls.TextField {
            id: shopField
            Kirigami.FormData.label: i18n("Workshop:")
            visible: form.performedByOptions[performedByBox.currentIndex].value === "shop"
            text: form.original.shopName ?? ""
            onAccepted: form.save()
        }

        NotesArea {
            id: notesField
            Kirigami.FormData.label: i18n("Notes:")
            text: form.original.notes ?? ""
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Attachments")
        }

        // FormLayout lays out its direct children and keeps references to
        // them; Repeater delegates replaced on every model change leave it
        // with dangling items (TypeErrors in FormLayout.qml). One stable
        // ColumnLayout as its child avoids that.
        ColumnLayout {
            visible: form.storedAttachments.length + form.pendingFiles.length > 0
            spacing: 0

            // Stored files: click opens them in the default application.
            Repeater {
                model: form.storedAttachments
                delegate: RowLayout {
                    required property var modelData
                    Controls.Button {
                        text: modelData.filename
                        icon.name: "mail-attachment"
                        flat: true
                        // Empty url: the garage names a file outside attachments/.
                        enabled: modelData.url !== ""
                        // Qt.openUrlExternally goes through QDesktopServices (and
                        // the OpenURI portal inside Flatpak).
                        onClicked: Qt.openUrlExternally(modelData.url)
                    }
                    Controls.ToolButton {
                        icon.name: "edit-delete"
                        Controls.ToolTip.text: i18n("Remove attachment")
                        Controls.ToolTip.visible: hovered
                        onClicked: {
                            removeAttachmentDialog.attachmentId = modelData.id;
                            removeAttachmentDialog.filename = modelData.filename;
                            removeAttachmentDialog.open();
                        }
                    }
                }
            }

            // Chosen but not yet saved files, shown in italics until Save.
            Repeater {
                model: form.pendingFiles
                delegate: RowLayout {
                    required property var modelData
                    required property int index
                    Controls.Label {
                        text: form.fileName(modelData)
                        font.italic: true
                    }
                    Controls.ToolButton {
                        icon.name: "list-remove"
                        Controls.ToolTip.text: i18n("Do not attach")
                        Controls.ToolTip.visible: hovered
                        onClicked: form.pendingFiles = form.pendingFiles.filter((_, i) => i !== index)
                    }
                }
            }
        }

        Controls.Button {
            text: i18n("Attach File…")
            icon.name: "document-open"
            onClicked: fileDialog.open()
        }
    }
}
