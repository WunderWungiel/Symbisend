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

            SettingsHeading { text: "Receive via link" }

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
                        text: webShare.url == "" ? "Not connected to a network."
                              : !discovery.serverRunning ? "The server is stopped."
                              : "Open this link in your browser:"
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
                        text: webShare.url == "" ? "Connect" : "Start server"
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
                title: "Encryption"
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
                    text: "LocalSend uses a self-signed certificate. You need to accept it in your browser."
                }
            }
            SettingsSwitch {
                title: "Automatically accept requests"
                checked: settings.receiveViaLinkAutoAccept
                onToggled: settings.receiveViaLinkAutoAccept = value
            }
            // Only for this link (starts as the PIN from Settings). Turning it
            // on asks for the PIN; tapping the row while on changes it.
            SettingsSwitch {
                title: "Require PIN"
                subTitle: webShare.pin != "" ? "The PIN is \"" + webShare.pin + "\"" : ""
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
        titleText: "PIN"
        buttonTexts: ["OK", "Cancel"]

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
                placeholderText: "Enter PIN"
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
            }
        }

        onButtonClicked: {
            if (index == 0 && pinField.text != "")
                webShare.pin = pinField.text
        }
    }
}
