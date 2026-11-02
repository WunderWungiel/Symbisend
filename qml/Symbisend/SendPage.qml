import QtQuick 1.1
import com.nokia.symbian 1.1

// Bare functional UI - meant to be restyled/replaced. See docs/qml-api.md.
Page {
    id: page

    tools: ToolBarLayout {
        ToolButton {
            iconSource: "toolbar-back"
            onClicked: Qt.quit()
        }
        ToolButton {
            iconSource: "toolbar-refresh"
            enabled: !discovery.scanning
            onClicked: discovery.refresh()
        }
        ToolButton {
            text: "Log"
            onClicked: page.pageStack.push(Qt.resolvedUrl("LogPage.qml"))
        }
        ToolButton {
            iconSource: "toolbar-settings"
            onClicked: page.pageStack.push(Qt.resolvedUrl("SettingsPage.qml"))
        }
    }

    Column {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 8 }
        spacing: 4

        Label {
            text: myDevice.alias
            font.pixelSize: 26
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
