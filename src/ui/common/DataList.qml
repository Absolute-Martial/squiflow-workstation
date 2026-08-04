import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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
    signal refreshRequested()
    signal nextPageRequested()
    signal rowActivated(string stableId)
    signal sortRequested(string field, bool descending)
    signal filterRequested(string field, string text)

    contentItem: ColumnLayout {
        spacing: Theme.spacing

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
                Accessible.name: qsTr("Filter records")
                onAccepted: root.filterRequested(filterFieldBox.currentValue, text)
            }
            ComboBox {
                id: sortField
                model: root.columns
                textRole: "titleKey"
                valueRole: "id"
                visible: count > 0
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
            currentIndex: root.errorMessage.length > 0 ? 2
                          : (records.count === 0 && !root.loading ? 1 : 0)

            ListView {
                id: records
                model: root.sourceModel
                reuseItems: true
                clip: true
                cacheBuffer: 400
                activeFocusOnTab: true
                keyNavigationEnabled: true
                Accessible.name: qsTr("Records")
                ScrollBar.vertical: ScrollBar {}

                delegate: ItemDelegate {
                    required property string stableId
                    required property string title
                    required property string subtitle
                    width: ListView.view.width
                    text: subtitle.length > 0 ? title + "\n" + subtitle : title
                    Accessible.name: text
                    Accessible.role: Accessible.ListItem
                    onClicked: root.rowActivated(stableId)
                    Keys.onReturnPressed: root.rowActivated(stableId)
                    Keys.onEnterPressed: root.rowActivated(stableId)
                }

                footer: Item {
                    width: records.width
                    height: root.hasMore || root.loading ? 52 : 0
                    BusyIndicator {
                        anchors.centerIn: parent
                        running: root.loading
                        visible: running
                    }
                    Button {
                        anchors.centerIn: parent
                        visible: root.hasMore && !root.loading
                        text: qsTr("Load more")
                        onClicked: root.nextPageRequested()
                    }
                }
            }

            Label {
                text: root.emptyMessage
                color: Theme.mutedText
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.Wrap
                Accessible.role: Accessible.StaticText
            }

            Column {
                spacing: Theme.spacing
                Label {
                    text: root.errorMessage
                    color: Theme.error
                    wrapMode: Text.Wrap
                    Accessible.role: Accessible.AlertMessage
                }
                Button {
                    text: qsTr("Try again")
                    onClicked: root.refreshRequested()
                }
            }
        }
    }
}
