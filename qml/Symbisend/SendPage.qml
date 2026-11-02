import QtQuick 1.1
import com.nokia.symbian 1.1

// Send tab: nearby LocalSend devices. Bare functional UI - meant to be
// restyled/replaced. See docs/qml-api.md.
Page {
    id: page

    // Light theme = the inverted Symbian style; every component follows it.
    property bool platformInverted: settings.theme == "light"

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
        Label {
            platformInverted: page.platformInverted
            width: parent.width
            wrapMode: Text.Wrap
            font.pixelSize: 18
            color: page.platformInverted ? platformStyle.colorNormalMidInverted : platformStyle.colorNormalMid
            text: network.online
                  ? network.localAddress + " (" + network.connectionName + ")"
                    + (discovery.serverRunning ? "" : " - server down")
                    + (discovery.multicastListening ? "" : " - no multicast")
                  : "Connecting..."
        }
        ProgressBar {
            platformInverted: page.platformInverted
            width: parent.width
            visible: discovery.scanning
            minimumValue: 0
            maximumValue: 100
            value: discovery.scanProgress
        }
    }

    ListView {
        id: list
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom; topMargin: 8 }
        clip: true
        model: peerModel

        delegate: ListItem {
            platformInverted: page.platformInverted
            Column {
                anchors.fill: parent.paddingItem
                ListItemText {
                    platformInverted: page.platformInverted
                    role: "Title"
                    text: model.alias
                }
                ListItemText {
                    platformInverted: page.platformInverted
                    role: "SubTitle"
                    text: model.ip + ":" + model.port + "  " + model.protocol + "  "
                          + model.deviceModel + " (" + model.deviceType + ")  via " + model.source
                }
            }
        }
    }

    Label {
        platformInverted: page.platformInverted
        anchors.centerIn: list
        visible: list.count === 0
        color: page.platformInverted ? platformStyle.colorNormalMidInverted : platformStyle.colorNormalMid
        text: discovery.scanning ? "Searching..." : "No devices found"
    }
}
