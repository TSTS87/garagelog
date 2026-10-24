// Hero card at the top of the vehicle page (role: qml, pure UI, ADR-015).
// Gradient in the colour of the vehicle type with a large line drawing of
// the vehicle, type/powertrain chips, name, licence plate, the odometer as
// big figure and the most urgent interval. All values come from C++.

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.tmx.garagelog

Rectangle {
    id: hero

    /** Values from Vehicles.get(): name, type, make, model, year, plate, powertrain, icon, odometerM, archived. */
    property var vehicle: ({})
    /** Smaller variant for low windows: no chips, smaller figures. */
    property bool compact: false

    readonly property string type: vehicle.type ?? "other"
    readonly property real pad: Kirigami.Units.gridUnit * (compact ? 0.9 : 1.3)

    implicitHeight: content.implicitHeight + 2 * pad
    radius: Style.cardRadius
    // A horizontal gradient from the light to the deep accent gives the card
    // depth without images; Rectangle draws it with the rounded corners.
    gradient: Gradient {
        orientation: Gradient.Horizontal
        GradientStop { position: 0.0; color: Style.accent(hero.type) }
        GradientStop { position: 1.0; color: Style.accentDeep(hero.type) }
    }

    // Big translucent drawing of the vehicle on the right; purely decorative.
    Kirigami.Icon {
        anchors.right: parent.right
        anchors.rightMargin: hero.pad
        anchors.verticalCenter: parent.verticalCenter
        height: Math.min(parent.height - hero.pad, Kirigami.Units.gridUnit * 10)
        width: height
        visible: hero.width > Kirigami.Units.gridUnit * 26
        source: hero.vehicle.icon ?? ""
        isMask: true
        // The mask colour ignores alpha, so the transparency comes from opacity.
        color: "white"
        opacity: 0.16
    }

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: hero.pad
        spacing: Kirigami.Units.smallSpacing

        // Chips: vehicle type, electric drive, archived.
        Flow {
            Layout.fillWidth: true
            Layout.bottomMargin: Kirigami.Units.smallSpacing
            visible: !hero.compact
            spacing: Kirigami.Units.smallSpacing

            HeroChip {
                iconSource: hero.vehicle.icon ?? ""
                text: hero.vehicle.typeLabel ?? ""
            }
            HeroChip {
                visible: hero.vehicle.powertrain === "electric"
                iconSource: "qrc:/qml/icons/charge.svg"
                text: i18n("Electric")
            }
            HeroChip {
                visible: hero.vehicle.archived === true
                iconSource: "archive-extract"
                text: i18n("Archived")
            }
        }

        Controls.Label {
            Layout.fillWidth: true
            text: hero.vehicle.name ?? ""
            textFormat: Text.PlainText
            color: "white"
            font.pointSize: Kirigami.Theme.defaultFont.pointSize * (hero.compact ? 1.5 : 2.0)
            font.weight: Font.Bold
            elide: Text.ElideRight
        }

        // "Volkswagen Golf 2019" and the plate styled like a real one.
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            Controls.Label {
                readonly property string makeModelYear: [hero.vehicle.make, hero.vehicle.model, hero.vehicle.year]
                    .filter(part => part !== undefined && part !== null && part !== "").join(" ")
                visible: makeModelYear !== ""
                text: makeModelYear
                textFormat: Text.PlainText
                color: Qt.rgba(1, 1, 1, 0.85)
                elide: Text.ElideRight
                Layout.maximumWidth: hero.width / 2
            }
            LicensePlate {
                visible: (hero.vehicle.plate ?? "") !== ""
                text: hero.vehicle.plate ?? ""
            }
        }

        // Odometer and next due side by side; Flow wraps them on narrow pages.
        Flow {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.gridUnit * 1.5

            ColumnLayout {
                spacing: 0

                Controls.Label {
                    text: i18n("Odometer").toUpperCase()
                    color: Qt.rgba(1, 1, 1, 0.7)
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    font.letterSpacing: 1.2
                    font.weight: Font.DemiBold
                }
                Controls.Label {
                    text: Format.distance(hero.vehicle.odometerM ?? 0)
                    color: "white"
                    font.pointSize: Kirigami.Theme.defaultFont.pointSize * (hero.compact ? 1.8 : Style.heroFigureScale)
                    font.weight: Font.Black
                    font.features: { "tnum": 1 }
                }
            }

            // Most urgent interval on a frosted panel; hidden without intervals.
            Rectangle {
                visible: Intervals.worstStatus !== ""
                width: Math.min(dueRow.implicitWidth + Kirigami.Units.largeSpacing * 2, content.width)
                height: dueRow.implicitHeight + Kirigami.Units.largeSpacing * 1.5
                radius: Style.tileRadius
                color: Qt.rgba(1, 1, 1, 0.15)
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.2)

                RowLayout {
                    id: dueRow
                    anchors.fill: parent
                    anchors.leftMargin: Kirigami.Units.largeSpacing
                    anchors.rightMargin: Kirigami.Units.largeSpacing
                    spacing: Kirigami.Units.largeSpacing

                    // White ring around the dot keeps red/yellow/green visible on every gradient.
                    Rectangle {
                        implicitWidth: Kirigami.Units.iconSizes.small
                        implicitHeight: implicitWidth
                        radius: width / 2
                        color: "white"
                        StatusDot {
                            anchors.centerIn: parent
                            width: parent.width - 4
                            height: width
                            status: Intervals.worstStatus
                        }
                    }

                    ColumnLayout {
                        id: dueText
                        Layout.fillWidth: true
                        spacing: 0
                        readonly property var next: Intervals.nextDue

                        Controls.Label {
                            Layout.fillWidth: true
                            text: dueText.next.name === undefined ? i18n("Next due") : i18n("Next due: %1", dueText.next.name)
                            textFormat: Text.PlainText
                            color: "white"
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        Controls.Label {
                            Layout.fillWidth: true
                            text: dueText.next.name === undefined
                                ? i18n("No interval with a recorded service yet")
                                : Format.dueSummary(dueText.next.remainingM, dueText.next.remainingDays, dueText.next.status)
                            color: Qt.rgba(1, 1, 1, 0.85)
                            font: Kirigami.Theme.smallFont
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }

    /** Small translucent pill with icon and text on the gradient. */
    component HeroChip: Rectangle {
        id: chip
        property string iconSource
        property alias text: chipLabel.text

        width: chipRow.implicitWidth + Kirigami.Units.largeSpacing * 1.5
        height: chipRow.implicitHeight + Kirigami.Units.smallSpacing * 1.5
        radius: height / 2
        color: Qt.rgba(1, 1, 1, 0.18)

        RowLayout {
            id: chipRow
            anchors.centerIn: parent
            spacing: Kirigami.Units.smallSpacing
            Kirigami.Icon {
                source: chip.iconSource
                implicitWidth: Kirigami.Units.iconSizes.small
                implicitHeight: implicitWidth
                isMask: true
                color: "white"
            }
            Controls.Label {
                id: chipLabel
                color: "white"
                font: Kirigami.Theme.smallFont
            }
        }
    }

    /** Licence plate look: white field, dark frame, bold spaced letters. */
    component LicensePlate: Rectangle {
        property alias text: plateLabel.text

        implicitWidth: plateLabel.implicitWidth + Kirigami.Units.largeSpacing * 1.5
        implicitHeight: plateLabel.implicitHeight + Kirigami.Units.smallSpacing
        radius: 4
        color: "#fafafa"
        border.width: 1.5
        border.color: "#1f2937"

        Controls.Label {
            id: plateLabel
            textFormat: Text.PlainText
            anchors.centerIn: parent
            color: "#111827"
            font.weight: Font.Bold
            font.letterSpacing: 1.5
        }
    }
}
