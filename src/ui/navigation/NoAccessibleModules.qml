import QtQuick
import QtQuick.Controls
import SquiFlow

Pane {
    Column {
        anchors.centerIn: parent
        spacing: Theme.spacing
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("No accessible modules")
            font.pixelSize: Theme.titleSize
            Accessible.role: Accessible.Heading
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Your active modules and permissions do not currently expose a screen.")
            color: Theme.mutedText
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            width: Math.min(480, parent.width)
            Accessible.role: Accessible.StaticText
        }
    }
}
