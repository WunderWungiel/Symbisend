import QtQuick 1.1
import com.nokia.symbian 1.1

PageStackWindow {
    id: window
    showStatusBar: true
    // The main page uses its own bottom TabBar instead of the toolbar; only
    // pushed pages (e.g. Log) need the real toolbar for their back button.
    showToolBar: pageStack.depth > 1

    initialPage: Page {
        id: mainPage

        TabGroup {
            id: tabGroup
            anchors { top: parent.top; left: parent.left; right: parent.right; bottom: tabBar.top }
            currentTab: receiveTab

            ReceivePage { id: receiveTab }
            SendPage { id: sendTab }
            SettingsPage {
                id: settingsTab
                onLogRequested: mainPage.pageStack.push(Qt.resolvedUrl("LogPage.qml"))
            }
        }

        // Plain TabBar: rectangular tabs with separators, no button outlines
        // (TabButtons outside a ButtonRow use the tab-bar graphics).
        TabBar {
            id: tabBar
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }

            TabButton { tab: receiveTab; iconSource: "toolbar-home" }
            TabButton { tab: sendTab; iconSource: "toolbar-share" }
            TabButton { tab: settingsTab; iconSource: "toolbar-settings" }
        }
    }
}
