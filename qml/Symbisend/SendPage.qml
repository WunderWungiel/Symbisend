import QtQuick 1.1
import com.nokia.symbian 1.1

// Send tab: nearby LocalSend devices. Bare functional UI - meant to be
// restyled/replaced. See docs/qml-api.md.
Page {
    id: page

    // Light theme = the inverted Symbian style; every component follows it.
    property bool platformInverted: settings.theme == "light"

    // Info button on a device row; main.qml opens DeviceDetailsPage.
    signal deviceInfoRequested(string fingerprint)
    // Edit button on the Selection card; main.qml opens SelectionPage.
    signal editSelectionRequested()
    // Heart button next to Refresh; main.qml opens FavoritesPage.
    signal favoritesRequested()

    // The Selection card's Add button toggles the send option row below it.
    property bool addOptionsShown: false

    function paste() {
        var text = picker.clipboardText() // logged by C++
        if (text.length > 0)
            selection.addText(text)
        else
            picker.checkClipboard() // emptied meanwhile: just disable the button
    }

    // A copy made in another app is not signalled, so look at the clipboard
    // again whenever the Paste button may come into view.
    function updateClipboard() {
        if (page.visible && Qt.application.active)
            picker.checkClipboard()
    }
    onVisibleChanged: updateClipboard()
    Connections {
        target: Qt.application
        onActiveChanged: page.updateClipboard()
    }

    // Keep the device list current while it is on screen: not on another
    // tab, under a pushed page, or with the app in the background.
    Binding {
        target: discovery
        property: "autoRefresh"
        // No scans while sending or receiving: they would compete with the transfer.
        value: page.visible && Qt.application.active && !sender.busy && !receiver.busy
    }

    // Tap on a device: send the selection to it.
    function sendTo(fingerprint) {
        if (selection.count === 0) {
            picker.showInfo("Select something to send first")
            return
        }
        if (sender.send(fingerprint))
            sendDialog.open()
        else if (sender.errorText != "")
            picker.showInfo(sender.errorText)
    }

    Connections {
        target: selection
        onFolderTruncated: picker.showInfo("Only the first " + maxFiles + " files of the folder were added")
        // Start collapsed again the next time the card appears.
        onCountChanged: if (selection.count === 0) page.addOptionsShown = false
    }

    Column {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 8 }
        spacing: 4

        Item {
            width: parent.width
            height: refreshButton.height

            Label {
                platformInverted: page.platformInverted
                anchors { left: parent.left; right: refreshButton.left; verticalCenter: parent.verticalCenter }
                elide: Text.ElideRight
                text: "Nearby devices"
                font.pixelSize: 26
            }
            ToolButton {
                id: refreshButton
                platformInverted: page.platformInverted
                anchors.right: favoritesButton.left
                iconSource: "toolbar-refresh"
                enabled: !discovery.scanning
                onClicked: discovery.refresh()
            }
            ToolButton {
                id: favoritesButton
                platformInverted: page.platformInverted
                anchors.right: parent.right
                iconSource: page.platformInverted ? "images/favorite-inverted.svg"
                                                  : "images/favorite.svg"
                onClicked: page.favoritesRequested()
            }
        }

        SelectionCard {
            width: parent.width
            visible: selection.count > 0
            platformInverted: page.platformInverted
            onEditClicked: page.editSelectionRequested()
            onAddClicked: page.addOptionsShown = !page.addOptionsShown
        }

        // What to send. Picked items go into `selection`; once there is
        // something, the Selection card takes over and this row only comes
        // back below it while the card's Add button is toggled on.
        Row {
            id: sendOptions
            width: parent.width
            spacing: platformStyle.paddingSmall
            visible: selection.count === 0 || page.addOptionsShown
            onVisibleChanged: page.updateClipboard()

            property real buttonWidth: (width - 4 * spacing) / 5

            SendOptionButton {
                width: sendOptions.buttonWidth
                label: "File"
                icon: "send-file"
                onClicked: picker.pickFile()
            }
            SendOptionButton {
                width: sendOptions.buttonWidth
                label: "Media"
                icon: "send-media"
                onClicked: mediaMenu.open()
            }
            SendOptionButton {
                width: sendOptions.buttonWidth
                label: "Paste"
                icon: "send-paste"
                enabled: picker.clipboardHasText
                onClicked: page.paste()
            }
            SendOptionButton {
                width: sendOptions.buttonWidth
                label: "Text"
                icon: "send-text"
                onClicked: textDialog.openNew()
            }
            SendOptionButton {
                width: sendOptions.buttonWidth
                label: "Folder"
                icon: "send-folder"
                onClicked: picker.pickFolder()
            }
        }
    }

    ListView {
        id: list
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom; topMargin: 8 }
        clip: true
        model: peerModel

        delegate: PeerItem {
            name: model.alias
            deviceType: model.deviceType
            protocol: model.protocol
            system: model.deviceModel
            onInfoClicked: page.deviceInfoRequested(model.fingerprint)
            onClicked: page.sendTo(model.fingerprint)
        }
    }

    Label {
        platformInverted: page.platformInverted
        anchors.centerIn: list
        visible: list.count === 0
        color: page.platformInverted ? platformStyle.colorNormalMidInverted : platformStyle.colorNormalMid
        text: discovery.scanning ? "Searching..." : "No devices found"
    }

    // ---------------------------------------------------- dialogs

    // The gallery fetcher shows one media type at a time.
    ContextMenu {
        id: mediaMenu
        platformInverted: page.platformInverted
        MenuLayout {
            MenuItem {
                platformInverted: page.platformInverted
                text: "Images"
                onClicked: picker.pickMedia("image")
            }
            MenuItem {
                platformInverted: page.platformInverted
                text: "Videos"
                onClicked: picker.pickMedia("video")
            }
        }
    }

    SendDialog {
        id: sendDialog
        platformInverted: page.platformInverted
    }

    SendTextDialog {
        id: textDialog
        platformInverted: page.platformInverted
    }
}
