import QtQuick
import SquiFlow
import "../common"

OperationalModulePage {
    required property var navigationBridge
    pageTitle: qsTr("Companion")
    pageSubtitle: qsTr("Manage due work, attention, snooze, completion, recurrence summaries, and the next occurrence calculated by the domain.")
    createText: qsTr("Create task")
    emptyText: qsTr("No tasks")
    guidanceText: qsTr("Manage due work, attention, snooze, completion, recurrence summaries, and the next occurrence calculated by the domain.")
    capabilityLabels: [qsTr('List and board'), qsTr('Attention'), qsTr('Calendar'), qsTr('Snooze and complete'), qsTr('Recurrence')]
}
