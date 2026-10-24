// Common frame of the add/edit forms (role: qml, pure UI).
// Toolbar actions Save/Delete/Cancel with their shortcuts, the error header,
// the delete confirmation and the guard against closing twice. The fuel,
// service, interval and vehicle forms derive from it and only add fields.

import QtQuick
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page

    /** True while the form creates a new record; bound by the derived form. */
    property bool isNew: true
    /** Enables Save; bound by the derived form to its field validation. */
    property bool canSave: false
    /** Shows Delete; by default for every existing record. */
    property bool canDelete: !isNew
    /** Texts of the delete confirmation. */
    property string deleteTitle: ""
    property string deleteSubtitle: ""
    /** Further toolbar actions, placed between Save and Delete (e.g. Archive). */
    property list<Kirigami.Action> extraActions
    /** Shown in the header; the derived form sets it when a model call fails. */
    property string errorText: ""

    /**
     * True once the form has emitted saved, removed or canceled. Enter in a
     * text field reaches both onAccepted and the FormLayout key handler, and
     * the page's shortcuts stay active during the pop animation, so without
     * this a second trigger would save twice or pop the page below.
     */
    property bool closing: false

    signal saved(string id)
    signal canceled()
    signal removed()

    /** Save was triggered and canSave holds; the form stores and calls finishSave() or sets errorText. */
    signal saveRequested()
    /** The user confirmed Delete; the form deletes and calls finishRemove() or sets errorText. */
    signal removeRequested()

    /** Asks the form to save; ignored while closing or invalid. */
    function save() {
        if (!closing && canSave) {
            saveRequested();
        }
    }

    /** Leaves the form without saving; ignored while it is already closing. */
    function cancel() {
        if (!closing) {
            closing = true;
            canceled();
        }
    }

    /** Closes the form after a successful save of the record @p id. */
    function finishSave(id) {
        closing = true;
        saved(id);
    }

    /** Closes the form after a successful delete. */
    function finishRemove() {
        closing = true;
        removed();
    }

    readonly property Kirigami.Action saveAction: Kirigami.Action {
        text: i18n("Save")
        icon.name: "document-save"
        enabled: page.canSave
        shortcut: StandardKey.Save
        onTriggered: page.save()
    }
    readonly property Kirigami.Action deleteAction: Kirigami.Action {
        visible: page.canDelete
        text: i18n("Delete")
        icon.name: "edit-delete"
        onTriggered: deleteDialog.open()
    }
    readonly property Kirigami.Action cancelAction: Kirigami.Action {
        text: i18n("Cancel")
        icon.name: "dialog-cancel"
        shortcut: "Escape"
        onTriggered: page.cancel()
    }

    // A list property cannot be spliced declaratively, so the toolbar list
    // is built in JS: Save, the form's own actions, Delete, Cancel.
    actions: {
        const list = [saveAction];
        for (let i = 0; i < extraActions.length; ++i) {
            list.push(extraActions[i]);
        }
        list.push(deleteAction, cancelAction);
        return list;
    }

    header: Kirigami.InlineMessage {
        position: Kirigami.InlineMessage.Position.Header
        type: Kirigami.MessageType.Error
        text: page.errorText
        visible: text !== ""
    }

    // PromptDialog is Kirigami's small confirmation dialog: title, one
    // sentence, buttons. Deleting cannot be undone, so it asks first.
    Kirigami.PromptDialog {
        id: deleteDialog
        title: page.deleteTitle
        subtitle: page.deleteSubtitle
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                text: i18n("Delete")
                icon.name: "edit-delete"
                onTriggered: {
                    deleteDialog.close();
                    page.removeRequested();
                }
            }
        ]
    }
}
