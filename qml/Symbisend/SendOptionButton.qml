import QtQuick 1.1
import com.nokia.symbian 1.1

// One of the "what to send" buttons on the Send page (File, Media, ...):
// a framed button with an icon above a short label, like LocalSend's
// selection cards.
Button {
    id: root

    property string label
    // Base name of a white/black SVG pair in images/ ("send-file" ->
    // images/send-file.svg and images/send-file-inverted.svg).
    property string icon

    platformInverted: settings.theme == "light"
    height: content.height + 2 * platformStyle.paddingMedium

    Column {
        id: content
        anchors.centerIn: parent
        width: parent.width - 2 * platformStyle.paddingSmall
        // The Button frame greys out by itself; dim the icon and label too.
        opacity: root.enabled ? 1.0 : 0.4
        spacing: platformStyle.paddingSmall

        Image {
            anchors.horizontalCenter: parent.horizontalCenter
            width: platformStyle.graphicSizeSmall
            height: platformStyle.graphicSizeSmall
            sourceSize.width: width
            sourceSize.height: height
            smooth: true
            source: "images/" + root.icon + (root.platformInverted ? "-inverted" : "") + ".svg"
        }

        Label {
            platformInverted: root.platformInverted
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            font.pixelSize: platformStyle.fontSizeSmall
            text: root.label
        }
    }
}
