// SPDX-FileCopyrightText: 2026 Wunder Wungiel
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 1.1
import com.nokia.symbian 1.1

// Asks for one line of text for a row on the Settings page. The owner checks
// what was entered:
//
//   ValueDialog {
//       id: portDialog
//       titleText: "Port"
//       digitsOnly: true
//       onEntered: settings.port = parseInt(value)
//   }
//   ...
//   onClicked: portDialog.openWith(settings.port)
CommonDialog {
    id: root

    property alias text: field.text
    property bool digitsOnly: false

    signal entered(string value)

    function openWith(value) {
        field.text = String(value)
        open()
    }

    platformInverted: settings.theme == "light"
    buttonTexts: [i18n.t("general.confirm") + i18n.rev, i18n.t("general.cancel") + i18n.rev]

    content: Item {
        width: root.platformContentMaximumWidth
        height: field.height + 2 * platformStyle.paddingLarge

        TextField {
            id: field
            platformInverted: root.platformInverted
            anchors {
                left: parent.left; right: parent.right
                verticalCenter: parent.verticalCenter
                margins: platformStyle.paddingLarge
            }
            maximumLength: 64
            inputMethodHints: root.digitsOnly ? Qt.ImhDigitsOnly
                                              : Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
        }
    }

    onButtonClicked: {
        if (index == 0)
            root.entered(field.text)
    }
}
