import QtQuick
import QtQuick.Controls
import SquiFlow
import "../common"

PrimaryListPage {
    id: root
    property var partyBridge: null
    commandBridge: partyBridge
    pageTitle: qsTr("Parties")
    pageSubtitle: qsTr("Customers, suppliers, contacts, and billing terms")
    emptyTitle: qsTr("No customers or suppliers yet")
    createText: qsTr("New party")
    detailComponent: partyDetail

    onCreateRequested: partyEditor.openForCreate()
    onRecordSelected: stableId => {
        if (partyBridge) partyBridge.load(stableId)
    }

    Component { id: partyDetail; PartyDetailPage {} }
    PartyFormPage { id: partyEditor; bridge: root.partyBridge }
}
