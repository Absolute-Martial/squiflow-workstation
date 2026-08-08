import QtQuick
import SquiFlow
import "../common"

OperationalModulePage {
    required property var navigationBridge
    pageTitle: qsTr("Files")
    pageSubtitle: qsTr("Search by stable file identity, inspect safe preview metadata and lineage, and handle missing or offline volumes without exposing trusted paths.")
    createText: qsTr("Add file")
    emptyText: qsTr("No files")
    guidanceText: qsTr("Search by stable file identity, inspect safe preview metadata and lineage, and handle missing or offline volumes without exposing trusted paths.")
    capabilityLabels: [qsTr('Search and grid'), qsTr('Safe preview'), qsTr('Record links'), qsTr('Version lineage'), qsTr('Forget with reason')]
}
