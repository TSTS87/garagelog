// Multi-line notes field used by all forms (role: qml, pure UI).
// A TextArea that wraps lines and lets Tab/Shift+Tab move the focus, so
// keyboard users can leave the field like every other one (M15).

import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami

Controls.TextArea {
    wrapMode: TextEdit.Wrap
    implicitHeight: Kirigami.Units.gridUnit * 4

    // A TextArea inserts a tab character on Tab and so traps the focus.
    // Notes never need one; Keys handlers run before the default handling,
    // and nextItemInFocusChain() is the item Tab would normally move to.
    Keys.onTabPressed: nextItemInFocusChain(true).forceActiveFocus(Qt.TabFocusReason)
    Keys.onBacktabPressed: nextItemInFocusChain(false).forceActiveFocus(Qt.BacktabFocusReason)
}
