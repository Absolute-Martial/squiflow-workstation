import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentControls 1.0 as FC
import SquiFlow

ColumnLayout {
    id: root
    property string title: qsTr("Nothing here yet")
    property string detail: qsTr("Create the first record or refresh this page.")
    property string actionText: ""
    signal actionTriggered()
    spacing: Theme.space12
    Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

    FC.Icon {
        icon: "ic_fluent_document_24_regular"
        size: 32
        color: Theme.primary
        Layout.alignment: Qt.AlignHCenter
        Accessible.ignored: true
    }
    Label {
        text: root.title
        color: Theme.onSurface
        font.family: Theme.fontFamily
        font.pointSize: Theme.typeSubtitle
        font.weight: Theme.weightStrong
        Layout.alignment: Qt.AlignHCenter
        Accessible.role: Accessible.Heading
    }
    Label {
        text: root.detail
        color: Theme.onSurfaceVariant
        font.family: Theme.fontFamily
        font.pointSize: Theme.typeBody
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        Layout.maximumWidth: 420
        Layout.alignment: Qt.AlignHCenter
    }
    Button {
        visible: root.actionText.length > 0
        text: root.actionText
        font.family: Theme.fontFamily
        font.pointSize: Theme.typeLabel
        Accessible.name: text
        Layout.alignment: Qt.AlignHCenter
        onClicked: root.actionTriggered()
    }
}
