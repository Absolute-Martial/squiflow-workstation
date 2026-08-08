import QtQuick
import QtQuick.Controls
import SquiFlow

Item {
    id: root
    required property var bridge

    Loader {
        id: destination
        anchors.fill: parent
        active: root.bridge.hasAccessibleModules
        source: root.bridge.currentComponentUrl
        asynchronous: false
        onLoaded: {
            if (item && item.hasOwnProperty("navigationBridge"))
                item.navigationBridge = root.bridge
            if (item && item.hasOwnProperty("dashboardBridge"))
                item.dashboardBridge = root.bridge.currentDashboardBridge
        }
    }

    Label {
        anchors.centerIn: parent
        visible: destination.status === Loader.Error
        text: qsTr("This module could not be opened.")
        color: Theme.error
        Accessible.role: Accessible.StaticText
    }
}
