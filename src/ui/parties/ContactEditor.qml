import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SquiFlow

ColumnLayout {
    id: root
    property string label: ""
    property string value: ""
    property bool pending: false
    signal edited(string label, string value)
    spacing: Theme.space8
    ComboBox {
        id: contactKind
        model: [qsTr("Phone"), qsTr("Email"), qsTr("Address"), qsTr("Other")]
        enabled: !root.pending
        Accessible.name: qsTr("Contact type")
    }
    TextField {
        id: contactValue
        Layout.fillWidth: true
        text: root.value
        enabled: !root.pending
        font.family: Theme.fontFamily
        font.pointSize: Theme.typeBody
        Accessible.name: qsTr("Contact value")
        onEditingFinished: root.edited(contactKind.currentText, text)
    }
}
