// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 1.1
import com.nokia.symbian 1.1

// Progress of a transfer started with sender.send() or sendToAddress():
// connecting, waiting for the receiver, PIN entry, total progress with the time left in the title, and
// the result. Cancel stops the transfer; Close just hides the finished one.
// With settings.autoFinish a fully sent transfer closes by itself, like
// ReceiveDialog.
CommonDialog {
    id: root

    platformInverted: settings.theme == "light"

    property bool busy: sender.busy
    property string sendState: sender.state

    // 75 -> "1:15"
    function formatTime(s) {
        var m = Math.floor(s / 60)
        var r = s % 60
        return m + ":" + (r < 10 ? "0" : "") + r
    }

    titleText: {
        var rev = i18n.rev
        if (sendState == "connecting") return i18n.t("symbisend.send.connecting")
        if (sendState == "hashing") return i18n.t("symbisend.send.preparing")
        if (sendState == "waiting") return i18n.t("sendPage.waiting")
        if (sendState == "pin") return i18n.t("symbisend.send.pinRequired")
        if (sendState == "sending")
            return sender.remainingSeconds < 0 ? i18n.t("symbisend.send.totalProgress")
                                               : i18n.t("progressPage.total.title.sending",
                                                        { time: formatTime(sender.remainingSeconds) })
        if (sendState == "finished") return i18n.t("general.finished")
        if (sendState == "finishedWithErrors") return i18n.t("progressPage.total.title.finishedError")
        if (sendState == "declined") return i18n.t("web.rejected")
        if (sendState == "cancelled") return i18n.t("symbisend.general.cancelled")
        return i18n.t("general.error")
    }
    buttonTexts: [i18n.rev + (busy ? i18n.t("general.cancel")
                               : finishTimer.running ? i18n.t("general.close") + " (" + finishTimer.counter + ")"
                               : i18n.t("general.close"))]

    // Size the dialog from the column explicitly: the default sizing reads
    // childrenRect, which lags behind when rows appear (state changes,
    // file name wrapping) and the last row ended up under the button.
    height: privateTitleHeight + contentColumn.height + privateButtonsHeight

    onButtonClicked: {
        if (busy)
            sender.cancel()
    }

    CountdownTimer {
        id: finishTimer
        onElapsed: {
            console.log("SendDialog: auto finish - closing")
            root.close()
        }
    }

    onSendStateChanged: {
        finishTimer.stop()
        // Opened while the PIN field is shown: start with an empty one.
        if (sendState == "pin")
            pinField.text = ""
        else if (sendState == "finished" && settings.autoFinish
                 && (status == DialogStatus.Open || status == DialogStatus.Opening))
            finishTimer.begin(3)
    }
    // Closed by hand during the countdown.
    onStatusChanged: if (status == DialogStatus.Closing || status == DialogStatus.Closed) finishTimer.stop()

    content: Column {
        id: contentColumn
        width: root.platformContentMaximumWidth
        spacing: platformStyle.paddingMedium

        Item { width: 1; height: platformStyle.paddingSmall }

        Label {
            platformInverted: root.platformInverted
            anchors { left: parent.left; right: parent.right; margins: platformStyle.paddingLarge }
            wrapMode: Text.Wrap
            text: {
                var rev = i18n.rev
                var peer = { name: sender.peerAlias }
                if (root.sendState == "connecting") return i18n.t("symbisend.send.connectingTo", peer)
                if (root.sendState == "hashing") return i18n.t("symbisend.send.hashing")
                if (root.sendState == "waiting") return i18n.t("symbisend.send.waitingFor", peer)
                if (root.sendState == "pin") return sender.errorText != "" ? sender.errorText
                                                                       : i18n.t("symbisend.send.asksForPin", peer)
                if (root.sendState == "sending")
                    return sender.fileName + (sender.fileCount > 1
                                              ? " (" + i18n.t("symbisend.general.xOfY",
                                                              { curr: sender.fileIndex, n: sender.fileCount }) + ")"
                                              : "")
                if (root.sendState == "finished") return i18n.t("symbisend.send.sentTo", peer)
                if (root.sendState == "finishedWithErrors")
                    return (sender.filesDone > 0
                            ? i18n.t("symbisend.send.sentSome", { curr: sender.filesDone, n: sender.fileCount,
                                                                 name: sender.peerAlias }) + "\n"
                            : "") + sender.errorText
                if (root.sendState == "cancelled")
                    return sender.errorText != "" ? sender.errorText : i18n.t("symbisend.general.transferCancelled")
                return sender.errorText
            }
        }

        ProgressBar {
            platformInverted: root.platformInverted
            anchors { left: parent.left; right: parent.right; margins: platformStyle.paddingLarge }
            visible: root.sendState == "connecting" || root.sendState == "hashing"
                     || root.sendState == "waiting" || root.sendState == "sending" || root.sendState == "finished"
                     || root.sendState == "finishedWithErrors"
            indeterminate: root.sendState == "connecting" || root.sendState == "waiting"
            minimumValue: 0
            maximumValue: 1
            value: sender.progress
        }

        Label {
            platformInverted: root.platformInverted
            anchors { left: parent.left; right: parent.right; margins: platformStyle.paddingLarge }
            visible: root.sendState == "sending"
            font.pixelSize: platformStyle.fontSizeSmall
            text: i18n.t("symbisend.general.xOfY", { curr: selection.formatSize(sender.bytesSent),
                                                         n: selection.formatSize(sender.bytesTotal) }) + i18n.rev
        }

        Row {
            anchors { left: parent.left; right: parent.right; margins: platformStyle.paddingLarge }
            spacing: platformStyle.paddingMedium
            visible: root.sendState == "pin"

            TextField {
                id: pinField
                platformInverted: root.platformInverted
                width: parent.width - pinButton.width - parent.spacing
                placeholderText: i18n.t("web.enterPin") + i18n.rev
            }
            Button {
                id: pinButton
                platformInverted: root.platformInverted
                text: i18n.t("symbisend.general.ok") + i18n.rev
                enabled: pinField.text != ""
                onClicked: {
                    pinField.closeSoftwareInputPanel()
                    sender.submitPin(pinField.text)
                }
            }
        }

        Item { width: 1; height: platformStyle.paddingSmall }
    }
}
