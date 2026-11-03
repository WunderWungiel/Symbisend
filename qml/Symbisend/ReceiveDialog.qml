import QtQuick 1.1
import com.nokia.symbian 1.1

// An incoming transfer (receiver): the offer with Decline / Accept, then the
// progress and the result. main.qml opens it when an offer arrives
// (receiver.state "request"). Bare functional layout, meant to be polished.
Dialog {
    id: root

    platformInverted: settings.theme == "light"

    property string recvState: receiver.state
    property color mutedColor: platformInverted ? platformStyle.colorNormalMidInverted
                                                : platformStyle.colorNormalMid
    property color textColor: platformInverted ? platformStyle.colorNormalLightInverted
                                               : platformStyle.colorNormalLight
    // Two buttons side by side, or one of the same width centered (like CommonDialog).
    property real buttonWidth: Math.round((platformContentMaximumWidth - 3 * platformStyle.paddingMedium) / 2)

    // 75 -> "1:15"
    function formatTime(s) {
        var m = Math.floor(s / 60)
        var r = s % 60
        return m + ":" + (r < 10 ? "0" : "") + r
    }

    function offerText() {
        if (receiver.isMessage)
            return "wants to send you a message"
        var n = receiver.fileCount
        return "wants to send you " + (n == 1 ? "a file" : n + " files")
                + " (" + selection.formatSize(receiver.totalSize) + ")"
    }

    // settings.autoFinish, like the official app: once every file is saved the
    // Close button counts down "Close (3)", "Close (2)", ... and the dialog
    // closes by itself. Not for messages (they are there to be read) nor for
    // failed or cancelled transfers.
    property int finishCounter: 3

    Timer {
        id: finishTimer
        interval: 1000
        repeat: true
        onTriggered: {
            if (root.finishCounter <= 1) {
                stop()
                console.log("ReceiveDialog: auto finish - closing")
                root.close()
            } else {
                root.finishCounter--
            }
        }
    }

    onRecvStateChanged: {
        finishTimer.stop()
        // Declined (idle): nothing more to show.
        if (recvState == "idle") {
            close()
        } else if (recvState == "finished" && !receiver.isMessage && settings.autoFinish
                   && (status == DialogStatus.Open || status == DialogStatus.Opening)) {
            finishCounter = 3
            finishTimer.start()
        }
    }
    // Closed by hand during the countdown.
    onStatusChanged: if (status == DialogStatus.Closing || status == DialogStatus.Closed) finishTimer.stop()

    // Sized from the column explicitly, like SendDialog.
    height: privateTitleHeight + contentColumn.height + privateButtonsHeight

    title: Item {
        anchors { left: parent.left; right: parent.right }
        height: platformStyle.graphicSizeSmall + 2 * platformStyle.paddingLarge

        Text {
            anchors { fill: parent; margins: platformStyle.paddingLarge }
            font { family: platformStyle.fontFamilyRegular; pixelSize: platformStyle.fontSizeLarge }
            color: root.platformInverted ? platformStyle.colorNormalLinkInverted
                                         : platformStyle.colorNormalLink
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            text: {
                if (root.recvState == "request") return receiver.isMessage ? "Message" : "Receive"
                if (root.recvState == "receiving")
                    return receiver.remainingSeconds < 0 ? "Receiving"
                                                         : "Receiving (" + root.formatTime(receiver.remainingSeconds) + ")"
                if (root.recvState == "finished") return receiver.isMessage ? "Message" : "Finished"
                if (root.recvState == "cancelled") return "Cancelled"
                return "Error"
            }
        }
    }

    content: Column {
        id: contentColumn
        width: root.platformContentMaximumWidth
        spacing: platformStyle.paddingMedium

        Item { width: 1; height: platformStyle.paddingSmall }

        // ------------------------------------------------ Offer
        Label {
            platformInverted: root.platformInverted
            anchors { left: parent.left; right: parent.right; margins: platformStyle.paddingLarge }
            visible: root.recvState == "request"
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            font.pixelSize: Math.round(platformStyle.fontSizeLarge * 1.3)
            text: receiver.peerAlias
        }

        // System name (LocalSend's deviceModel: "Windows", "Samsung", ...).
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.recvState == "request" && receiver.peerDeviceModel != ""
            width: systemText.paintedWidth + 2 * platformStyle.paddingMedium
            height: systemText.paintedHeight + platformStyle.paddingSmall
            radius: 3
            color: root.platformInverted ? "#1f000000" : "#33ffffff"

            Text {
                id: systemText
                anchors.centerIn: parent
                font { family: platformStyle.fontFamilyRegular; pixelSize: platformStyle.fontSizeSmall }
                color: root.mutedColor
                text: receiver.peerDeviceModel
            }
        }

        Label {
            platformInverted: root.platformInverted
            anchors { left: parent.left; right: parent.right; margins: platformStyle.paddingLarge }
            visible: root.recvState == "request"
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: root.offerText()
        }

        // The message itself (before and after accepting), scrollable if long.
        Rectangle {
            anchors { left: parent.left; right: parent.right; margins: platformStyle.paddingLarge }
            visible: receiver.isMessage && (root.recvState == "request" || root.recvState == "finished")
            height: Math.min(messageText.paintedHeight, 160) + 2 * platformStyle.paddingMedium
            radius: 3
            color: root.platformInverted ? "#14000000" : "#1affffff"

            Flickable {
                id: messageFlick
                anchors { fill: parent; margins: platformStyle.paddingMedium }
                contentWidth: width
                contentHeight: messageText.paintedHeight
                flickableDirection: Flickable.VerticalFlick
                clip: true

                Text {
                    id: messageText
                    width: messageFlick.width
                    wrapMode: Text.Wrap
                    font { family: platformStyle.fontFamilyRegular; pixelSize: platformStyle.fontSizeMedium }
                    color: root.textColor
                    text: receiver.message
                }
            }
        }

        // Not implemented yet, like on the device details page.
        ToolButton {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.recvState == "request"
            platformInverted: root.platformInverted
            iconSource: root.platformInverted ? "images/verified-user-inverted.svg"
                                              : "images/verified-user.svg"
            text: "Verify"
            onClicked: console.log("ReceiveDialog: verify clicked (not implemented)")
        }

        // ------------------------------------------------ Progress
        Label {
            platformInverted: root.platformInverted
            anchors { left: parent.left; right: parent.right; margins: platformStyle.paddingLarge }
            visible: root.recvState == "receiving"
            wrapMode: Text.WrapAnywhere
            text: (receiver.fileName != "" ? receiver.fileName : "Waiting for " + receiver.peerAlias + "...")
                  + (receiver.fileCount > 1
                     ? " (" + Math.min(receiver.filesDone + 1, receiver.fileCount) + " of " + receiver.fileCount + ")"
                     : "")
        }

        ProgressBar {
            platformInverted: root.platformInverted
            anchors { left: parent.left; right: parent.right; margins: platformStyle.paddingLarge }
            visible: root.recvState == "receiving" || (root.recvState == "finished" && !receiver.isMessage)
            minimumValue: 0
            maximumValue: 1
            value: receiver.progress
        }

        Label {
            platformInverted: root.platformInverted
            anchors { left: parent.left; right: parent.right; margins: platformStyle.paddingLarge }
            visible: root.recvState == "receiving"
            font.pixelSize: platformStyle.fontSizeSmall
            text: selection.formatSize(receiver.bytesReceived) + " of " + selection.formatSize(receiver.bytesTotal)
        }

        // ------------------------------------------------ Result
        Label {
            platformInverted: root.platformInverted
            anchors { left: parent.left; right: parent.right; margins: platformStyle.paddingLarge }
            visible: (root.recvState == "finished" && !receiver.isMessage)
                     || root.recvState == "failed" || root.recvState == "cancelled"
            wrapMode: Text.Wrap
            text: {
                if (root.recvState == "finished")
                    return "Saved " + (receiver.filesDone == 1 ? "1 file" : receiver.filesDone + " files")
                            + " from " + receiver.peerAlias
                            // Own line for the folder: Qt allows a line break after
                            // "\", so "F:\Symbisend" would otherwise split there.
                            + "\nto " + receiver.folder
                return receiver.errorText
            }
        }

        Item { width: 1; height: platformStyle.paddingSmall }
    }

    buttons: Item {
        width: parent.width
        height: privateStyle.toolBarHeightLandscape + 2 * platformStyle.paddingSmall

        Row {
            anchors.centerIn: parent
            spacing: platformStyle.paddingMedium

            ToolButton {
                visible: root.recvState == "request"
                width: root.buttonWidth
                height: privateStyle.toolBarHeightLandscape
                platformInverted: root.platformInverted
                iconSource: root.platformInverted ? "images/close-inverted.svg" : "images/close.svg"
                text: "Decline"
                onClicked: receiver.decline() // closes via state "idle"
            }
            ToolButton {
                visible: root.recvState == "request"
                width: root.buttonWidth
                height: privateStyle.toolBarHeightLandscape
                platformInverted: root.platformInverted
                iconSource: root.platformInverted ? "images/check-inverted.svg" : "images/check.svg"
                text: "Accept"
                onClicked: receiver.accept()
            }
            ToolButton {
                visible: root.recvState != "request"
                width: root.buttonWidth
                height: privateStyle.toolBarHeightLandscape
                platformInverted: root.platformInverted
                text: root.recvState == "receiving" ? "Cancel"
                      : finishTimer.running ? "Close (" + root.finishCounter + ")" : "Close"
                onClicked: {
                    if (root.recvState == "receiving")
                        receiver.cancel()
                    else
                        root.close()
                }
            }
        }
    }
}
