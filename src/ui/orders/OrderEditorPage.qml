import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SquiFlow
import "../common"

Popup {
    id: root
    property var bridge: null
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(860, Overlay.overlay ? Overlay.overlay.width - Theme.space32 : 860)
    height: Math.min(720, Overlay.overlay ? Overlay.overlay.height - Theme.space32 : 720)
    padding: Theme.space24
    closePolicy: Popup.NoAutoClose

    function openForCreate() {
        if (!bridge || !bridge.canWrite) return
        bridge.beginCreate()
        open()
    }

    background: Rectangle {
        color: Theme.surfaceOverlay
        border.color: Theme.border
        border.width: Theme.borderWidth
        radius: Theme.radiusLarge
    }
    contentItem: ColumnLayout {
        spacing: Theme.space16
        Label {
            text: qsTr("Order editor")
            font.family: Theme.fontFamily
            font.pointSize: Theme.typeTitle
            font.weight: Theme.weightStrong
            Accessible.role: Accessible.Heading
        }
        StatusBanner {
            Layout.fillWidth: true
            visible: root.bridge && root.bridge.offlineReadOnly
            kind: StatusBanner.Warning
            title: qsTr("Read-only while offline")
            detail: qsTr("Only the owner can change orders while disconnected.")
        }
        Form {
            Layout.fillWidth: true
            fields: root.bridge ? root.bridge.headerFields : []
            pending: root.bridge ? root.bridge.pending : false
            formError: root.bridge ? root.bridge.errorMessage : ""
            onFieldEdited: (id, value) => root.bridge.setHeaderField(id, value)
            onSubmitRequested: root.bridge.submit()
            onCancelRequested: {
                if (root.bridge && root.bridge.dirty) unsaved.open()
                else root.close()
            }
        }
        OrderLineEditor {
            Layout.fillWidth: true
            Layout.fillHeight: true
            bridge: root.bridge
        }
    }
    Connections {
        target: root.bridge
        enabled: root.bridge !== null
        function onSaved() { root.close() }
    }
    UnsavedChangesDialog {
        id: unsaved
        onDiscardConfirmed: {
            if (root.bridge) root.bridge.cancel()
            root.close()
        }
    }
}
