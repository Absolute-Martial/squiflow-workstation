import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SquiFlow

PageScaffold {
    id: root
    required property var navigationBridge
    property var commandBridge: null
    property string pageTitle: ""
    property string pageSubtitle: ""
    property string emptyTitle: qsTr("No records")
    property string createText: ""
    property string selectedId: ""
    property Component detailComponent: null
    readonly property var listBridge: navigationBridge.currentListBridge
    readonly property bool commandAvailable: commandBridge !== null
    signal createRequested()
    signal recordSelected(string stableId)

    title: pageTitle
    subtitle: pageSubtitle
    breadcrumb: qsTr("Workspace")

    commandContent: CommandBar {
        primaryText: root.createText
        primaryEnabled: root.commandAvailable && root.commandBridge.canWrite
        pending: root.commandAvailable && root.commandBridge.pending
        onPrimaryTriggered: root.createRequested()
    }

    StatusBanner {
        Layout.fillWidth: true
        visible: !root.listBridge
        kind: StatusBanner.Permission
        title: qsTr("Module access changed")
        detail: qsTr("This page is no longer available for the active session.")
    }

    SplitView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        orientation: root.compact ? Qt.Vertical : Qt.Horizontal

        DataList {
            SplitView.fillWidth: true
            SplitView.fillHeight: true
            SplitView.minimumWidth: 300
            sourceModel: root.listBridge ? root.listBridge.model : null
            columns: root.listBridge ? root.listBridge.columns : []
            loading: root.listBridge ? root.listBridge.loading : false
            hasMore: root.listBridge ? root.listBridge.hasMore : false
            errorMessage: root.listBridge ? root.listBridge.errorMessage : ""
            emptyMessage: root.emptyTitle
            onRefreshRequested: if (root.listBridge) root.listBridge.refresh()
            onNextPageRequested: if (root.listBridge) root.listBridge.nextPage()
            onRowActivated: stableId => {
                root.selectedId = stableId
                if (root.listBridge) root.listBridge.selectRow(stableId)
                root.recordSelected(stableId)
            }
            onSortRequested: (field, descending) => {
                if (root.listBridge) root.listBridge.refresh(field, descending, "", "")
            }
            onFilterRequested: (field, value) => {
                if (root.listBridge) root.listBridge.refresh("", false, field, value)
            }
        }

        Pane {
            visible: !root.compact || root.selectedId.length > 0
            SplitView.preferredWidth: 360
            SplitView.minimumWidth: 280
            SplitView.fillHeight: true
            Loader {
                anchors.fill: parent
                sourceComponent: root.selectedId.length > 0 ? root.detailComponent : emptyDetail
                onLoaded: {
                    if (item && item.hasOwnProperty("stableId")) item.stableId = root.selectedId
                    if (item && item.hasOwnProperty("bridge")) item.bridge = root.commandBridge
                }
            }
        }
    }

    Component {
        id: emptyDetail
        EmptyState {
            title: qsTr("Select a record")
            detail: qsTr("Details and available actions will appear here.")
        }
    }
}
