import QtQuick 1.1
import com.nokia.symbian 1.1

// One discovered device on the Send page: device-type icon on the left,
// name on the right with protocol and system badges below it.
ListItem {
    id: root

    property string name
    property string deviceType    // LocalSend: mobile, desktop, web, headless, server
    property string protocol      // "http" / "https"
    property string system        // LocalSend deviceModel: "Linux", "Samsung", ...

    // Info button on the right; will open the device details page.
    signal infoClicked

    width: parent ? parent.width : implicitWidth
    platformInverted: settings.theme == "light"
    height: Math.max(platformStyle.graphicSizeLarge,
                     texts.height + 2 * platformStyle.paddingLarge)

    // Same theme-change workaround as SettingsItem: ListItemText drops its
    // colour binding once the row has been pressed.
    function textColor(isSubTitle) {
        if (root.platformInverted)
            return isSubTitle ? platformStyle.colorNormalMidInverted : platformStyle.colorNormalLightInverted
        return isSubTitle ? platformStyle.colorNormalMid : platformStyle.colorNormalLight
    }

    // LocalSend treats a missing/unknown device type as a desktop.
    function iconName(type) {
        if (type == "mobile" || type == "web" || type == "headless" || type == "server")
            return type
        return "desktop"
    }

    Image {
        id: icon
        anchors { left: root.paddingItem.left; verticalCenter: parent.verticalCenter }
        width: platformStyle.graphicSizeMedium
        height: platformStyle.graphicSizeMedium
        sourceSize.width: width
        sourceSize.height: height
        smooth: true
        source: "images/device-" + root.iconName(root.deviceType)
                + (root.platformInverted ? "-inverted" : "") + ".svg"
    }

    Column {
        id: texts
        anchors {
            left: icon.right
            leftMargin: platformStyle.paddingLarge
            right: infoButton.left
            rightMargin: platformStyle.paddingMedium
            verticalCenter: parent.verticalCenter
        }
        spacing: platformStyle.paddingSmall

        ListItemText {
            width: parent.width
            mode: root.mode
            role: "Title"
            platformInverted: root.platformInverted
            onPlatformInvertedChanged: color = root.textColor(false)
            elide: Text.ElideRight
            text: root.name
        }

        Row {
            spacing: platformStyle.paddingSmall

            Repeater {
                model: [root.protocol.toUpperCase(), root.system]

                Rectangle {
                    visible: modelData != ""
                    width: badgeText.paintedWidth + 2 * platformStyle.paddingMedium
                    height: badgeText.paintedHeight + platformStyle.paddingSmall
                    radius: height / 2
                    color: "transparent"
                    border.width: 1
                    border.color: badgeText.color

                    Text {
                        id: badgeText
                        anchors.centerIn: parent
                        font { family: platformStyle.fontFamilyRegular; pixelSize: platformStyle.fontSizeSmall }
                        color: root.textColor(true)
                        text: modelData
                    }
                }
            }
        }
    }

    ToolButton {
        id: infoButton
        anchors { right: root.paddingItem.right; verticalCenter: parent.verticalCenter }
        platformInverted: root.platformInverted
        iconSource: root.platformInverted ? "images/info-inverted.svg" : "images/info.svg"
        onClicked: root.infoClicked()
    }
}
