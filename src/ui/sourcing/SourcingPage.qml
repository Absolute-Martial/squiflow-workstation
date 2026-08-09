import QtQuick
import SquiFlow
import "../common"

OperationalModulePage {
    required property var navigationBridge
    pageTitle: qsTr("Sourcing")
    pageSubtitle: qsTr("Review supplier history and authoritative debt before recording a purchase or confirming an exactly-once settlement.")
    createText: qsTr("Record purchase")
    emptyText: qsTr("No suppliers or purchases")
    guidanceText: qsTr("Review supplier history and authoritative debt before recording a purchase or confirming an exactly-once settlement.")
    capabilityLabels: [qsTr('Supplier profile'), qsTr('Purchase history'), qsTr('Outstanding debt'), qsTr('Settlement confirmation')]
}
