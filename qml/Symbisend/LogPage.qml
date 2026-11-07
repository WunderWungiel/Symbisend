// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 1.1
import com.nokia.symbian 1.1

// Debug log (the last ~300 lines). Opened from Settings > Other.
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
        ToolButton {
            platformInverted: page.platformInverted
            text: i18n.t("symbisend.general.clear") + i18n.rev
            onClicked: logger.clear()
        }
    }

    // One delegate per line: new lines only lay out themselves, instead of
    // the whole log being one big re-wrapped text.
    ListView {
        id: list
        anchors { fill: parent; margins: platformStyle.paddingMedium }
        // No rubber-banding past the first/last line.
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        model: logger.model

        // Follow the newest lines, unless the user scrolled up to read.
        property bool follow: true
        onMovementEnded: follow = atYEnd
        onCountChanged: if (follow) positionViewAtEnd()
        Component.onCompleted: positionViewAtEnd()

        delegate: Label {
            platformInverted: page.platformInverted
            width: list.width
            wrapMode: Text.WrapAnywhere
            font.pixelSize: platformStyle.fontSizeSmall
            text: model.line
        }
    }

    ScrollDecorator { flickableItem: list; platformInverted: page.platformInverted }
}
