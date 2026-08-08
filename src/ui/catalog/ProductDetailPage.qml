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
                text: root.record.name || root.stableId
                font.family: Theme.fontFamily
                font.pointSize: Theme.typeSubtitle
                font.weight: Theme.weightStrong
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Accessible.role: Accessible.Heading
            }
            StatusBanner {
                Layout.fillWidth: true
                visible: root.record.archived === true
                kind: StatusBanner.Warning
                title: qsTr("Archived")
                detail: qsTr("This item is retained for historical records and cannot be selected for new work.")
            }
            SectionCard {
                Layout.fillWidth: true
                title: qsTr("Description")
                Label {
                    text: root.record.description || qsTr("No description")
                    color: Theme.onSurfaceVariant
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
            SectionCard {
                Layout.fillWidth: true
                title: qsTr("Aliases")
                EmptyState {
                    visible: !root.record.aliases || root.record.aliases.length === 0
                    title: qsTr("No aliases")
                    detail: qsTr("Aliases make barcode and counter searches easier without changing the official name.")
                }
            }
        }
        ErrorState { detail: root.bridge ? root.bridge.errorMessage : ""; onRetryRequested: root.bridge.load(root.stableId) }
        ErrorState {
            title: qsTr("Catalog details are unavailable")
            detail: qsTr("The authenticated catalog data provider has not started.")
        }
    }
}
