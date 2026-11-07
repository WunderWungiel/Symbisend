// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 1.1
import com.nokia.symbian 1.1

// Section title on the Settings page: accent-coloured text with extra space
// above it (separating it from the previous section) and a thin rule below.
Item {
    id: root

    property alias text: label.text
    property bool platformInverted: settings.theme == "light"

    width: parent ? parent.width : screen.width
    height: label.y + label.height + platformStyle.paddingMedium

    Text {
        id: label
        anchors {
            left: parent.left; right: parent.right
            leftMargin: platformStyle.paddingLarge
            rightMargin: platformStyle.paddingLarge
        }
        y: 2 * platformStyle.paddingLarge
        elide: Text.ElideRight
        font {
            family: platformStyle.fontFamilyRegular
            pixelSize: platformStyle.fontSizeMedium
            bold: true
        }
        color: root.platformInverted ? platformStyle.colorNormalLinkInverted
                                     : platformStyle.colorNormalLink
    }

    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 1
        color: label.color
        opacity: 0.6
    }
}
