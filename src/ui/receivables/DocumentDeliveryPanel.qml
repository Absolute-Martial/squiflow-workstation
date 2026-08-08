import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SquiFlow
import "../common"

SectionCard {
    id: root
    property var bridge: null
    title: qsTr("Document")
    subtitle: qsTr("Save, print, and delivery operations keep their own evidence.")
    RowLayout {
        Layout.fillWidth: true
        Button {
            text: qsTr("Save PDF")
            enabled: root.bridge && root.bridge.canSavePdf && !root.bridge.pending
            onClicked: root.bridge.savePdf()
        }
        Button {
            text: qsTr("Print")
            enabled: root.bridge && root.bridge.canPrint && !root.bridge.pending
            onClicked: root.bridge.printDocument()
        }
        Button {
            text: qsTr("Send")
            enabled: root.bridge && root.bridge.canSend && !root.bridge.pending && !root.bridge.unknownOutcome
            onClicked: deliveryConfirm.open()
        }
    }
    ConfirmDialog {
        id: deliveryConfirm
        message: qsTr("Send this immutable invoice document to the selected recipient?")
        onConfirmed: if (root.bridge) root.bridge.sendDocument()
    }
}
