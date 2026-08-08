import QtQuick
import SquiFlow
import "../common"

OperationalModulePage {
    required property var navigationBridge
    pageTitle: qsTr("Quotations")
    pageSubtitle: qsTr("Prepare drafts, preserve issued revisions, and use domain-authorized issue, revise, accept, expire, conversion, and PDF actions.")
    createText: qsTr("Create quotation")
    emptyText: qsTr("No quotations")
    guidanceText: qsTr("Prepare drafts, preserve issued revisions, and use domain-authorized issue, revise, accept, expire, conversion, and PDF actions.")
    capabilityLabels: [qsTr('Draft lines'), qsTr('Revision history'), qsTr('Issue and PDF'), qsTr('Accept or expire'), qsTr('Quote to order')]
}
