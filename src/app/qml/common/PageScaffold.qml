import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SquiFlow

Item {
    id: root
    property string title: ""
    property string subtitle: ""
    property string breadcrumb: ""
    property bool compact: width < 700
    default property alias content: body.data
    property alias commandContent: commands.data

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: root.compact ? Theme.pageMarginCompact : Theme.pageMargin
        anchors.rightMargin: root.compact ? Theme.pageMarginCompact : Theme.pageMargin
        anchors.topMargin: root.compact ? Theme.pageMarginCompact : Theme.pageMargin
        anchors.bottomMargin: root.compact ? Theme.pageMarginCompact : Theme.pageMargin
        spacing: Theme.space24

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space12
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space4
                Label {
                    visible: root.breadcrumb.length > 0
                    text: root.breadcrumb
                    color: Theme.onSurfaceVariant
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.typeCaption
                    Accessible.role: Accessible.StaticText
                }
                Label {
                    text: root.title
                    color: Theme.onSurface
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.typeTitle
                    font.weight: Theme.weightStrong
                    elide: Text.ElideRight
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
                    Accessible.role: Accessible.StaticText
                }
            }
            RowLayout { id: commands; spacing: Theme.space8 }
        }

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.space24
        }
    }
}
