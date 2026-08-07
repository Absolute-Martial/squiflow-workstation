import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SquiFlow
import "../common"

SectionCard {
    id: root
    property var bridge: null
    title: qsTr("Payments and allocations")
    subtitle: qsTr("Incoming money is recorded independently and allocated explicitly.")
    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.space8
        Repeater {
            model: root.bridge ? root.bridge.allocations : []
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                Label { text: modelData.paymentReference; Layout.fillWidth: true; elide: Text.ElideRight }
                Label {
                    text: modelData.amountText
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.typeBody
                    font.weight: Theme.weightStrong
                    Accessible.name: qsTr("Allocated amount") + ": " + text
                }
            }
        }
        Button {
            text: qsTr("Record payment")
            enabled: root.bridge && root.bridge.canRecordPayment && !root.bridge.pending && !root.bridge.unknownOutcome
            onClicked: root.bridge.beginPayment()
        }
    }
}
