import QtQuick
import SquiFlow
import "../common"

OperationalModulePage {
    required property var navigationBridge
    pageTitle: qsTr("Administration and settings")
    pageSubtitle: qsTr("Manage activation, people, roles, rights, numbering, diagnostics, appearance, language, cache, support locations, and version without exposing secrets.")
    createText: qsTr("Add person")
    emptyText: qsTr("No administration records")
    guidanceText: qsTr("Manage activation, people, roles, rights, numbering, diagnostics, appearance, language, cache, support locations, and version without exposing secrets.")
    capabilityLabels: [qsTr('Modules'), qsTr('People and rights'), qsTr('Numbering'), qsTr('Sync diagnostics'), qsTr('Appearance and support')]
}
