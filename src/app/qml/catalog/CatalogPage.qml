import QtQuick
import SquiFlow
import "../common"

PrimaryListPage {
    id: root
    property var catalogBridge: null
    commandBridge: catalogBridge
    pageTitle: qsTr("Catalog")
    pageSubtitle: qsTr("Products, services, descriptions, aliases, and history")
    emptyTitle: qsTr("No products or services yet")
    createText: qsTr("New catalog item")
    detailComponent: productDetail

    onCreateRequested: productEditor.openForCreate()
    onRecordSelected: stableId => {
        if (catalogBridge) catalogBridge.load(stableId)
    }

    Component { id: productDetail; ProductDetailPage {} }
    ProductFormPage { id: productEditor; bridge: root.catalogBridge }
}
