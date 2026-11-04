import QtQuick 1.1
import com.nokia.symbian 1.1

// The favourite devices (the `favorites` model), opened with the heart
// button on the Send page. Each row shows the name and address; tapping it
// edits the favourite (FavoriteDialog), the delete button removes it after
// asking. Has its own back button at the top, so main.qml hides the toolbar.
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
            text: "Favourites"
            font.pixelSize: 26
        }
    }

    // ------------------------------------------------ Entries
    ListView {
        id: list
        anchors {
            left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom
            topMargin: platformStyle.paddingMedium
        }
        clip: true
        model: favorites

        delegate: Item {
            id: row
            width: list.width
            height: Math.max(texts.height, deleteButton.height) + 2 * platformStyle.paddingSmall

            // Press feedback; deleteButton (declared later) stays on top.
            Rectangle {
                anchors.fill: parent
                visible: rowMouse.pressed
                color: page.mutedColor
                opacity: 0.3
            }
            MouseArea {
                id: rowMouse
                anchors.fill: parent
                onClicked: favouriteDialog.openEdit(favorites.favorite(model.fingerprint))
            }

            Column {
                id: texts
                anchors {
                    left: parent.left; leftMargin: platformStyle.paddingLarge
                    right: deleteButton.left; rightMargin: platformStyle.paddingSmall
                    verticalCenter: parent.verticalCenter
                }

                Label {
                    platformInverted: page.platformInverted
                    width: parent.width
                    elide: Text.ElideRight
                    text: model.alias
                }
                Label {
                    platformInverted: page.platformInverted
                    width: parent.width
                    elide: Text.ElideRight
                    font.pixelSize: platformStyle.fontSizeSmall
                    color: page.mutedColor
                    text: model.ip + ":" + model.port
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
                onClicked: removeDialog.openFor(model.fingerprint, model.alias)
            }
        }
    }

    Label {
        platformInverted: page.platformInverted
        anchors.centerIn: list
        visible: favorites.count === 0
        color: page.mutedColor
        text: "No favourites"
    }

    ScrollDecorator {
        flickableItem: list
        platformInverted: page.platformInverted
    }

    // ------------------------------------------------ Dialogs
    FavoriteDialog { id: favouriteDialog }

    QueryDialog {
        id: removeDialog
        platformInverted: page.platformInverted

        property string fingerprint

        function openFor(fp, alias) {
            fingerprint = fp
            message = "Do you really want to remove " + alias + " from favourites?"
            open()
        }

        titleText: "Remove from favourites"
        acceptButtonText: "Delete"
        rejectButtonText: "Cancel"
        onAccepted: favorites.remove(fingerprint)
    }
}
