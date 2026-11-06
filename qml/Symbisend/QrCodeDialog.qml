import QtQuick 1.1
import com.nokia.symbian 1.1

// The official app's QR dialog: a QR code of `qrText` in the text colour, with
// `address` below it, and `pin` below that if there is one.
CommonDialog {
    id: root

    property string address
    property string qrText
    property string pin

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
                                            - (pinRow.visible ? pinRow.height + column.spacing : 0)
                                            - column.spacing - 2 * platformStyle.paddingLarge)
                anchors.horizontalCenter: parent.horizontalCenter
                width: side
                height: side
                sourceSize.width: side
                sourceSize.height: side
                smooth: false
                source: root.qrText != "" ? "image://qrcode/" + (root.platformInverted ? "000000" : "ffffff")
                                          + "/" + encodeURIComponent(root.qrText) : ""
            }
            Label {
                id: addressLabel
                platformInverted: root.platformInverted
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WrapAnywhere
                text: root.address
            }
            Row {
                id: pinRow
                visible: root.pin != ""
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: platformStyle.paddingSmall

                Image {
                    id: pinIcon
                    anchors.verticalCenter: parent.verticalCenter
                    width: platformStyle.graphicSizeSmall
                    height: platformStyle.graphicSizeSmall
                    sourceSize.width: width
                    sourceSize.height: height
                    source: root.platformInverted ? "images/vpn-key-inverted.svg" : "images/vpn-key.svg"
                }
                // Up to 64 characters, so it may have to wrap.
                Label {
                    platformInverted: root.platformInverted
                    width: Math.min(pinWidth.width, column.width - pinIcon.width - pinRow.spacing)
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WrapAnywhere
                    text: root.pin
                }
                Label {
                    id: pinWidth
                    visible: false
                    text: root.pin
                }
            }
        }
    }
}
