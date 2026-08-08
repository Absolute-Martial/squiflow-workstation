import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentControls 1.0 as FC
import SquiFlow

Control {
    id: root
    required property var fields
    property bool pending: false
    property string formError: ""
    property int visualState: 0
    signal fieldEdited(string id, string value)
    signal submitRequested()
    signal cancelRequested()

    states: [
        State { name: "pending"; when: root.pending; PropertyChanges { target: root; visualState: 2 } },
        State { name: "invalid"; when: !root.pending && root.formError.length > 0; PropertyChanges { target: root; visualState: 1 } },
        State { name: "idle"; when: !root.pending && root.formError.length === 0; PropertyChanges { target: root; visualState: 0 } }
    ]

    contentItem: ColumnLayout {
        spacing: Theme.space12
        StatusBanner {
            Layout.fillWidth: true
            visible: root.visualState === 1
            kind: StatusBanner.Error
            title: qsTr("Review the highlighted fields")
            detail: root.formError
        }
        Repeater {
            model: root.fields
            delegate: ColumnLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: Theme.space4
                Label {
                    text: modelData.label + (modelData.required ? " *" : "")
                    color: Theme.onSurface
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.typeLabel
                }
                TextField {
                    Layout.fillWidth: true
                    text: modelData.value
                    enabled: !root.pending
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.typeBody
                    Accessible.name: modelData.label
                    Accessible.description: modelData.error
                    onEditingFinished: root.fieldEdited(modelData.id, text)
                }
                Label {
                    visible: modelData.error.length > 0
                    text: modelData.error
                    color: Theme.error
                    font.family: Theme.fontFamily
                    font.pointSize: Theme.typeCaption
                    Accessible.role: Accessible.AlertMessage
                }
            }
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            Button {
                text: qsTr("Cancel")
                enabled: !root.pending
                font.family: Theme.fontFamily
                font.pointSize: Theme.typeLabel
                onClicked: root.cancelRequested()
            }
            Button {
                text: root.pending ? qsTr("Saving…") : qsTr("Save")
                enabled: !root.pending
                highlighted: true
                font.family: Theme.fontFamily
                font.pointSize: Theme.typeLabel
                onClicked: root.submitRequested()
            }
            FC.BusyIndicator {
                running: root.pending && !Theme.reducedMotion
                visible: root.pending
                Accessible.name: qsTr("Saving changes")
            }
        }
    }
}
