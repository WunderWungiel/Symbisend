import QtQuick 1.1
import com.nokia.symbian 1.1

// After a quick save, like the official app: offers to open the file that
// arrived last, and goes away by itself after 5 s. The buttons are our own
// because a CommonDialog rebuilds its buttons whenever their text changes,
// which would lose a tap during the countdown.
Dialog {
    id: root

    platformInverted: settings.theme == "light"

    property string filePath: ""
    property real buttonWidth: Math.floor((platformContentMaximumWidth - 4 * platformStyle.paddingSmall) / 3)

    function openFor(path) {
        filePath = path
        closeTimer.begin(5)
        open()
    }

    CountdownTimer {
        id: closeTimer
        onElapsed: root.close()
    }

    onStatusChanged: if (status == DialogStatus.Closing || status == DialogStatus.Closed) closeTimer.stop()

    // Sized from the column explicitly, like SendDialog.
    height: privateTitleHeight + contentColumn.height + privateButtonsHeight

    title: Item {
        anchors { left: parent.left; right: parent.right }
        height: platformStyle.graphicSizeSmall + 2 * platformStyle.paddingLarge

        Text {
            anchors { fill: parent; margins: platformStyle.paddingLarge }
            font { family: platformStyle.fontFamilyRegular; pixelSize: platformStyle.fontSizeLarge }
            color: root.platformInverted ? platformStyle.colorNormalLinkInverted
                                         : platformStyle.colorNormalLink
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            text: "Open file"
        }
    }

    content: Column {
        id: contentColumn
        width: root.platformContentMaximumWidth

        Item { width: 1; height: platformStyle.paddingLarge }

        Label {
            platformInverted: root.platformInverted
            anchors { left: parent.left; right: parent.right; margins: platformStyle.paddingLarge }
            wrapMode: Text.Wrap
            text: "Do you want to open the received file?"
        }

        Item { width: 1; height: platformStyle.paddingLarge }
    }

    buttons: Item {
        width: parent.width
        height: privateStyle.toolBarHeightLandscape + 2 * platformStyle.paddingSmall

        Row {
            anchors.centerIn: parent
            spacing: platformStyle.paddingSmall

            ToolButton {
                width: root.buttonWidth
                height: privateStyle.toolBarHeightLandscape
                platformInverted: root.platformInverted
                text: "Open"
                onClicked: {
                    root.close()
                    if (!history.openPath(root.filePath))
                        picker.showInfo("The file no longer exists or no application can open it.")
                }
            }
            ToolButton {
                width: root.buttonWidth
                height: privateStyle.toolBarHeightLandscape
                platformInverted: root.platformInverted
                text: "Folder"
                onClicked: {
                    root.close()
                    if (!history.openFolder(root.filePath.substring(0, root.filePath.lastIndexOf("/"))))
                        picker.showInfo("The folder can't be shown.")
                }
            }
            ToolButton {
                width: root.buttonWidth
                height: privateStyle.toolBarHeightLandscape
                platformInverted: root.platformInverted
                text: "Close (" + closeTimer.counter + ")"
                onClicked: root.close()
            }
        }
    }
}
