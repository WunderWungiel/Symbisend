// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 1.1
import com.nokia.symbian 1.1

// A received text message, in full, pushed by HistoryPage (tapping a text
// entry). An editable field so it can be selected and copied; edits are not
// saved. A page instead of a CommonDialog: copy/paste in a TextArea inside a
// dialog is broken before Belle FP2 (see SendTextPage).
Page {
    id: page

    // Set when pushed.
    property string senderAlias: ""
    property string text: ""

    // Light theme = the inverted Symbian style; every component follows it.
    property bool platformInverted: settings.theme == "light"

    tools: ToolBarLayout {
        ToolButton {
            platformInverted: page.platformInverted
            iconSource: "toolbar-back"
            onClicked: page.pageStack.pop()
        }
    }

    Label {
        id: title
        platformInverted: page.platformInverted
        anchors {
            left: parent.left; right: parent.right; top: parent.top
            margins: platformStyle.paddingLarge
        }
        elide: Text.ElideRight
        font.pixelSize: 26
        text: page.senderAlias
    }

    TextArea {
        id: textArea
        platformInverted: page.platformInverted
        anchors {
            left: parent.left; right: parent.right; top: title.bottom; bottom: parent.bottom
            margins: platformStyle.paddingLarge
        }
        text: page.text
    }
}
