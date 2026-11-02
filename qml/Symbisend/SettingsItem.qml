import QtQuick 1.1
import com.nokia.symbian 1.1

// One row on the Settings page: a title, an optional value/description
// below it, and optional controls (buttons, a switch, ...) on the right.
// Child items go into the controls row:
//
//   SettingsItem {
//       title: "Server"; subTitle: "Running"
//       Button { text: "Stop" }
//   }
ListItem {
    id: root

    property string title
    property string subTitle
    default property alias controls: controlRow.children

    width: parent ? parent.width : implicitWidth
    height: Math.max(platformStyle.graphicSizeLarge,
                     texts.height + 2 * platformStyle.paddingMedium,
                     controlRow.height + 2 * platformStyle.paddingMedium)

    Column {
        id: texts
        anchors {
            left: root.paddingItem.left
            right: controlRow.width > 0 ? controlRow.left : root.paddingItem.right
            rightMargin: controlRow.width > 0 ? platformStyle.paddingMedium : 0
            verticalCenter: parent.verticalCenter
        }

        ListItemText {
            width: parent.width
            mode: root.mode
            role: "Title"
            platformInverted: root.platformInverted
            elide: Text.ElideRight
            text: root.title
        }
        ListItemText {
            width: parent.width
            visible: text != ""
            mode: root.mode
            role: "SubTitle"
            platformInverted: root.platformInverted
            elide: Text.ElideRight
            text: root.subTitle
        }
    }

    Row {
        id: controlRow
        anchors { right: root.paddingItem.right; verticalCenter: parent.verticalCenter }
        spacing: platformStyle.paddingMedium
    }
}
