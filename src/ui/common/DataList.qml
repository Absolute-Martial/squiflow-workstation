import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentControls 1.0 as FC
import SquiFlow

Control {
    id: root
    required property var sourceModel
    property var columns: []
    property bool loading: false
    property bool hasMore: false
    property string errorMessage: ""
    property string emptyMessage: qsTr("No records")
    property string filterText: ""
    property string filterField: ""
    property int viewIndex: 0
    signal refreshRequested()
    signal nextPageRequested()
    signal rowActivated(string stableId)
    signal sortRequested(string field, bool descending)
    signal filterRequested(string field, string text)

    states: [
        State { name: "error"; when: root.errorMessage.length > 0; PropertyChanges { target: root; viewIndex: 2 } },
        State { name: "empty"; when: root.errorMessage.length === 0 && records.count === 0 && !root.loading; PropertyChanges { target: root; viewIndex: 1 } },
        State { name: "content"; when: root.errorMessage.length === 0 && (records.count > 0 || root.loading); PropertyChanges { target: root; viewIndex: 0 } }
    ]

    contentItem: ColumnLayout {
        spacing: Theme.space12
        RowLayout {
            Layout.fillWidth: true
            ComboBox {
                id: filterFieldBox
                model: root.columns
                textRole: "titleKey"
                valueRole: "id"
                visible: count > 0
                Accessible.name: qsTr("Filter field")
            }
            TextField {
                id: filterInput
                Layout.fillWidth: true
                placeholderText: qsTr("Filter")
                text: root.filterText
                enabled: !root.loading
                font.family: Theme.fontFamily
                font.pointSize: Theme.typeBody
                Accessible.name: qsTr("Filter records")
                onAccepted: root.filterRequested(filterFieldBox.currentValue, text)
            }
            ComboBox {
                id: sortField
                model: root.columns
                textRole: "titleKey"
                valueRole: "id"
                visible: count > 0
                enabled: !root.loading
                Accessible.name: qsTr("Sort field")
                onActivated: root.sortRequested(currentValue, false)
            }
            ToolButton {
                text: qsTr("Refresh")
                enabled: !root.loading
                Accessible.name: qsTr("Refresh list")
                onClicked: root.refreshRequested()
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.viewIndex

            ListView {
                id: records
                model: root.sourceModel
                reuseItems: true
                clip: true
                cacheBuffer: 400
                activeFocusOnTab: true
                keyNavigationEnabled: true
                Accessible.name: qsTr("Records")
                ScrollBar.vertical: FC.ScrollBar {}
                delegate: ItemDelegate {
                    required property string stableId
                    required property string title
                    required property string subtitle
                    width: ListView.view.width
                    text: subtitle.length > 0 ? title + "\n" + subtitle : title
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.typeBody
                    Accessible.name: text
                    Accessible.role: Accessible.ListItem
                    onClicked: root.rowActivated(stableId)
                    Keys.onReturnPressed: root.rowActivated(stableId)
                    Keys.onEnterPressed: root.rowActivated(stableId)
                }
                footer: Item {
                    width: records.width
                    height: root.hasMore || root.loading ? 52 : 0
                    FC.BusyIndicator {
                        anchors.centerIn: parent
                        running: root.loading && !Theme.reducedMotion
                        visible: root.loading
                        Accessible.name: qsTr("Loading records")
                    }
                    Button {
                        anchors.centerIn: parent
                        visible: root.hasMore && !root.loading
                        text: qsTr("Load more")
                        onClicked: root.nextPageRequested()
                    }
                }
            }
            EmptyState {
                title: root.emptyMessage
                detail: qsTr("Change the filter or create the first record.")
                actionText: qsTr("Refresh")
                onActionTriggered: root.refreshRequested()
            }
            ErrorState {
                detail: root.errorMessage
                onRetryRequested: root.refreshRequested()
            }
        }
    }
}
