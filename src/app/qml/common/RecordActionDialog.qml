import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SquiFlow

Dialog {
    id: root
    required property var bridge
    property string actionId: ""
    property string heading: qsTr("Confirm action")
    property string detail: qsTr("Review the authoritative record before continuing.")
    property bool reasonRequired: false
    modal: true
    title: heading
    standardButtons: Dialog.Cancel | Dialog.Ok
    Accessible.name: heading

    ColumnLayout {
        width: Math.min(520, root.parent ? root.parent.width - Theme.spacingXl * 2 : 520)
        Label { Layout.fillWidth: true; text: root.detail; wrapMode: Text.Wrap }
        TextArea {
            id: reason
            Layout.fillWidth: true
            visible: root.reasonRequired
            placeholderText: qsTr("Reason")
            Accessible.name: placeholderText
        }
    }

    onOpened: reason.forceActiveFocus()
    onAccepted: {
        if (root.reasonRequired && reason.text.trim().length === 0) {
            reason.forceActiveFocus()
            return
        }
        if (root.bridge) root.bridge.executeAction(root.actionId, { reason: reason.text.trim() })
        reason.clear()
    }
    onRejected: reason.clear()
}
