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
            Repeater { model: 5; LoadingSkeleton { Layout.fillWidth: true; running: root.loading } }
        }
        ColumnLayout {
            spacing: Theme.space12
            Label {
                text: root.record.productName || root.stableId
                font.family: Theme.fontFamily
                font.pointSize: Theme.typeSubtitle
                font.weight: Theme.weightStrong
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Accessible.role: Accessible.Heading
            }
            Label {
                text: root.record.amountText || "—"
                color: Theme.primary
                font.family: Theme.fontFamily
                font.pointSize: Theme.typeTitleLarge
                font.weight: Theme.weightStrong
                Accessible.name: qsTr("Rate") + ": " + text
            }
            SectionCard {
                Layout.fillWidth: true
                title: qsTr("Applies to")
                Label { text: root.record.partyName || qsTr("All customers"); wrapMode: Text.WordWrap }
            }
            SectionCard {
                Layout.fillWidth: true
                title: qsTr("Effective period")
                Label { text: root.record.effectivePeriodText || qsTr("No time limit"); wrapMode: Text.WordWrap }
            }
            PriceResolutionPanel {
                Layout.fillWidth: true
                sourceText: root.record.sourceText || qsTr("Stored rate")
                explanation: root.record.resolutionExplanation || qsTr("The pricing service selects the authoritative rate; this page does not calculate prices.")
            }
        }
        ErrorState { detail: root.bridge ? root.bridge.errorMessage : ""; onRetryRequested: root.bridge.load(root.stableId) }
        ErrorState {
            title: qsTr("Rate details are unavailable")
            detail: qsTr("The authenticated pricing provider has not started.")
        }
    }
}
