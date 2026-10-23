// Drop-down field for forms (role: qml, pure UI).
// A ComboBox that reports Enter as submitted() while its list is closed,
// so Enter saves the form from every field (M15). A plain ComboBox
// swallows Enter, so the form never sees it.

import QtQuick
import QtQuick.Controls as Controls

Controls.ComboBox {
    id: box

    /** Enter was pressed while the list was closed; forms connect this to save(). */
    signal submitted()

    /** With the list open, Enter picks the highlighted item as usual. */
    function handleEnter(event) {
        if (box.popup.visible) {
            event.accepted = false;
        } else {
            box.submitted();
        }
    }

    // Keys handlers run before the ComboBox's own key handling.
    Keys.onReturnPressed: event => handleEnter(event)
    Keys.onEnterPressed: event => handleEnter(event)
}
