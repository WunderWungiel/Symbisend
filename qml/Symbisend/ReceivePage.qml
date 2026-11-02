import QtQuick 1.1
import com.nokia.symbian 1.1

// Receive tab (home). Lives inside the TabGroup in main.qml; the toolbar
// belongs to the main page, so this page has no `tools` of its own.
Page {
    id: page

    // Light theme = the inverted Symbian style; every component follows it.
    property bool platformInverted: settings.theme == "light"

    // Everything above this y belongs to the logo + name block.
    property int contentBottom: linkButton.y

    Column {
        id: identity
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: platformStyle.paddingLarge

        // Centered slightly above the middle of the page (at 42% of its
        // height); pushed further up only if it would run into whatever sits
        // below it (contentBottom).
        y: Math.max(0, Math.min(Math.round(page.height * 0.42 - height / 2),
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

            // Slow clockwise spin, one turn per 20 s. Paused (not reset) while
            // another tab is shown or the app is in the background.
            NumberAnimation on rotation {
                from: 0
                to: 360
                duration: 20000
                loops: Animation.Infinite
                running: true
                paused: !(page.visible && Qt.application.active)
            }
        }

        Label {
            id: nameLabel
            platformInverted: page.platformInverted
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(implicitWidth, page.width - 2 * platformStyle.paddingLarge)
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            font.pixelSize: platformStyle.fontSizeLarge
            text: myDevice.alias
        }
    }

    // "Receive via link" - not implemented yet (LocalSend's web share
    // receive). Tapping it only logs.
    Button {
        id: linkButton
        platformInverted: page.platformInverted
        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: parent.bottom
            bottomMargin: Math.max(2 * platformStyle.paddingLarge, Math.round(page.height * 0.1))
        }
        text: "Receive via link"
        onClicked: console.log("ReceivePage: Receive via link is not implemented yet")
    }
}
