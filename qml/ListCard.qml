// List of GarageLog's tabs and pages (role: qml, pure UI, ADR-015).
// Error banner above a card that holds a scrolling ListView with optional
// section headings and a centred empty state. Used by the fuel, service and
// interval tabs and the due page, which only supply model, rows and texts.

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: card

    /** Model and row delegate of the list. */
    property alias model: list.model
    property alias delegate: list.delegate
    /** Error of the model; the banner is hidden while it is empty. */
    property string errorText: ""
    /** Model role that groups rows under headings; "" = no headings. */
    property string sectionProperty: ""
    property Component sectionDelegate: null
    property alias topMargin: list.topMargin
    /** Room below the last row, so the floating add button never covers it. */
    property alias bottomMargin: list.bottomMargin
    /** Shown in the middle of the card while the list is empty (an EmptyState). */
    property alias emptyState: emptyLoader.sourceComponent

    spacing: Kirigami.Units.largeSpacing

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        type: Kirigami.MessageType.Error
        text: card.errorText
        visible: text !== ""
    }

    Card {
        Layout.fillWidth: true
        Layout.fillHeight: true

        ListView {
            id: list
            anchors.fill: parent
            anchors.margins: Kirigami.Units.smallSpacing
            clip: true
            bottomMargin: Kirigami.Units.gridUnit * 4
            Controls.ScrollBar.vertical: Controls.ScrollBar {}

            // Sections group consecutive rows with the same value of that role
            // under one heading; the models sort accordingly.
            section.property: card.sectionProperty
            section.delegate: card.sectionDelegate

            // A Loader creates the empty state only while it is needed; with
            // a fixed width the loaded item takes that width and wraps its text.
            Loader {
                id: emptyLoader
                anchors.centerIn: parent
                width: parent.width - Kirigami.Units.gridUnit * 4
                active: list.count === 0
            }
        }
    }
}
