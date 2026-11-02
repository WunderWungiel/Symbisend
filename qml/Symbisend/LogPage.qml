import QtQuick 1.1
import com.nokia.symbian 1.1

// Debug log (the last ~300 lines). Opened from Settings > Other.
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
        anchors { fill: parent; margins: platformStyle.paddingMedium }
        contentWidth: width
        contentHeight: logLabel.height
        // Vertical only, and no rubber-banding past the first/last line.
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Label {
            id: logLabel
            platformInverted: page.platformInverted
            width: flick.width
            wrapMode: Text.WrapAnywhere
            font.pixelSize: platformStyle.fontSizeSmall
            text: logger.logText
            // Follow the newest lines.
            onHeightChanged: flick.contentY = Math.max(0, height - flick.height)
        }
    }

    ScrollDecorator { flickableItem: flick; platformInverted: page.platformInverted }
}
