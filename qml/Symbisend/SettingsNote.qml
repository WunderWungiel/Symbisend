import QtQuick 1.1
import com.nokia.symbian 1.1

// A wrapped note between the rows of the Settings page.
Item {
    id: root

    property alias text: label.text
    property bool warning: false
    property bool platformInverted: settings.theme == "light"

    width: parent ? parent.width : screen.width
    height: label.height + 2 * platformStyle.paddingMedium

    Text {
        id: label
        anchors {
            left: parent.left; right: parent.right
            verticalCenter: parent.verticalCenter
            leftMargin: platformStyle.paddingLarge
            rightMargin: platformStyle.paddingLarge
        }
        wrapMode: Text.WordWrap
        font {
            family: platformStyle.fontFamilyRegular
            pixelSize: platformStyle.fontSizeSmall
        }
        color: root.warning ? "#ff9800"
                            : root.platformInverted ? platformStyle.colorNormalMidInverted
                                                    : platformStyle.colorNormalMid
    }
}
