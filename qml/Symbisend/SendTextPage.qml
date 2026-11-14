// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 1.1
import com.nokia.symbian 1.1

// Text entry for a text message, pushed by main.qml (Send tab > Text: new)
// or SelectionPage (tapping a text message: edit). A page instead of a
// CommonDialog: copy/paste in a TextArea inside a dialog is broken before
// Belle FP2. Confirm adds it to / updates it in `selection`, back discards.
Page {
    id: page

    // Index of the text item being edited in `selection`, -1 for a new one.
    property int editIndex: -1
    // Initial text, set when pushed.
    property string initialText: ""

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
            text: i18n.t("general.confirm") + i18n.rev
            onClicked: page.confirm()
        }
    }

    function confirm() {
        if (editIndex < 0)
            selection.addText(textArea.text) // empty text is ignored
        else
            selection.setText(editIndex, textArea.text) // empty text removes it
        page.pageStack.pop()
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
        text: page.editIndex < 0 ? i18n.t("dialogs.messageInput.title") + i18n.rev
                                 : i18n.t("symbisend.send.editText") + i18n.rev
    }

    TextArea {
        id: textArea
        platformInverted: page.platformInverted
        anchors {
            left: parent.left; right: parent.right; top: title.bottom; bottom: parent.bottom
            margins: platformStyle.paddingLarge
        }
        text: page.initialText
        placeholderText: i18n.t("dialogs.messageInput.title") + i18n.rev
    }
}
