import QtQuick
import QtQuick.Controls
import FluentControls 1.0 as FC

FC.FluentDialog {
    id: root
    property string message: ""
    signal confirmed()
    modal: true
    title: qsTr("Confirm action")
    standardButtons: Dialog.Ok | Dialog.Cancel
    closePolicy: Popup.NoAutoClose
    onAccepted: root.confirmed()
    contentItem: Label {
        text: root.message
        wrapMode: Text.WordWrap
        font.family: Theme.fontFamily
        font.pointSize: Theme.typeBody
        color: Theme.onSurface
        Accessible.role: Accessible.StaticText
    }
}
