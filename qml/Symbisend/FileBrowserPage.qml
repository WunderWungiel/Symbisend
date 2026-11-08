// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 1.1
import com.nokia.symbian 1.1

// The Send page's File button: browse the drives and check any number of
// files and folders, then Add puts them into the selection. Symbian has no
// native multi-select file picker. The check marks live in C++
// (`fileBrowser`) and stay while moving between folders. Has its own back
// button at the top (one folder up, or closes the page on the drive list),
// so main.qml hides the toolbar.
Page {
    id: page

    // Read by main.qml: this page has its own back button.
    property bool ownBackButton: true

    // Light theme = the inverted Symbian style; every component follows it.
    property bool platformInverted: settings.theme == "light"

    property color mutedColor: platformInverted ? platformStyle.colorNormalMidInverted
                                                : platformStyle.colorNormalMid

    // The list is read again each time the page is opened (files may have
    // changed meanwhile); the folder is the one left last time.
    onStatusChanged: {
        if (status == PageStatus.Activating)
            fileBrowser.refresh()
    }

    function back() {
        if (fileBrowser.atRoot) {
            fileBrowser.clearChecked()
            page.pageStack.pop()
            return
        }
        var from = fileBrowser.up()
        if (from >= 0)
            list.positionViewAtIndex(from, ListView.Center)
    }

    function addChecked() {
        selection.addFiles(fileBrowser.checkedFiles())
        var folders = fileBrowser.checkedFolders()
        for (var i = 0; i < folders.length; ++i)
            selection.addFolder(folders[i])
        fileBrowser.clearChecked()
        page.pageStack.pop()
    }

    Connections {
        target: fileBrowser
        onOpenFailed: picker.showInfo(i18n.t("symbisend.history.cannotOpenFolder") + i18n.rev)
        onFolderChanged: list.positionViewAtBeginning()
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
            onClicked: page.back()
        }
        Label {
            platformInverted: page.platformInverted
            anchors {
                left: backButton.right; leftMargin: platformStyle.paddingMedium
                right: addButton.left; rightMargin: platformStyle.paddingMedium
                verticalCenter: backButton.verticalCenter
            }
            // Deep paths: the end (the current folder) is what matters.
            elide: fileBrowser.atRoot ? Text.ElideRight : Text.ElideLeft
            text: fileBrowser.atRoot ? i18n.t("symbisend.picker.selectFiles") + i18n.rev
                                     : fileBrowser.folderText
            font.pixelSize: 26
        }
        ToolButton {
            id: addButton
            platformInverted: page.platformInverted
            anchors { right: parent.right; verticalCenter: backButton.verticalCenter }
            enabled: fileBrowser.checkedCount > 0
            text: (fileBrowser.checkedCount > 0
                   ? i18n.t("general.add") + " (" + fileBrowser.checkedCount + ")"
                   : i18n.t("general.add")) + i18n.rev
            onClicked: page.addChecked()
        }
    }

    // ------------------------------------------------ Summary of what is checked
    Label {
        id: summary
        platformInverted: page.platformInverted
        anchors {
            left: parent.left; right: parent.right; top: header.bottom
            leftMargin: platformStyle.paddingLarge; rightMargin: platformStyle.paddingLarge
            topMargin: platformStyle.paddingSmall
        }
        height: visible ? implicitHeight : 0
        visible: fileBrowser.checkedCount > 0
        elide: Text.ElideRight
        font.pixelSize: platformStyle.fontSizeSmall
        color: page.mutedColor
        text: i18n.t("sendTab.selection.files", { files: fileBrowser.checkedCount }) + " · "
              + i18n.t("sendTab.selection.size", { size: fileBrowser.checkedSizeText }) + i18n.rev
    }

    // ------------------------------------------------ Entries
    ListView {
        id: list
        anchors {
            left: parent.left; right: parent.right; top: summary.bottom; bottom: parent.bottom
            topMargin: platformStyle.paddingSmall
        }
        clip: true
        model: fileBrowser

        delegate: ListItem {
            id: row
            width: list.width
            platformInverted: page.platformInverted
            height: Math.max(platformStyle.graphicSizeLarge,
                             texts.height + 2 * platformStyle.paddingLarge)
            subItemIndicator: model.isDir

            // ListItemText drops its colour binding once the row has been
            // pressed (see PeerItem).
            function textColor(isSubTitle) {
                if (row.platformInverted)
                    return isSubTitle ? platformStyle.colorNormalMidInverted : platformStyle.colorNormalLightInverted
                return isSubTitle ? platformStyle.colorNormalMid : platformStyle.colorNormalLight
            }

            // Folders open, files toggle their check mark.
            onClicked: {
                if (model.isDir)
                    fileBrowser.open(index)
                else
                    fileBrowser.toggle(index)
            }

            Image {
                id: icon
                anchors { left: row.paddingItem.left; verticalCenter: parent.verticalCenter }
                width: platformStyle.graphicSizeMedium
                height: platformStyle.graphicSizeMedium
                sourceSize.width: width
                sourceSize.height: height
                smooth: true
                source: "images/send-" + (model.isDir ? "folder" : "file")
                        + (row.platformInverted ? "-inverted" : "") + ".svg"
            }

            Column {
                id: texts
                anchors {
                    left: icon.right; leftMargin: platformStyle.paddingLarge
                    right: checkBox.visible ? checkBox.left : row.paddingItem.right
                    rightMargin: platformStyle.paddingMedium
                    verticalCenter: parent.verticalCenter
                }
                spacing: platformStyle.paddingSmall

                ListItemText {
                    width: parent.width
                    mode: row.mode
                    role: "Title"
                    platformInverted: row.platformInverted
                    onPlatformInvertedChanged: color = row.textColor(false)
                    elide: Text.ElideRight
                    text: model.name
                }
                ListItemText {
                    width: parent.width
                    visible: text != ""
                    mode: row.mode
                    role: "SubTitle"
                    platformInverted: row.platformInverted
                    onPlatformInvertedChanged: color = row.textColor(true)
                    elide: Text.ElideRight
                    text: model.isDir ? model.modifiedText
                                      : model.sizeText + (model.modifiedText != "" ? " · " + model.modifiedText : "")
                }
            }

            // Shows the model's state only: the MouseArea on top keeps
            // CheckBox from toggling itself (that would break the binding).
            CheckBox {
                id: checkBox
                // paddingItem already leaves room for a folder's arrow.
                anchors { right: row.paddingItem.right; verticalCenter: parent.verticalCenter }
                platformInverted: row.platformInverted
                visible: !model.isDrive
                checked: model.checked

                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -platformStyle.paddingMedium
                    onClicked: fileBrowser.toggle(index)
                }
            }
        }
    }

    ScrollDecorator {
        flickableItem: list
        platformInverted: page.platformInverted
    }

    Label {
        platformInverted: page.platformInverted
        anchors.centerIn: list
        visible: list.count === 0
        color: page.mutedColor
        text: i18n.t("assetPicker.emptyList") + i18n.rev
    }
}
