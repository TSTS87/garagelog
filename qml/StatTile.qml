// Key figure tile of the statistics bento grid (role: qml, pure UI, ADR-015).
// Icon tile and caption on top, the figure in large tabular digits, a small
// detail line below. All texts are formatted by the caller via Format.

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.tmx.garagelog

Card {
    id: tile

    property string iconSource
    property color accent: Kirigami.Theme.highlightColor
    property string caption
    property string value
    property string detail

    implicitHeight: column.implicitHeight + Kirigami.Units.largeSpacing * 2.5

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing * 1.25
        spacing: Kirigami.Units.smallSpacing

        RowLayout {
            spacing: Kirigami.Units.mediumSpacing
            IconTile {
                source: tile.iconSource
                accent: tile.accent
                size: Kirigami.Units.gridUnit * 1.6
            }
            Controls.Label {
                Layout.fillWidth: true
                text: tile.caption
                color: Kirigami.Theme.disabledTextColor
                elide: Text.ElideRight
            }
        }
        Item {
            Layout.fillHeight: true
        }
        Controls.Label {
            Layout.fillWidth: true
            text: tile.value
            textFormat: Text.PlainText
            font.pointSize: Kirigami.Theme.defaultFont.pointSize * Style.figureScale
            font.weight: Font.Bold
            font.features: { "tnum": 1 }
            elide: Text.ElideRight
        }
        Controls.Label {
            Layout.fillWidth: true
            text: tile.detail
            textFormat: Text.PlainText
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
            elide: Text.ElideRight
        }
    }
}
