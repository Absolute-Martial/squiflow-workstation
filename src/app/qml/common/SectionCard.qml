import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SquiFlow

Rectangle {
    id: root
    property string title: ""
    property string subtitle: ""
    default property alias content: body.data
    color: Theme.surface
    border.color: Theme.border
    border.width: Theme.borderWidth
    radius: Theme.radiusLarge
    implicitHeight: layout.implicitHeight + Theme.space24 * 2

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: Theme.space24
        spacing: Theme.space12
        Label {
            visible: root.title.length > 0
            text: root.title
            color: Theme.onSurface
            font.family: Theme.fontFamily
            font.pointSize: Theme.typeSubtitle
            font.weight: Theme.weightStrong
            Accessible.role: Accessible.Heading
        }
        Label {
            visible: root.subtitle.length > 0
            text: root.subtitle
            color: Theme.onSurfaceVariant
            font.family: Theme.fontFamily
            font.pointSize: Theme.typeBody
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        ColumnLayout { id: body; Layout.fillWidth: true; spacing: Theme.space12 }
    }
}
