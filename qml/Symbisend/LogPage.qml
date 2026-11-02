import QtQuick 1.1
import com.nokia.symbian 1.1

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
            text: "Clear"
            onClicked: logger.clear()
        }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        anchors.margins: 6
        contentWidth: width
        contentHeight: logLabel.height
        clip: true

        Text {
            id: logLabel
            width: flick.width
            wrapMode: Text.WrapAnywhere
            font.pixelSize: 14
            font.family: "Nokia Sans"
            color: page.platformInverted ? platformStyle.colorNormalLightInverted
                                         : platformStyle.colorNormalLight
            text: logger.logText
            onHeightChanged: flick.contentY = Math.max(0, height - flick.height)
        }
    }

    ScrollDecorator { flickableItem: flick; platformInverted: page.platformInverted }
}
