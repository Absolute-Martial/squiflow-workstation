import QtQuick
import QtQuick.Controls
import FluentControls 1.0 as FC

FC.FluentDialog {
    id: root
    signal discardConfirmed()
    signal keepEditing()
    modal: true
    title: qsTr("Unsaved changes")
    standardButtons: Dialog.Discard | Dialog.Cancel
    closePolicy: Popup.NoAutoClose
    contentItem: Label {
        text: qsTr("Discard the changes and continue?")
        wrapMode: Text.WordWrap
        font.family: Theme.fontFamily
        font.pointSize: Theme.typeBody
        color: Theme.onSurface
        Accessible.role: Accessible.AlertMessage
    }
    onDiscarded: root.discardConfirmed()
    onRejected: root.keepEditing()
}
