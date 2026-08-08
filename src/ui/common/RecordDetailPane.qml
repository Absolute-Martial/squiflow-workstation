import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SquiFlow

Pane {
    id: root
    required property var bridge
    padding: Theme.spacingMd
    Accessible.name: bridge && bridge.hasRecord ? bridge.title : qsTr("Record details")

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: Theme.spacingMd

            EmptyState {
                Layout.fillWidth: true
                visible: !root.bridge || (!root.bridge.loading && !root.bridge.hasRecord)
                title: qsTr("Select a record")
                detail: qsTr("Authoritative details, history, and permitted actions will appear here.")
            }

            LoadingSkeleton {
                Layout.fillWidth: true
                visible: root.bridge && root.bridge.loading
            }

            SectionCard {
                Layout.fillWidth: true
                visible: root.bridge && root.bridge.hasRecord
                title: root.bridge ? root.bridge.title : ""
                subtitle: root.bridge ? root.bridge.subtitle : ""

                Repeater {
                    model: root.bridge ? root.bridge.fields : []
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        Label {
                            Layout.preferredWidth: 150
                            text: modelData.label
                            color: Theme.textSecondary
                            wrapMode: Text.Wrap
                        }
                        Label {
                            Layout.fillWidth: true
                            text: modelData.value
                            color: Theme.textPrimary
                            wrapMode: Text.Wrap
                            Accessible.name: modelData.label + ": " + modelData.value
                        }
                    }
                }
            }

            SectionCard {
                Layout.fillWidth: true
                visible: root.bridge && root.bridge.lines.length > 0
                title: qsTr("Lines and related items")
                Repeater {
                    model: root.bridge ? root.bridge.lines : []
                    delegate: ColumnLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        Label { text: modelData.title; font.weight: Font.DemiBold; wrapMode: Text.Wrap }
                        Label { text: modelData.subtitle; color: Theme.textSecondary; wrapMode: Text.Wrap }
                        Label {
                            text: [modelData.quantity, modelData.amount].filter(value => value.length > 0).join(" · ")
                            color: Theme.textSecondary
                        }
                    }
                }
            }

            SectionCard {
                Layout.fillWidth: true
                visible: root.bridge && root.bridge.actions.length > 0
                title: qsTr("Available actions")
                subtitle: qsTr("Only actions authorized for the current session and record are shown.")
                Flow {
                    Layout.fillWidth: true
                    spacing: Theme.spacingSm
                    Repeater {
                        model: root.bridge ? root.bridge.actions : []
                        delegate: Button {
                            required property var modelData
                            text: modelData.label
                            enabled: root.bridge && !root.bridge.pending
                            Accessible.name: text
                            onClicked: root.bridge.executeAction(modelData.id, {})
                        }
                    }
                }
            }

            SectionCard {
                Layout.fillWidth: true
                visible: root.bridge && root.bridge.history.length > 0
                title: qsTr("History")
                Repeater {
                    model: root.bridge ? root.bridge.history : []
                    delegate: ColumnLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        Label { text: modelData.label; font.weight: Font.DemiBold }
                        Label { text: modelData.detail; color: Theme.textSecondary; wrapMode: Text.Wrap }
                        Label { text: modelData.occurredAt; color: Theme.textTertiary }
                    }
                }
            }
        }
    }
}
