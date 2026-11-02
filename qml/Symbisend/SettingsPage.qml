import QtQuick 1.1
import com.nokia.symbian 1.1

// Settings tab. Most settings are only stored for now (see docs/qml-api.md).
Page {
    id: page

    signal aboutRequested

    // Choices for the selection rows: what is stored, what is shown.
    property variant themeValues: ["dark", "light"]
    property variant themeNames: ["Dark", "Light"]
    property variant languageValues: ["system", "en"]
    property variant languageNames: ["System", "English"]

    function nameOf(values, names, value) {
        for (var i = 0; i < values.length; ++i) {
            if (values[i] == value)
                return names[i]
        }
        return value
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.height
        clip: true

        Column {
            id: column
            width: flick.width

            // ------------------------------------------------ General
            SettingsHeading { text: "General" }

            SettingsItem {
                title: "Theme"
                subTitle: page.nameOf(page.themeValues, page.themeNames, settings.theme)
                onClicked: themeDialog.open()
            }
            SettingsItem {
                title: "Language"
                subTitle: page.nameOf(page.languageValues, page.languageNames, settings.language)
                onClicked: languageDialog.open()
            }

            // ------------------------------------------------ Receive
            SettingsHeading { text: "Receive" }

            SettingsSwitch {
                title: "Quick save"
                checked: settings.quickSave
                onToggled: settings.quickSave = value
            }
            SettingsSwitch {
                title: "Require PIN"
                checked: settings.requirePin
                onToggled: settings.requirePin = value
            }
            SettingsItem {
                title: "Save to folder"
                subTitle: settings.downloadFolder
                onClicked: folderMenu.open()
            }
            SettingsSwitch {
                title: "Save media to gallery"
                checked: settings.saveMediaToGallery
                onToggled: settings.saveMediaToGallery = value
            }
            SettingsSwitch {
                title: "Auto finish"
                checked: settings.autoFinish
                onToggled: settings.autoFinish = value
            }
            SettingsSwitch {
                title: "Save to history"
                checked: settings.saveToHistory
                onToggled: settings.saveToHistory = value
            }

            // ------------------------------------------------ Network
            SettingsHeading { text: "Network" }

            SettingsItem {
                title: "Server"
                subTitle: discovery.serverRunning ? "Running" : "Stopped"

                Button {
                    text: discovery.serverRunning ? "Restart" : "Start"
                    onClicked: discovery.restartServer()
                }
                Button {
                    text: "Stop"
                    enabled: discovery.serverRunning
                    onClicked: discovery.stopServer()
                }
            }
            SettingsItem {
                title: "Device name"
                subTitle: myDevice.alias
                onClicked: nameDialog.openWith(myDevice.alias)

                Button {
                    text: "Random"
                    onClicked: myDevice.randomizeAlias()
                }
            }

            // ------------------------------------------------ Other
            SettingsHeading { text: "Other" }

            SettingsItem {
                title: "About Symbisend"
                onClicked: page.aboutRequested()

                Button {
                    text: "Open"
                    onClicked: page.aboutRequested()
                }
            }
        }
    }

    ScrollDecorator { flickableItem: flick }

    // ---------------------------------------------------- dialogs

    SelectionDialog {
        id: themeDialog
        titleText: "Theme"
        model: page.themeNames
        onAccepted: settings.theme = page.themeValues[selectedIndex]
    }

    SelectionDialog {
        id: languageDialog
        titleText: "Language"
        model: page.languageNames
        onAccepted: settings.language = page.languageValues[selectedIndex]
    }

    ContextMenu {
        id: folderMenu
        MenuLayout {
            MenuItem {
                text: "Choose folder..."
                onClicked: settings.pickDownloadFolder()
            }
            MenuItem {
                text: "Use default"
                enabled: !settings.downloadFolderIsDefault
                onClicked: settings.resetDownloadFolder()
            }
        }
    }

    CommonDialog {
        id: nameDialog
        titleText: "Device name"
        buttonTexts: ["OK", "Cancel"]

        function openWith(name) {
            nameField.text = name
            open()
        }

        content: Item {
            width: nameDialog.platformContentMaximumWidth
            height: nameField.height + 2 * platformStyle.paddingLarge

            TextField {
                id: nameField
                anchors {
                    left: parent.left; right: parent.right
                    verticalCenter: parent.verticalCenter
                    margins: platformStyle.paddingLarge
                }
                maximumLength: 64
                inputMethodHints: Qt.ImhNoPredictiveText
            }
        }

        onButtonClicked: {
            if (index == 0)
                myDevice.alias = nameField.text // ignored if empty
        }
    }
}
