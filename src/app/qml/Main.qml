import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentControls 1.0 as FC
import SquiFlow
import "common"
import "navigation"

ApplicationWindow {
    id: root
    readonly property var lifecycleBridge: applicationSurface
    readonly property bool compactNavigation: width < 900
    property bool shutdownAfterDiscard: false
    width: 1180
    height: 760
    minimumWidth: 640
    minimumHeight: 480
    visible: true
    title: qsTr("SquiFlow")
    color: nativeWindowBridge.backdropActive ? "transparent" : Theme.background

    Binding { target: Theme; property: "mode"; value: shellState.themeChoice }
    Binding { target: Theme; property: "highContrast"; value: shellState.highContrast }
    Binding { target: Theme; property: "reducedMotion"; value: shellState.reducedMotion }

    Component.onCompleted: nativeWindowBridge.setup(root, Theme.dark, Theme.highContrast)
    Connections {
        target: Theme
        function onDarkChanged() {
            nativeWindowBridge.setAppearance(Theme.dark, Theme.highContrast)
        }
        function onHighContrastChanged() {
            nativeWindowBridge.setAppearance(Theme.dark, Theme.highContrast)
        }
    }

    function routeTitle(route) {
        if (route === "dashboard.home") return qsTr("Dashboard")
        if (route === "parties.list") return qsTr("Customers and suppliers")
        if (route === "catalog.list") return qsTr("Catalog")
        if (route === "pricing.rates") return qsTr("Pricing")
        if (route === "orders.list") return qsTr("Orders")
        if (route === "orders.counter_sale") return qsTr("Counter sale")
        if (route === "jobs.list") return qsTr("Jobs")
        if (route === "receivables.invoices") return qsTr("Receivables")
        return qsTr("SquiFlow workspace")
    }

    function requestRoute(stableId) {
        shellState.requestRoute(stableId)
    }

    onClosing: close => {
        close.accepted = false
        if (shellState.dirty) {
            root.shutdownAfterDiscard = true
            shellState.requestRoute(navigationBridge.currentRoute)
            unsavedDialog.open()
        } else {
            lifecycleBridge.requestShutdown()
        }
    }

    Connections {
        target: shellState
        function onRouteApproved(stableId) {
            if (root.shutdownAfterDiscard) {
                root.shutdownAfterDiscard = false
                root.lifecycleBridge.requestShutdown()
            } else {
                navigationBridge.selectRoute(stableId)
            }
        }
        function onUnsavedDecisionChanged() {
            if (shellState.unsavedDecisionPending)
                unsavedDialog.open()
        }
    }

    Shortcut { sequence: "Ctrl+K"; onActivated: commandPalette.open() }
    Shortcut { sequence: StandardKey.Find; onActivated: globalSearch.forceActiveFocus() }

    NavigationDrawer {
        id: drawer
        parent: Overlay.overlay
        sourceModel: navigationModel
        onRouteRequested: stableId => root.requestRoute(stableId)
    }

    UnsavedChangesDialog {
        id: unsavedDialog
        anchors.centerIn: parent
        onDiscardConfirmed: shellState.resolveUnsaved(true)
        onKeepEditing: {
            root.shutdownAfterDiscard = false
            shellState.resolveUnsaved(false)
        }
    }

    Dialog {
        id: commandPalette
        anchors.centerIn: parent
        width: Math.min(root.width - Theme.pageMargin * 2, 560)
        modal: true
        title: qsTr("Command palette")
        standardButtons: Dialog.Close
        contentItem: ColumnLayout {
            TextField {
                id: paletteSearch
                Layout.fillWidth: true
                placeholderText: qsTr("Search commands")
                Accessible.name: placeholderText
            }
            Label {
                text: qsTr("Available commands follow your module activation and rights.")
                color: Theme.mutedText
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
        onOpened: paletteSearch.forceActiveFocus()
    }

    Popup {
        id: notificationPopup
        x: Math.max(0, root.width - width - Theme.spacing)
        y: root.header ? root.header.height : 0
        width: Math.min(root.width - Theme.spacing * 2, 420)
        height: Math.min(360, notificationList.contentHeight + Theme.spacing * 2)
        padding: Theme.spacing
        background: Rectangle { color: Theme.surfaceRaised; border.color: Theme.border; radius: Theme.radius }
        ListView {
            id: notificationList
            anchors.fill: parent
            model: shellState.notifications
            spacing: Theme.spacingCompact
            delegate: StatusBanner {
                required property var modelData
                width: notificationList.width
                title: modelData.messageKey === "notification.offline" ?
                           qsTr("Connection unavailable") : qsTr("Application message")
                detail: modelData.detail
                kind: modelData.severity === 3 ? StatusBanner.Error :
                      modelData.severity === 2 ? StatusBanner.Warning :
                      modelData.severity === 1 ? StatusBanner.Success : StatusBanner.Information
                dismissible: true
                onDismissed: shellState.dismissNotification(modelData.id)
            }
        }
    }

    header: ToolBar {
        contentHeight: 56
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.spacing
            anchors.rightMargin: Theme.spacing
            spacing: Theme.spacingCompact
            ToolButton {
                visible: root.compactNavigation
                contentItem: FC.Icon { icon: "ic_fluent_navigation_20_regular"; size: 20 }
                Accessible.name: qsTr("Open module navigation")
                onClicked: drawer.open()
            }
            ToolButton {
                contentItem: FC.Icon { icon: "ic_fluent_arrow_left_20_regular"; size: 20 }
                Accessible.name: qsTr("Back")
                onClicked: navigationBridge.goBack()
            }
            ToolButton {
                contentItem: FC.Icon { icon: "ic_fluent_arrow_right_20_regular"; size: 20 }
                Accessible.name: qsTr("Forward")
                onClicked: navigationBridge.goForward()
            }
            ColumnLayout {
                Layout.preferredWidth: 220
                spacing: 0
                Label {
                    text: root.routeTitle(navigationBridge.currentRoute)
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.typeSubtitle
                    font.weight: Theme.weightStrong
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Label {
                    text: shellState.tenantName + " · " + shellState.userName
                    color: Theme.mutedText
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
            TextField {
                id: globalSearch
                visible: root.width >= 900
                Layout.fillWidth: true
                Layout.maximumWidth: 420
                placeholderText: qsTr("Search the workspace")
                Accessible.name: placeholderText
                onAccepted: commandPalette.open()
            }
            Item { Layout.fillWidth: true }
            Label {
                text: shellState.connectivityState === 0 ? qsTr("Online") :
                      shellState.connectivityState === 1 ? qsTr("Offline") :
                      shellState.connectivityState === 2 ? qsTr("Syncing") : qsTr("Needs attention")
                color: shellState.connectivityState === 0 ? Theme.positive :
                       shellState.connectivityState === 3 ? Theme.error : Theme.warning
                Accessible.name: qsTr("Connectivity: ") + text
            }
            ToolButton {
                contentItem: FC.Icon { icon: "ic_fluent_search_20_regular"; size: 20 }
                Accessible.name: qsTr("Open command palette")
                onClicked: commandPalette.open()
            }
            ToolButton {
                contentItem: FC.Icon {
                    icon: shellState.notifications.length
                          ? "ic_fluent_alert_20_filled"
                          : "ic_fluent_alert_20_regular"
                    size: 20
                    color: shellState.notifications.length ? Theme.primary : Theme.onSurface
                }
                Accessible.name: qsTr("Notifications")
                onClicked: notificationPopup.open()
            }
            ToolButton {
                id: settingsButton
                contentItem: FC.Icon { icon: "ic_fluent_more_horizontal_20_regular"; size: 20 }
                Accessible.name: qsTr("Appearance and settings")
                onClicked: settingsMenu.open()
                Menu {
                    id: settingsMenu
                    y: settingsButton.height
                    MenuItem { text: qsTr("Use system theme"); onTriggered: shellState.setThemeChoice(0) }
                    MenuItem { text: qsTr("Light theme"); onTriggered: shellState.setThemeChoice(1) }
                    MenuItem { text: qsTr("Dark theme"); onTriggered: shellState.setThemeChoice(2) }
                    MenuSeparator {}
                    MenuItem {
                        text: qsTr("High contrast")
                        checkable: true
                        checked: shellState.highContrast
                        onTriggered: shellState.setHighContrast(checked)
                    }
                    MenuItem {
                        text: qsTr("Reduce motion")
                        checkable: true
                        checked: shellState.reducedMotion
                        onTriggered: shellState.setReducedMotion(checked)
                    }
                }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        NavigationRail {
            Layout.fillHeight: true
            visible: !root.compactNavigation
            sourceModel: navigationModel
            onRouteRequested: stableId => root.requestRoute(stableId)
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            NavigationHost {
                anchors.fill: parent
                visible: navigationBridge.hasAccessibleModules
                bridge: navigationBridge
            }

            NoAccessibleModules {
                anchors.fill: parent
                visible: !navigationBridge.hasAccessibleModules
            }
        }
    }
}
