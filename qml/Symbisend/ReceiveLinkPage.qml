// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 1.1
import com.nokia.symbian 1.1

// "Receive via link" (like the official app's WebSharePage in receive mode):
// while this page is open, browsers that open the link get LocalSend's upload
// page and can send files to us. Leaving the page turns the link off.
// Incoming requests still open the ReceiveDialog (main.qml).
Page {
    id: page

    // Light theme = the inverted Symbian style; every component follows it.
    property bool platformInverted: settings.theme == "light"

    Component.onCompleted: webShare.startReceive()
    Component.onDestruction: webShare.stop()

    tools: ToolBarLayout {
        ToolButton {
            platformInverted: page.platformInverted
            iconSource: "toolbar-back"
            onClicked: page.pageStack.pop()
        }
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

            SettingsHeading { text: i18n.t("webReceivePage.title") + i18n.rev }

            // The link, or why there is none.
            Item {
                width: column.width
                height: linkColumn.height + 2 * platformStyle.paddingLarge

                Column {
                    id: linkColumn
                    anchors {
                        left: parent.left; right: parent.right
                        verticalCenter: parent.verticalCenter
                        margins: platformStyle.paddingLarge
                    }
                    spacing: platformStyle.paddingMedium

                    Label {
                        platformInverted: page.platformInverted
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: i18n.rev + (webShare.url == "" ? i18n.t("symbisend.receiveLink.noNetwork")
                              : !discovery.serverRunning ? i18n.t("symbisend.receiveLink.serverStopped")
                              : i18n.plural("webSharePage.openLink", 1))
                    }
                    Label {
                        platformInverted: page.platformInverted
                        visible: webShare.url != "" && discovery.serverRunning
                        width: parent.width
                        wrapMode: Text.WrapAnywhere
                        font.pixelSize: platformStyle.fontSizeLarge
                        text: webShare.url
                    }
                    Row {
                        visible: webShare.url != "" && discovery.serverRunning
                        spacing: platformStyle.paddingMedium

                        ToolButton {
                            platformInverted: page.platformInverted
                            iconSource: page.platformInverted ? "images/content-copy-inverted.svg"
                                                              : "images/content-copy.svg"
                            onClicked: picker.copyText(webShare.url)
                        }
                        ToolButton {
                            platformInverted: page.platformInverted
                            iconSource: page.platformInverted ? "images/qr-code-inverted.svg"
                                                              : "images/qr-code.svg"
                            onClicked: qrDialog.open()
                        }
                    }
                    Button {
                        platformInverted: page.platformInverted
                        visible: webShare.url == "" || !discovery.serverRunning
                        text: webShare.url == "" ? i18n.t("symbisend.receiveLink.connect") + i18n.rev : i18n.t("symbisend.receiveLink.startServer") + i18n.rev
                        onClicked: {
                            if (webShare.url == "")
                                network.connectNetwork()
                            else
                                discovery.startServer()
                        }
                    }
                }
            }

            // Only for this link, off every time the page opens. Restarts
            // the server as https / http (the link changes with it).
            SettingsSwitch {
                title: i18n.t("webSharePage.encryption") + i18n.rev
                checked: webShare.encrypted
                onToggled: webShare.encrypted = value
            }
            Item {
                visible: webShare.encrypted
                width: column.width
                height: visible ? encryptionHint.height + 2 * platformStyle.paddingMedium : 0

                Label {
                    id: encryptionHint
                    platformInverted: page.platformInverted
                    anchors {
                        left: parent.left; right: parent.right
                        verticalCenter: parent.verticalCenter
                        leftMargin: platformStyle.paddingLarge; rightMargin: platformStyle.paddingLarge
                    }
                    wrapMode: Text.Wrap
                    font.pixelSize: platformStyle.fontSizeSmall
                    text: i18n.t("webSharePage.encryptionHint") + i18n.rev
                }
            }
            SettingsSwitch {
                title: i18n.t("webSharePage.autoAccept") + i18n.rev
                checked: settings.receiveViaLinkAutoAccept
                onToggled: settings.receiveViaLinkAutoAccept = value
            }
            // Only for this link (starts as the PIN from Settings). Turning it
            // on asks for the PIN; tapping the row while on changes it.
            SettingsSwitch {
                title: i18n.t("webSharePage.requirePin") + i18n.rev
                subTitle: webShare.pin != "" ? i18n.t("webSharePage.pinHint", { pin: webShare.pin }) + i18n.rev : ""
                checked: webShare.pin != ""
                rowToggles: false
                onClicked: pinDialog.openWith(webShare.pin)
                onToggled: {
                    if (value)
                        pinDialog.openWith("")
                    else
                        webShare.pin = ""
                }
            }
        }
    }

    ScrollDecorator { flickableItem: flick; platformInverted: page.platformInverted }

    // Like the official app, the code carries the PIN, so that a browser
    // opened from it doesn't ask; upload.html reads it from the query.
    QrCodeDialog {
        id: qrDialog
        address: webShare.url
        qrText: webShare.pin != "" ? webShare.url + "/?pin=" + encodeURIComponent(webShare.pin)
                                   : webShare.url
        pin: webShare.pin
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
            if (index == 0 && pinField.text != "")
                webShare.pin = pinField.text
        }
    }
}
