import QtQuick 1.1
import com.nokia.symbian 1.1

// The Send page's "Selection" card, shown instead of the File/Media/...
// buttons once something was picked. Modelled on LocalSend's selection card:
// title with a close (clear) button, element count and total size, a row of
// item tiles, and Edit / Add buttons.
Rectangle {
    id: root

    property bool platformInverted: settings.theme == "light"

    signal editClicked
    signal addClicked

    height: column.height + 2 * platformStyle.paddingMedium
    radius: 3
    color: platformInverted ? "#14000000" : "#1affffff"

    Column {
        id: column
        x: platformStyle.paddingMedium
        y: platformStyle.paddingMedium
        width: parent.width - 2 * platformStyle.paddingMedium
        spacing: platformStyle.paddingSmall

        // ------------------------------------------------ Title + close
        Item {
            width: parent.width
            height: closeButton.height

            Label {
                platformInverted: root.platformInverted
                anchors { left: parent.left; right: closeButton.left; verticalCenter: parent.verticalCenter }
                elide: Text.ElideRight
                font.pixelSize: platformStyle.fontSizeLarge
                text: "Selection"
            }
            ToolButton {
                id: closeButton
                platformInverted: root.platformInverted
                anchors.right: parent.right
                iconSource: root.platformInverted ? "images/close-inverted.svg" : "images/close.svg"
                onClicked: selection.clear()
            }
        }

        // ------------------------------------------------ Summary
        Label {
            platformInverted: root.platformInverted
            font.pixelSize: platformStyle.fontSizeSmall
            text: "Elements: " + selection.count
        }
        Label {
            platformInverted: root.platformInverted
            font.pixelSize: platformStyle.fontSizeSmall
            text: "Size: " + selection.totalSizeText
        }

        // ------------------------------------------------ Item tiles
        ListView {
            id: tiles
            width: parent.width
            height: platformStyle.graphicSizeLarge
            orientation: ListView.Horizontal
            spacing: platformStyle.paddingSmall
            clip: true
            model: selection
            // Follow newly added items.
            onCountChanged: positionViewAtEnd()

            delegate: SelectionTile {
                kind: model.kind
                platformInverted: root.platformInverted
            }
        }

        // ------------------------------------------------ Edit / Add
        // A ToolButton with icon + text is ~170 px wide in portrait (toolbar
        // height + 15 x paddingMedium), so two don't fit in the card: cap each
        // at half the row.
        Item {
            id: buttonRow
            width: parent.width
            height: addButton.height

            property real spacing: platformStyle.paddingMedium
            property real maxButtonWidth: (width - spacing) / 2

            ToolButton {
                platformInverted: root.platformInverted
                width: Math.min(implicitWidth, buttonRow.maxButtonWidth)
                anchors { right: addButton.left; rightMargin: buttonRow.spacing }
                iconSource: root.platformInverted ? "images/edit-inverted.svg" : "images/edit.svg"
                text: "Edit"
                onClicked: root.editClicked()
            }
            ToolButton {
                id: addButton
                platformInverted: root.platformInverted
                width: Math.min(implicitWidth, buttonRow.maxButtonWidth)
                anchors.right: parent.right
                iconSource: "toolbar-add"
                text: "Add"
                onClicked: root.addClicked()
            }
        }
    }
}
