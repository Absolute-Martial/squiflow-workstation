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
            Repeater {
                model: 5
                LoadingSkeleton { Layout.fillWidth: true; running: root.loading }
            }
        }
        ColumnLayout {
            spacing: Theme.space12
            Label {
                text: root.record.displayName || root.stableId
                font.family: Theme.fontFamily
                font.pointSize: Theme.typeSubtitle
                font.weight: Theme.weightStrong
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Accessible.role: Accessible.Heading
            }
            Label { text: root.record.kind || ""; color: Theme.onSurfaceVariant }
            Label { text: root.record.roles || ""; color: Theme.onSurfaceVariant }
            SectionCard {
                Layout.fillWidth: true
                title: qsTr("Billing terms")
                Label { text: root.record.billing || qsTr("Pay per job"); wrapMode: Text.WordWrap }
            }
            SectionCard {
                Layout.fillWidth: true
                title: qsTr("Contacts")
                EmptyState {
                    visible: !root.record.contacts || root.record.contacts.length === 0
                    title: qsTr("No contact details")
                    detail: qsTr("Add a phone, email, or address when editing this party.")
                }
            }
        }
        ErrorState {
            detail: root.bridge ? root.bridge.errorMessage : ""
            onRetryRequested: if (root.bridge) root.bridge.load(root.stableId)
        }
        ErrorState {
            title: qsTr("Party details are unavailable")
            detail: qsTr("The authenticated party data provider has not started.")
        }
    }
}
