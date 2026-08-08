import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SquiFlow
import "../common"

SectionCard {
    id: root
    property string sourceText: ""
    property string explanation: ""
    title: qsTr("Resolution evidence")
    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.space8
        Label {
            text: root.sourceText
            color: Theme.primary
            font.family: Theme.fontFamily
            font.pointSize: Theme.typeLabel
            font.weight: Theme.weightStrong
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Label {
            text: root.explanation
            color: Theme.onSurfaceVariant
            font.family: Theme.fontFamily
            font.pointSize: Theme.typeBody
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }
}
