import QtQuick
import QtQuick.Controls
import SquiFlow

Item {
    id: root
    required property var navigationBridge
    Accessible.name: navigationBridge.currentRoute

    Column {
        anchors.fill: parent
        anchors.margins: Theme.spacing * 2
        spacing: Theme.spacing

        Label {
            text: navigationBridge.currentRoute
            font.pixelSize: Theme.titleSize
            Accessible.role: Accessible.Heading
        }

        Label {
            text: qsTr("Select or refresh this module's list to load records.")
            color: Theme.mutedText
            Accessible.role: Accessible.StaticText
        }
    }
}
