// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 1.1
import com.nokia.symbian 1.1

// Receive tab (home). Lives inside the TabGroup in main.qml; the toolbar
// belongs to the main page, so this page has no `tools` of its own.
Page {
    id: page

    // Light theme = the inverted Symbian style; every component follows it.
    property bool platformInverted: settings.theme == "light"

    // "Receive via link" was tapped (main.qml opens ReceiveLinkPage).
    signal linkRequested
    // The History button was tapped (main.qml opens HistoryPage).
    signal historyRequested

    // Everything above this y belongs to the logo + name block.
    property int contentBottom: linkButton.y

    Column {
        id: identity
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 2 * platformStyle.paddingLarge

        // Centered slightly above the middle of the page (at 42% of its
        // height); pushed further up only if it would run into whatever sits
        // below it (contentBottom).
        y: Math.max(0, Math.min(Math.round(page.height * 0.42 - height / 2),
                                page.contentBottom - platformStyle.paddingLarge - height))

        Image {
            id: logo
            property int size: Math.max(48, Math.min(
                Math.round(page.width * 0.55),
                page.contentBottom - 3 * platformStyle.paddingLarge - nameLabel.height - identity.spacing))

            anchors.horizontalCenter: parent.horizontalCenter
            width: size
            height: size
            sourceSize.width: size
            sourceSize.height: size
            fillMode: Image.PreserveAspectFit
            smooth: true
            source: "images/localsend-logo.svg"

            // Slow clockwise spin, one turn per 15 s, like the official app.
            // Paused (not reset) while the server is down (as there), another
            // tab is shown or the app is in the background.
            NumberAnimation on rotation {
                from: 0
                to: 360
                duration: 15000
                loops: Animation.Infinite
                running: true
                paused: !(discovery.serverRunning && page.visible && Qt.application.active)
            }
        }

        Label {
            id: nameLabel
            platformInverted: page.platformInverted
            anchors.horizontalCenter: parent.horizontalCenter
            // A fixed width, centred text. Not min(implicitWidth, …): in Qt
            // Quick 1 a Text with an explicit width reports implicitWidth
            // from its constrained layout, so a renamed alias kept the old
            // width and was drawn off-centre (or elided too early).
            width: page.width - 2 * platformStyle.paddingLarge
            horizontalAlignment: Text.AlignHCenter
            // Long names wrap between words (inside a word only if it alone
            // is too wide), at most two lines, then "…".
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            maximumLineCount: 2
            elide: Text.ElideRight
            // Big, like the official app (48 px under a 200 px logo).
            font.pixelSize: 2 * platformStyle.fontSizeLarge
            text: myDevice.alias
        }
    }

    // History of received files (HistoryPage), left of the Info button.
    ToolButton {
        id: historyButton
        platformInverted: page.platformInverted
        anchors {
            top: infoButton.top
            right: infoButton.left
        }
        iconSource: page.platformInverted ? "images/history-inverted.svg" : "images/history.svg"
        onClicked: page.historyRequested()
    }

    // Our own alias, address and port, top right.
    ToolButton {
        id: infoButton
        platformInverted: page.platformInverted
        anchors {
            top: parent.top; topMargin: 4
            right: parent.right; rightMargin: platformStyle.paddingMedium
        }
        iconSource: page.platformInverted ? "images/info-inverted.svg" : "images/info.svg"
        onClicked: infoDialog.open()
    }

    QueryDialog {
        id: infoDialog
        platformInverted: page.platformInverted
        titleText: i18n.t("receiveHistoryPage.entryActions.info") + i18n.rev
        message: i18n.rev + i18n.t("receiveTab.infoBox.alias") + " " + myDevice.alias
                 + "\n" + i18n.t("receiveTab.infoBox.ip") + " "
                 + (network.localAddress !== "" ? network.localAddress : "-")
                 + "\n" + i18n.t("receiveTab.infoBox.port") + " " + myDevice.port
        acceptButtonText: i18n.t("general.close") + i18n.rev
    }

    // "Receive via link": browsers upload to us (ReceiveLinkPage).
    Button {
        id: linkButton
        platformInverted: page.platformInverted
        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: parent.bottom
            bottomMargin: Math.max(2 * platformStyle.paddingLarge, Math.round(page.height * 0.1))
        }
        text: i18n.t("receiveTab.link") + i18n.rev
        onClicked: page.linkRequested()
    }
}
