import QtQuick 1.1
import com.nokia.symbian 1.1

PageStackWindow {
    id: window
    showStatusBar: true
    showToolBar: true

    // Set to true to show text under the tab icons (portrait only; Symbian
    // hides tab labels in landscape when an icon is present).
    property bool showTabLabels: false

    initialPage: Page {
        id: mainPage

        tools: ToolBarLayout {
            ButtonRow {
                TabButton {
                    tab: receiveTab
                    iconSource: "toolbar-home"
                    text: window.showTabLabels ? "Receive" : ""
                }
                TabButton {
                    tab: sendTab
                    iconSource: "toolbar-share"
                    text: window.showTabLabels ? "Send" : ""
                }
                TabButton {
                    tab: settingsTab
                    iconSource: "toolbar-settings"
                    text: window.showTabLabels ? "Settings" : ""
                }
            }
        }

        TabGroup {
            id: tabGroup
            anchors.fill: parent
            currentTab: receiveTab

            ReceivePage { id: receiveTab }
            SendPage { id: sendTab }
            SettingsPage {
                id: settingsTab
                onLogRequested: mainPage.pageStack.push(Qt.resolvedUrl("LogPage.qml"))
            }
        }
    }
}
