import QtQuick 1.1
import com.nokia.symbian 1.1

// The official app's QR dialog: a QR code of `qrText`, with `address` below it.
CommonDialog {
    id: root

    property string address
    property string qrText

    platformInverted: settings.theme == "light"
    titleText: "QR Code"
    buttonTexts: ["Close"]

    content: Item {
        width: root.platformContentMaximumWidth
        height: column.height + 2 * platformStyle.paddingLarge

        Column {
            id: column
            anchors {
                left: parent.left; right: parent.right
                verticalCenter: parent.verticalCenter
                margins: platformStyle.paddingLarge
            }
            spacing: platformStyle.paddingMedium

            // Whatever the screen leaves: landscape has far less height.
            Image {
                id: code
                property int side: Math.min(column.width,
                                            root.platformContentMaximumHeight - addressLabel.height
                                            - column.spacing - 2 * platformStyle.paddingLarge)
                anchors.horizontalCenter: parent.horizontalCenter
                width: side
                height: side
                sourceSize.width: side
                sourceSize.height: side
                smooth: false
                source: root.qrText != "" ? "image://qrcode/" + encodeURIComponent(root.qrText) : ""
            }
            Label {
                id: addressLabel
                platformInverted: root.platformInverted
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WrapAnywhere
                text: root.address
            }
        }
    }
}
