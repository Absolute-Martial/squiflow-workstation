import QtQuick
import SquiFlow
import "../common"

PrimaryListPage {
    id: root
    property var ordersBridge: null
    commandBridge: ordersBridge
    pageTitle: qsTr("Orders")
    pageSubtitle: qsTr("Agreed work, frozen prices, and lifecycle evidence")
    emptyTitle: qsTr("No orders yet")
    createText: qsTr("New order")
    detailComponent: orderDetail

    onCreateRequested: orderEditor.openForCreate()
    onRecordSelected: stableId => {
        if (ordersBridge) ordersBridge.load(stableId)
    }

    Component { id: orderDetail; OrderDetailPage {} }
    OrderEditorPage { id: orderEditor; bridge: root.ordersBridge }
}
