import QtQuick 1.1
import com.nokia.symbian 1.1

// Receive tab (home). Lives inside the TabGroup in main.qml; the toolbar
// belongs to the main page, so this page has no `tools` of its own.
Page {
    id: page

    // Everything above this y belongs to the logo + name block.
    property int contentBottom: page.height

    Column {
        id: identity
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: platformStyle.paddingLarge

        // Centered on the page; pushed up only if it would run into whatever
        // sits below it (contentBottom).
        y: Math.max(0, Math.min(Math.round((page.height - height) / 2),
                                page.contentBottom - platformStyle.paddingLarge - height))

        Image {
            id: logo
            property int size: Math.max(48, Math.min(
                Math.round(page.width * 0.55),
                page.contentBottom - 3 * platformStyle.paddingLarge - nameLabel.height - identity.spacing))

            anchors.horizontalCenter: parent.horizontalCenter
            width: size
            height: size
            sourceSize.width: size
            sourceSize.height: size
            fillMode: Image.PreserveAspectFit
            smooth: true
            source: "images/symbisend.svg"
        }

        Label {
            id: nameLabel
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(implicitWidth, page.width - 2 * platformStyle.paddingLarge)
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            font.pixelSize: platformStyle.fontSizeLarge
            text: myDevice.alias
        }
    }
}
