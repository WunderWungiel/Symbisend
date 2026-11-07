// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 1.1
import com.nokia.symbian 1.1

// History of received files and messages (the `history` model), opened with
// the History button on the Home tab. Like LocalSend's history page: Open
// folder / Clear, then every entry with its type icon, name (or the
// message), date, size and sender. Has its own back button at the top, so
// main.qml hides the toolbar. Tapping a file opens it in the phone's app for
// it; tapping a text message shows it in a dialog. The three dots button of an
// entry opens a menu: Open file, Information, Delete from history.
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
            text: i18n.t("receiveHistoryPage.title") + i18n.rev
            font.pixelSize: 26
        }
    }

    // ------------------------------------------------ Open folder / Clear
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
            text: i18n.t("receiveHistoryPage.openFolder") + i18n.rev
            onClicked: {
                if (!history.openFolder(settings.downloadFolder))
                    openFolderFailedDialog.open()
            }
        }
        ToolButton {
            platformInverted: page.platformInverted
            width: Math.min(implicitWidth, buttonRow.maxButtonWidth)
            anchors { left: openFolderButton.right; leftMargin: buttonRow.spacing }
            iconSource: "toolbar-delete"
            text: i18n.t("receiveHistoryPage.deleteHistory") + i18n.rev
            enabled: history.count > 0
            onClicked: clearDialog.open()
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

            function openEntry() {
                if (isText)
                    textDialog.openWith(model.text, model.senderAlias)
                else if (!history.openFile(index))
                    openFailedDialog.openFor(model.fileName)
            }

            function showInformation() {
                var lines = []
                if (!isText) {
                    lines.push(i18n.t("dialogs.fileInfo.fileName") + " " + model.fileName)
                    lines.push(i18n.t("dialogs.fileInfo.path") + " " + model.path)
                }
                lines.push(i18n.t("dialogs.fileInfo.size") + " " + model.sizeText)
                lines.push(i18n.t("dialogs.fileInfo.sender") + " " + model.senderAlias)
                lines.push(i18n.t("dialogs.fileInfo.time") + " " + model.timeText)
                infoDialog.openWith(isText ? i18n.t("receiveHistoryPage.entryActions.info") : i18n.t("dialogs.fileInfo.title"),
                                    lines.join("\n"))
            }

            // Press feedback; moreButton (declared later) stays on top.
            Rectangle {
                anchors.fill: parent
                visible: rowMouse.pressed
                color: page.mutedColor
                opacity: 0.3
            }
            MouseArea {
                id: rowMouse
                anchors.fill: parent
                onClicked: row.openEntry()
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
                    // Text messages: their first line, shortened (made in C++,
                    // so long messages aren't split or laid out here).
                    text: model.preview
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
                onClicked: {
                    // The menu acts on list.currentIndex/currentItem: ListView
                    // keeps them on this entry if a new one arrives meanwhile.
                    list.currentIndex = index
                    entryMenu.open()
                }
            }
        }
    }

    Label {
        platformInverted: page.platformInverted
        anchors.centerIn: list
        visible: history.count === 0
        color: page.mutedColor
        text: i18n.t("receiveHistoryPage.empty") + i18n.rev
    }

    // ------------------------------------------------ Entry menu
    ContextMenu {
        id: entryMenu
        platformInverted: page.platformInverted
        MenuLayout {
            MenuItem {
                platformInverted: page.platformInverted
                text: i18n.t("receiveHistoryPage.entryActions.open") + i18n.rev
                // Tapping a message shows it; it has no file.
                visible: list.currentItem != null && !list.currentItem.isText
                onClicked: list.currentItem.openEntry()
            }
            MenuItem {
                platformInverted: page.platformInverted
                text: i18n.t("receiveHistoryPage.entryActions.info") + i18n.rev
                onClicked: list.currentItem.showInformation()
            }
            MenuItem {
                platformInverted: page.platformInverted
                text: i18n.t("receiveHistoryPage.entryActions.deleteFromHistory") + i18n.rev
                onClicked: history.removeEntry(list.currentIndex)
            }
        }
    }

    // ------------------------------------------------ Dialogs
    // Not a QueryDialog: its message breaks lines only between words (a long
    // path would be cut off) and would treat a sender's name as rich text.
    CommonDialog {
        id: infoDialog
        platformInverted: page.platformInverted
        buttonTexts: [i18n.t("general.close") + i18n.rev]

        function openWith(title, text) {
            titleText = title
            infoLabel.text = text
            open()
        }

        content: Item {
            width: infoDialog.platformContentMaximumWidth
            height: Math.min(infoLabel.paintedHeight + 2 * platformStyle.paddingLarge,
                             infoDialog.platformContentMaximumHeight)

            Flickable {
                id: infoFlickable
                anchors { fill: parent; margins: platformStyle.paddingLarge }
                contentHeight: infoLabel.paintedHeight
                flickableDirection: Flickable.VerticalFlick
                interactive: contentHeight > height
                clip: true

                Label {
                    id: infoLabel
                    platformInverted: page.platformInverted
                    width: infoFlickable.width
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }
            }
        }
    }

    // A received text message, in full, in an editable field (like the
    // Device name dialog in Settings) so it can be selected and copied.
    // Edits are not saved.
    CommonDialog {
        id: textDialog
        platformInverted: page.platformInverted
        buttonTexts: [i18n.t("general.close") + i18n.rev]

        function openWith(text, senderAlias) {
            titleText = senderAlias
            textArea.text = text
            open()
        }

        content: Item {
            width: textDialog.platformContentMaximumWidth
            height: textArea.height + 2 * platformStyle.paddingLarge

            TextArea {
                id: textArea
                platformInverted: page.platformInverted
                anchors {
                    left: parent.left; right: parent.right
                    verticalCenter: parent.verticalCenter
                    margins: platformStyle.paddingLarge
                }
                // Grows with the text, up to what fits; scrolls beyond that.
                height: Math.min(implicitHeight, Math.round(page.height * 0.5))
            }
        }
    }

    QueryDialog {
        id: openFailedDialog
        platformInverted: page.platformInverted
        property string fileName: ""
        function openFor(name) { fileName = name; open() }
        titleText: i18n.t("dialogs.cannotOpenFile.title") + i18n.rev
        message: i18n.t("dialogs.cannotOpenFile.content", { file: fileName }) + i18n.rev
        acceptButtonText: i18n.t("general.close") + i18n.rev
    }

    QueryDialog {
        id: openFolderFailedDialog
        platformInverted: page.platformInverted
        titleText: i18n.t("symbisend.history.cannotOpenFolder") + i18n.rev
        message: i18n.t("symbisend.history.cannotOpenFolderContent", { folder: settings.downloadFolder }) + i18n.rev
        acceptButtonText: i18n.t("general.close") + i18n.rev
    }

    // Like the official app: confirm first. Only the list is cleared, the
    // received files stay.
    QueryDialog {
        id: clearDialog
        platformInverted: page.platformInverted
        titleText: i18n.t("dialogs.historyClearDialog.title") + i18n.rev
        message: i18n.t("symbisend.history.clearContent") + i18n.rev
        acceptButtonText: i18n.t("general.delete") + i18n.rev
        rejectButtonText: i18n.t("general.cancel") + i18n.rev
        onAccepted: history.clear()
    }

    ScrollDecorator {
        flickableItem: list
        platformInverted: page.platformInverted
    }
}
