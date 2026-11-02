import QtQuick 1.1
import com.nokia.symbian 1.1

// Progress of a transfer started with sender.send(): waiting for the
// receiver, PIN entry, total progress with the time left in the title, and
// the result. Cancel stops the transfer; Close just hides the finished one.
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
        if (sendState == "waiting") return "Waiting for response"
        if (sendState == "pin") return "PIN required"
        if (sendState == "sending")
            return sender.remainingSeconds < 0 ? "Total progress"
                                               : "Total progress (" + formatTime(sender.remainingSeconds) + ")"
        if (sendState == "finished") return "Finished"
        if (sendState == "declined") return "Declined"
        if (sendState == "cancelled") return "Cancelled"
        return "Error"
    }
    buttonTexts: [busy ? "Cancel" : "Close"]

    onButtonClicked: {
        if (busy)
            sender.cancel()
    }

    // Opened while the PIN field is shown: start with an empty one.
    onSendStateChanged: if (sendState == "pin") pinField.text = ""

    content: Column {
        width: root.platformContentMaximumWidth
        spacing: platformStyle.paddingMedium

        Item { width: 1; height: platformStyle.paddingSmall }

        Label {
            platformInverted: root.platformInverted
            anchors { left: parent.left; right: parent.right; margins: platformStyle.paddingLarge }
            wrapMode: Text.Wrap
            text: {
                if (root.sendState == "waiting") return "Waiting for " + sender.peerAlias + " to accept..."
                if (root.sendState == "pin") return sender.errorText != "" ? sender.errorText
                                                                       : sender.peerAlias + " asks for a PIN"
                if (root.sendState == "sending")
                    return sender.fileName + (sender.fileCount > 1
                                              ? " (" + sender.fileIndex + " of " + sender.fileCount + ")" : "")
                if (root.sendState == "finished") return "Sent to " + sender.peerAlias
                if (root.sendState == "cancelled") return "The transfer was cancelled"
                return sender.errorText
            }
        }

        ProgressBar {
            platformInverted: root.platformInverted
            anchors { left: parent.left; right: parent.right; margins: platformStyle.paddingLarge }
            visible: root.sendState == "waiting" || root.sendState == "sending" || root.sendState == "finished"
            indeterminate: root.sendState == "waiting"
            minimumValue: 0
            maximumValue: 1
            value: sender.progress
        }

        Label {
            platformInverted: root.platformInverted
            anchors { left: parent.left; right: parent.right; margins: platformStyle.paddingLarge }
            visible: root.sendState == "sending"
            font.pixelSize: platformStyle.fontSizeSmall
            text: selection.formatSize(sender.bytesSent) + " of " + selection.formatSize(sender.bytesTotal)
        }

        Row {
            anchors { left: parent.left; right: parent.right; margins: platformStyle.paddingLarge }
            spacing: platformStyle.paddingMedium
            visible: root.sendState == "pin"

            TextField {
                id: pinField
                platformInverted: root.platformInverted
                width: parent.width - pinButton.width - parent.spacing
                placeholderText: "PIN"
            }
            Button {
                id: pinButton
                platformInverted: root.platformInverted
                text: "OK"
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
