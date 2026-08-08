import QtQuick
import QtQuick.Controls
import SquiFlow
import "../common"

Popup {
    id: root
    property var bridge: null
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(600, Overlay.overlay ? Overlay.overlay.width - Theme.space32 : 600)
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
    contentItem: Form {
        fields: root.bridge ? root.bridge.fields : []
        pending: root.bridge ? root.bridge.pending : false
        formError: root.bridge ? root.bridge.errorMessage : ""
        onFieldEdited: (id, value) => root.bridge.setField(id, value)
        onSubmitRequested: root.bridge.submit()
        onCancelRequested: {
            if (root.bridge && root.bridge.dirty) unsaved.open()
            else root.close()
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
