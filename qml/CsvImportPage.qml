// CSV import of fuel entries (role: qml, pure UI).
// Choose a file, assign its columns to the fuel fields (pre-filled by
// DataTransfer.suggestCsvMapping(), e.g. for Fuelio), check the dry-run
// preview with per-row errors, then import into one vehicle (SPEC §4.6).

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.tmx.garagelog

Kirigami.ScrollablePage {
    id: page

    /** Vehicle to import into, preselected by Main.qml. */
    property string vehicleId: ""

    property string fileName: ""
    /** Column assignment as DataTransfer expects it; replaced as a whole on every change so bindings update. */
    property var mapping: ({})
    /**
     * Column names for the pickers. Depends only on file and header row,
     * so changing a column assignment does not replace the pickers' model
     * (which would reset their selection).
     */
    readonly property int headerRow: mapping.headerRow ?? 0
    readonly property var columnNames: fileName === "" ? [] : DataTransfer.csvHeader(headerRow)
    /** Dry run, re-evaluated whenever the mapping changes. */
    readonly property var preview: fileName === "" ? ({ count: 0, entries: [], errors: [] }) : DataTransfer.previewCsv(mapping)

    /**
     * Picker for one mapping key; entry 0 means "not in the file". An
     * inline component is a small reusable type local to this file.
     */
    component ColumnPicker: Controls.ComboBox {
        required property string key
        visible: page.fileName !== ""
        model: [i18n("– none –")].concat(page.columnNames)
        currentIndex: (page.mapping[key] ?? -1) + 1
        // Replacing the model resets the selection and drops the binding;
        // restore it once the new model is in place.
        onModelChanged: Qt.callLater(() => currentIndex = Qt.binding(() => (page.mapping[key] ?? -1) + 1))
        onActivated: index => page.setMapping(key, index - 1)
    }

    signal imported()
    signal canceled()

    /** Copy-and-replace keeps "mapping" a new object, so dependent bindings re-run. */
    function setMapping(key, value) {
        const m = Object.assign({}, mapping);
        m[key] = value;
        mapping = m;
    }

    title: i18n("Import Fuel Entries from CSV")

    actions: [
        Kirigami.Action {
            text: i18n("Import %1 Entries", page.preview.count)
            icon.name: "document-import"
            enabled: page.preview.count > 0 && vehicleBox.currentIndex >= 0
            onTriggered: {
                const count = DataTransfer.importCsv(Vehicles.idAt(vehicleBox.currentIndex), page.mapping);
                if (count >= 0) {
                    applicationWindow().showPassiveNotification(i18np("%1 entry imported", "%1 entries imported", count));
                    page.imported();
                }
            }
        },
        Kirigami.Action {
            text: i18n("Cancel")
            icon.name: "dialog-cancel"
            shortcut: "Escape"
            onTriggered: page.canceled()
        }
    ]

    header: Kirigami.InlineMessage {
        position: Kirigami.InlineMessage.Position.Header
        type: Kirigami.MessageType.Error
        text: DataTransfer.errorString
        visible: text !== ""
    }

    FileDialog {
        id: fileDialog
        title: i18n("Choose CSV File")
        fileMode: FileDialog.OpenFile
        nameFilters: [i18n("CSV files (*.csv *.txt)"), i18n("All files (*)")]
        onAccepted: {
            if (DataTransfer.loadCsv(selectedFile)) {
                page.fileName = Format.fileName(selectedFile.toString());
                page.mapping = DataTransfer.suggestCsvMapping();
            }
        }
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Kirigami.FormLayout {
            Layout.fillWidth: true

            RowLayout {
                Kirigami.FormData.label: i18n("File:")
                Controls.Button {
                    text: i18n("Choose File…")
                    icon.name: "document-open"
                    // Choosing a file is the only possible first step.
                    focus: true
                    onClicked: fileDialog.open()
                }
                Controls.Label {
                    text: page.fileName
                    textFormat: Text.PlainText
                }
            }

            // Vehicles is a QAbstractListModel; textRole picks the role shown.
            Controls.ComboBox {
                id: vehicleBox
                Kirigami.FormData.label: i18n("Vehicle:")
                model: Vehicles
                textRole: "name"
                Component.onCompleted: {
                    for (let i = 0; i < Vehicles.count; ++i) {
                        if (Vehicles.idAt(i) === page.vehicleId) {
                            currentIndex = i;
                        }
                    }
                }
            }

            Kirigami.Separator {
                visible: page.fileName !== ""
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18n("Columns")
            }

            // 1-based like a line number in a text editor.
            Controls.SpinBox {
                visible: page.fileName !== ""
                Kirigami.FormData.label: i18n("Header in row:")
                from: 1
                to: 9999
                value: (page.mapping.headerRow ?? 0) + 1
                onValueModified: page.setMapping("headerRow", value - 1)
            }

            ColumnPicker {
                key: "dateColumn"
                Kirigami.FormData.label: i18n("Date:")
            }
            ColumnPicker {
                key: "odometerColumn"
                Kirigami.FormData.label: i18n("Odometer (km):")
            }
            ColumnPicker {
                key: "quantityColumn"
                Kirigami.FormData.label: i18n("Quantity (l or kWh):")
            }
            ColumnPicker {
                key: "totalCostColumn"
                Kirigami.FormData.label: i18n("Total price:")
            }
            ColumnPicker {
                key: "unitPriceColumn"
                Kirigami.FormData.label: i18n("Unit price:")
            }
            ColumnPicker {
                key: "fullTankColumn"
                Kirigami.FormData.label: i18n("Full tank:")
            }
            ColumnPicker {
                key: "stationColumn"
                Kirigami.FormData.label: i18n("Station:")
            }
            ColumnPicker {
                key: "notesColumn"
                Kirigami.FormData.label: i18n("Notes:")
            }

            Controls.TextField {
                visible: page.fileName !== ""
                Kirigami.FormData.label: i18n("Date format:")
                text: page.mapping.dateFormat ?? "yyyy-MM-dd"
                onEditingFinished: page.setMapping("dateFormat", text)
            }

            Controls.ComboBox {
                visible: page.fileName !== ""
                Kirigami.FormData.label: i18n("Decimal separator:")
                model: [i18n("Point (40.5)"), i18n("Comma (40,5)")]
                currentIndex: page.mapping.decimalSeparator === "," ? 1 : 0
                onActivated: index => page.setMapping("decimalSeparator", index === 1 ? "," : ".")
            }

            Controls.Label {
                Layout.maximumWidth: Kirigami.Units.gridUnit * 22
                text: i18n("Values must be in km, litres (or kWh) and your currency. Rows with errors are skipped.")
                wrapMode: Text.WordWrap
                font: Kirigami.Theme.smallFont
                color: Kirigami.Theme.disabledTextColor
            }
        }

        Kirigami.Heading {
            visible: page.fileName !== ""
            level: 3
            text: i18np("Preview: %1 valid row", "Preview: %1 valid rows", page.preview.count)
        }

        // Errors first: they decide whether the mapping is right.
        Repeater {
            model: page.preview.errors.slice(0, 10)
            delegate: Controls.Label {
                required property string modelData
                Layout.fillWidth: true
                text: modelData
                textFormat: Text.PlainText
                color: Kirigami.Theme.negativeTextColor
                wrapMode: Text.WordWrap
            }
        }
        Controls.Label {
            visible: page.preview.errors.length > 10
            text: i18n("… and %1 more errors", page.preview.errors.length - 10)
            color: Kirigami.Theme.negativeTextColor
        }

        Repeater {
            model: page.preview.entries.slice(0, 15)
            delegate: Controls.Label {
                required property var modelData
                Layout.fillWidth: true
                text: [i18n("Row %1", modelData.line), Format.date(modelData.date), Format.distance(modelData.odometerM),
                       Format.quantity(modelData.quantity, false), Format.money(modelData.totalCostC),
                       modelData.fullTank ? "" : i18n("partial"), modelData.station].filter(part => part !== "").join(" · ")
                elide: Text.ElideRight
            }
        }
    }
}
