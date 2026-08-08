import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SquiFlow
import "../common"

SectionCard {
    id: root
    property var bridge: null
    title: qsTr("Customer statement")
    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.space8
        Label {
            text: root.bridge ? root.bridge.statementPeriodText : qsTr("No statement selected")
            color: Theme.onSurfaceVariant
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        RowLayout {
            Button {
                text: qsTr("Prepare statement")
                enabled: root.bridge && root.bridge.canPrepareStatement && !root.bridge.pending
                onClicked: root.bridge.prepareStatement()
            }
            Button {
                text: qsTr("Send")
                enabled: root.bridge && root.bridge.canSendStatement && !root.bridge.pending && !root.bridge.unknownOutcome
                onClicked: root.bridge.sendStatement()
            }
        }
    }
}
