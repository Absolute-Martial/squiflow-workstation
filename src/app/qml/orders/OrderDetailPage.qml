import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SquiFlow
import "../common"

Item {
    id: root
    property string stableId: ""
    property var bridge: null
    readonly property bool loading: bridge && bridge.loading
    readonly property var record: bridge ? bridge.currentRecord : ({})

    StackLayout {
        anchors.fill: parent
        currentIndex: !root.bridge ? 3 : root.loading ? 0
                      : root.bridge.errorMessage.length > 0 ? 2 : 1
        ColumnLayout {
            Repeater { model: 6; LoadingSkeleton { Layout.fillWidth: true; running: root.loading } }
        }
        ColumnLayout {
            spacing: Theme.space12
            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: root.record.orderNumber || root.stableId
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.typeSubtitle
                    font.weight: Theme.weightStrong
                    Layout.fillWidth: true
                    Accessible.role: Accessible.Heading
                }
                PriceEvidenceBadge { text: root.record.stateText || qsTr("Open") }
            }
            Label { text: root.record.partyName || qsTr("Walk-in customer"); color: Theme.onSurfaceVariant }
            Label {
                text: root.record.totalText || "—"
                color: Theme.primary
                font.family: Theme.fontFamily
                font.pointSize: Theme.typeTitleLarge
                font.weight: Theme.weightStrong
                Accessible.name: qsTr("Order total") + ": " + text
            }
            SectionCard {
                Layout.fillWidth: true
                title: qsTr("Lines")
                ListView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(contentHeight, 300)
                    model: root.record.lines || []
                    reuseItems: true
                    delegate: ItemDelegate {
                        required property var modelData
                        width: ListView.view.width
                        text: modelData.description + "  " + modelData.amountText
                        font.family: Theme.fontFamily
                        font.pointSize: Theme.typeBody
                        PriceEvidenceBadge {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.priceSourceText
                            overridden: modelData.priceOverridden
                        }
                    }
                }
            }
            StatusBanner {
                Layout.fillWidth: true
                visible: root.record.unknownOutcome === true
                kind: StatusBanner.Warning
                title: qsTr("Confirming the result")
                detail: qsTr("Do not repeat the action. SquiFlow is checking whether it completed.")
            }
            CommandBar {
                primaryText: qsTr("Confirm order")
                primaryEnabled: root.bridge && root.bridge.canConfirm && !root.record.unknownOutcome
                pending: root.bridge ? root.bridge.pending : false
                onPrimaryTriggered: confirmOrder.open()
                Button {
                    text: qsTr("Cancel order")
                    enabled: root.bridge && root.bridge.canCancel && !root.bridge.pending
                    onClicked: cancelOrder.open()
                }
            }
        }
        ErrorState { detail: root.bridge ? root.bridge.errorMessage : ""; onRetryRequested: root.bridge.load(root.stableId) }
        ErrorState {
            title: qsTr("Order details are unavailable")
            detail: qsTr("The authenticated orders provider has not started.")
        }
    }
    ConfirmDialog {
        id: confirmOrder
        message: qsTr("Confirm this order and freeze its current price evidence?")
        onConfirmed: if (root.bridge) root.bridge.confirm(root.stableId)
    }
    ConfirmDialog {
        id: cancelOrder
        message: qsTr("Cancel this order? The evidence remains in history.")
        onConfirmed: if (root.bridge) root.bridge.cancelOrder(root.stableId)
    }
}
