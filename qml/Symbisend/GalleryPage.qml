// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 1.1
import com.nokia.symbian 1.1

// The Send page's Media button: the phone's photos and videos, newest
// first; tap to check any number, then Add puts them into the selection.
// Our own grid rather than the system gallery picker, which also listed
// caches and thumbnail folders (`gallery` leaves those out). Has its own
// back button at the top, so main.qml hides the toolbar.
Page {
    id: page

    // Read by main.qml: this page has its own back button.
    property bool ownBackButton: true

    // Light theme = the inverted Symbian style; every component follows it.
    property bool platformInverted: settings.theme == "light"

    property color mutedColor: platformInverted ? platformStyle.colorNormalMidInverted
                                                : platformStyle.colorNormalMid

    // Listed again each time the page is opened (new photos); thumbnails
    // are made in the background only while it is open.
    onStatusChanged: {
        if (status == PageStatus.Activating)
            gallery.load()
        else if (status == PageStatus.Deactivating)
            gallery.close()
    }

    function back() {
        gallery.clearChecked()
        page.pageStack.pop()
    }

    function addChecked() {
        selection.addFiles(gallery.checkedFiles())
        gallery.clearChecked()
        page.pageStack.pop()
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
            elide: Text.ElideRight
            text: i18n.t("sendTab.picker.media") + i18n.rev
            font.pixelSize: 26
        }
        ToolButton {
            id: addButton
            platformInverted: page.platformInverted
            anchors { right: parent.right; verticalCenter: backButton.verticalCenter }
            enabled: gallery.checkedCount > 0
            text: (gallery.checkedCount > 0
                   ? i18n.t("general.add") + " (" + gallery.checkedCount + ")"
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
        visible: gallery.checkedCount > 0
        elide: Text.ElideRight
        font.pixelSize: platformStyle.fontSizeSmall
        color: page.mutedColor
        text: i18n.t("sendTab.selection.files", { files: gallery.checkedCount }) + " · "
              + i18n.t("sendTab.selection.size", { size: gallery.checkedSizeText }) + i18n.rev
    }

    // ------------------------------------------------ Grid
    GridView {
        id: grid
        anchors {
            left: parent.left; right: parent.right; top: summary.bottom; bottom: parent.bottom
            topMargin: platformStyle.paddingSmall
        }
        clip: true
        model: gallery
        // The rows just below the screen start loading early.
        cacheBuffer: cellHeight * 2

        // About 90 px tiles: 4 across in portrait on a 360 px screen.
        property int columns: Math.max(3, Math.floor(width / 90))
        cellWidth: Math.floor(width / columns)
        cellHeight: cellWidth

        delegate: Item {
            width: grid.cellWidth
            height: grid.cellHeight

            Rectangle {
                id: tile
                anchors { fill: parent; margins: 1 }
                color: page.platformInverted ? "#d8d8d8" : "#262626"
                clip: true

                Image {
                    anchors.fill: parent
                    asynchronous: true
                    fillMode: Image.PreserveAspectCrop
                    sourceSize.width: tile.width
                    sourceSize.height: tile.height
                    source: model.thumbnail
                }

                Image {
                    anchors.centerIn: parent
                    visible: model.isVideo
                    width: platformStyle.graphicSizeMedium
                    height: platformStyle.graphicSizeMedium
                    sourceSize.width: width
                    sourceSize.height: height
                    smooth: true
                    source: "images/send-video" + (page.platformInverted ? "-inverted" : "") + ".svg"
                }

                // Videos: the file name, as there is no thumbnail.
                Text {
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 2 }
                    visible: model.isVideo
                    elide: Text.ElideMiddle
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: platformStyle.fontSizeSmall - 2
                    color: page.mutedColor
                    text: model.name
                }

                Rectangle {
                    anchors.fill: parent
                    visible: model.checked
                    color: "black"
                    opacity: 0.45
                }
                Image {
                    anchors { top: parent.top; right: parent.right; margins: 4 }
                    visible: model.checked
                    width: platformStyle.graphicSizeSmall
                    height: platformStyle.graphicSizeSmall
                    sourceSize.width: width
                    sourceSize.height: height
                    smooth: true
                    source: "images/check-circle.svg"
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: gallery.toggle(index)
            }
        }
    }

    ScrollDecorator {
        flickableItem: grid
        platformInverted: page.platformInverted
    }

    BusyIndicator {
        anchors.centerIn: grid
        platformInverted: page.platformInverted
        visible: gallery.loading && grid.count === 0
        running: visible
        width: platformStyle.graphicSizeLarge
        height: width
    }

    Label {
        platformInverted: page.platformInverted
        anchors.centerIn: grid
        visible: !gallery.loading && grid.count === 0
        color: page.mutedColor
        text: i18n.t("assetPicker.emptyList") + i18n.rev
    }
}
