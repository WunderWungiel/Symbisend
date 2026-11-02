import QtQuick 1.1
import com.nokia.symbian 1.1

// About Symbisend. Opened from Settings > Other; also leads to the debug log.
Page {
    id: page

    tools: ToolBarLayout {
        ToolButton {
            iconSource: "toolbar-back"
            onClicked: page.pageStack.pop()
        }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.height + 2 * platformStyle.paddingLarge
        clip: true

        Column {
            id: column
            anchors {
                left: parent.left; right: parent.right; top: parent.top
                margins: platformStyle.paddingLarge
            }
            spacing: platformStyle.paddingLarge

            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 96
                height: 96
                sourceSize.width: 96
                sourceSize.height: 96
                smooth: true
                source: "images/symbisend.svg"
            }
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                font.pixelSize: platformStyle.fontSizeLarge
                text: "Symbisend"
            }
            Label {
                width: parent.width
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                text: "A LocalSend client for Symbian.\n"
                      + "Compatible with LocalSend (protocol v2)."
            }
            Label {
                width: parent.width
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: platformStyle.fontSizeSmall
                text: "This device: " + myDevice.alias + "\n"
                      + (network.online ? network.localAddress + ":" + myDevice.port
                                        : "Not connected")
            }
            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Show log"
                onClicked: page.pageStack.push(Qt.resolvedUrl("LogPage.qml"))
            }
        }
    }

    ScrollDecorator { flickableItem: flick }
}
