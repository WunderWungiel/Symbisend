import QtQuick 1.1
import com.nokia.symbian 1.1

// History of received files and messages (the `history` model), opened with
// the History button on the Home tab. Like LocalSend's history page: Open
// folder / Delete history, then every entry with its type icon, name (or the
// message), date, size and sender. Has its own back button at the top, so
// main.qml hides the toolbar.
Page {
    id: page

    // Read by main.qml: this page has its own back button.
    property bool ownBackButton: true

    // Light theme = the inverted Symbian style; every component follows it.
    property bool platformInverted: settings.theme == "light"

    property color mutedColor: platformInverted ? platformStyle.colorNormalMidInverted
                                                : platformStyle.colorNormalMid

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
            text: "History"
            font.pixelSize: 26
        }
    }

    // ------------------------------------------------ Open folder / Delete history
    // Icon + text ToolButtons, capped at half the row each (see SelectionCard).
    Item {
        id: buttonRow
        anchors {
            left: parent.left; right: parent.right; top: header.bottom
            leftMargin: platformStyle.paddingLarge; rightMargin: platformStyle.paddingLarge
            topMargin: platformStyle.paddingSmall
        }
        height: openFolderButton.height

        property real spacing: platformStyle.paddingMedium
        property real maxButtonWidth: (width - spacing) / 2

        ToolButton {
            id: openFolderButton
            platformInverted: page.platformInverted
            width: Math.min(implicitWidth, buttonRow.maxButtonWidth)
            anchors.left: parent.left
            iconSource: page.platformInverted ? "images/send-folder-inverted.svg" : "images/send-folder.svg"
            text: "Open folder"
            // TODO: open settings.downloadFolder
        }
        ToolButton {
            platformInverted: page.platformInverted
            width: Math.min(implicitWidth, buttonRow.maxButtonWidth)
            anchors { left: openFolderButton.right; leftMargin: buttonRow.spacing }
            iconSource: "toolbar-delete"
            text: "Delete history"
            // TODO: clear the history
        }
    }

    // ------------------------------------------------ Entries
    ListView {
        id: list
        anchors {
            left: parent.left; right: parent.right; top: buttonRow.bottom; bottom: parent.bottom
            topMargin: platformStyle.paddingMedium
        }
        clip: true
        model: history

        delegate: Item {
            id: row
            width: list.width
            height: Math.max(tile.height, texts.height, moreButton.height) + 2 * platformStyle.paddingSmall

            property bool isText: model.kind == "text"

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
                id: texts
                anchors {
                    left: tile.right; leftMargin: platformStyle.paddingMedium
                    right: moreButton.left; rightMargin: platformStyle.paddingSmall
                    verticalCenter: parent.verticalCenter
                }

                Label {
                    platformInverted: page.platformInverted
                    width: parent.width
                    elide: Text.ElideRight
                    // Text messages: their first line.
                    text: row.isText ? model.text.split("\n")[0] : model.fileName
                }
                Label {
                    platformInverted: page.platformInverted
                    width: parent.width
                    elide: Text.ElideRight
                    font.pixelSize: platformStyle.fontSizeSmall
                    color: page.mutedColor
                    text: model.timeText + " - " + model.sizeText + " - " + model.senderAlias
                }
            }

            ToolButton {
                id: moreButton
                platformInverted: page.platformInverted
                anchors {
                    right: parent.right; rightMargin: platformStyle.paddingMedium
                    verticalCenter: parent.verticalCenter
                }
                iconSource: page.platformInverted ? "images/more-vert-inverted.svg" : "images/more-vert.svg"
                // TODO: entry menu
            }
        }
    }

    Label {
        platformInverted: page.platformInverted
        anchors.centerIn: list
        visible: history.count === 0
        color: page.mutedColor
        text: "No history"
    }

    ScrollDecorator {
        flickableItem: list
        platformInverted: page.platformInverted
    }
}
