// Segmented pill switch (role: qml, pure UI, ADR-015).
// Replaces the classic TabBar: a rounded track with equally wide segments
// and an accent "thumb" that slides to the chosen one. Used for the vehicle
// page sections and the statistics period.

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Rectangle {
    id: bar

    /** Segments as [{ text, icon }]; icon is optional (qrc URL or icon name). */
    property var model: []
    property int currentIndex: 0
    /** Colour of the thumb under the current segment. */
    property color accent: Kirigami.Theme.highlightColor
    /** Narrow bars show only the icons (if every segment has one); the text moves to a tooltip. */
    readonly property bool iconsOnly: model.every(m => (m.icon ?? "") !== "")
        && width < model.length * Kirigami.Units.gridUnit * 6

    readonly property real inset: Kirigami.Units.smallSpacing / 2
    readonly property real segmentWidth: (width - 2 * inset) / Math.max(1, model.length)

    implicitHeight: Kirigami.Units.gridUnit * 2.2
    radius: height / 2
    // A translucent text tint instead of a fixed colour: the track stays
    // visible on the window background as well as on white views.
    color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.08)

    // The thumb is a sibling below the segments, so it can slide between
    // them; Behavior animates every change of x, whoever sets currentIndex.
    Rectangle {
        x: bar.inset + bar.currentIndex * bar.segmentWidth
        y: bar.inset
        width: bar.segmentWidth
        height: bar.height - 2 * bar.inset
        radius: height / 2
        color: bar.accent
        Behavior on x {
            NumberAnimation {
                duration: Kirigami.Units.longDuration
                easing.type: Easing.OutCubic
            }
        }
    }

    Row {
        x: bar.inset
        y: bar.inset

        Repeater {
            model: bar.model

            // AbstractButton brings focus, keyboard activation and
            // accessibility; we draw it ourselves.
            Controls.AbstractButton {
                id: segment

                required property var modelData
                required property int index
                readonly property bool current: index === bar.currentIndex

                width: bar.segmentWidth
                height: bar.height - 2 * bar.inset
                text: modelData.text
                focusPolicy: Qt.StrongFocus
                Accessible.role: Accessible.PageTab
                Controls.ToolTip.visible: bar.iconsOnly && hovered
                Controls.ToolTip.text: text

                onClicked: bar.currentIndex = index
                // Arrow keys walk through the segments like in a tab bar.
                Keys.onLeftPressed: if (bar.currentIndex > 0) bar.currentIndex--
                Keys.onRightPressed: if (bar.currentIndex < bar.model.length - 1) bar.currentIndex++

                background: Rectangle {
                    radius: height / 2
                    color: !segment.current && segment.hovered
                        ? Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.06) : "transparent"
                    border.width: segment.visualFocus ? 2 : 0
                    border.color: Kirigami.Theme.focusColor
                }

                contentItem: Item {
                    RowLayout {
                        anchors.centerIn: parent
                        spacing: Kirigami.Units.smallSpacing

                        Kirigami.Icon {
                            visible: (segment.modelData.icon ?? "") !== ""
                            source: segment.modelData.icon ?? ""
                            implicitWidth: Kirigami.Units.iconSizes.small
                            implicitHeight: implicitWidth
                            isMask: true
                            color: segmentLabel.color
                        }
                        Controls.Label {
                            id: segmentLabel
                            visible: !bar.iconsOnly
                            text: segment.text
                            font.weight: segment.current ? Font.DemiBold : Font.Normal
                            color: segment.current ? "white" : Kirigami.Theme.textColor
                            Behavior on color {
                                ColorAnimation {
                                    duration: Kirigami.Units.shortDuration
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
