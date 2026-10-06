import QtQuick
import Tibia 1.0

Rectangle {
    id: root
    required property Item targetItem
    parent: targetItem.parent
    property var colorKeys: []
    readonly property string selectedKey: Backend.uiTheme.highlightedColor || ""
    anchors.fill: targetItem
    z: 10000
    visible: pulse.running && pulse.step % 2 === 0
    color: "#30ffcc66"
    border.color: "#ffcc66"
    border.width: 2
    Timer {
        id: pulse
        property int step: 0
        interval: 180; repeat: true
        onTriggered: { step++; if (step >= 8) stop(); }
    }
    Connections {
        target: Backend.uiTheme
        ignoreUnknownSignals: true
        function onHighlightedColorChanged() {
            pulse.stop(); pulse.step = 0;
            if (Backend.uiTheme.style === "fluent-dark" && root.colorKeys.indexOf(root.selectedKey) >= 0) pulse.start();
        }
    }
}
