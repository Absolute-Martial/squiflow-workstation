import QtQuick
import SquiFlow
import "../common"

OperationalModulePage {
    required property var navigationBridge
    pageTitle: qsTr("Jobs")
    pageSubtitle: qsTr("Coordinate assignments, milestones, related orders, parties, files, and bounded event-driven progress.")
    createText: qsTr("Create job")
    emptyText: qsTr("No jobs")
    guidanceText: qsTr("Coordinate assignments, milestones, related orders, parties, files, and bounded event-driven progress.")
    capabilityLabels: [qsTr('Assignments'), qsTr('Milestones'), qsTr('Status actions'), qsTr('Related records'), qsTr('Bounded progress')]
}
