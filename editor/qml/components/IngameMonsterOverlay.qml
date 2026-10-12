import QtQuick

Item {
    id: root
    required property var source
    required property var controller
    property real viewScale: 1
    property bool alignToMapPixels: false
    // QRhi rounds the viewport origin in screen pixels, not individual labels.
    readonly property real renderedCenterX: alignToMapPixels
        ? Math.round((controller.visualX + 0.5 - width / 64) * 32 * viewScale)
          / (32 * viewScale) + width / 64 - 0.5 : controller.visualX
    readonly property real renderedCenterY: alignToMapPixels
        ? Math.round((controller.visualY + 0.5 - height / 64) * 32 * viewScale)
          / (32 * viewScale) + height / 64 - 0.5 : controller.visualY
    property var creatures: []
    readonly property int centerX: Math.floor(controller.visualX)
    readonly property int centerY: Math.floor(controller.visualY)
    property string cachedKey: ""
    property string creatureKey: ""
    property int cachedX: 0
    property int cachedY: 0
    readonly property int padding: Math.max(0, Math.min(6,
        Math.floor((512 - Math.ceil(width / 32)) / 2),
        Math.floor((512 - Math.ceil(height / 32)) / 2)))
    clip: true
    ListModel { id: labels }
    function syncLabels(next) {
        const desired = {};
        for (const entry of next) desired[entry.x + ":" + entry.y] = entry;
        for (let i = labels.count - 1; i >= 0; --i) {
            const row = labels.get(i);
            if (!desired[row.tileX + ":" + row.tileY]) labels.remove(i);
        }
        const indexes = {};
        for (let i = 0; i < labels.count; ++i) {
            const row = labels.get(i);
            indexes[row.tileX + ":" + row.tileY] = i;
        }
        for (const entry of next) {
            const row = {labelName: entry.name, tileX: entry.x, tileY: entry.y,
                         elevation: entry.elevation, isNpc: !!entry.npc};
            const index = indexes[entry.x + ":" + entry.y];
            if (index === undefined) labels.append(row);
            else {
                const old = labels.get(index);
                for (const key of Object.keys(row))
                    if (old[key] !== row[key]) labels.setProperty(index, key, row[key]);
            }
        }
    }
    function refresh(force) {
        const key = controller.z + ":" + Math.ceil(width / 32) + ":" + Math.ceil(height / 32)
                  + ":" + visible;
        if (!force && key === cachedKey && Math.abs(centerX - cachedX) <= padding
                && Math.abs(centerY - cachedY) <= padding) return;
        cachedKey = key; cachedX = centerX; cachedY = centerY;
        const next = visible && source && typeof source.previewMonsterLabels === "function"
                ? source.previewMonsterLabels(centerX, centerY, controller.z,
                                              Math.ceil(width / 32) + padding * 2,
                                              Math.ceil(height / 32) + padding * 2) : [];
        const nextKey = JSON.stringify(next);
        if (nextKey !== creatureKey) {
            creatureKey = nextKey;
            creatures = next;
            syncLabels(next);
        }
    }
    onCenterXChanged: refresh()
    onCenterYChanged: refresh()
    onWidthChanged: refresh()
    onHeightChanged: refresh()
    onVisibleChanged: refresh()
    onSourceChanged: refresh(true)
    Component.onCompleted: refresh()
    Connections {
        target: root.controller
        function onZChanged() { root.refresh(true) }
    }
    Connections {
        target: root.source
        ignoreUnknownSignals: true
        function onContentUpdated() { root.refresh(!root.source || !root.source.otbm) }
        function onViewFlagsChanged() { root.refresh(true) }
        function onReadersChanged() { root.refresh(true) }
    }
    Connections {
        target: root.source && root.source.otbm ? root.source.otbm : null
        ignoreUnknownSignals: true
        function onMapChanged() { root.refresh(true) }
        function onLoadedChanged() { root.refresh(true) }
    }
    Repeater {
        model: labels
        delegate: Item {
            objectName: "monsterInformation"
            required property string labelName
            required property int tileX
            required property int tileY
            required property int elevation
            required property bool isNpc
            width: 27; height: 4
            // Map creatures are rendered without the preview player's 8px displacement.
            x: (tileX - root.renderedCenterX) * 32 + root.width / 2
               - elevation - width / 2
            y: (tileY - root.renderedCenterY) * 32 + root.height / 2
               - elevation - 18
            scale: 1 / Math.max(0.01, root.viewScale)
            transformOrigin: Item.Top
            OtclientName {
                anchors.horizontalCenter: parent.horizontalCenter
                y: -12
                text: labelName
                hostile: !isNpc
                npc: !!isNpc
            }
            Rectangle {
                anchors.fill: parent
                color: "black"
                Rectangle { objectName: "monsterHealthFill"; x: 1; y: 1; width: 25; height: 2; color: isNpc ? "#66ccff" : "#bf0a0a" }
            }
        }
    }
}
