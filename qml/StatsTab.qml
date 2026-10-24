// Statistics tab of the vehicle page (role: qml, pure UI).
// Period pill switch (year / 12 months / all), a bento grid of key figures
// (ADR-015), the consumption line chart (KQuickCharts, ADR-009) and the cost
// distribution by category, each in a card.
// All numbers come from the Stats singleton (C++); QML only lays them out.

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.quickcharts as Charts
import org.kde.quickcharts.controls as ChartsControls
import org.tmx.garagelog

Item {
    id: tab

    /** Accent of the vehicle type: featured tile, chart line, fuel bar. */
    property color accent: Kirigami.Theme.highlightColor

    /** Nothing recorded for this vehicle at all; tiles of zeros would say nothing. */
    readonly property bool empty: FuelEntries.count === 0 && ServiceEntries.count === 0

    Kirigami.PlaceholderMessage {
        anchors.centerIn: parent
        width: parent.width - Kirigami.Units.gridUnit * 4
        visible: tab.empty
        icon.name: "office-chart-line"
        text: i18n("No statistics yet")
        explanation: i18n("Costs, consumption and charts appear here once you add fuel or service entries.")
    }

    Controls.ScrollView {
        id: scroll
        anchors.fill: parent
        visible: !tab.empty

        // Without this the content would be as wide as its implicit width and
        // scroll sideways; the tab should only scroll vertically.
        contentWidth: availableWidth

        ColumnLayout {
            width: scroll.availableWidth
            spacing: Kirigami.Units.largeSpacing

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                type: Kirigami.MessageType.Error
                text: Stats.errorString
                visible: text !== ""
            }

            // The pill switch picks the period; the date range sits beside it.
            Flow {
                Layout.fillWidth: true
                spacing: Kirigami.Units.largeSpacing

                PillBar {
                    id: periodBar
                    readonly property var keys: ["year", "12months", "all"]
                    width: Math.min(scroll.availableWidth, Kirigami.Units.gridUnit * 24)
                    model: [{ text: i18n("This Year") }, { text: i18n("Last 12 Months") }, { text: i18n("All Time") }]
                    accent: tab.accent
                    currentIndex: Math.max(0, keys.indexOf(Stats.period))
                    onCurrentIndexChanged: Stats.period = keys[currentIndex]
                }

                Controls.Label {
                    // Same height as the bar, so the text sits on its middle.
                    height: periodBar.height
                    verticalAlignment: Text.AlignVCenter
                    text: Stats.periodFrom === "" ? i18n("until %1", Format.date(Stats.periodTo))
                        : i18nc("date range", "%1 – %2", Format.date(Stats.periodFrom), Format.date(Stats.periodTo))
                    color: Kirigami.Theme.disabledTextColor
                }
            }

            // Bento grid: the total cost as large featured tile, the other
            // figures around it. Four columns on wide pages, two on narrow ones.
            GridLayout {
                id: bento
                readonly property bool wide: scroll.availableWidth > Kirigami.Units.gridUnit * 36

                Layout.fillWidth: true
                columns: wide ? 4 : 2
                columnSpacing: Kirigami.Units.largeSpacing
                rowSpacing: Kirigami.Units.largeSpacing

                // Featured tile in the vehicle colour with the fuel/service split as bar.
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.columnSpan: 2
                    Layout.rowSpan: bento.wide ? 2 : 1
                    Layout.preferredWidth: 2 // with the 1s below: equal column widths
                    implicitHeight: totalColumn.implicitHeight + Kirigami.Units.largeSpacing * 3
                    radius: Style.cardRadius
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: tab.accent }
                        GradientStop { position: 1.0; color: Qt.darker(tab.accent, 1.6) }
                    }

                    ColumnLayout {
                        id: totalColumn
                        anchors.fill: parent
                        anchors.margins: Kirigami.Units.largeSpacing * 1.5
                        spacing: Kirigami.Units.smallSpacing

                        Controls.Label {
                            text: i18n("Total cost").toUpperCase()
                            color: Qt.rgba(1, 1, 1, 0.75)
                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                            font.weight: Font.DemiBold
                            font.letterSpacing: 1.2
                        }
                        Item {
                            Layout.fillHeight: true
                        }
                        Controls.Label {
                            Layout.fillWidth: true
                            text: Format.money(Stats.totalCostC)
                            color: "white"
                            font.pointSize: Kirigami.Theme.defaultFont.pointSize * Style.heroFigureScale
                            font.weight: Font.Black
                            font.features: { "tnum": 1 }
                            elide: Text.ElideRight
                        }

                        // Share of fuel in the total, as a two-part bar.
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.topMargin: Kirigami.Units.smallSpacing
                            implicitHeight: Kirigami.Units.smallSpacing * 2
                            radius: height / 2
                            color: Qt.rgba(1, 1, 1, 0.3)
                            Rectangle {
                                width: Stats.totalCostC > 0 ? parent.width * Stats.fuelCostC / Stats.totalCostC : 0
                                height: parent.height
                                radius: height / 2
                                color: "white"
                            }
                        }
                        Controls.Label {
                            Layout.fillWidth: true
                            text: i18n("Fuel %1 · Service %2", Format.money(Stats.fuelCostC), Format.money(Stats.serviceCostC))
                            color: Qt.rgba(1, 1, 1, 0.85)
                            font: Kirigami.Theme.smallFont
                            elide: Text.ElideRight
                        }
                    }
                }

                StatTile {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 1
                    iconSource: "qrc:/qml/icons/odometer.svg"
                    accent: Style.seriesColors[2]
                    caption: i18n("Cost per %1", Format.distanceLabel)
                    value: Stats.costPerKmC === null ? "–" : Format.moneyPerKm(Stats.costPerKmC)
                    detail: Stats.distanceM === null ? i18n("no distance yet") : i18n("over %1", Format.distance(Stats.distanceM))
                }
                StatTile {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 1
                    iconSource: "qrc:/qml/icons/interval.svg"
                    accent: Style.seriesColors[3]
                    caption: i18n("Cost per month")
                    value: Stats.costPerMonthC === null ? "–" : Format.money(Stats.costPerMonthC)
                    detail: i18n("average")
                }
                StatTile {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.columnSpan: 2
                    Layout.preferredWidth: 2
                    iconSource: Style.fuelIcon(Stats.electric)
                    accent: Style.seriesColors[1]
                    caption: i18n("Consumption")
                    value: Stats.averageConsumption === null ? "–" : Format.consumption(Stats.averageConsumption, Stats.electric)
                    detail: i18n("average between full tanks")
                }
            }

            Card {
                Layout.fillWidth: true
                implicitHeight: chartColumn.implicitHeight + Kirigami.Units.largeSpacing * 3

                ColumnLayout {
                    id: chartColumn
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing * 1.5
                    spacing: Kirigami.Units.largeSpacing

                    Kirigami.Heading {
                        level: 4
                        // Stats.electric is re-announced on every reload, also after a unit change.
                        text: i18n("Consumption (%1)", Format.consumptionUnit(Stats.electric))
                    }

                    Item {
                        id: chartBox
                        Layout.fillWidth: true
                        // Half a label of room above, so the top axis label clears the heading.
                        Layout.topMargin: Kirigami.Units.smallSpacing
                        implicitHeight: Kirigami.Units.gridUnit * 12

                        readonly property bool hasLine: values.length >= 2
                        // Start the axis at zero so differences are not exaggerated;
                        // the top is rounded up to an even number, so the five axis
                        // labels land on readable values (0, 2.5, 5, ...).
                        readonly property real axisMax: values.length === 0 ? 2
                            : Math.max(2, Math.ceil(Math.max(...values) * 1.2 / 2) * 2)
                        /** Chart values in the display unit (l/100 km, mpg, kWh/100 km, ...). */
                        readonly property var values: Stats.consumptionValues.map(v => Format.consumptionValue(v, Stats.electric))

                        Controls.Label {
                            anchors.centerIn: parent
                            visible: !chartBox.hasLine
                            text: i18n("At least three full tanks are needed for a consumption curve.")
                            color: Kirigami.Theme.disabledTextColor
                        }

                        Item {
                            anchors.fill: parent
                            visible: chartBox.hasLine

                            // Y axis labels on the left: five plain labels at the
                            // quarter marks of the fixed range 0 … axisMax. KQuickCharts'
                            // AxisLabels created no labels inside the card layout.
                            Item {
                                id: yLabels
                                anchors { left: parent.left; top: chart.top; bottom: chart.bottom }
                                width: Kirigami.Units.gridUnit * 2

                                Repeater {
                                    model: 5
                                    Controls.Label {
                                        required property int index
                                        // index 0 at the top (axisMax), 4 at the bottom (0).
                                        y: yLabels.height * index / 4 - height / 2
                                        text: Format.number(chartBox.axisMax * (4 - index) / 4, 1)
                                        font: Kirigami.Theme.smallFont
                                        color: Kirigami.Theme.disabledTextColor
                                    }
                                }
                            }

                            ChartsControls.GridLines {
                                anchors.fill: chart
                                chart: chart
                                direction: ChartsControls.GridLines.Vertical
                                major.frequency: 1
                                major.count: 3
                                major.color: Kirigami.Theme.alternateBackgroundColor
                                minor.visible: false
                            }

                            // LineChart draws the values of its valueSources evenly spaced
                            // from left to right (one point per closing full tank).
                            Charts.LineChart {
                                id: chart
                                anchors { left: yLabels.right; leftMargin: Kirigami.Units.smallSpacing; right: parent.right; top: parent.top; bottom: xLabels.top }

                                yRange {
                                    automatic: false
                                    from: 0
                                    to: chartBox.axisMax
                                }
                                lineWidth: 3
                                fillOpacity: 0.2
                                valueSources: Charts.ArraySource { array: chartBox.values }
                                colorSource: Charts.SingleValueSource { value: tab.accent }
                                nameSource: Charts.SingleValueSource { value: i18n("Consumption") }
                            }

                            // First and last date under the line, enough to read the time span.
                            RowLayout {
                                id: xLabels
                                anchors { left: chart.left; right: chart.right; bottom: parent.bottom }
                                Controls.Label {
                                    text: Stats.consumptionDates.length > 0 ? Format.date(Stats.consumptionDates[0]) : ""
                                    font: Kirigami.Theme.smallFont
                                    color: Kirigami.Theme.disabledTextColor
                                }
                                Item { Layout.fillWidth: true }
                                Controls.Label {
                                    text: Stats.consumptionDates.length > 0 ? Format.date(Stats.consumptionDates[Stats.consumptionDates.length - 1]) : ""
                                    font: Kirigami.Theme.smallFont
                                    color: Kirigami.Theme.disabledTextColor
                                }
                            }
                        }
                    }

                }
            }

            Card {
                Layout.fillWidth: true
                implicitHeight: categoryColumn.implicitHeight + Kirigami.Units.largeSpacing * 3

                ColumnLayout {
                    id: categoryColumn
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing * 1.5
                    spacing: Kirigami.Units.largeSpacing

                    Kirigami.Heading {
                        level: 4
                        text: i18n("Costs by Category")
                    }

                    Controls.Label {
                        visible: Stats.categoryCosts.length === 0
                        text: i18n("No costs in this period.")
                        color: Kirigami.Theme.disabledTextColor
                    }

                    // Horizontal bars: name, bar scaled to the largest amount, amount.
                    // Plain rectangles read better than a chart with a separate legend.
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 3
                        columnSpacing: Kirigami.Units.largeSpacing
                        rowSpacing: Kirigami.Units.smallSpacing

                        Repeater {
                            model: Stats.categoryCosts
                            delegate: Controls.Label {
                                required property var modelData
                                required property int index
                                Layout.row: index
                                Layout.column: 0
                                text: modelData.name
                            }
                        }
                        Repeater {
                            model: Stats.categoryCosts
                            delegate: Item {
                                required property var modelData
                                required property int index
                                Layout.row: index
                                Layout.column: 1
                                Layout.fillWidth: true
                                implicitHeight: Kirigami.Units.gridUnit
                                Rectangle {
                                    // The list is sorted, so the first entry is the largest.
                                    width: Math.max(2, parent.width * modelData.costC / Stats.categoryCosts[0].costC)
                                    height: parent.height
                                    radius: Kirigami.Units.cornerRadius
                                    // Fuel in the vehicle colour, every other category in its own series colour.
                                    color: modelData.isFuel ? tab.accent : Style.seriesColors[(index + 1) % Style.seriesColors.length]
                                }
                            }
                        }
                        Repeater {
                            model: Stats.categoryCosts
                            delegate: Controls.Label {
                                required property var modelData
                                required property int index
                                Layout.row: index
                                Layout.column: 2
                                Layout.alignment: Qt.AlignRight
                                text: Format.money(modelData.costC)
                            }
                        }
                    }
                }
            }

            // Breathing room under the last card.
            Item {
                implicitHeight: Kirigami.Units.smallSpacing
            }
        }
    }
}
