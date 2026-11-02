import QtQuick 1.1
import com.nokia.symbian 1.1

// Edit the selection, opened with the Selection card's Edit button. Like
// LocalSend's "selected files" page: every item with its size and a delete
// button, plus "Delete all". Tapping a text message edits it. Closes itself
// once the selection is empty. Has its own back button at the top, so
// main.qml hides the toolbar.
Page {
    id: page

    // Read by main.qml: this page has its own back button.
    property bool ownBackButton: true

    // Light theme = the inverted Symbian style; every component follows it.
    property bool platformInverted: settings.theme == "light"

    property color mutedColor: platformInverted ? platformStyle.colorNormalMidInverted
                                                : platformStyle.colorNormalMid

    Connections {
        target: selection
        onCountChanged: {
            if (selection.count === 0 && page.status == PageStatus.Active)
                page.pageStack.pop()
        }
    }

    // ------------------------------------------------ Header
    Item {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: 8; rightMargin: 8 }
        height: backButton.height + 8

        ToolButton {
            id: backButton
            platformInverted: page.platformInverted
            anchors { left: parent.left; bottom: parent.bottom }
            iconSource: "toolbar-back"
            onClicked: page.pageStack.pop()
        }
        Label {
            platformInverted: page.platformInverted
            anchors {
                left: backButton.right; leftMargin: platformStyle.paddingMedium
                right: parent.right; verticalCenter: backButton.verticalCenter
            }
            elide: Text.ElideRight
            text: "Selection"
            font.pixelSize: 26
        }
    }

    // ------------------------------------------------ Summary + Delete all
    Item {
        id: summary
        anchors {
            left: parent.left; right: parent.right; top: header.bottom
            leftMargin: platformStyle.paddingLarge; rightMargin: platformStyle.paddingMedium
            topMargin: platformStyle.paddingSmall
        }
        height: Math.max(summaryLabels.height, deleteAllButton.height)

        Column {
            id: summaryLabels
            anchors {
                left: parent.left; right: deleteAllButton.left; rightMargin: platformStyle.paddingMedium
                verticalCenter: parent.verticalCenter
            }

            Label {
                platformInverted: page.platformInverted
                width: parent.width
                elide: Text.ElideRight
                font.pixelSize: platformStyle.fontSizeSmall
                color: page.mutedColor
                text: "Elements: " + selection.count
            }
            Label {
                platformInverted: page.platformInverted
                width: parent.width
                elide: Text.ElideRight
                font.pixelSize: platformStyle.fontSizeSmall
                color: page.mutedColor
                text: "Size: " + selection.totalSizeText
            }
        }

        ToolButton {
            id: deleteAllButton
            platformInverted: page.platformInverted
            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
            iconSource: "toolbar-delete"
            text: "Delete all"
            onClicked: selection.clear()
        }
    }

    // ------------------------------------------------ Items
    ListView {
        id: list
        anchors {
            left: parent.left; right: parent.right; top: summary.bottom; bottom: parent.bottom
            topMargin: platformStyle.paddingMedium
        }
        clip: true
        model: selection

        delegate: Item {
            id: row
            width: list.width
            height: Math.max(tile.height, deleteButton.height) + 2 * platformStyle.paddingSmall

            property bool isText: model.kind == "text"

            // Tapping a text message opens it for editing.
            MouseArea {
                anchors.fill: parent
                enabled: row.isText
                onClicked: textDialog.openEdit(index, model.text)
            }

            SelectionTile {
                id: tile
                anchors {
                    left: parent.left; leftMargin: platformStyle.paddingLarge
                    verticalCenter: parent.verticalCenter
                }
                width: platformStyle.graphicSizeMedium
                kind: model.kind
                platformInverted: page.platformInverted
            }

            Column {
                anchors {
                    left: tile.right; leftMargin: platformStyle.paddingMedium
                    right: deleteButton.left; rightMargin: platformStyle.paddingSmall
                    verticalCenter: parent.verticalCenter
                }

                Label {
                    platformInverted: page.platformInverted
                    width: parent.width
                    elide: Text.ElideRight
                    // Text messages: their first line. Folder contents show
                    // their path inside the folder.
                    text: row.isText ? model.text.split("\n")[0] : model.relativePath
                }
                Label {
                    platformInverted: page.platformInverted
                    width: parent.width
                    elide: Text.ElideRight
                    font.pixelSize: platformStyle.fontSizeSmall
                    color: page.mutedColor
                    text: row.isText ? "Text, " + model.sizeText + " - tap to edit" : model.sizeText
                }
            }

            ToolButton {
                id: deleteButton
                platformInverted: page.platformInverted
                anchors {
                    right: parent.right; rightMargin: platformStyle.paddingMedium
                    verticalCenter: parent.verticalCenter
                }
                iconSource: "toolbar-delete"
                onClicked: selection.remove(index)
            }
        }
    }

    ScrollDecorator {
        flickableItem: list
        platformInverted: page.platformInverted
    }

    SendTextDialog {
        id: textDialog
        platformInverted: page.platformInverted
    }
}
