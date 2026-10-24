// Section heading inside the list cards (role: qml, pure UI, ADR-015).
// Small spaced capitals like "OCTOBER 2026" or "OVERDUE"; used as
// ListView section delegate and as caption above groups of tiles.

import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami

Controls.Label {
    topPadding: Kirigami.Units.largeSpacing
    bottomPadding: Kirigami.Units.smallSpacing
    leftPadding: Kirigami.Units.largeSpacing + Kirigami.Units.smallSpacing
    color: Kirigami.Theme.disabledTextColor
    font.pointSize: Kirigami.Theme.smallFont.pointSize
    font.weight: Font.DemiBold
    font.letterSpacing: 1.2
    font.capitalization: Font.AllUppercase
}
