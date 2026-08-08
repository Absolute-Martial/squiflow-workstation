import QtQuick
import SquiFlow
import "../common"

PrimaryListPage {
    id: root
    property var receivablesBridge: null
    commandBridge: receivablesBridge
    pageTitle: qsTr("Receivables")
    pageSubtitle: qsTr("Invoices, incoming payments, allocations, and statements")
    emptyTitle: qsTr("No invoices yet")
    createText: qsTr("New invoice")
    detailComponent: invoiceDetail

    onCreateRequested: {
        if (receivablesBridge) receivablesBridge.beginInvoice()
    }
    onRecordSelected: stableId => {
        if (receivablesBridge) receivablesBridge.loadInvoice(stableId)
    }

    Component { id: invoiceDetail; InvoiceDetailPage {} }
}
