import QtQuick
import SquiFlow

Rectangle {
    id: root
    property bool running: true
    implicitHeight: 18
    radius: Theme.radiusSmall
    color: Theme.border
    opacity: running ? (Theme.reducedMotion ? 0.4 : 0.45) : 0
    Accessible.name: qsTr("Loading")

    states: [
        State {
            name: "idle"
            when: !root.running
            PropertyChanges { target: root; opacity: 0 }
        },
        State {
            name: "reduced"
            when: root.running && Theme.reducedMotion
            PropertyChanges { target: root; opacity: 0.4 }
        },
        State {
            name: "loading"
            when: root.running && !Theme.reducedMotion
            PropertyChanges { target: root; opacity: 0.45 }
        }
    ]
    transitions: Transition {
        NumberAnimation {
            property: "opacity"
            duration: Theme.motionFast
            easing.type: Theme.easingStandard
        }
    }
    SequentialAnimation on opacity {
        running: root.running && !Theme.reducedMotion
        loops: Animation.Infinite
        NumberAnimation { to: 0.2; duration: Theme.motionSlow * 2 }
        NumberAnimation { to: 0.55; duration: Theme.motionSlow * 2 }
    }
}
