import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SquiFlow

AbstractButton {
    id: root
    property string label: ""
    property string value: "—"
    property string detail: ""
    property bool loading: false
    property color tone: Theme.primary
    property color stateColor: Theme.surfaceRaised
    Accessible.name: label + ": " + value + (detail.length ? ". " + detail : "")
    Accessible.description: loading ? qsTr("Loading metric") : detail
    focusPolicy: Qt.StrongFocus
    implicitWidth: 220
    implicitHeight: 132

    states: [
        State { name: "disabled"; when: !root.enabled; PropertyChanges { target: root; opacity: 0.55; stateColor: Theme.controlDisabled } },
        State { name: "loading"; when: root.enabled && root.loading; PropertyChanges { target: root; opacity: 0.75; stateColor: Theme.control } },
        State { name: "pressed"; when: root.enabled && root.down; PropertyChanges { target: root; stateColor: Theme.controlPressed } },
        State { name: "focused"; when: root.enabled && root.activeFocus; PropertyChanges { target: root; stateColor: Theme.selection } },
        State { name: "hovered"; when: root.enabled && root.hovered; PropertyChanges { target: root; stateColor: Theme.controlHover } },
        State { name: "idle"; when: root.enabled; PropertyChanges { target: root; opacity: 1; stateColor: Theme.surfaceRaised } }
    ]
    transitions: Transition {
        ColorAnimation { property: "stateColor"; duration: Theme.motionFast; easing.type: Theme.easingStandard }
        NumberAnimation { property: "opacity"; duration: Theme.motionFast; easing.type: Theme.easingStandard }
    }

    background: Rectangle {
        color: root.stateColor
        border.color: root.activeFocus ? Theme.focus : Theme.border
        border.width: root.activeFocus ? 2 : Theme.borderWidth
        radius: Theme.radiusLarge
    }
    contentItem: ColumnLayout {
        anchors.margins: Theme.space24
        spacing: Theme.space8
        Label {
            text: root.label
            color: Theme.onSurfaceVariant
            font.family: Theme.fontFamily
            font.pointSize: Theme.typeLabel
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        Label {
            text: root.loading ? qsTr("Loading…") : root.value
            color: root.tone
            font.family: Theme.fontFamily
            font.pointSize: Theme.typeTitleLarge
            font.weight: Theme.weightStrong
            Layout.fillWidth: true
        }
        Label {
            visible: root.detail.length > 0
            text: root.detail
            color: Theme.onSurfaceVariant
            font.family: Theme.fontFamily
            font.pointSize: Theme.typeCaption
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
    }
}
