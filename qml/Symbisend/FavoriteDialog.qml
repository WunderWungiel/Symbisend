import QtQuick 1.1
import com.nokia.symbian 1.1

// LocalSend's favourite dialog: name, IP address and port of a device, saved
// to `favorites` on Confirm. Open with openFor(device), device being a
// peerModel.peer() map, or openEdit(favorites.favorite(...)) for an existing
// one. An empty name = "(auto)": the device's own alias.
CommonDialog {
    id: root

    property string fingerprint
    // The device's own alias, "" if unknown (editing a custom-named
    // favourite of a device that isn't discovered right now).
    property string deviceAlias
    property bool editing: false

    platformInverted: settings.theme == "light"
    titleText: editing ? "Edit favourite" : "Add to favourites"
    buttonTexts: ["Cancel", "Confirm"]

    property color mutedColor: platformInverted ? platformStyle.colorNormalMidInverted
                                                : platformStyle.colorNormalMid

    function openFor(device) {
        editing = false
        fingerprint = device.fingerprint || ""
        deviceAlias = device.alias || ""
        nameField.text = deviceAlias
        ipField.text = device.ip || ""
        portField.text = device.port ? String(device.port) : ""
        open()
    }

    function openEdit(favourite) {
        editing = true
        fingerprint = favourite.fingerprint || ""
        var peer = peerModel.peer(fingerprint)
        deviceAlias = favourite.customAlias ? (peer.alias || "") : (favourite.alias || "")
        nameField.text = favourite.alias || ""
        ipField.text = favourite.ip || ""
        portField.text = favourite.port ? String(favourite.port) : ""
        open()
    }

    // Same check as FavoritesModel::add(), for a specific message.
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
    // what was typed.
    Timer {
        id: reopenTimer
        interval: 300
        onTriggered: root.open()
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

            Label {
                platformInverted: root.platformInverted
                color: root.mutedColor
                text: "Device name"
            }
            TextField {
                id: nameField
                platformInverted: root.platformInverted
                width: parent.width
                maximumLength: 64
                placeholderText: "(auto)"
                inputMethodHints: Qt.ImhNoPredictiveText
            }

            Item { width: 1; height: platformStyle.paddingSmall }

            Label {
                platformInverted: root.platformInverted
                color: root.mutedColor
                text: "IP Address"
            }
            TextField {
                id: ipField
                platformInverted: root.platformInverted
                width: parent.width
                maximumLength: 15
                inputMethodHints: Qt.ImhPreferNumbers | Qt.ImhNoPredictiveText
            }

            Item { width: 1; height: platformStyle.paddingSmall }

            Label {
                platformInverted: root.platformInverted
                color: root.mutedColor
                text: "Port"
            }
            TextField {
                id: portField
                platformInverted: root.platformInverted
                width: parent.width
                maximumLength: 5
                inputMethodHints: Qt.ImhDigitsOnly
                validator: IntValidator { bottom: 1; top: 65535 }
            }
        }
    }

    onButtonClicked: {
        nameField.closeSoftwareInputPanel()
        ipField.closeSoftwareInputPanel()
        portField.closeSoftwareInputPanel()
        if (index != 1)
            return
        var ip = ipField.text.replace(/^\s+|\s+$/g, "")
        var port = parseInt(portField.text, 10)
        if (!validIp(ip)) {
            picker.showInfo("Invalid IP address")
            reopenTimer.start()
            return
        }
        if (!(port >= 1 && port <= 65535)) {
            picker.showInfo("Invalid port")
            reopenTimer.start()
            return
        }
        if (!favorites.add(fingerprint, nameField.text, deviceAlias, ip, port))
            picker.showInfo(editing ? "Could not save the favourite" : "Could not add to favourites")
    }
}
