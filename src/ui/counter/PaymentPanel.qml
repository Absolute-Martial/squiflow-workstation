import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SquiFlow
import "../common"

SectionCard {
    id: root
    property var bridge: null
    title: qsTr("Payment")
    subtitle: qsTr("Amounts are validated and totaled in C++ using exact minor units.")
    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.space12
        Label {
            text: root.bridge ? root.bridge.totalText : "—"
            color: Theme.primary
            font.family: Theme.fontFamily
            font.pointSize: Theme.typeTitleLarge
            font.weight: Theme.weightStrong
            Accessible.name: qsTr("Sale total") + ": " + text
        }
        ComboBox {
            Layout.fillWidth: true
            model: root.bridge ? root.bridge.paymentMethods : []
            enabled: root.bridge && !root.bridge.pending
            Accessible.name: qsTr("Payment method")
            onActivated: if (root.bridge) root.bridge.setPaymentMethod(currentValue)
        }
        TextField {
            Layout.fillWidth: true
            enabled: root.bridge && !root.bridge.pending
            placeholderText: qsTr("Amount received")
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            Accessible.name: qsTr("Amount received")
            onEditingFinished: if (root.bridge) root.bridge.setAmountReceived(text)
        }
        StatusBanner {
            Layout.fillWidth: true
            visible: root.bridge && root.bridge.errorMessage.length > 0
            kind: StatusBanner.Error
            title: qsTr("Sale not completed")
            detail: root.bridge ? root.bridge.errorMessage : ""
        }
        CommandBar {
            primaryText: qsTr("Complete sale")
            primaryEnabled: root.bridge && root.bridge.canComplete && !root.bridge.unknownOutcome
            pending: root.bridge ? root.bridge.pending : false
            onPrimaryTriggered: root.bridge.completeSale()
            Button {
                text: qsTr("Clear")
                enabled: root.bridge && root.bridge.canClear && !root.bridge.pending
                onClicked: clearConfirm.open()
            }
        }
    }
    ConfirmDialog {
        id: clearConfirm
        message: qsTr("Clear every line from this sale?")
        onConfirmed: if (root.bridge) root.bridge.clearCart()
    }
}
