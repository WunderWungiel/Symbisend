import QtQuick 1.1
import com.nokia.symbian 1.1

// LocalSend's "Enter address": the IP of a device that wasn't found.
// addressEntered() comes once the dialog is gone, for a valid address only;
// the text stays for the next time, e.g. to correct a typo.
CommonDialog {
    id: root

    signal addressEntered(string ip)

    // What Confirm left for afterClose; "" = it was invalid.
    property string enteredIp

    platformInverted: settings.theme == "light"
    titleText: "Enter address"
    buttonTexts: ["Cancel", "Confirm"]

    function validIp(text) {
        var parts = text.split(".")
        if (parts.length != 4)
            return false
        for (var i = 0; i < 4; ++i) {
            if (!/^[0-9]{1,3}$/.test(parts[i]) || parseInt(parts[i], 10) > 255)
                return false
        }
        return true
    }

    // The dialog closes on every button; after a mistake it opens again with
    // what was typed, and the next dialog must not open while this one closes.
    Timer {
        id: afterClose
        interval: 300
        onTriggered: {
            if (root.enteredIp != "")
                root.addressEntered(root.enteredIp)
            else
                root.open()
        }
    }

    content: Item {
        width: root.platformContentMaximumWidth
        height: fields.height + 2 * platformStyle.paddingLarge

        Column {
            id: fields
            anchors {
                left: parent.left; right: parent.right
                verticalCenter: parent.verticalCenter
                margins: platformStyle.paddingLarge
            }
            spacing: platformStyle.paddingSmall

            Item {
                width: parent.width
                height: ipField.height

                Label {
                    id: ipLabel
                    platformInverted: root.platformInverted
                    anchors { left: parent.left; verticalCenter: parent.verticalCenter }
                    text: "IP:"
                }
                TextField {
                    id: ipField
                    platformInverted: root.platformInverted
                    anchors {
                        left: ipLabel.right; right: parent.right
                        leftMargin: platformStyle.paddingMedium
                    }
                    maximumLength: 15
                    inputMethodHints: Qt.ImhPreferNumbers | Qt.ImhNoPredictiveText
                }
            }

            Label {
                platformInverted: root.platformInverted
                color: platformInverted ? platformStyle.colorNormalMidInverted
                                        : platformStyle.colorNormalMid
                font.pixelSize: platformStyle.fontSizeSmall
                text: "Example: 192.168.1.123"
            }
        }
    }

    onButtonClicked: {
        ipField.closeSoftwareInputPanel()
        if (index != 1)
            return
        var ip = ipField.text.replace(/^\s+|\s+$/g, "")
        enteredIp = validIp(ip) ? ip : ""
        if (enteredIp == "")
            picker.showInfo("Invalid IP address")
        afterClose.start()
    }
}
