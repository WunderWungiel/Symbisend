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

    function reload() {
        var d = peerModel.peer(fingerprint)
        if (d.fingerprint !== undefined)
            device = d
    }

    onFingerprintChanged: reload()

    Connections {
        target: peerModel
        onPeerChanged: {
            if (fingerprint == page.fingerprint)
                page.reload()
        }
    }

    // ------------------------------------------------ Header
    Item {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 8 }
        height: backButton.height

        ToolButton {
            id: backButton
            platformInverted: page.platformInverted
            anchors.left: parent.left
            iconSource: "toolbar-back"
            onClicked: page.pageStack.pop()
        }
        Label {
            platformInverted: page.platformInverted
            anchors {
                left: backButton.right; leftMargin: platformStyle.paddingMedium
                right: parent.right; verticalCenter: parent.verticalCenter
            }
            elide: Text.ElideRight
            text: "Device details"
            font.pixelSize: 26
        }
    }

    Flickable {
        id: flick
        anchors {
            left: parent.left; right: parent.right
            top: header.bottom; bottom: parent.bottom
            topMargin: platformStyle.paddingLarge
        }
        contentWidth: width
        contentHeight: content.height + 2 * platformStyle.paddingLarge
        clip: true

        Column {
            id: content
            x: platformStyle.paddingLarge
            width: flick.width - 2 * platformStyle.paddingLarge
            spacing: platformStyle.paddingLarge

            // ------------------------------------------------ Actions
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: platformStyle.paddingLarge

                Button {
                    platformInverted: page.platformInverted
                    width: Math.min(180, (content.width - platformStyle.paddingLarge) / 2)
                    iconSource: page.platformInverted ? "images/favorite-border-inverted.svg"
                                                      : "images/favorite-border.svg"
                    text: "Favourite"
                    onClicked: console.log("DeviceDetailsPage: favourite clicked (not implemented)")
                }
                Button {
                    platformInverted: page.platformInverted
                    width: Math.min(180, (content.width - platformStyle.paddingLarge) / 2)
                    iconSource: page.platformInverted ? "images/verified-user-inverted.svg"
                                                      : "images/verified-user.svg"
                    text: "Verify"
                    onClicked: console.log("DeviceDetailsPage: verify clicked (not implemented)")
                }
            }

            // ------------------------------------------------ Info
            Column {
                id: info
                width: parent.width
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
            Column {
                width: parent.width
                spacing: platformStyle.paddingSmall

                Label {
                    platformInverted: page.platformInverted
                    font.pixelSize: platformStyle.fontSizeLarge
                    text: "Logs"
                }

                Label {
                    platformInverted: page.platformInverted
                    visible: logRepeater.count === 0
                    color: page.mutedColor
                    text: "No logs available."
                }

                Repeater {
                    id: logRepeater
                    model: page.device.logs || []

                    Row {
                        width: parent.width
                        spacing: platformStyle.paddingSmall

                        Label {
                            id: logTime
                            platformInverted: page.platformInverted
                            text: "[" + modelData.time + "]"
                        }
                        Label {
                            platformInverted: page.platformInverted
                            width: parent.width - logTime.width - parent.spacing
                            wrapMode: Text.Wrap
                            text: modelData.text
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
}
