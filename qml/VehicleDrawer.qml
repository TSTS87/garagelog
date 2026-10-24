// Side bar with the vehicle list (role: qml, pure UI).
// Brand header, the vehicles from the Vehicles model as rows with a tile in
// their type colour, then "Due" with an overdue badge, "Data" (menu),
// "Settings" and the archive switch (look from ADR-015). Navigation and
// dialogs are left to Main.qml via signals.

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.tmx.garagelog

// GlobalDrawer is Kirigami's application side bar. On wide windows it is
// docked permanently (modal: false), on narrow ones it slides in over the
// content and is opened via the hamburger handle. We leave its "actions"
// empty and fill topContent with our own rows to get the custom look.
Kirigami.GlobalDrawer {
    id: drawer

    /** Highlighted vehicle, set by Main.qml. */
    property string currentVehicleId: ""
    /** True while the due overview is shown, to highlight its row. */
    property bool dueShown: false
    /** True while the settings are shown, to highlight their row. */
    property bool settingsShown: false

    signal vehicleSelected(string vehicleId)
    signal addRequested()
    signal dueRequested()
    signal settingsRequested()
    /** One of "exportJson", "importJson", "exportCsv", "importCsv". */
    signal dataRequested(string what)

    /** Runs a drawer entry and closes the drawer on narrow windows. */
    function trigger(callback) {
        callback();
        if (modal) {
            close();
        }
    }

    // applicationWindow() is a Kirigami helper returning the ApplicationWindow
    // this item lives in; wideScreen flips when the window gets narrow.
    modal: !applicationWindow().wideScreen
    handleVisible: modal
    width: Kirigami.Units.gridUnit * 15

    topContent: [
        // Brand header: app icon, name and the open garage file.
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.largeSpacing
            Layout.bottomMargin: Kirigami.Units.gridUnit
            spacing: Kirigami.Units.mediumSpacing

            Image {
                source: "qrc:/packaging/icons/io.github.TSTS87.GarageLog.svg"
                sourceSize.width: Kirigami.Units.iconSizes.medium
                sourceSize.height: Kirigami.Units.iconSizes.medium
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                Controls.Label {
                    text: i18n("GarageLog")
                    font.weight: Font.Bold
                    font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.25
                }
                Controls.Label {
                    Layout.fillWidth: true
                    // Only the file name; the full path is shown in the settings.
                    text: Format.fileName(Garage.filePath)
                    textFormat: Text.PlainText
                    font: Kirigami.Theme.smallFont
                    color: Kirigami.Theme.disabledTextColor
                    elide: Text.ElideMiddle
                }
            }
        },

        RowLayout {
            Layout.fillWidth: true
            Layout.rightMargin: Kirigami.Units.smallSpacing

            SectionHeader {
                Layout.fillWidth: true
                text: i18n("Vehicles")
            }
            Controls.ToolButton {
                icon.name: "list-add"
                text: i18n("Add Vehicle")
                display: Controls.AbstractButton.IconOnly
                Controls.ToolTip.visible: hovered
                Controls.ToolTip.text: text
                onClicked: drawer.trigger(() => drawer.addRequested())
            }
        },

        ListView {
            id: list
            Layout.fillWidth: true
            // The drawer scrolls as a whole; the list itself shows all rows.
            implicitHeight: contentHeight
            interactive: false
            model: Vehicles

            delegate: DrawerRow {
                // "required property" pulls the matching model role into the
                // delegate explicitly (Qt 6 style) instead of the implicit model.xxx.
                required property string vehicleId
                required property string name
                required property string type
                required property string iconName
                required property var odometerM
                required property bool archived

                width: ListView.view.width
                text: name
                subtitle: archived ? i18n("%1 · archived", Format.distance(odometerM)) : Format.distance(odometerM)
                iconSource: iconName
                accent: Style.accent(type)
                filledIcon: true
                selected: vehicleId === drawer.currentVehicleId
                opacity: archived ? 0.6 : 1.0
                onClicked: drawer.trigger(() => drawer.vehicleSelected(vehicleId))
            }
        },

        // "Add" row at the end of the list in grey: the obvious next step
        // for a new garage, still unobtrusive with many vehicles.
        DrawerRow {
            Layout.fillWidth: true
            text: i18n("Add Vehicle")
            iconSource: "list-add"
            accent: Kirigami.Theme.disabledTextColor
            onClicked: drawer.trigger(() => drawer.addRequested())
        },

        SectionHeader {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.largeSpacing
            text: i18n("Garage")
        },

        DrawerRow {
            Layout.fillWidth: true
            text: i18n("Due")
            iconSource: "qrc:/qml/icons/due.svg"
            accent: DueIntervals.overdueCount > 0 ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.highlightColor
            selected: drawer.dueShown
            // The overdue count makes reminders visible without opening the page.
            badge: DueIntervals.overdueCount > 0 ? String(DueIntervals.overdueCount) : ""
            badgeToolTip: i18n("Due (%1 overdue)", DueIntervals.overdueCount)
            onClicked: drawer.trigger(() => drawer.dueRequested())
        },

        DrawerRow {
            id: dataRow
            Layout.fillWidth: true
            text: i18n("Data")
            iconSource: "document-export"
            accent: Kirigami.Theme.highlightColor
            onClicked: dataMenu.popup(dataRow, dataRow.width - Kirigami.Units.largeSpacing, 0)

            // A plain menu replaces the drawer's submenu; it opens beside the row.
            Controls.Menu {
                id: dataMenu
                Controls.MenuItem {
                    text: i18n("Export Everything (JSON)…")
                    icon.name: "document-export"
                    onTriggered: drawer.trigger(() => drawer.dataRequested("exportJson"))
                }
                Controls.MenuItem {
                    text: i18n("Import Export Folder (JSON)…")
                    icon.name: "document-import"
                    onTriggered: drawer.trigger(() => drawer.dataRequested("importJson"))
                }
                Controls.MenuItem {
                    text: i18n("Export Tables (CSV)…")
                    icon.name: "text-csv"
                    onTriggered: drawer.trigger(() => drawer.dataRequested("exportCsv"))
                }
                Controls.MenuItem {
                    text: i18n("Import Fuel Entries (CSV)…")
                    icon.name: "text-csv"
                    onTriggered: drawer.trigger(() => drawer.dataRequested("importCsv"))
                }
            }
        },

        DrawerRow {
            Layout.fillWidth: true
            text: i18n("Settings")
            iconSource: "settings-configure"
            accent: Kirigami.Theme.highlightColor
            selected: drawer.settingsShown
            onClicked: drawer.trigger(() => drawer.settingsRequested())
        },

        Controls.Switch {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.largeSpacing
            text: i18n("Show Archived")
            checked: Vehicles.showArchived
            onToggled: Vehicles.showArchived = checked
        }
    ]

    /**
     * Row of the side bar: icon tile, title, optional subtitle and badge.
     * Selected rows get a tint of their accent and a bar on the left.
     */
    component DrawerRow: Controls.ItemDelegate {
        id: drawerRow

        property string subtitle
        property string iconSource
        property color accent: Kirigami.Theme.highlightColor
        property bool filledIcon: false
        property bool selected: false
        /** Short text in a red pill on the right (e.g. overdue count); "" hides it. */
        property string badge
        property string badgeToolTip

        horizontalPadding: Kirigami.Units.largeSpacing
        verticalPadding: Kirigami.Units.smallSpacing
        Accessible.description: subtitle

        background: Rectangle {
            anchors.fill: parent
            anchors.leftMargin: Kirigami.Units.smallSpacing
            anchors.rightMargin: Kirigami.Units.smallSpacing
            radius: Style.tileRadius
            color: drawerRow.selected ? Qt.rgba(drawerRow.accent.r, drawerRow.accent.g, drawerRow.accent.b, 0.16)
                : drawerRow.hovered ? Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.06)
                : "transparent"
            border.width: drawerRow.visualFocus ? 2 : 0
            border.color: Kirigami.Theme.focusColor

            // Accent bar on the left edge marks the current page.
            Rectangle {
                visible: drawerRow.selected
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                width: 3
                height: parent.height * 0.5
                radius: 1.5
                color: drawerRow.accent
            }
        }

        contentItem: RowLayout {
            spacing: Kirigami.Units.mediumSpacing

            IconTile {
                source: drawerRow.iconSource
                accent: drawerRow.accent
                filled: drawerRow.filledIcon
                size: Kirigami.Units.gridUnit * (drawerRow.subtitle !== "" ? 2 : 1.6)
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                Controls.Label {
                    Layout.fillWidth: true
                    text: drawerRow.text
                    textFormat: Text.PlainText
                    font.weight: drawerRow.selected ? Font.DemiBold : Font.Normal
                    elide: Text.ElideRight
                }
                Controls.Label {
                    Layout.fillWidth: true
                    visible: drawerRow.subtitle !== ""
                    text: drawerRow.subtitle
                    textFormat: Text.PlainText
                    font: Kirigami.Theme.smallFont
                    color: Kirigami.Theme.disabledTextColor
                    elide: Text.ElideRight
                }
            }
            Rectangle {
                visible: drawerRow.badge !== ""
                implicitWidth: Math.max(implicitHeight, badgeLabel.implicitWidth + Kirigami.Units.smallSpacing * 2)
                implicitHeight: badgeLabel.implicitHeight + 2
                radius: height / 2
                color: Kirigami.Theme.negativeTextColor
                Controls.ToolTip.visible: badgeHover.hovered
                Controls.ToolTip.text: drawerRow.badgeToolTip
                HoverHandler {
                    id: badgeHover
                }
                Controls.Label {
                    id: badgeLabel
                    anchors.centerIn: parent
                    text: drawerRow.badge
                    color: "white"
                    font: Kirigami.Theme.smallFont
                }
            }
        }
    }
}
