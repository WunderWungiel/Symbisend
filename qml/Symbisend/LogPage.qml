import QtQuick 1.1
import com.nokia.symbian 1.1

Page {
    id: page

    tools: ToolBarLayout {
        ToolButton {
            iconSource: "toolbar-back"
            onClicked: page.pageStack.pop()
        }
        ToolButton {
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
            color: "#e0e0e0"
            text: logger.logText
            onHeightChanged: flick.contentY = Math.max(0, height - flick.height)
        }
    }

    ScrollDecorator { flickableItem: flick }
}
