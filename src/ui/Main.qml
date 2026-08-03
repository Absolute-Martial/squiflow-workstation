import QtQuick
import QtQuick.Controls
import SquiFlow
ApplicationWindow {id: root;required property var applicationSurface;width: 1180;height: 760;minimumWidth: 800;minimumHeight: 560;visible: true;title: qsTr("SquiFlow");color: Theme.background;onClosing: close => {close.accepted=false;applicationSurface.requestShutdown()}Column {anchors.centerIn: parent;spacing: Theme.spacing;Label{text: root.title;font.pixelSize: Theme.titleSize}Label{text: qsTr("Ready");color: Theme.mutedText}}}
