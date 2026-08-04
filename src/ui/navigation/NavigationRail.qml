import QtQuick
import QtQuick.Controls
import SquiFlow

Frame {
    id: root
    required property var sourceModel
    signal routeRequested(string stableId)
    implicitWidth: 240
    padding: Theme.spacing

    ListView {
        anchors.fill: parent
        model: root.sourceModel
        reuseItems: true
        clip: true
        spacing: 2
        keyNavigationEnabled: true
        activeFocusOnTab: true
        Accessible.name: qsTr("Modules")

        delegate: ItemDelegate {
            required property string stableId
            required property string titleKey
            required property string iconName
            required property bool selected
            width: ListView.view.width
            text: qsTr(titleKey)
            highlighted: selected
            Accessible.name: text
            Accessible.role: Accessible.MenuItem
            ToolTip.visible: hovered
            ToolTip.text: text
            onClicked: root.routeRequested(stableId)
            Keys.onReturnPressed: root.routeRequested(stableId)
            Keys.onEnterPressed: root.routeRequested(stableId)
        }
    }
}
