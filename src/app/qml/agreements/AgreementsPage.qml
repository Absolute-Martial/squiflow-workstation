import QtQuick
import SquiFlow
import "../common"

OperationalModulePage {
    required property var navigationBridge
    pageTitle: qsTr("Agreements")
    pageSubtitle: qsTr("Review effective rates, caps, periods, authoritative consumption, supersession, and only the lifecycle actions returned by the domain.")
    createText: qsTr("Create agreement")
    emptyText: qsTr("No agreements")
    guidanceText: qsTr("Review effective rates, caps, periods, authoritative consumption, supersession, and only the lifecycle actions returned by the domain.")
    capabilityLabels: [qsTr('Rates and caps'), qsTr('Periods'), qsTr('Consumption'), qsTr('Amendments'), qsTr('Supersession')]
}
