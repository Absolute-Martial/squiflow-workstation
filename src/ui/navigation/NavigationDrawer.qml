import QtQuick
import QtQuick.Controls
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
        anchors.margins: Theme.spacing
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
            required property bool selected
            width: ListView.view.width
            text: qsTr(titleKey)
            highlighted: selected
            Accessible.name: text
            Accessible.role: Accessible.MenuItem
            onClicked: {
                root.routeRequested(stableId)
                root.close()
            }
            Keys.onReturnPressed: clicked()
            Keys.onEnterPressed: clicked()
        }
    }
}
