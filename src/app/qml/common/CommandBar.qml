import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentControls 1.0 as FC
import SquiFlow

RowLayout {
    id: root
    property string primaryText: ""
    property bool pending: false
    property bool primaryEnabled: true
    signal primaryTriggered()
    default property alias secondaryActions: secondary.data
    spacing: Theme.space8

    RowLayout { id: secondary; spacing: Theme.space8 }
    Button {
        visible: root.primaryText.length > 0
        text: root.pending ? qsTr("Working…") : root.primaryText
        enabled: root.primaryEnabled && !root.pending
        font.family: Theme.fontFamily
        font.pointSize: Theme.typeLabel
        Accessible.name: text
        Accessible.description: root.pending ? qsTr("Operation in progress") : ""
        onClicked: root.primaryTriggered()
    }
    FC.BusyIndicator {
        visible: root.pending
        running: root.pending && !Theme.reducedMotion
        implicitWidth: 24
        implicitHeight: 24
        Accessible.name: qsTr("Operation in progress")
    }
}
