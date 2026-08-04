import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SquiFlow
import "navigation"

ApplicationWindow {
    id: root
    readonly property var lifecycleBridge: applicationSurface
    readonly property bool compactNavigation: width < 900
    width: 1180
    height: 760
    minimumWidth: 640
    minimumHeight: 480
    visible: true
    title: qsTr("SquiFlow")
    color: Theme.background

    onClosing: close => {
        close.accepted = false
        lifecycleBridge.requestShutdown()
    }

    NavigationDrawer {
        id: drawer
        parent: Overlay.overlay
        sourceModel: navigationModel
        onRouteRequested: stableId => navigationBridge.selectRoute(stableId)
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.spacing
            anchors.rightMargin: Theme.spacing
            ToolButton {
                visible: root.compactNavigation
                text: "☰"
                Accessible.name: qsTr("Open module navigation")
                onClicked: drawer.open()
            }
            ToolButton {
                text: "‹"
                Accessible.name: qsTr("Back")
                onClicked: navigationBridge.goBack()
            }
            ToolButton {
                text: "›"
                Accessible.name: qsTr("Forward")
                onClicked: navigationBridge.goForward()
            }
            Label {
                Layout.fillWidth: true
                text: navigationBridge.currentRoute || root.title
                font.pixelSize: Theme.titleSize
                elide: Text.ElideRight
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        NavigationRail {
            Layout.fillHeight: true
            visible: !root.compactNavigation
            sourceModel: navigationModel
            onRouteRequested: stableId => navigationBridge.selectRoute(stableId)
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            NavigationHost {
                anchors.fill: parent
                visible: navigationBridge.hasAccessibleModules
                bridge: navigationBridge
            }

            NoAccessibleModules {
                anchors.fill: parent
                visible: !navigationBridge.hasAccessibleModules
            }
        }
    }
}
