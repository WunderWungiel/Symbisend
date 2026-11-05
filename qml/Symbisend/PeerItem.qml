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
    property bool favorite: false // shows a check mark after the name

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

        Item {
            width: parent.width
            height: nameText.height

            // Measures the whole name: nameText's own size is the elided one.
            Text {
                id: nameMeasure
                visible: false
                font: nameText.font
                text: root.name
            }
            ListItemText {
                id: nameText
                // As wide as the name, so that the mark comes right after it.
                width: Math.min(Math.ceil(nameMeasure.paintedWidth),
                                parent.width - (root.favorite ? favoriteMark.width + platformStyle.paddingSmall : 0))
                mode: root.mode
                role: "Title"
                platformInverted: root.platformInverted
                onPlatformInvertedChanged: color = root.textColor(false)
                elide: Text.ElideRight
                text: root.name
            }
            Image {
                id: favoriteMark
                anchors {
                    left: nameText.right; leftMargin: platformStyle.paddingSmall
                    verticalCenter: nameText.verticalCenter
                }
                visible: root.favorite
                width: platformStyle.graphicSizeTiny
                height: platformStyle.graphicSizeTiny
                sourceSize.width: width
                sourceSize.height: height
                smooth: true
                source: root.platformInverted ? "images/check-circle-inverted.svg"
                                              : "images/check-circle.svg"
            }
        }

        Row {
            spacing: platformStyle.paddingSmall

            Repeater {
                model: [root.protocol.toUpperCase(), root.system]

                Rectangle {
                    visible: modelData != ""
                    width: badgeText.paintedWidth + 2 * platformStyle.paddingMedium
                    height: badgeText.paintedHeight + platformStyle.paddingSmall
                    // Filled chip without a border, like LocalSend's DeviceBadge:
                    // a 1px outline with a pill radius renders smeared on
                    // low-DPI Symbian^3 screens, a solid fill stays crisp.
                    radius: 3
                    color: root.platformInverted ? "#1f000000" : "#33ffffff"

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
