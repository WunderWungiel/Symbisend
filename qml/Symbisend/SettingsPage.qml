// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

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
    property variant themeNames: [i18n.t("settingsTab.general.brightnessOptions.dark") + i18n.rev, i18n.t("settingsTab.general.brightnessOptions.light") + i18n.rev]

    property variant deviceTypeValues: ["mobile", "desktop", "web", "headless", "server"]
    property variant deviceTypeNames: [i18n.t("symbisend.deviceTypes.mobile") + i18n.rev,
        i18n.t("symbisend.deviceTypes.desktop") + i18n.rev,
        i18n.t("symbisend.deviceTypes.web") + i18n.rev,
        i18n.t("symbisend.deviceTypes.headless") + i18n.rev,
        i18n.t("symbisend.deviceTypes.server") + i18n.rev]

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
            SettingsHeading { text: i18n.t("settingsTab.general.title") + i18n.rev }

            SettingsItem {
                title: i18n.t("settingsTab.general.brightness") + i18n.rev
                subTitle: page.nameOf(page.themeValues, page.themeNames, settings.theme)
                onClicked: themeDialog.open()
            }
            SettingsItem {
                title: i18n.t("settingsTab.general.language") + i18n.rev
                subTitle: i18n.languageName(settings.language) + i18n.rev
                onClicked: languageDialog.open()
            }

            // ------------------------------------------------ Receive
            SettingsHeading { text: i18n.t("settingsTab.receive.title") + i18n.rev }

            SettingsSwitch {
                title: i18n.t("settingsTab.receive.quickSave") + i18n.rev
                checked: settings.quickSave
                onToggled: {
                    settings.quickSave = value
                    if (value)
                        quickSaveDialog.open()
                }
            }
            SettingsSwitch {
                title: i18n.t("settingsTab.receive.quickSaveFromFavorites") + i18n.rev
                checked: settings.quickSaveFromFavorites
                onToggled: {
                    settings.quickSaveFromFavorites = value
                    if (value)
                        quickSaveFavouritesDialog.open()
                }
            }
            // Turning it on asks for the PIN first (stays off on Cancel or
            // an empty PIN); tapping the row while on changes the PIN.
            SettingsSwitch {
                title: i18n.t("settingsTab.receive.requirePin") + i18n.rev
                subTitle: settings.requirePin ? i18n.t("symbisend.settings.changePin") + i18n.rev : ""
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
                title: i18n.t("settingsTab.receive.destination") + i18n.rev
                subTitle: settings.downloadFolder
                onClicked: folderMenu.open()
            }
            SettingsSwitch {
                title: i18n.t("settingsTab.receive.saveToGallery") + i18n.rev
                checked: settings.saveMediaToGallery
                onToggled: settings.saveMediaToGallery = value
            }
            SettingsSwitch {
                title: i18n.t("settingsTab.receive.autoFinish") + i18n.rev
                checked: settings.autoFinish
                onToggled: settings.autoFinish = value
            }
            SettingsSwitch {
                title: i18n.t("settingsTab.receive.saveToHistory") + i18n.rev
                checked: settings.saveToHistory
                onToggled: settings.saveToHistory = value
            }
            SettingsSwitch {
                visible: settings.advancedSettings
                title: i18n.t("settingsTab.receive.verifyChecksums") + i18n.rev
                checked: settings.verifyChecksums
                onToggled: settings.verifyChecksums = value
            }

            // ------------------------------------------------ Send
            SettingsHeading { text: i18n.t("settingsTab.send.title") + i18n.rev; visible: settings.advancedSettings }

            SettingsSwitch {
                visible: settings.advancedSettings
                title: i18n.t("settingsTab.send.createChecksums") + i18n.rev
                subTitle: i18n.t("symbisend.settings.createChecksumsHint") + i18n.rev
                checked: settings.createChecksums
                onToggled: settings.createChecksums = value
            }

            // ------------------------------------------------ Network
            SettingsHeading { text: i18n.t("settingsTab.network.title") + i18n.rev }

            SettingsNote {
                visible: discovery.needRestart
                warning: true
                text: i18n.t("settingsTab.network.needRestart") + i18n.rev
            }

            SettingsItem {
                title: i18n.t("settingsTab.network.server") + i18n.rev
                subTitle: discovery.serverRunning ? i18n.t("symbisend.settings.serverRunning") + i18n.rev : i18n.t("symbisend.settings.serverStopped") + i18n.rev

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
            SettingsItem {
                title: i18n.t("settingsTab.network.alias") + i18n.rev
                subTitle: myDevice.alias
                onClicked: nameDialog.openWith(myDevice.alias)

                // Dice: a new random alias, like the official app.
                ToolButton {
                    platformInverted: page.platformInverted
                    iconSource: page.platformInverted ? "images/casino-inverted.svg"
                                                      : "images/casino.svg"
                    onClicked: myDevice.randomizeAlias()
                }
            }
            SettingsItem {
                visible: settings.advancedSettings
                title: i18n.t("settingsTab.network.deviceType") + i18n.rev
                subTitle: page.nameOf(page.deviceTypeValues, page.deviceTypeNames, myDevice.deviceType)
                onClicked: deviceTypeDialog.open()
            }
            SettingsItem {
                visible: settings.advancedSettings
                title: i18n.t("settingsTab.network.deviceModel") + i18n.rev
                subTitle: myDevice.deviceModel
                onClicked: deviceModelDialog.openWith(myDevice.deviceModel)
            }
            SettingsItem {
                visible: settings.advancedSettings
                title: i18n.t("settingsTab.network.port") + i18n.rev
                subTitle: String(settings.port)
                onClicked: portDialog.openWith(settings.port)
            }
            SettingsItem {
                visible: settings.advancedSettings
                title: i18n.t("settingsTab.network.discoveryTimeout") + i18n.rev
                subTitle: settings.discoveryTimeout + " ms"
                onClicked: discoveryTimeoutDialog.openWith(settings.discoveryTimeout)
            }
            // Applied at once: a running server restarts as https / http.
            SettingsSwitch {
                visible: settings.advancedSettings
                title: i18n.t("settingsTab.network.encryption") + i18n.rev
                checked: settings.encryption
                onToggled: {
                    settings.encryption = value
                    if (!value)
                        encryptionOffDialog.open()
                }
            }
            SettingsItem {
                visible: settings.advancedSettings
                title: i18n.t("settingsTab.network.multicastGroup") + i18n.rev
                subTitle: settings.multicastGroup
                onClicked: multicastDialog.openWith(settings.multicastGroup)
            }
            SettingsNote {
                visible: settings.port != settings.defaultPort
                text: i18n.t("settingsTab.network.portWarning", { defaultPort: settings.defaultPort }) + i18n.rev
            }
            SettingsNote {
                visible: settings.multicastGroup != settings.defaultMulticastGroup
                text: i18n.t("settingsTab.network.multicastGroupWarning", { defaultMulticast: settings.defaultMulticastGroup }) + i18n.rev
            }

            // ------------------------------------------------ Other
            SettingsHeading { text: i18n.t("settingsTab.other.title") + i18n.rev }

            SettingsItem {
                title: i18n.t("symbisend.settings.about") + i18n.rev
                onClicked: page.aboutRequested()
            }
            SettingsItem {
                title: i18n.t("symbisend.settings.log") + i18n.rev
                onClicked: page.logRequested()
            }
            SettingsItem {
                title: i18n.t("symbisend.settings.restoreDefaults") + i18n.rev
                onClicked: restoreDialog.open()
            }
            SettingsSwitch {
                title: i18n.t("settingsTab.advancedSettings") + i18n.rev
                checked: settings.advancedSettings
                onToggled: settings.advancedSettings = value
            }
        }
    }

    ScrollDecorator { flickableItem: flick; platformInverted: page.platformInverted }

    // ---------------------------------------------------- dialogs

    SelectionDialog {
        id: themeDialog
        platformInverted: page.platformInverted
        titleText: i18n.t("settingsTab.general.brightness") + i18n.rev
        model: page.themeNames
        onAccepted: settings.theme = page.themeValues[selectedIndex]
    }

    SelectionDialog {
        id: languageDialog
        platformInverted: page.platformInverted
        titleText: i18n.t("settingsTab.general.language") + i18n.rev
        model: i18n.languageNames
        onAccepted: settings.language = i18n.languageCodes[selectedIndex]
    }

    SelectionDialog {
        id: deviceTypeDialog
        platformInverted: page.platformInverted
        titleText: i18n.t("settingsTab.network.deviceType") + i18n.rev
        model: page.deviceTypeNames
        onAccepted: myDevice.deviceType = page.deviceTypeValues[selectedIndex]
    }

    ValueDialog {
        id: deviceModelDialog
        titleText: i18n.t("settingsTab.network.deviceModel") + i18n.rev
        onEntered: myDevice.deviceModel = value
    }

    ValueDialog {
        id: portDialog
        titleText: i18n.t("settingsTab.network.port") + i18n.rev
        digitsOnly: true
        onEntered: settings.port = parseInt(value, 10) // ignored if invalid
    }

    ValueDialog {
        id: multicastDialog
        titleText: i18n.t("settingsTab.network.multicastGroup") + i18n.rev
        onEntered: settings.multicastGroup = value // ignored if invalid
    }

    ValueDialog {
        id: discoveryTimeoutDialog
        titleText: i18n.t("settingsTab.network.discoveryTimeout") + " (ms)" + i18n.rev
        digitsOnly: true
        onEntered: settings.discoveryTimeout = parseInt(value, 10) // ignored if invalid
    }

    ContextMenu {
        id: folderMenu
        platformInverted: page.platformInverted
        MenuLayout {
            MenuItem {
                platformInverted: page.platformInverted
                text: i18n.t("symbisend.settings.chooseFolder") + i18n.rev
                onClicked: settings.pickDownloadFolder()
            }
            MenuItem {
                platformInverted: page.platformInverted
                text: i18n.t("symbisend.settings.useDefaultFolder") + i18n.rev
                enabled: !settings.downloadFolderIsDefault
                onClicked: settings.resetDownloadFolder()
            }
        }
    }

    CommonDialog {
        id: pinDialog
        platformInverted: page.platformInverted
        titleText: i18n.t("dialogs.pin.title") + i18n.rev
        buttonTexts: [i18n.t("symbisend.general.ok") + i18n.rev, i18n.t("general.cancel") + i18n.rev]

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
                placeholderText: i18n.t("web.enterPin") + i18n.rev
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

    QueryDialog {
        id: restoreDialog
        platformInverted: page.platformInverted
        titleText: i18n.t("symbisend.settings.restoreDefaults") + i18n.rev
        message: i18n.t("symbisend.settings.restoreDefaultsContent") + i18n.rev
        acceptButtonText: i18n.t("symbisend.settings.restore") + i18n.rev
        rejectButtonText: i18n.t("general.cancel") + i18n.rev
        onAccepted: {
            settings.restoreDefaults()
            myDevice.restoreDefaults()
        }
    }

    // The official app's notices after turning quick save on.
    QueryDialog {
        id: quickSaveDialog
        platformInverted: page.platformInverted
        titleText: i18n.t("dialogs.quickSaveNotice.title") + i18n.rev
        message: i18n.t("dialogs.quickSaveNotice.content") + i18n.rev
        acceptButtonText: i18n.t("general.close") + i18n.rev
    }

    QueryDialog {
        id: quickSaveFavouritesDialog
        platformInverted: page.platformInverted
        titleText: i18n.t("dialogs.quickSaveFromFavoritesNotice.title") + i18n.rev
        message: i18n.t("dialogs.quickSaveFromFavoritesNotice.content") + i18n.rev
        acceptButtonText: i18n.t("general.close") + i18n.rev
    }

    // The official app's notice after turning encryption off.
    QueryDialog {
        id: encryptionOffDialog
        platformInverted: page.platformInverted
        titleText: i18n.t("dialogs.encryptionDisabledNotice.title") + i18n.rev
        message: i18n.t("dialogs.encryptionDisabledNotice.content") + i18n.rev
        acceptButtonText: i18n.t("symbisend.general.ok") + i18n.rev
    }

    CommonDialog {
        id: nameDialog
        platformInverted: page.platformInverted
        titleText: i18n.t("settingsTab.network.alias") + i18n.rev
        buttonTexts: [i18n.t("symbisend.general.ok") + i18n.rev, i18n.t("general.cancel") + i18n.rev]

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
