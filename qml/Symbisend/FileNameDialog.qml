import QtQuick 1.1
import com.nokia.symbian 1.1

// LocalSend's file name dialog, for a file on the Options page: openFor() with
// its row in `receiveFiles`. Confirm renames it and closes, unless the name
// isn't usable: then it stays open with the reason below the field. A Dialog
// and not a CommonDialog, whose buttons always close it.
Dialog {
    id: root

    property int row: -1
    property string originalName
    property string errorText

    platformInverted: settings.theme == "light"

    function openFor(fileRow, original, current) {
        row = fileRow
        originalName = original
        nameField.text = current
        errorText = ""
        open()
    }

    function confirm() {
        errorText = receiveFiles.nameError(nameField.text)
        if (errorText == "" && receiveFiles.rename(row, nameField.text))
            close()
    }

    onStatusChanged: if (status == DialogStatus.Closing) nameField.closeSoftwareInputPanel()

    // Sized from the column explicitly, like ReceiveDialog.
    height: privateTitleHeight + fields.height + 2 * platformStyle.paddingLarge + privateButtonsHeight

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
            text: "Enter file name"
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
            spacing: platformStyle.paddingMedium

            Label {
                platformInverted: root.platformInverted
                width: parent.width
                wrapMode: Text.WrapAnywhere
                text: "Original: " + root.originalName
            }
            TextField {
                id: nameField
                platformInverted: root.platformInverted
                width: parent.width
                inputMethodHints: Qt.ImhNoPredictiveText
                onTextChanged: root.errorText = receiveFiles.nameError(text)
            }
            Label {
                platformInverted: root.platformInverted
                width: parent.width
                visible: root.errorText != ""
                wrapMode: Text.Wrap
                font.pixelSize: platformStyle.fontSizeSmall
                color: "#ff9800"
                text: root.errorText
            }
        }
    }

    buttons: Item {
        width: parent.width
        height: privateStyle.toolBarHeightLandscape + 2 * platformStyle.paddingSmall

        Row {
            anchors.centerIn: parent
            spacing: platformStyle.paddingMedium

            ToolButton {
                width: Math.round((root.platformContentMaximumWidth - 3 * platformStyle.paddingMedium) / 2)
                platformInverted: root.platformInverted
                text: "Cancel"
                onClicked: root.close()
            }
            ToolButton {
                width: Math.round((root.platformContentMaximumWidth - 3 * platformStyle.paddingMedium) / 2)
                platformInverted: root.platformInverted
                text: "Confirm"
                onClicked: root.confirm()
            }
        }
    }
}
