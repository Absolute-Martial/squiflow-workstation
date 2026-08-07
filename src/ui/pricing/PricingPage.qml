import QtQuick
import SquiFlow
import "../common"

PrimaryListPage {
    id: root
    property var pricingBridge: null
    commandBridge: pricingBridge
    pageTitle: qsTr("Pricing")
    pageSubtitle: qsTr("Default, customer-specific, and time-bounded rates")
    emptyTitle: qsTr("No rates yet")
    createText: qsTr("New rate")
    detailComponent: rateDetail

    onCreateRequested: rateEditor.openForCreate()
    onRecordSelected: stableId => {
        if (pricingBridge) pricingBridge.load(stableId)
    }

    Component { id: rateDetail; RateDetailPage {} }
    RateFormPage { id: rateEditor; bridge: root.pricingBridge }
}
