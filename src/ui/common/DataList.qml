import QtQuick
import QtQuick.Controls
ListView {id: root;required property var sourceModel;model: sourceModel;reuseItems: true;clip: true;cacheBuffer: 400;ScrollBar.vertical: ScrollBar {}}
