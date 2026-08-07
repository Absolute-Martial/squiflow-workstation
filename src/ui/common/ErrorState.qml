import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentControls 1.0 as FC
import SquiFlow

ColumnLayout {
    id: root
    property string title: qsTr("This section could not be loaded")
    property string detail: qsTr("Your other modules are still available.")
    signal retryRequested()
    spacing: Theme.space12
    Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
    Accessible.role: Accessible.AlertMessage

    FC.Icon {
        icon: "ic_fluent_error_circle_24_filled"
        size: 32
        color: Theme.error
        Layout.alignment: Qt.AlignHCenter
    }
    Label {
        text: root.title
        color: Theme.onSurface
        font.family: Theme.fontFamily
        font.pointSize: Theme.typeSubtitle
        font.weight: Theme.weightStrong
        Layout.alignment: Qt.AlignHCenter
    }
    Label {
        text: root.detail
        color: Theme.onSurfaceVariant
        font.family: Theme.fontFamily
        font.pointSize: Theme.typeBody
        wrapMode: Text.WordWrap
        Layout.maximumWidth: 460
        Layout.alignment: Qt.AlignHCenter
    }
    Button {
        text: qsTr("Try again")
        font.family: Theme.fontFamily
        font.pointSize: Theme.typeLabel
        Accessible.name: text
        Layout.alignment: Qt.AlignHCenter
        onClicked: root.retryRequested()
    }
}
