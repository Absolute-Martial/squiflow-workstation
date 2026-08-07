import QtQuick
import QtQuick.Controls
import FluentControls 1.0 as FC
import SquiFlow

Drawer {
    id: root
    required property var sourceModel
    signal routeRequested(string stableId)
    width: Math.min(320, Math.max(240, parent ? parent.width * 0.82 : 280))
    height: parent ? parent.height : 600
    modal: true

    ListView {
        anchors.fill: parent
        anchors.margins: Theme.space12
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
            required property bool selected
            width: ListView.view.width
            text: qsTr(titleKey)
            highlighted: selected
            font.family: Theme.fontFamily
            font.pointSize: Theme.typeBody
            Accessible.name: text
            Accessible.role: Accessible.MenuItem
            onClicked: {
                root.routeRequested(stableId)
                root.close()
            }
            Keys.onReturnPressed: clicked()
            Keys.onEnterPressed: clicked()
            FC.FocusIndicator { control: item; margins: -2 }
        }
    }
}
