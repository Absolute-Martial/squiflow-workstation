import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SquiFlow
import "../common"

SectionCard {
    id: root
    property var bridge: null
    title: qsTr("Order lines")
    subtitle: qsTr("Prices and totals are resolved by C++ and stored as exact minor units.")
    ListView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 180
        model: root.bridge ? root.bridge.draftLines : []
        reuseItems: true
        delegate: ItemDelegate {
            required property var modelData
            width: ListView.view.width
            text: modelData.description + "  " + modelData.quantityText + "  " + modelData.amountText
            font.family: Theme.fontFamily
            font.pointSize: Theme.typeBody
            Accessible.description: modelData.priceEvidenceText
        }
    }
    RowLayout {
        Layout.fillWidth: true
        TextField {
            id: productSearch
            Layout.fillWidth: true
            enabled: root.bridge && !root.bridge.pending
            placeholderText: qsTr("Search product or scan barcode")
            Accessible.name: qsTr("Product search")
            onAccepted: if (root.bridge) root.bridge.searchProduct(text)
        }
        Button {
            text: qsTr("Add line")
            enabled: root.bridge && root.bridge.canAddLine && !root.bridge.pending
            onClicked: root.bridge.addSelectedProduct()
        }
    }
}
