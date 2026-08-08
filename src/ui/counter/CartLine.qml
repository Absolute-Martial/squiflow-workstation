import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SquiFlow

ItemDelegate {
    id: root
    required property var line
    signal removeRequested()
    signal quantityRequested(string value)
    width: ListView.view ? ListView.view.width : implicitWidth
    Accessible.name: line.description + ", " + line.amountText
    contentItem: RowLayout {
        spacing: Theme.space12
        ColumnLayout {
            Layout.fillWidth: true
            Label {
                text: root.line.description
                font.family: Theme.fontFamily
                font.pointSize: Theme.typeBody
                font.weight: Theme.weightStrong
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Label {
                text: root.line.priceEvidenceText
                color: Theme.onSurfaceVariant
                font.family: Theme.fontFamily
                font.pointSize: Theme.typeCaption
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
        TextField {
            text: root.line.quantityText
            enabled: root.enabled
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            validator: RegularExpressionValidator { regularExpression: /^[0-9]+([.][0-9]{0,3})?$/ }
            Accessible.name: qsTr("Quantity for") + " " + root.line.description
            onAccepted: root.quantityRequested(text)
        }
        Label {
            text: root.line.amountText
            font.family: Theme.fontFamily
            font.pointSize: Theme.typeBody
            font.weight: Theme.weightStrong
            Accessible.name: qsTr("Line total") + ": " + text
        }
        ToolButton {
            text: qsTr("Remove")
            enabled: root.enabled
            Accessible.name: qsTr("Remove") + " " + root.line.description
            onClicked: root.removeRequested()
        }
    }
}
