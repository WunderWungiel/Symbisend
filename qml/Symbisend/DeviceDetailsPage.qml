import QtQuick 1.1
import com.nokia.symbian 1.1

// Details of one discovered device, opened with the Info button on the Send
// page. Modelled on LocalSend's DeviceDetailsPage: Favourite / Verify buttons,
// name / version / address, and the log of every time the device was
// confirmed. Has its own back button at the top, so main.qml hides the toolbar.
Page {
    id: page

    property string fingerprint
    // Snapshot from peerModel.peer(); refreshed whenever the peer is
    // re-confirmed. Kept as is if the peer disappears from the list.
    property variant device: ({})

    // Read by main.qml: this page has its own back button.
    property bool ownBackButton: true

    // Light theme = the inverted Symbian style; every component follows it.
    property bool platformInverted: settings.theme == "light"

    property color mutedColor: platformInverted ? platformStyle.colorNormalMidInverted
                                                : platformStyle.colorNormalMid

    // favorites.contains() is not a notifying property: refreshed below.
    property bool isFavourite: false

    function reload() {
        var d = peerModel.peer(fingerprint)
        if (d.fingerprint !== undefined)
            device = d
        isFavourite = favorites.contains(fingerprint)
    }

    onFingerprintChanged: reload()

    Connections {
        target: favorites
        onFavoritesChanged: {
            if (fingerprint == page.fingerprint)
                page.isFavourite = favorites.contains(page.fingerprint)
        }
    }

    Connections {
        target: peerModel
        onPeerChanged: {
            if (fingerprint == page.fingerprint)
                page.reload()
        }
    }

    // Everything scrolls together, header included.
    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: content.height + platformStyle.paddingLarge
        flickableDirection: Flickable.VerticalFlick
        clip: true

        Column {
            id: content
            width: flick.width
            spacing: platformStyle.paddingLarge

            // ------------------------------------------------ Header
            Item {
                x: 8
                width: parent.width - 16
                height: backButton.height + 8

                ToolButton {
                    id: backButton
                    platformInverted: page.platformInverted
                    anchors { left: parent.left; bottom: parent.bottom }
                    iconSource: "toolbar-back"
                    onClicked: page.pageStack.pop()
                }
                Label {
                    platformInverted: page.platformInverted
                    anchors {
                        left: backButton.right; leftMargin: platformStyle.paddingMedium
                        right: parent.right; verticalCenter: backButton.verticalCenter
                    }
                    elide: Text.ElideRight
                    text: "Device details"
                    font.pixelSize: 26
                }
            }

            // ------------------------------------------------ Actions
            // ToolButtons: icon left of the text at tool-bar height. A Button
            // stacks the icon above the text, which makes it very tall.
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: platformStyle.paddingLarge

                ToolButton {
                    platformInverted: page.platformInverted
                    iconSource: "images/" + (page.isFavourite ? "favorite" : "favorite-border")
                                + (page.platformInverted ? "-inverted.svg" : ".svg")
                    text: "Favourite"
                    onClicked: {
                        if (page.isFavourite)
                            removeFavouriteDialog.open()
                        else if (page.device.fingerprint !== undefined)
                            favouriteDialog.openFor(page.device)
                    }
                }
                ToolButton {
                    platformInverted: page.platformInverted
                    iconSource: page.platformInverted ? "images/verified-user-inverted.svg"
                                                      : "images/verified-user.svg"
                    text: "Verify"
                    onClicked: verifyDialog.openFor(page.fingerprint)
                }
            }

            // ------------------------------------------------ Info
            Column {
                id: info
                x: platformStyle.paddingLarge
                width: parent.width - 2 * platformStyle.paddingLarge
                spacing: platformStyle.paddingSmall

                // Key column as wide as the widest key, like LocalSend's table.
                property real keyWidth: Math.max(nameKey.paintedWidth, versionKey.paintedWidth,
                                                 addressKey.paintedWidth)
                                        + platformStyle.paddingLarge
                property real valueWidth: width - keyWidth

                Row {
                    Label {
                        id: nameKey
                        platformInverted: page.platformInverted
                        width: info.keyWidth
                        color: page.mutedColor
                        text: "Name"
                    }
                    Label {
                        platformInverted: page.platformInverted
                        width: info.valueWidth
                        wrapMode: Text.Wrap
                        text: page.device.alias || ""
                    }
                }
                Row {
                    Label {
                        id: versionKey
                        platformInverted: page.platformInverted
                        width: info.keyWidth
                        color: page.mutedColor
                        text: "Version"
                    }
                    Label {
                        platformInverted: page.platformInverted
                        width: info.valueWidth
                        wrapMode: Text.Wrap
                        text: page.device.version ? "Protocol v" + page.device.version : ""
                    }
                }
                Row {
                    Label {
                        id: addressKey
                        platformInverted: page.platformInverted
                        width: info.keyWidth
                        color: page.mutedColor
                        text: "Address"
                    }
                    Label {
                        platformInverted: page.platformInverted
                        width: info.valueWidth
                        wrapMode: Text.WrapAnywhere
                        text: page.device.ip ? page.device.ip + ":" + page.device.port : ""
                    }
                }
            }

            // ------------------------------------------------ Logs
            // Same section heading as on the Settings page.
            SettingsHeading {
                text: "Logs"
                platformInverted: page.platformInverted
            }

            // The log lines sit in a shaded box, like an internal log.
            Rectangle {
                x: platformStyle.paddingLarge
                width: parent.width - 2 * platformStyle.paddingLarge
                height: logColumn.height + 2 * platformStyle.paddingMedium
                radius: 3
                color: page.platformInverted ? "#14000000" : "#1affffff"

                Column {
                    id: logColumn
                    x: platformStyle.paddingMedium
                    y: platformStyle.paddingMedium
                    width: parent.width - 2 * platformStyle.paddingMedium
                    spacing: platformStyle.paddingSmall

                    Text {
                        visible: logRepeater.count === 0
                        font { family: platformStyle.fontFamilyRegular; pixelSize: platformStyle.fontSizeSmall }
                        color: page.mutedColor
                        text: "No logs available."
                    }

                    Repeater {
                        id: logRepeater
                        model: page.device.logs || []

                        Row {
                            width: logColumn.width
                            spacing: platformStyle.paddingSmall

                            Text {
                                id: logTime
                                font { family: platformStyle.fontFamilyRegular; pixelSize: platformStyle.fontSizeSmall }
                                color: page.mutedColor
                                text: "[" + modelData.time + "]"
                            }
                            Text {
                                width: parent.width - logTime.width - parent.spacing
                                wrapMode: Text.Wrap
                                font { family: platformStyle.fontFamilyRegular; pixelSize: platformStyle.fontSizeSmall }
                                color: page.platformInverted ? platformStyle.colorNormalLightInverted
                                                             : platformStyle.colorNormalLight
                                text: modelData.text
                            }
                        }
                    }
                }
            }
        }
    }

    ScrollDecorator {
        flickableItem: flick
        platformInverted: page.platformInverted
    }

    VerifyDialog { id: verifyDialog }

    FavoriteDialog { id: favouriteDialog }

    QueryDialog {
        id: removeFavouriteDialog
        platformInverted: page.platformInverted
        titleText: "Remove from favourites"
        message: "Do you really want to remove " + (page.device.alias || "this device")
                 + " from favourites?"
        acceptButtonText: "Delete"
        rejectButtonText: "Cancel"
        onAccepted: favorites.remove(page.fingerprint)
    }
}
