import QtQuick

Item {
    id: root
    required property var source
    required property var controller
    property real viewScale: 1
    property var creatures: []
    readonly property int centerX: Math.floor(controller.visualX)
    readonly property int centerY: Math.floor(controller.visualY)
    function refresh() {
        creatures = visible && source && typeof source.previewMonsterLabels === "function"
                ? source.previewMonsterLabels(centerX, centerY, controller.z,
                                              Math.ceil(width / 32), Math.ceil(height / 32)) : []
    }
    onCenterXChanged: refresh()
    onCenterYChanged: refresh()
    onWidthChanged: refresh()
    onHeightChanged: refresh()
    onVisibleChanged: refresh()
    onSourceChanged: refresh()
    Component.onCompleted: refresh()
    Connections {
        target: root.controller
        function onZChanged() { root.refresh() }
    }
    Connections {
        target: root.source
        ignoreUnknownSignals: true
        function onContentUpdated() { root.refresh() }
        function onViewFlagsChanged() { root.refresh() }
        function onReadersChanged() { root.refresh() }
    }
    Repeater {
        model: root.creatures
        delegate: Item {
            objectName: "monsterInformation"
            required property var modelData
            width: 27; height: 4
            // Map creatures are rendered without the preview player's 8px displacement.
            x: (modelData.x - root.controller.visualX) * 32 + root.width / 2
               - modelData.elevation - width / 2
            y: (modelData.y - root.controller.visualY) * 32 + root.height / 2
               - modelData.elevation - 18
            scale: 1 / Math.max(0.01, root.viewScale)
            transformOrigin: Item.Top
            OtclientName {
                anchors.horizontalCenter: parent.horizontalCenter
                y: -12
                text: modelData.name
                hostile: !modelData.npc
                npc: !!modelData.npc
            }
            Rectangle {
                anchors.fill: parent
                color: "black"
                Rectangle { objectName: "monsterHealthFill"; x: 1; y: 1; width: 25; height: 2; color: modelData.npc ? "#66ccff" : "#bf0a0a" }
            }
        }
    }
}
