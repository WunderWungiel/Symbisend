import QtQuick 1.1
import com.nokia.symbian 1.1

// Settings tab. Bare functional UI - meant to be restyled/replaced.
// See docs/qml-api.md.
Page {
    id: page

    signal logRequested

    Column {
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 8 }
        spacing: 8

        Label {
            text: "Download folder"
            font.pixelSize: 24
        }
        Label {
            width: parent.width
            wrapMode: Text.WrapAnywhere
            font.pixelSize: 20
            text: settings.downloadFolder
        }
        Label {
            width: parent.width
            wrapMode: Text.Wrap
            font.pixelSize: 16
            color: "#aaaaaa"
            text: settings.downloadFolderIsDefault ? "Default" : "Chosen by you"
        }
        Button {
            width: parent.width
            text: "Change folder..."
            onClicked: settings.pickDownloadFolder()
        }
        Button {
            width: parent.width
            text: "Use default"
            enabled: !settings.downloadFolderIsDefault
            onClicked: settings.resetDownloadFolder()
        }

        Label {
            text: "Debugging"
            font.pixelSize: 24
        }
        Button {
            width: parent.width
            text: "Show log"
            onClicked: page.logRequested()
        }
    }
}
