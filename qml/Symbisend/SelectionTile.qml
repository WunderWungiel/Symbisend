import QtQuick 1.1
import com.nokia.symbian 1.1

// A square tile with the icon of one selected item (file, image, video or
// text), used by the Selection card and the Selection edit page.
Rectangle {
    id: root

    // selection's "kind" role: "file", "image", "video" or "text".
    property string kind
    property bool platformInverted: settings.theme == "light"

    width: platformStyle.graphicSizeLarge
    height: width
    radius: 3
    color: platformInverted ? "#1f000000" : "#26ffffff"

    Image {
        anchors.centerIn: parent
        width: Math.round(parent.width * 0.55)
        height: width
        sourceSize.width: width
        sourceSize.height: height
        smooth: true
        source: {
            var name = root.kind == "image" ? "send-media"
                     : root.kind == "video" ? "send-video"
                     : root.kind == "text" ? "send-text"
                     : "send-file"
            return "images/" + name + (root.platformInverted ? "-inverted" : "") + ".svg"
        }
    }
}
