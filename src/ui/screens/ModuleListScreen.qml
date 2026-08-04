import QtQuick
import QtQuick.Controls
import SquiFlow
import "../common"

Item {
    id: root
    required property var navigationBridge
    readonly property var listBridge: navigationBridge.currentListBridge
    Accessible.name: navigationBridge.currentRoute

    Column {
        anchors.fill: parent
        anchors.margins: Theme.spacing * 2
        spacing: Theme.spacing

        Label {
            text: navigationBridge.currentRoute
            font.pixelSize: Theme.titleSize
            Accessible.role: Accessible.Heading
        }

        DataList {
            width: parent.width
            height: parent.height - y
            sourceModel: root.listBridge ? root.listBridge.model : null
            columns: root.listBridge ? root.listBridge.columns : []
            loading: root.listBridge ? root.listBridge.loading : false
            hasMore: root.listBridge ? root.listBridge.hasMore : false
            errorMessage: root.listBridge ? root.listBridge.errorMessage : ""
            emptyMessage: qsTr("No records to display")
            onRefreshRequested: root.listBridge.refresh()
            onNextPageRequested: root.listBridge.nextPage()
            onRowActivated: stableId => root.listBridge.selectRow(stableId)
            onSortRequested: (field, descending) =>
                root.listBridge.refresh(field, descending, "", "")
            onFilterRequested: (field, value) =>
                root.listBridge.refresh("", false, field, value)
        }
    }
}
