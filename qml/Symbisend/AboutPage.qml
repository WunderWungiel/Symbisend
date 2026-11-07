// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 1.1
import com.nokia.symbian 1.1

// About Symbisend. Opened from Settings > Other.
Page {
    id: page

    // Light theme = the inverted Symbian style; every component follows it.
    property bool platformInverted: settings.theme == "light"

    tools: ToolBarLayout {
        ToolButton {
            platformInverted: page.platformInverted
            iconSource: "toolbar-back"
            onClicked: page.pageStack.pop()
        }
    }

    Column {
        anchors {
            left: parent.left; right: parent.right
            verticalCenter: parent.verticalCenter
            margins: platformStyle.paddingLarge
        }
        spacing: platformStyle.paddingLarge

        Image {
            // Smaller in landscape, so everything fits without scrolling.
            property int size: Math.min(96, Math.round(page.height / 3))

            anchors.horizontalCenter: parent.horizontalCenter
            width: size
            height: size
            sourceSize.width: size
            sourceSize.height: size
            smooth: true
            source: "images/symbisend.svg"
        }
        Label {
            platformInverted: page.platformInverted
            anchors.horizontalCenter: parent.horizontalCenter
            font.pixelSize: platformStyle.fontSizeLarge
            text: "Symbisend"
        }
        Label {
            platformInverted: page.platformInverted
            width: parent.width
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            text: i18n.t("symbisend.about.description") + i18n.rev
        }
        Label {
            platformInverted: page.platformInverted
            width: parent.width
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            text: i18n.t("aboutPage.author") + ": Wunder Wungiel" + i18n.rev
        }
    }
}
