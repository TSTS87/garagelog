// Settings page (role: qml, pure UI).
// Units and currency of the open garage, and the garage file itself:
// show the path, open another garage, create a new one, save a copy
// (SPEC §6). Values are read and written through the Garage singleton.

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.tmx.garagelog

Kirigami.ScrollablePage {
    id: page

    // Main.qml checks this to keep the page open after a garage switch.
    objectName: "settingsPage"
    title: i18n("Settings")

    readonly property var distanceOptions: [
        { value: "km", text: i18n("Kilometres (km)") },
        { value: "mi", text: i18n("Miles (mi)") }
    ]
    readonly property var volumeOptions: [
        { value: "l", text: i18n("Litres (l)") },
        { value: "gal_us", text: i18n("US gallons (gal)") },
        { value: "gal_uk", text: i18n("Imperial gallons (gal)") }
    ]

    header: Kirigami.InlineMessage {
        position: Kirigami.InlineMessage.Position.Header
        type: Kirigami.MessageType.Error
        text: Garage.errorString
        visible: text !== ""
    }

    // One FileDialog per purpose: open needs an existing file, new and
    // save-as ask for a new name (SaveFile also asks before overwriting).
    FileDialog {
        id: openDialog
        title: i18n("Open Garage")
        fileMode: FileDialog.OpenFile
        currentFolder: Garage.folderUrl
        nameFilters: [i18n("GarageLog garages (*.sqlite)"), i18n("All files (*)")]
        onAccepted: if (Garage.openGarage(selectedFile)) applicationWindow().showPassiveNotification(i18n("Garage opened"))
    }

    FileDialog {
        id: newDialog
        title: i18n("New Garage")
        fileMode: FileDialog.SaveFile
        currentFolder: Garage.folderUrl
        defaultSuffix: "sqlite"
        nameFilters: [i18n("GarageLog garages (*.sqlite)")]
        onAccepted: if (Garage.newGarage(selectedFile)) applicationWindow().showPassiveNotification(i18n("New garage created"))
    }

    FileDialog {
        id: saveAsDialog
        title: i18n("Save Garage As")
        fileMode: FileDialog.SaveFile
        currentFolder: Garage.folderUrl
        defaultSuffix: "sqlite"
        nameFilters: [i18n("GarageLog garages (*.sqlite)")]
        onAccepted: if (Garage.saveGarageAs(selectedFile)) applicationWindow().showPassiveNotification(i18n("Garage saved; you are now working in the copy"))
    }

    Kirigami.FormLayout {
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Units")
        }

        // The ComboBox index is computed from the options array, not with
        // indexOfValue() in a binding (see STATUS.md); writing the setting
        // re-evaluates the binding through Garage.settingsChanged.
        Controls.ComboBox {
            Kirigami.FormData.label: i18n("Distance:")
            model: page.distanceOptions
            textRole: "text"
            currentIndex: page.distanceOptions.findIndex(o => o.value === Garage.distanceUnit)
            onActivated: index => Garage.distanceUnit = page.distanceOptions[index].value
        }

        Controls.ComboBox {
            Kirigami.FormData.label: i18n("Fuel volume:")
            model: page.volumeOptions
            textRole: "text"
            currentIndex: page.volumeOptions.findIndex(o => o.value === Garage.volumeUnit)
            onActivated: index => Garage.volumeUnit = page.volumeOptions[index].value
        }

        Controls.ComboBox {
            Kirigami.FormData.label: i18n("Currency:")
            model: Garage.currencies
            currentIndex: Garage.currencies.indexOf(Garage.currency)
            onActivated: index => Garage.currency = Garage.currencies[index]
        }

        Controls.Label {
            Layout.maximumWidth: Kirigami.Units.gridUnit * 22
            text: i18n("Stored values are not converted; only the display changes. Electric vehicles always use kWh. The currency only changes the symbol.")
            wrapMode: Text.WordWrap
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Garage File")
        }

        // SelectableLabel is a read-only text the user can select and copy,
        // handy for a long path.
        Kirigami.SelectableLabel {
            Kirigami.FormData.label: i18n("Open garage:")
            Layout.maximumWidth: Kirigami.Units.gridUnit * 22
            text: Garage.filePath
            wrapMode: Text.WrapAnywhere
        }

        RowLayout {
            Controls.Button {
                text: i18n("Open Garage…")
                icon.name: "document-open"
                onClicked: openDialog.open()
            }
            Controls.Button {
                text: i18n("New Garage…")
                icon.name: "document-new"
                onClicked: newDialog.open()
            }
            Controls.Button {
                text: i18n("Save Garage As…")
                icon.name: "document-save-as"
                onClicked: saveAsDialog.open()
            }
        }

        Controls.Label {
            Layout.maximumWidth: Kirigami.Units.gridUnit * 22
            text: i18n("A garage is one file plus its “attachments” folder next to it. “Save As” copies both and continues in the copy.")
            wrapMode: Text.WordWrap
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
        }
    }
}
