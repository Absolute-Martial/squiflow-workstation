import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SquiFlow
import "../common"

PageScaffold {
    id: root
    required property var navigationBridge
    property var saleBridge: null
    title: qsTr("Counter sale")
    subtitle: qsTr("Keyboard-first sale entry with authoritative prices and totals")
    breadcrumb: qsTr("Orders")

    StatusBanner {
        Layout.fillWidth: true
        visible: !root.saleBridge
        kind: StatusBanner.Error
        title: qsTr("Counter sale is unavailable")
        detail: qsTr("The authenticated sale provider has not started. No sale has been created.")
    }
    StatusBanner {
        Layout.fillWidth: true
        visible: root.saleBridge && root.saleBridge.unknownOutcome
        kind: StatusBanner.Warning
        title: qsTr("Confirming the sale result")
        detail: qsTr("Do not submit again. SquiFlow is checking whether the payment and receipt completed.")
    }

    RowLayout {
        Layout.fillWidth: true
        TextField {
            id: productSearch
            Layout.fillWidth: true
            enabled: root.saleBridge && !root.saleBridge.pending
            placeholderText: qsTr("Scan barcode or search product")
            font.family: Theme.fontFamily
            font.pointSize: Theme.typeBodyLarge
            Accessible.name: qsTr("Product or barcode")
            onAccepted: if (root.saleBridge) root.saleBridge.lookup(text)
        }
        Button {
            text: qsTr("Add")
            enabled: root.saleBridge && root.saleBridge.canAdd && productSearch.text.length > 0
            onClicked: root.saleBridge.lookup(productSearch.text)
        }
    }

    SplitView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        orientation: root.compact ? Qt.Vertical : Qt.Horizontal
        SectionCard {
            SplitView.fillWidth: true
            SplitView.fillHeight: true
            title: qsTr("Cart")
            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.saleBridge && root.saleBridge.lines.length > 0 ? 1 : 0
                EmptyState {
                    title: qsTr("Cart is empty")
                    detail: qsTr("Scan a barcode or search for a product to begin.")
                }
                ListView {
                    model: root.saleBridge ? root.saleBridge.lines : []
                    reuseItems: true
                    clip: true
                    delegate: CartLine {
                        required property var modelData
                        line: modelData
                        enabled: root.saleBridge && !root.saleBridge.pending
                        onRemoveRequested: root.saleBridge.removeLine(modelData.id)
                        onQuantityRequested: value => root.saleBridge.setQuantity(modelData.id, value)
                    }
                }
            }
        }
        PaymentPanel {
            SplitView.preferredWidth: 360
            SplitView.minimumWidth: 300
            SplitView.fillHeight: true
            bridge: root.saleBridge
        }
    }

    Shortcut {
        sequence: "F2"
        enabled: root.saleBridge && !root.saleBridge.pending
        onActivated: productSearch.forceActiveFocus()
    }
    Shortcut {
        sequence: "Ctrl+Enter"
        enabled: root.saleBridge && root.saleBridge.canComplete && !root.saleBridge.pending
        onActivated: root.saleBridge.completeSale()
    }
}
