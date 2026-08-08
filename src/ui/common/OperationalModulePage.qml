import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SquiFlow

PageScaffold {
    id: root
    required property var navigationBridge
    property string pageTitle: ""
    property string pageSubtitle: ""
    property string emptyText: qsTr("No records")
    property string createText: ""
    property string guidanceText: ""
    property var capabilityLabels: []
    readonly property var listBridge: navigationBridge.currentListBridge
    readonly property var recordBridge: navigationBridge.currentRecordBridge
    readonly property bool detailVisible: recordBridge && recordBridge.hasRecord

    title: pageTitle
    subtitle: pageSubtitle
    breadcrumb: qsTr("Workspace")
    Accessible.name: pageTitle

    commandContent: CommandBar {
        primaryText: root.createText
        primaryEnabled: root.recordBridge && root.recordBridge.canCreate && !root.recordBridge.pending
        pending: root.recordBridge ? root.recordBridge.pending : false
        onPrimaryTriggered: if (root.recordBridge) root.recordBridge.beginCreate()
    }

    StatusBanner {
        Layout.fillWidth: true
        visible: !root.listBridge || !root.recordBridge
        kind: StatusBanner.Permission
        title: qsTr("Module access changed")
        detail: qsTr("This page is unavailable for the active session. Return to another module or sign in again.")
        Accessible.role: Accessible.AlertMessage
    }

    StatusBanner {
        Layout.fillWidth: true
        visible: root.recordBridge && root.recordBridge.errorMessage.length > 0
        kind: StatusBanner.Error
        title: qsTr("The request could not be completed")
        detail: root.recordBridge ? root.recordBridge.errorMessage : ""
        Accessible.role: Accessible.AlertMessage
    }

    SplitView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        orientation: root.compact ? Qt.Vertical : Qt.Horizontal

        ColumnLayout {
            SplitView.fillWidth: true
            SplitView.fillHeight: true
            SplitView.minimumWidth: 320
            spacing: Theme.spacingMd

            SectionCard {
                Layout.fillWidth: true
                visible: root.guidanceText.length > 0
                title: qsTr("What you can do here")
                subtitle: root.guidanceText

                Flow {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSm
                    Repeater {
                        model: root.capabilityLabels
                        delegate: Label {
                            required property var modelData
                            text: modelData
                            color: Theme.textSecondary
                            padding: Theme.spacingXs
                            background: Rectangle {
                                color: Theme.surfaceAlt
                                radius: Theme.radiusSm
                                border.color: Theme.border
                            }
                        }
                    }
                }
            }

            DataList {
                Layout.fillWidth: true
                Layout.fillHeight: true
                sourceModel: root.listBridge ? root.listBridge.model : null
                columns: root.listBridge ? root.listBridge.columns : []
                loading: root.listBridge ? root.listBridge.loading : false
                hasMore: root.listBridge ? root.listBridge.hasMore : false
                errorMessage: root.listBridge ? root.listBridge.errorMessage : ""
                emptyMessage: root.emptyText
                onRefreshRequested: if (root.listBridge) root.listBridge.refresh()
                onNextPageRequested: if (root.listBridge) root.listBridge.nextPage()
                onRowActivated: stableId => {
                    if (root.listBridge && root.listBridge.selectRow(stableId) && root.recordBridge)
                        root.recordBridge.loadRecord(stableId)
                }
                onSortRequested: (field, descending) => {
                    if (root.listBridge) root.listBridge.refresh(field, descending, "", "")
                }
                onFilterRequested: (field, value) => {
                    if (root.listBridge) root.listBridge.refresh("", false, field, value)
                }
            }
        }

        RecordDetailPane {
            SplitView.preferredWidth: 420
            SplitView.minimumWidth: 300
            SplitView.fillHeight: true
            bridge: root.recordBridge
            visible: !root.compact || root.detailVisible
        }
    }

    Component.onCompleted: {
        if (root.listBridge) root.listBridge.refresh()
    }
}
