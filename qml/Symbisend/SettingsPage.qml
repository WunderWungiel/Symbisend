import QtQuick 1.1
import com.nokia.symbian 1.1

// Settings tab. Most settings are only stored for now (see docs/qml-api.md).
Page {
    id: page

    // Light theme = the inverted Symbian style; every component follows it.
    property bool platformInverted: settings.theme == "light"

    signal aboutRequested
    signal logRequested

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
                title: "Quick save for favourites"
                checked: settings.quickSaveFromFavorites
                onToggled: settings.quickSaveFromFavorites = value
            }
            // Turning it on asks for the PIN first (stays off on Cancel or
            // an empty PIN); tapping the row while on changes the PIN.
            SettingsSwitch {
                title: "Require PIN"
                subTitle: settings.requirePin ? "Tap to change the PIN" : ""
                checked: settings.requirePin
                rowToggles: false
                onClicked: pinDialog.openWith(settings.requirePin ? settings.pin : "")
                onToggled: {
                    if (value)
                        pinDialog.openWith("")
                    else
                        settings.requirePin = false // also clears the PIN
                }
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

                // Icon-only (flat) tool buttons: restart / start, and stop,
                // which is greyed out while stopped.
                ToolButton {
                    platformInverted: page.platformInverted
                    iconSource: discovery.serverRunning ? "toolbar-refresh"
                                                        : "toolbar-mediacontrol-play"
                    onClicked: discovery.restartServer()
                }
                ToolButton {
                    platformInverted: page.platformInverted
                    iconSource: "toolbar-mediacontrol-stop"
                    enabled: discovery.serverRunning
                    onClicked: discovery.stopServer()
                }
            }
            // Applied at once: a running server restarts as https / http.
            SettingsSwitch {
                title: "Encryption"
                checked: settings.encryption
                onToggled: {
                    settings.encryption = value
                    if (!value)
                        encryptionOffDialog.open()
                }
            }
            SettingsItem {
                title: "Device name"
                subTitle: myDevice.alias
                onClicked: nameDialog.openWith(myDevice.alias)

                Button {
                    platformInverted: page.platformInverted
                    text: "Random"
                    onClicked: myDevice.randomizeAlias()
                }
            }

            // ------------------------------------------------ Other
            SettingsHeading { text: "Other" }

            SettingsItem {
                title: "About Symbisend"
                onClicked: page.aboutRequested()
            }
            SettingsItem {
                title: "Log"
                onClicked: page.logRequested()
            }
        }
    }

    ScrollDecorator { flickableItem: flick; platformInverted: page.platformInverted }

    // ---------------------------------------------------- dialogs

    SelectionDialog {
        id: themeDialog
        platformInverted: page.platformInverted
        titleText: "Theme"
        model: page.themeNames
        onAccepted: settings.theme = page.themeValues[selectedIndex]
    }

    SelectionDialog {
        id: languageDialog
        platformInverted: page.platformInverted
        titleText: "Language"
        model: page.languageNames
        onAccepted: settings.language = page.languageValues[selectedIndex]
    }

    ContextMenu {
        id: folderMenu
        platformInverted: page.platformInverted
        MenuLayout {
            MenuItem {
                platformInverted: page.platformInverted
                text: "Choose folder..."
                onClicked: settings.pickDownloadFolder()
            }
            MenuItem {
                platformInverted: page.platformInverted
                text: "Use default"
                enabled: !settings.downloadFolderIsDefault
                onClicked: settings.resetDownloadFolder()
            }
        }
    }

    CommonDialog {
        id: pinDialog
        platformInverted: page.platformInverted
        titleText: "PIN"
        buttonTexts: ["OK", "Cancel"]

        function openWith(pin) {
            pinField.text = pin
            open()
        }

        content: Item {
            width: pinDialog.platformContentMaximumWidth
            height: pinField.height + 2 * platformStyle.paddingLarge

            // Any text, not only digits: the standard keyboard.
            TextField {
                id: pinField
                platformInverted: page.platformInverted
                anchors {
                    left: parent.left; right: parent.right
                    verticalCenter: parent.verticalCenter
                    margins: platformStyle.paddingLarge
                }
                maximumLength: 64
                placeholderText: "Enter PIN"
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
            }
        }

        onButtonClicked: {
            if (index != 0 || pinField.text == "")
                return // keeps the old state / PIN
            settings.pin = pinField.text
            settings.requirePin = true
        }
    }

    // The official app's notice after turning encryption off.
    QueryDialog {
        id: encryptionOffDialog
        platformInverted: page.platformInverted
        titleText: "Encryption disabled"
        message: "Communication now takes place via the unencrypted HTTP protocol. "
                 + "To use HTTPS protocol, enable encryption again."
        acceptButtonText: "OK"
    }

    CommonDialog {
        id: nameDialog
        platformInverted: page.platformInverted
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
                platformInverted: page.platformInverted
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
