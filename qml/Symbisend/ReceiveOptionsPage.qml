// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 1.1
import com.nokia.symbian 1.1

// LocalSend's receive options, opened from the offer in ReceiveDialog: where
// this transfer goes, and which files to take under what name. Everything
// lives in `receiver` and `receiveFiles`, so Accept in the dialog sees it. Has
// its own back button at the top, so main.qml hides the toolbar. Leaves by
// itself when the offer ends (the sender withdrew it, the connection dropped);
// finished() tells main.qml to bring the dialog back.
Page {
    id: page

    // Read by main.qml: this page has its own back button.
    property bool ownBackButton: true

    // Light theme = the inverted Symbian style; every component follows it.
    property bool platformInverted: settings.theme == "light"

    property color mutedColor: platformInverted ? platformStyle.colorNormalMidInverted
                                                : platformStyle.colorNormalMid
    property color textColor: platformInverted ? platformStyle.colorNormalLightInverted
                                               : platformStyle.colorNormalLight

    signal finished()

    function leave() {
        nameDialog.close()
        page.pageStack.pop()
        finished()
    }

    // Also when the page arrives: the offer may have ended on the way here.
    function checkOffer() {
        if (receiver.state != "request" && status == PageStatus.Active)
            leave()
    }

    onStatusChanged: checkOffer()

    Connections {
        target: receiver
        onStateChanged: page.checkOffer()
    }

    FileNameDialog { id: nameDialog }

    // ------------------------------------------------ Header
    Item {
        id: titleBar
        anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: 8; rightMargin: 8 }
        height: backButton.height + 8

        ToolButton {
            id: backButton
            platformInverted: page.platformInverted
            anchors { left: parent.left; bottom: parent.bottom }
            iconSource: "toolbar-back"
            onClicked: page.leave()
        }
        Label {
            platformInverted: page.platformInverted
            anchors {
                left: backButton.right; leftMargin: platformStyle.paddingMedium
                right: parent.right; verticalCenter: backButton.verticalCenter
            }
            elide: Text.ElideRight
            text: "Options"
            font.pixelSize: 26
        }
    }

    // ------------------------------------------------ Folder, gallery, files
    ListView {
        id: list
        anchors { left: parent.left; right: parent.right; top: titleBar.bottom; bottom: parent.bottom }
        clip: true
        // A pushed page has no size yet when it is created, and a ListView that
        // fills up then never makes room for its header.
        model: height > 0 ? receiveFiles : null

        header: Column {
            width: list.width
            spacing: platformStyle.paddingMedium

            Item {
                width: parent.width
                height: Math.max(folderTexts.height, folderButton.height)

                Column {
                    id: folderTexts
                    anchors {
                        left: parent.left; leftMargin: platformStyle.paddingLarge
                        right: folderButton.left; rightMargin: platformStyle.paddingMedium
                        verticalCenter: parent.verticalCenter
                    }
                    spacing: platformStyle.paddingSmall

                    Label {
                        platformInverted: page.platformInverted
                        width: parent.width
                        elide: Text.ElideRight
                        font.pixelSize: platformStyle.fontSizeLarge
                        text: "Save to folder"
                    }
                    Label {
                        platformInverted: page.platformInverted
                        width: parent.width
                        wrapMode: Text.WrapAnywhere
                        font.pixelSize: platformStyle.fontSizeSmall
                        color: page.mutedColor
                        text: receiver.folder
                    }
                }
                ToolButton {
                    id: folderButton
                    platformInverted: page.platformInverted
                    anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                    iconSource: page.platformInverted ? "images/edit-inverted.svg" : "images/edit.svg"
                    onClicked: receiver.pickFolder()
                }
            }

            SettingsSwitch {
                title: "Save media to gallery"
                checked: receiver.saveToGallery
                onToggled: receiver.saveToGallery = value
            }
            Label {
                platformInverted: page.platformInverted
                anchors {
                    left: parent.left; right: parent.right
                    leftMargin: platformStyle.paddingLarge; rightMargin: platformStyle.paddingLarge
                }
                visible: receiver.hasFolders && !receiver.saveToGallery
                wrapMode: Text.Wrap
                font.pixelSize: platformStyle.fontSizeSmall
                color: page.mutedColor
                text: "Turned off automatically because there are folders."
            }

            Item {
                width: parent.width
                height: undoButton.height

                Label {
                    platformInverted: page.platformInverted
                    anchors {
                        left: parent.left; leftMargin: platformStyle.paddingLarge
                        right: undoButton.left; rightMargin: platformStyle.paddingMedium
                        verticalCenter: parent.verticalCenter
                    }
                    elide: Text.ElideRight
                    font.pixelSize: platformStyle.fontSizeLarge
                    text: "Files"
                }
                ToolButton {
                    id: undoButton
                    platformInverted: page.platformInverted
                    anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                    iconSource: page.platformInverted ? "images/undo-inverted.svg" : "images/undo.svg"
                    onClicked: receiveFiles.reset()
                }
            }
        }

        delegate: Item {
            id: fileRow

            property bool isSelected: selected
            // The CheckBox flips its own `checked` when tapped, which breaks a
            // plain binding; push every change to it instead.
            onIsSelectedChanged: fileCheck.checked = isSelected

            width: list.width
            height: Math.max(fileIcon.height, fileTexts.height, editButton.height, fileCheck.height)
                    + platformStyle.paddingSmall

            Image {
                id: fileIcon
                anchors {
                    left: parent.left; leftMargin: platformStyle.paddingLarge
                    verticalCenter: parent.verticalCenter
                }
                width: 32
                height: 32
                sourceSize { width: 32; height: 32 }
                source: "images/" + (kind == "image" ? "send-media" : kind == "video" ? "send-video" : "send-file")
                        + (page.platformInverted ? "-inverted.svg" : ".svg")
            }
            Column {
                id: fileTexts
                anchors {
                    left: fileIcon.right; leftMargin: platformStyle.paddingMedium
                    right: editButton.left; rightMargin: platformStyle.paddingSmall
                    verticalCenter: parent.verticalCenter
                }

                Label {
                    platformInverted: page.platformInverted
                    width: parent.width
                    elide: Text.ElideMiddle
                    text: name
                }
                Label {
                    platformInverted: page.platformInverted
                    width: parent.width
                    elide: Text.ElideRight
                    font.pixelSize: platformStyle.fontSizeSmall
                    color: !selected ? page.mutedColor : renamed ? "#ff9800" : page.textColor
                    text: (!selected ? "Skipped" : renamed ? "Renamed" : "Unchanged") + " - " + sizeText
                }
            }
            ToolButton {
                id: editButton
                platformInverted: page.platformInverted
                anchors { right: fileCheck.left; verticalCenter: parent.verticalCenter }
                iconSource: page.platformInverted ? "images/edit-inverted.svg" : "images/edit.svg"
                enabled: selected
                onClicked: nameDialog.openFor(index, originalName, editName)
            }
            CheckBox {
                id: fileCheck
                platformInverted: page.platformInverted
                anchors { right: parent.right; rightMargin: platformStyle.paddingMedium; verticalCenter: parent.verticalCenter }
                checked: fileRow.isSelected
                onClicked: receiveFiles.setSelected(index, checked)
            }
        }
    }

    ScrollDecorator {
        platformInverted: page.platformInverted
        flickableItem: list
    }
}
