import QtQuick 1.1
import com.nokia.symbian 1.1

// Section heading on the Settings page.
ListHeading {
    id: root

    property alias text: label.text

    width: parent ? parent.width : implicitWidth
    platformInverted: settings.theme == "light"

    ListItemText {
        id: label
        anchors.fill: root.paddingItem
        role: "Heading"
        platformInverted: root.platformInverted
    }
}
