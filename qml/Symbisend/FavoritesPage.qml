import QtQuick 1.1
import com.nokia.symbian 1.1

// The favourite devices (the `favorites` model), opened with the heart
// button on the Send page. Each row shows the name and address; tapping it
// sends the selection there, like the official app. The row's buttons edit the
// favourite (FavoriteDialog) and remove it after asking; the Add button at the
// top adds a device by its address. Has its own back button at the top, so
// main.qml hides the toolbar.
Page {
    id: page

    // A row was tapped and the page is closing; main.qml passes it on to the
    // Send page, which shows the transfer.
    signal sendRequested(string fingerprint)

    // Read by main.qml: this page has its own back button.
    property bool ownBackButton: true

    // Light theme = the inverted Symbian style; every component follows it.
    property bool platformInverted: settings.theme == "light"

    property color mutedColor: platformInverted ? platformStyle.colorNormalMidInverted
                                                : platformStyle.colorNormalMid

    function sendTo(fingerprint) {
        if (selection.count === 0) {
            picker.showInfo("Select something to send first")
            return
        }
        page.pageStack.pop()
        page.sendRequested(fingerprint)
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
                right: addButton.left; verticalCenter: backButton.verticalCenter
            }
            elide: Text.ElideRight
            text: "Favourites"
            font.pixelSize: 26
        }
        ToolButton {
            id: addButton
            platformInverted: page.platformInverted
            anchors { right: parent.right; bottom: parent.bottom }
            visible: !favouriteDialog.lookingUp
            iconSource: "toolbar-add"
            onClicked: favouriteDialog.openNew()
        }
        // The new favourite's address is being asked who is there.
        BusyIndicator {
            platformInverted: page.platformInverted
            anchors.centerIn: addButton
            visible: favouriteDialog.lookingUp
            running: visible
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
        // The dialog is still in use: its fields go into the new favourite.
        enabled: !favouriteDialog.lookingUp

        delegate: Item {
            id: row
            width: list.width
            height: Math.max(texts.height, deleteButton.height) + 2 * platformStyle.paddingSmall

            // Press feedback; the buttons (declared later) stay on top.
            Rectangle {
                anchors.fill: parent
                visible: rowMouse.pressed
                color: page.mutedColor
                opacity: 0.3
            }
            MouseArea {
                id: rowMouse
                anchors.fill: parent
                onClicked: page.sendTo(model.fingerprint)
            }

            Column {
                id: texts
                anchors {
                    left: parent.left; leftMargin: platformStyle.paddingLarge
                    right: editButton.left; rightMargin: platformStyle.paddingSmall
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
                id: editButton
                platformInverted: page.platformInverted
                anchors { right: deleteButton.left; verticalCenter: parent.verticalCenter }
                iconSource: "toolbar-settings"
                onClicked: favouriteDialog.openEdit(favorites.favorite(model.fingerprint))
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
