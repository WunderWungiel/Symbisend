import QtQuick 1.1
import com.nokia.symbian 1.1

PageStackWindow {
    id: window
    showStatusBar: true
    // The main page uses its own bottom TabBar instead of the toolbar; only
    // pushed pages (e.g. Log) need the real toolbar for their back button.
    // Pages with their own back button at the top (ownBackButton) hide it too.
    showToolBar: pageStack.depth > 1
                 && !(pageStack.currentPage && pageStack.currentPage.ownBackButton)
    // Light theme = the inverted Symbian style (background, status bar,
    // toolbar); the pages bind their components to it themselves.
    platformInverted: settings.theme == "light"

    initialPage: Page {
        id: mainPage

        // Never shown (showToolBar is false on this page), but required: for a
        // page without tools the ToolBar shrinks itself to height 0, and the
        // next pushed page's tools then get height 0 too - no back button.
        tools: ToolBarLayout {}

        TabGroup {
            id: tabGroup
            anchors { top: parent.top; left: parent.left; right: parent.right; bottom: tabBar.top }
            currentTab: receiveTab

            ReceivePage {
                id: receiveTab
                onLinkRequested: mainPage.pageStack.push(Qt.resolvedUrl("ReceiveLinkPage.qml"))
                onHistoryRequested: mainPage.pageStack.push(Qt.resolvedUrl("HistoryPage.qml"))
            }
            SendPage {
                id: sendTab
                currentTab: tabGroup.currentTab === sendTab
                onDeviceInfoRequested: mainPage.pageStack.push(Qt.resolvedUrl("DeviceDetailsPage.qml"),
                                                               { fingerprint: fingerprint })
                onEditSelectionRequested: mainPage.pageStack.push(Qt.resolvedUrl("SelectionPage.qml"))
                onFavoritesRequested: mainPage.pageStack.push(Qt.resolvedUrl("FavoritesPage.qml"))
            }
            SettingsPage {
                id: settingsTab
                onAboutRequested: mainPage.pageStack.push(Qt.resolvedUrl("AboutPage.qml"))
                onLogRequested: mainPage.pageStack.push(Qt.resolvedUrl("LogPage.qml"))
            }
        }

        // Plain TabBar: rectangular tabs with separators, no button outlines
        // (TabButtons outside a ButtonRow use the tab-bar graphics).
        TabBar {
            id: tabBar
            platformInverted: window.platformInverted
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }

            TabButton { tab: receiveTab; iconSource: "toolbar-home"; platformInverted: window.platformInverted }
            TabButton { tab: sendTab; iconSource: "toolbar-share"; platformInverted: window.platformInverted }
            TabButton { tab: settingsTab; iconSource: "toolbar-settings"; platformInverted: window.platformInverted }
        }
    }

    // Incoming transfers, wherever the user is in the app.
    ReceiveDialog { id: receiveDialog }

    Connections {
        target: receiver
        // "request" is the normal way in. Quick save and link auto accept skip
        // it and start at "receiving", or at "failed" if the download folder
        // can't be created. Failures also bring it back if it was closed.
        // open() does nothing if the dialog is already open.
        onStateChanged: {
            if (receiver.state == "request" || receiver.state == "receiving"
                    || receiver.state == "failed" || receiver.state == "finishedWithErrors")
                receiveDialog.open()
        }
    }
}
