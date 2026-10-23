// Statistics tab of the vehicle page (role: qml, pure UI).
// Period filter (year / 12 months / all), key figure tiles, the consumption
// line chart (KQuickCharts, ADR-009) and the cost distribution by category.
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

            // Three exclusive checkable buttons act as a segmented control;
            // Kirigami has none of its own. Flow (not RowLayout) wraps the date
            // range into a second line when the tab is a narrow column.
            Flow {
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.smallSpacing

                Repeater {
                    id: periodButtons
                    model: [
                        { key: "year", text: i18n("This Year") },
                        { key: "12months", text: i18n("Last 12 Months") },
                        { key: "all", text: i18n("All Time") }
                    ]
                    Controls.Button {
                        required property var modelData
                        text: modelData.text
                        checkable: true
                        checked: Stats.period === modelData.key
                        onClicked: Stats.period = modelData.key
                    }
                }

                Controls.Label {
                    // Same height as the buttons, so the text sits on their middle.
                    height: periodButtons.itemAt(0)?.height ?? implicitHeight
                    leftPadding: Kirigami.Units.largeSpacing
                    verticalAlignment: Text.AlignVCenter
                    text: Stats.periodFrom === "" ? i18n("until %1", Format.date(Stats.periodTo))
                        : i18nc("date range", "%1 – %2", Format.date(Stats.periodFrom), Format.date(Stats.periodTo))
                    color: Kirigami.Theme.disabledTextColor
                }
            }

            // Four tiles; two columns on narrow windows.
            GridLayout {
                Layout.fillWidth: true
                columns: scroll.availableWidth > Kirigami.Units.gridUnit * 36 ? 4 : 2
                columnSpacing: Kirigami.Units.largeSpacing
                rowSpacing: Kirigami.Units.largeSpacing

                Repeater {
                    model: [
                        { caption: i18n("Total cost"), value: Format.money(Stats.totalCostC),
                          detail: i18n("Fuel %1 · Service %2", Format.money(Stats.fuelCostC), Format.money(Stats.serviceCostC)) },
                        { caption: i18n("Cost per %1", Format.distanceLabel), value: Stats.costPerKmC === null ? "–" : Format.moneyPerKm(Stats.costPerKmC),
                          detail: Stats.distanceM === null ? i18n("no distance yet") : i18n("over %1", Format.distance(Stats.distanceM)) },
                        { caption: i18n("Cost per month"), value: Stats.costPerMonthC === null ? "–" : Format.money(Stats.costPerMonthC),
                          detail: i18n("average") },
                        { caption: i18n("Consumption"), value: Stats.averageConsumption === null ? "–" : Format.consumption(Stats.averageConsumption, Stats.electric),
                          detail: i18n("average between full tanks") }
                    ]

                    // AbstractCard is Kirigami's plain card: a framed, padded
                    // surface; we put our own caption/value layout inside.
                    Kirigami.AbstractCard {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1 // equal column widths regardless of text length

                        contentItem: ColumnLayout {
                            spacing: 0
                            Controls.Label {
                                text: modelData.caption
                                color: Kirigami.Theme.disabledTextColor
                            }
                            Kirigami.Heading {
                                Layout.fillWidth: true
                                level: 2
                                text: modelData.value
                                elide: Text.ElideRight
                            }
                            Controls.Label {
                                Layout.fillWidth: true
                                text: modelData.detail
                                font: Kirigami.Theme.smallFont
                                color: Kirigami.Theme.disabledTextColor
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }

            Kirigami.Heading {
                level: 3
                // Stats.electric is re-announced on every reload, also after a unit change.
                text: i18n("Consumption (%1)", Format.consumptionUnit(Stats.electric))
            }

            Item {
                id: chartBox
                Layout.fillWidth: true
                implicitHeight: Kirigami.Units.gridUnit * 12

                readonly property bool hasLine: values.length >= 2
                /** Chart values in the display unit (l/100 km, mpg, kWh/100 km, ...). */
                readonly property var values: Stats.consumptionValues.map(v => Format.consumptionValue(v, Stats.electric))

                Controls.Label {
                    anchors.centerIn: parent
                    visible: !chartBox.hasLine
                    text: i18n("At least three full tanks are needed for a consumption curve.")
                    color: Kirigami.Theme.disabledTextColor
                }

                // Hidden as a whole: AxisLabels does not recreate its labels when
                // it becomes visible again, so it must stay visible itself.
                Item {
                    anchors.fill: parent
                    visible: chartBox.hasLine

                    // Y axis labels on the left, computed by KQuickCharts from the chart's range.
                    ChartsControls.AxisLabels {
                        id: yLabels
                        anchors { left: parent.left; top: chart.top; bottom: chart.bottom }
                        // Fixed width: the labels are created after the first layout
                        // pass, so an implicit width would leave no room for them.
                        width: Kirigami.Units.gridUnit * 2
                        direction: ChartsControls.AxisLabels.VerticalBottomTop
                        delegate: Controls.Label {
                            text: Format.number(ChartsControls.AxisLabels.label, 1)
                            font: Kirigami.Theme.smallFont
                        }
                        source: Charts.ChartAxisSource { chart: chart; axis: Charts.ChartAxisSource.YAxis; itemCount: 5 }
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

                        // Start the axis at zero so differences are not exaggerated;
                        // the top is rounded up to an even number, so the five axis
                        // labels land on readable values (0, 2.5, 5, ...).
                        yRange {
                            automatic: false
                            from: 0
                            to: Math.max(2, Math.ceil(Math.max(...chartBox.values) * 1.2 / 2) * 2)
                        }
                        lineWidth: 2
                        fillOpacity: 0.15
                        valueSources: Charts.ArraySource { array: chartBox.values }
                        colorSource: Charts.SingleValueSource { value: Kirigami.Theme.highlightColor }
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

            Kirigami.Heading {
                level: 3
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
                Layout.bottomMargin: Kirigami.Units.largeSpacing
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
                            color: modelData.isFuel ? Kirigami.Theme.highlightColor : Kirigami.Theme.positiveTextColor
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
}
