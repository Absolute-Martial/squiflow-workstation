import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SquiFlow
import "../common"

PageScaffold {
    id: root
    required property var navigationBridge
    readonly property var listBridge: navigationBridge.currentListBridge
    title: qsTr("Module records")
    subtitle: qsTr("Authorized records for the selected module")
    breadcrumb: qsTr("Workspace")
    Accessible.name: navigationBridge.currentRoute

    StatusBanner {
        Layout.fillWidth: true
        visible: !root.listBridge
        kind: StatusBanner.Permission
        title: qsTr("This page is unavailable")
        detail: qsTr("The module was disabled or your access changed.")
    }
    DataList {
        Layout.fillWidth: true
        Layout.fillHeight: true
        sourceModel: root.listBridge ? root.listBridge.model : null
        columns: root.listBridge ? root.listBridge.columns : []
        loading: root.listBridge ? root.listBridge.loading : false
        hasMore: root.listBridge ? root.listBridge.hasMore : false
        errorMessage: root.listBridge ? root.listBridge.errorMessage : ""
        emptyMessage: qsTr("No records to display")
        onRefreshRequested: if (root.listBridge) root.listBridge.refresh()
        onNextPageRequested: if (root.listBridge) root.listBridge.nextPage()
        onRowActivated: stableId => { if (root.listBridge) root.listBridge.selectRow(stableId) }
        onSortRequested: (field, descending) => {
            if (root.listBridge) root.listBridge.refresh(field, descending, "", "")
        }
        onFilterRequested: (field, value) => {
            if (root.listBridge) root.listBridge.refresh("", false, field, value)
        }
    }
}
