import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentControls 1.0 as FC
import SquiFlow

Pane {
    ColumnLayout {
        anchors.centerIn: parent
        spacing: Theme.space12
        FC.Icon {
            icon: "ic_fluent_lock_closed_24_regular"
            size: 32
            color: Theme.onSurfaceVariant
            Layout.alignment: Qt.AlignHCenter
        }
        Label {
            text: qsTr("No accessible modules")
            font.family: Theme.fontFamily
            font.pointSize: Theme.typeTitle
            font.weight: Theme.weightStrong
            Layout.alignment: Qt.AlignHCenter
            Accessible.role: Accessible.Heading
        }
        Label {
            text: qsTr("Your active modules and permissions do not currently expose a screen.")
            color: Theme.onSurfaceVariant
            font.family: Theme.fontFamily
            font.pointSize: Theme.typeBody
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            Layout.maximumWidth: 480
            Layout.alignment: Qt.AlignHCenter
            Accessible.role: Accessible.StaticText
        }
    }
}
