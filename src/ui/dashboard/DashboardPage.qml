import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SquiFlow
import "../common"

PageScaffold {
    id: page
    required property var dashboardBridge
    property var navigationBridge
    readonly property var dashboardModel: dashboardBridge ? dashboardBridge.model : null
    title: qsTr("Dashboard")
    subtitle: qsTr("Current work, recent activity, and permitted shortcuts")
    breadcrumb: qsTr("Home")

    function labelFor(key) {
        switch (key) {
        case "dashboard.receivables_due": return qsTr("Receivables due")
        case "dashboard.open_jobs": return qsTr("Open jobs")
        case "dashboard.open_orders": return qsTr("Open orders")
        case "dashboard.quotations_waiting": return qsTr("Quotations awaiting action")
        default: return qsTr("Current metric")
        }
    }

    function detailFor(key) {
        switch (key) {
        case "dashboard.overdue": return qsTr("Includes overdue balances")
        case "dashboard.jobs_active": return qsTr("Jobs currently in progress")
        default: return ""
        }
    }

    Component.onCompleted: {
        if (dashboardBridge)
            dashboardBridge.refresh()
    }

    commandContent: CommandBar {
        primaryText: qsTr("Refresh")
        pending: page.dashboardModel ? page.dashboardModel.loading : false
        onPrimaryTriggered: page.dashboardBridge.refresh()
    }

    StatusBanner {
        Layout.fillWidth: true
        visible: page.dashboardModel && page.dashboardModel.offline
        kind: StatusBanner.Warning
        title: qsTr("Working from cached information")
        detail: qsTr("Reconnect to refresh authoritative totals and activity.")
    }

    ErrorState {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: page.dashboardModel && page.dashboardModel.errorKey.length > 0
        detail: page.dashboardModel ? page.dashboardModel.errorKey : ""
        onRetryRequested: page.dashboardBridge.refresh()
    }

    EmptyState {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: page.dashboardModel && page.dashboardModel.empty
        title: qsTr("Your workspace is ready")
        detail: qsTr("Create a customer, order, task, or file from its module to begin.")
        actionText: qsTr("Refresh dashboard")
        onActionTriggered: page.dashboardBridge.refresh()
    }

    GridLayout {
        Layout.fillWidth: true
        visible: page.dashboardModel && !page.dashboardModel.empty &&
                 page.dashboardModel.errorKey.length === 0
        columns: page.width >= 1100 ? 4 : page.width >= 700 ? 2 : 1
        columnSpacing: Theme.spacing
        rowSpacing: Theme.spacing
        Repeater {
            model: page.dashboardModel ? page.dashboardModel.metrics : []
            delegate: MetricCard {
                required property var modelData
                Layout.fillWidth: true
                label: page.labelFor(modelData.labelKey)
                value: modelData.valueText
                detail: page.detailFor(modelData.detailKey)
                onClicked: {
                    if (page.navigationBridge && modelData.routeId.length)
                        page.navigationBridge.selectRoute(modelData.routeId)
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: page.dashboardModel && !page.dashboardModel.empty &&
                 page.dashboardModel.errorKey.length === 0
        spacing: Theme.spacingWide

        SectionCard {
            Layout.fillWidth: true
            Layout.fillHeight: true
            title: qsTr("Recent activity")
            subtitle: qsTr("Authoritative updates visible to your account")
            Repeater {
                model: page.dashboardModel ? page.dashboardModel.activity : []
                delegate: ItemDelegate {
                    required property var modelData
                    Layout.fillWidth: true
                    text: modelData.detailText
                    Accessible.name: text
                    onClicked: {
                        if (page.navigationBridge && modelData.routeId.length)
                            page.navigationBridge.selectRoute(modelData.routeId)
                    }
                }
            }
        }

        SectionCard {
            Layout.preferredWidth: page.compact ? 0 : 300
            Layout.fillWidth: page.compact
            Layout.fillHeight: true
            title: qsTr("Quick actions")
            subtitle: qsTr("Only actions permitted for this account are shown")
            Repeater {
                model: page.dashboardModel ? page.dashboardModel.quickActions : []
                delegate: Button {
                    required property var modelData
                    Layout.fillWidth: true
                    text: modelData.labelKey === "dashboard.new_order" ?
                              qsTr("New order") :
                          modelData.labelKey === "dashboard.new_task" ?
                              qsTr("New task") : qsTr("Open module")
                    Accessible.name: text
                    onClicked: {
                        if (page.navigationBridge)
                            page.navigationBridge.selectRoute(modelData.routeId)
                    }
                }
            }
        }
    }
}
