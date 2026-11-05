import QtQuick 1.1
import com.nokia.symbian 1.1

// An incoming transfer (receiver): the offer with Decline / Accept, then the
// progress and the result. main.qml opens it when an offer arrives (receiver.state
// "request"), or at "receiving" / "failed" when an offer was accepted without
// asking (quick save, link auto accept). Options leaves for ReceiveOptionsPage and
// comes back. Bare functional layout, meant to be polished.
Dialog {
    id: root

    signal optionsRequested()

    platformInverted: settings.theme == "light"

    property string recvState: receiver.state
    property color mutedColor: platformInverted ? platformStyle.colorNormalMidInverted
                                                : platformStyle.colorNormalMid
    property color textColor: platformInverted ? platformStyle.colorNormalLightInverted
                                               : platformStyle.colorNormalLight
    // Two buttons side by side, or one of the same width centered (like CommonDialog).
    property real buttonWidth: Math.round((platformContentMaximumWidth - 3 * platformStyle.paddingMedium) / 2)
    // A lone text message. Like the official app it stays on screen when the
    // sender withdraws it: it has arrived already.
    property bool showsMessage: receiver.isMessage
                                && (recvState == "request" || recvState == "finished"
                                    || recvState == "cancelled")
    property bool showsOffer: recvState == "request" || showsMessage
    // Verify shares a line with Options, or with Copy and maybe Open. A
    // ToolButton's own width is too wide for that on a 360 px screen.
    property int offerActions: showsMessage && receiver.messageIsLink ? 3 : 2
    property real offerActionWidth: Math.floor((platformContentMaximumWidth - 2 * platformStyle.paddingLarge
                                                - (offerActions - 1) * platformStyle.paddingSmall) / offerActions)

    // 75 -> "1:15"
    function formatTime(s) {
        var m = Math.floor(s / 60)
        var r = s % 60
        return m + ":" + (r < 10 ? "0" : "") + r
    }

    function offerText() {
        if (receiver.isMessage)
            return receiver.messageIsLink ? "sent you a link:" : "sent you a message:"
        var n = receiver.fileCount
        return "wants to send you " + (n == 1 ? "a file" : n + " files")
                + " (" + selection.formatSize(receiver.totalSize) + ")"
    }

    function savedText() {
        return "Saved " + (receiver.filesDone == 1 ? "1 file" : receiver.filesDone + " files")
                + " from " + receiver.peerAlias
                // Own line for the folder: Qt allows a line break after
                // "\", so "F:\Symbisend" would otherwise split there.
                + "\nto " + receiver.folders.join("\n")
    }

    // A waiting message is answered as the dialog closes. Accepting does
    // nothing once the sender has withdrawn it.
    function dismiss() {
        if (recvState == "request")
            receiver.accept()
        close()
    }

    // settings.autoFinish, like the official app: once every file is saved the
    // Close button counts down "Close (3)", "Close (2)", ... and the dialog
    // closes by itself. Not for messages (they are there to be read) nor for
    // failed or cancelled transfers.
    CountdownTimer {
        id: finishTimer
        onElapsed: {
            console.log("ReceiveDialog: auto finish - closing")
            root.close()
        }
    }

    onRecvStateChanged: {
        finishTimer.stop()
        // Declined (idle): nothing more to show. A finished quick save closes
        // at once, main.qml then offers to open the file.
        if (recvState == "idle" || receiver.quickSaved) {
            close()
        } else if (recvState == "finished" && !receiver.isMessage && settings.autoFinish
                   && (status == DialogStatus.Open || status == DialogStatus.Opening)) {
            finishTimer.begin(3)
        }
    }
    // Closed by hand during the countdown.
    onStatusChanged: if (status == DialogStatus.Closing || status == DialogStatus.Closed) finishTimer.stop()

    // Opens on top of this dialog.
    VerifyDialog { id: verifyDialog }

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
                if (root.showsMessage) return "Message"
                if (root.recvState == "request") return "Receive"
                if (root.recvState == "receiving")
                    return receiver.remainingSeconds < 0 ? "Receiving"
                                                         : "Receiving (" + root.formatTime(receiver.remainingSeconds) + ")"
                if (root.recvState == "finished") return "Finished"
                if (root.recvState == "finishedWithErrors") return "Finished with error"
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
            visible: root.showsOffer
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            font.pixelSize: Math.round(platformStyle.fontSizeLarge * 1.3)
            text: receiver.peerAlias
        }

        // System name (LocalSend's deviceModel: "Windows", "Samsung", ...).
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.showsOffer && receiver.peerDeviceModel != ""
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
            visible: root.showsOffer
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: root.offerText()
        }

        // The message itself, scrollable if long.
        Rectangle {
            anchors { left: parent.left; right: parent.right; margins: platformStyle.paddingLarge }
            visible: root.showsMessage
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

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.showsOffer
            spacing: platformStyle.paddingSmall

            // Like the official app: only for senders on https, whose
            // fingerprint is their certificate's hash.
            ToolButton {
                width: root.offerActionWidth
                platformInverted: root.platformInverted
                iconSource: root.showsMessage ? ""
                            : root.platformInverted ? "images/verified-user-inverted.svg"
                                                    : "images/verified-user.svg"
                text: "Verify"
                enabled: receiver.peerHttps
                onClicked: verifyDialog.openFor(receiver.peerFingerprint)
            }
            ToolButton {
                visible: !root.showsMessage
                width: root.offerActionWidth
                platformInverted: root.platformInverted
                iconSource: "toolbar-settings"
                text: "Options"
                onClicked: {
                    root.close()
                    root.optionsRequested()
                }
            }
            ToolButton {
                visible: root.showsMessage
                width: root.offerActionWidth
                platformInverted: root.platformInverted
                text: "Copy"
                onClicked: {
                    picker.copyText(receiver.message)
                    root.dismiss()
                }
            }
            ToolButton {
                visible: root.showsMessage && receiver.messageIsLink
                width: root.offerActionWidth
                platformInverted: root.platformInverted
                text: "Open"
                onClicked: {
                    Qt.openUrlExternally(receiver.message.replace(/^\s+|\s+$/g, ""))
                    root.dismiss()
                }
            }
        }

        // ------------------------------------------------ Progress
        Label {
            platformInverted: root.platformInverted
            anchors { left: parent.left; right: parent.right; margins: platformStyle.paddingLarge }
            visible: root.recvState == "receiving"
            wrapMode: Text.WrapAnywhere
            text: (receiver.fileName != "" ? receiver.fileName : "Waiting for " + receiver.peerAlias + "...")
                  + (receiver.fileCount > 1
                     ? " (" + Math.min(receiver.filesDone + receiver.filesFailed + 1, receiver.fileCount)
                       + " of " + receiver.fileCount + ")"
                     : "")
        }

        ProgressBar {
            platformInverted: root.platformInverted
            anchors { left: parent.left; right: parent.right; margins: platformStyle.paddingLarge }
            visible: root.recvState == "receiving" || root.recvState == "finishedWithErrors"
                     || (root.recvState == "finished" && !receiver.isMessage)
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
                     || root.recvState == "failed"
                     || (root.recvState == "cancelled" && !receiver.isMessage)
                     || root.recvState == "finishedWithErrors"
            wrapMode: Text.Wrap
            text: {
                if (root.recvState == "finished")
                    return root.savedText()
                if (root.recvState == "finishedWithErrors" && receiver.filesDone > 0)
                    return root.savedText() + "\n" + receiver.errorText
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
                visible: root.recvState == "request" && !receiver.isMessage
                width: root.buttonWidth
                height: privateStyle.toolBarHeightLandscape
                platformInverted: root.platformInverted
                iconSource: root.platformInverted ? "images/close-inverted.svg" : "images/close.svg"
                text: "Decline"
                onClicked: receiver.decline() // closes via state "idle"
            }
            ToolButton {
                visible: root.recvState == "request" && !receiver.isMessage
                width: root.buttonWidth
                height: privateStyle.toolBarHeightLandscape
                platformInverted: root.platformInverted
                iconSource: root.platformInverted ? "images/check-inverted.svg" : "images/check.svg"
                text: "Accept"
                enabled: receiveFiles.selectedCount > 0
                onClicked: receiver.accept()
            }
            ToolButton {
                visible: root.recvState != "request" || receiver.isMessage
                width: root.buttonWidth
                height: privateStyle.toolBarHeightLandscape
                platformInverted: root.platformInverted
                text: root.recvState == "receiving" ? "Cancel"
                      : finishTimer.running ? "Close (" + finishTimer.counter + ")" : "Close"
                onClicked: {
                    if (root.recvState == "receiving")
                        receiver.cancel()
                    else
                        root.dismiss()
                }
            }
        }
    }
}
