import QtQuick
import QtQuick.Controls
import SquiFlow

Rectangle {
    id: root
    property string text: ""
    property bool overridden: false
    implicitWidth: label.implicitWidth + Theme.space16
    implicitHeight: label.implicitHeight + Theme.space8
    radius: implicitHeight / 2
    color: overridden ? Theme.warning : Theme.selection
    border.color: overridden ? Theme.warning : Theme.primary
    border.width: Theme.borderWidth
    Accessible.name: text
    Label {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.overridden ? Theme.onWarning : Theme.onSurface
        font.family: Theme.fontFamily
        font.pointSize: Theme.typeCaption
        font.weight: Theme.weightStrong
    }
}
