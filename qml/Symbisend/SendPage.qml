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

    function paste() {
        var text = picker.clipboardText() // logged by C++
        if (text.length > 0)
            selection.addText(text)
        else
            picker.showInfo("Clipboard is empty")
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
                anchors.right: parent.right
                iconSource: "toolbar-refresh"
                enabled: !discovery.scanning
                onClicked: discovery.refresh()
            }
        }

        // What to send. Picked items go into `selection`; once there is
        // something, the Selection card replaces the buttons.
        Row {
            id: sendOptions
            width: parent.width
            spacing: platformStyle.paddingSmall
            visible: selection.count === 0

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

        SelectionCard {
            width: parent.width
            visible: selection.count > 0
            platformInverted: page.platformInverted
            onEditClicked: page.editSelectionRequested()
            onAddClicked: addMenu.open()
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

    // The Selection card's Add button: every option of the button row.
    ContextMenu {
        id: addMenu
        platformInverted: page.platformInverted
        MenuLayout {
            MenuItem {
                platformInverted: page.platformInverted
                text: "File"
                onClicked: picker.pickFile()
            }
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
            MenuItem {
                platformInverted: page.platformInverted
                text: "Paste"
                onClicked: page.paste()
            }
            MenuItem {
                platformInverted: page.platformInverted
                text: "Text"
                onClicked: textDialog.openNew()
            }
            MenuItem {
                platformInverted: page.platformInverted
                text: "Folder"
                onClicked: picker.pickFolder()
            }
        }
    }

    SendTextDialog {
        id: textDialog
        platformInverted: page.platformInverted
    }
}
