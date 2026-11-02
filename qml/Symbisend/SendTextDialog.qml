import QtQuick 1.1
import com.nokia.symbian 1.1

// Text entry for a text message: new (openNew) or editing a selected one
// (openEdit). OK adds it to / updates it in `selection`.
CommonDialog {
    id: root

    // Index of the text item being edited in `selection`, -1 for a new one.
    property int editIndex: -1

    platformInverted: settings.theme == "light"
    titleText: editIndex < 0 ? "Send text" : "Edit text"
    buttonTexts: ["OK", "Cancel"]

    function openNew() {
        editIndex = -1
        textArea.text = ""
        open()
    }

    function openEdit(index, text) {
        editIndex = index
        textArea.text = text
        open()
    }

    content: Item {
        width: root.platformContentMaximumWidth
        height: textArea.height + 2 * platformStyle.paddingLarge

        TextArea {
            id: textArea
            platformInverted: root.platformInverted
            anchors {
                left: parent.left; right: parent.right
                verticalCenter: parent.verticalCenter
                margins: platformStyle.paddingLarge
            }
            height: 4 * platformStyle.fontSizeLarge + 2 * platformStyle.paddingLarge
            placeholderText: "Enter text"
        }
    }

    onButtonClicked: {
        if (index != 0)
            return
        if (editIndex < 0)
            selection.addText(textArea.text)
        else
            selection.setText(editIndex, textArea.text) // empty text removes it
    }
}
