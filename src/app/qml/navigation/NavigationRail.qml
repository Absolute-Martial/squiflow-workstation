import QtQuick
import QtQuick.Controls
import FluentControls 1.0 as FC
import SquiFlow

Frame {
    id: root
    required property var sourceModel
    signal routeRequested(string stableId)
    implicitWidth: 240
    padding: Theme.space12

    ListView {
        anchors.fill: parent
        model: root.sourceModel
        reuseItems: true
        clip: true
        spacing: Theme.space2
        keyNavigationEnabled: true
        activeFocusOnTab: true
        Accessible.name: qsTr("Modules")
        delegate: ItemDelegate {
            id: item
            required property string stableId
            required property string titleKey
            required property string iconName
            required property bool selected
            width: ListView.view.width
            text: qsTr(titleKey)
            highlighted: selected
            font.family: Theme.fontFamily
            font.pointSize: Theme.typeBody
            Accessible.name: text
            Accessible.role: Accessible.MenuItem
            ToolTip.visible: hovered
            ToolTip.text: text
            onClicked: root.routeRequested(stableId)
            Keys.onReturnPressed: root.routeRequested(stableId)
            Keys.onEnterPressed: root.routeRequested(stableId)
            FC.FocusIndicator { control: item; margins: -2 }
        }
    }
}
