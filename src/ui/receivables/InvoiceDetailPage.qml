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
            Repeater { model: 7; LoadingSkeleton { Layout.fillWidth: true; running: root.loading } }
        }
        ColumnLayout {
            spacing: Theme.space12
            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: root.record.numberText || qsTr("Draft invoice")
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.typeSubtitle
                    font.weight: Theme.weightStrong
                    Layout.fillWidth: true
                    Accessible.role: Accessible.Heading
                }
                Label { text: root.record.stateText || qsTr("Draft"); color: Theme.onSurfaceVariant }
            }
            Label { text: root.record.partyName || qsTr("Walk-in customer"); color: Theme.onSurfaceVariant }
            Label {
                text: root.record.totalText || "—"
                color: Theme.primary
                font.family: Theme.fontFamily
                font.pointSize: Theme.typeTitleLarge
                font.weight: Theme.weightStrong
                Accessible.name: qsTr("Invoice total") + ": " + text
            }
            SectionCard {
                Layout.fillWidth: true
                title: qsTr("Balance")
                Label {
                    text: root.record.balanceText || "—"
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.typeSubtitle
                    font.weight: Theme.weightStrong
                    Accessible.name: qsTr("Outstanding balance") + ": " + text
                }
            }
            PaymentAllocationPage { Layout.fillWidth: true; bridge: root.bridge }
            StatementPanel { Layout.fillWidth: true; bridge: root.bridge }
            DocumentDeliveryPanel { Layout.fillWidth: true; bridge: root.bridge }
            StatusBanner {
                Layout.fillWidth: true
                visible: root.record.unknownOutcome === true
                kind: StatusBanner.Warning
                title: qsTr("Confirming the result")
                detail: qsTr("Do not record another payment or resend the document while SquiFlow checks the outcome.")
            }
        }
        ErrorState { detail: root.bridge ? root.bridge.errorMessage : ""; onRetryRequested: root.bridge.loadInvoice(root.stableId) }
        ErrorState {
            title: qsTr("Invoice details are unavailable")
            detail: qsTr("The authenticated receivables provider has not started.")
        }
    }
}
