import QtQuick 1.1
import com.nokia.symbian 1.1

// Send tab: nearby LocalSend devices. Bare functional UI - meant to be
// restyled/replaced. See docs/qml-api.md.
Page {
    id: page

    Column {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 8 }
        spacing: 4

        Item {
            width: parent.width
            height: refreshButton.height

            Label {
                anchors { left: parent.left; right: refreshButton.left; verticalCenter: parent.verticalCenter }
                elide: Text.ElideRight
                text: "Nearby devices"
                font.pixelSize: 26
            }
            ToolButton {
                id: refreshButton
                anchors.right: parent.right
                iconSource: "toolbar-refresh"
                enabled: !discovery.scanning
                onClicked: discovery.refresh()
            }
        }
        Label {
            width: parent.width
            wrapMode: Text.Wrap
            font.pixelSize: 18
            color: "#aaaaaa"
            text: network.online
                  ? network.localAddress + " (" + network.connectionName + ")"
                    + (discovery.serverRunning ? "" : " - server down")
                    + (discovery.multicastListening ? "" : " - no multicast")
                  : "Connecting..."
        }
        ProgressBar {
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
            Column {
                anchors.fill: parent.paddingItem
                ListItemText {
                    role: "Title"
                    text: model.alias
                }
                ListItemText {
                    role: "SubTitle"
                    text: model.ip + ":" + model.port + "  " + model.protocol + "  "
                          + model.deviceModel + " (" + model.deviceType + ")  via " + model.source
                }
            }
        }
    }

    Label {
        anchors.centerIn: list
        visible: list.count === 0
        color: "#aaaaaa"
        text: discovery.scanning ? "Searching..." : "No devices found"
    }
}
