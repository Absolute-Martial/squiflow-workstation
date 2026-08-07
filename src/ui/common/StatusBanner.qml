import QtQuick
import QtQuick.Controls
import FluentControls 1.0 as FC

Item {
    id: root
    enum Kind { Information, Success, Warning, Error, Permission }
    property int kind: StatusBanner.Information
    property string title: ""
    property string detail: ""
    property bool dismissible: false
    signal dismissed()
    implicitHeight: info.implicitHeight
    Accessible.role: Accessible.AlertMessage
    Accessible.name: title + (detail.length ? ". " + detail : "")

    FC.InfoBar {
        id: info
        anchors.fill: parent
        title: root.title
        text: root.detail
        closable: root.dismissible
        timeout: -1
        severity: root.kind === StatusBanner.Success ? 1
                  : root.kind === StatusBanner.Warning ? 2
                  : root.kind === StatusBanner.Error ? 3 : 0
        onVisibleChanged: {
            if (!visible && root.visible)
                root.dismissed()
        }
    }
}
