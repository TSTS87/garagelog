// List row of GarageLog's lists (role: qml, pure UI, ADR-015).
// Icon tile, title with a detail line below, and a value column on the
// right (amount in bold plus a small detail). Shared by the fuel, service,
// interval and due lists so they all look and behave the same.

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.tmx.garagelog

// ItemDelegate is the clickable list row of Qt Quick Controls; background
// and contentItem are replaced to get rounded hover rows inside a card.
Controls.ItemDelegate {
    id: row

    property string iconSource
    property color accent: Kirigami.Theme.highlightColor
    /** Weaker tile, e.g. for partial fills. */
    property bool iconMuted: false
    property string title
    property string subtitle
    /** Main figure on the right (amount, remaining distance); "" hides it. */
    property string value
    property color valueColor: Kirigami.Theme.textColor
    /** Small line under the value; "" hides it. */
    property string detail
    /** Extra items between text and value column, e.g. an attachment icon. */
    default property alias trailing: trailingRow.data

    horizontalPadding: Kirigami.Units.largeSpacing
    verticalPadding: Kirigami.Units.mediumSpacing

    background: Rectangle {
        anchors.fill: parent
        anchors.leftMargin: Kirigami.Units.smallSpacing
        anchors.rightMargin: Kirigami.Units.smallSpacing
        radius: Style.tileRadius
        color: row.down ? Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.18)
            : row.hovered ? Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.05)
            : "transparent"
        border.width: row.visualFocus ? 2 : 0
        border.color: Kirigami.Theme.focusColor
    }

    contentItem: RowLayout {
        spacing: Kirigami.Units.largeSpacing

        IconTile {
            source: row.iconSource
            accent: row.accent
            opacity: row.iconMuted ? 0.5 : 1.0
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Controls.Label {
                Layout.fillWidth: true
                text: row.title
                // Labels default to AutoText and would render HTML typed into a name or
                // note ("<b>…</b>", <img>); everything a user typed is shown literally.
                textFormat: Text.PlainText
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Controls.Label {
                Layout.fillWidth: true
                visible: text !== ""
                text: row.subtitle
                textFormat: Text.PlainText
                font: Kirigami.Theme.smallFont
                color: Kirigami.Theme.disabledTextColor
                elide: Text.ElideRight
            }
        }

        Row {
            id: trailingRow
            spacing: Kirigami.Units.smallSpacing
        }

        ColumnLayout {
            spacing: 0
            visible: row.value !== "" || row.detail !== ""

            Controls.Label {
                Layout.alignment: Qt.AlignRight
                visible: text !== ""
                text: row.value
                textFormat: Text.PlainText
                color: row.valueColor
                font.weight: Font.Bold
                // Tabular figures keep the amounts of all rows aligned.
                font.features: { "tnum": 1 }
            }
            Controls.Label {
                Layout.alignment: Qt.AlignRight
                visible: text !== ""
                text: row.detail
                textFormat: Text.PlainText
                font: Kirigami.Theme.smallFont
                color: Kirigami.Theme.disabledTextColor
            }
        }
    }
}
