import QtQuick 1.1
import com.nokia.symbian 1.1

// LocalSend's Verify page as a dialog: the 16 icons or the text made from our
// fingerprint and the peer's (myDevice.verification()). Both devices show the
// same when they really talk to each other. Open with openFor(fingerprint).
Dialog {
    id: root

    platformInverted: settings.theme == "light"

    // { text, icons } from myDevice.verification()
    property variant verification: ({ text: "", icons: [] })
    property bool showText: false

    property color mutedColor: platformInverted ? platformStyle.colorNormalMidInverted
                                                : platformStyle.colorNormalMid
    property color textColor: platformInverted ? platformStyle.colorNormalLightInverted
                                               : platformStyle.colorNormalLight
    // For image://verify (rrggbb): the icons are drawn in the text colour.
    property string iconColor: platformInverted ? "000000" : "ffffff"
    property int iconSize: platformStyle.graphicSizeMedium

    function openFor(fingerprint) {
        verification = myDevice.verification(fingerprint)
        showText = false
        modeRow.checkedButton = iconsButton
        open()
    }

    // Sized from the column explicitly, like ReceiveDialog.
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
            text: "Verify"
        }
    }

    content: Column {
        id: contentColumn
        width: root.platformContentMaximumWidth
        spacing: platformStyle.paddingLarge

        Item { width: 1; height: platformStyle.paddingSmall }

        // Icons / Text, like the official SegmentedButton.
        // ButtonRow checks the clicked button itself (exclusive).
        ButtonRow {
            id: modeRow
            anchors { left: parent.left; right: parent.right; margins: platformStyle.paddingLarge }
            Button {
                id: iconsButton
                platformInverted: root.platformInverted
                text: "Icons"
                onClicked: root.showText = false
            }
            Button {
                platformInverted: root.platformInverted
                text: "Text"
                onClicked: root.showText = true
            }
        }

        Grid {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: !root.showText
            columns: 4
            spacing: platformStyle.paddingLarge * 1.5

            Repeater {
                model: root.verification.icons

                Image {
                    width: root.iconSize
                    height: root.iconSize
                    sourceSize { width: root.iconSize; height: root.iconSize }
                    source: "image://verify/" + modelData + "/" + root.iconColor
                }
            }
        }

        Text {
            anchors { left: parent.left; right: parent.right; margins: platformStyle.paddingLarge }
            visible: root.showText
            wrapMode: Text.WrapAnywhere
            font { family: platformStyle.fontFamilyRegular; pixelSize: platformStyle.fontSizeMedium }
            color: root.textColor
            text: root.verification.text
        }

        Text {
            anchors { left: parent.left; right: parent.right; margins: platformStyle.paddingLarge }
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            font { family: platformStyle.fontFamilyRegular; pixelSize: platformStyle.fontSizeSmall }
            color: root.mutedColor
            text: "Does it look the same on the other device?"
        }

        Item { width: 1; height: platformStyle.paddingSmall }
    }

    buttons: Item {
        width: parent.width
        height: privateStyle.toolBarHeightLandscape + 2 * platformStyle.paddingSmall

        ToolButton {
            anchors.centerIn: parent
            width: Math.round((root.platformContentMaximumWidth - 3 * platformStyle.paddingMedium) / 2)
            height: privateStyle.toolBarHeightLandscape
            platformInverted: root.platformInverted
            text: "Close"
            onClicked: root.close()
        }
    }
}
