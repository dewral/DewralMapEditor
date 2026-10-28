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
    property bool cacheValid: false
    property int cachedFloor: -1
    property int cachedColumns: 0
    property int cachedRows: 0
    property bool refreshQueued: false
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
                         elevation: entry.elevation, isNpc: !!entry.npc,
                         bitmapSource: entry.nameSource || "", bitmapWidth: entry.nameWidth || 0};
            const index = indexes[entry.x + ":" + entry.y];
            if (index === undefined) labels.append(row);
            else {
                const old = labels.get(index);
                for (const key of Object.keys(row))
                    if (old[key] !== row[key]) labels.setProperty(index, key, row[key]);
            }
        }
    }
    // Geometry bindings change together during zoom; query their final values once.
    function scheduleRefresh() {
        if (refreshQueued) return;
        refreshQueued = true;
        Qt.callLater(function() { root.refreshQueued = false; root.refresh(false); });
    }
    function refresh(force) {
        if (force) cacheValid = false;
        // Keep the last bounded viewport model across the zoom cutoff. Hidden
        // delegates cost no drawing and can be reused when names become visible.
        if (!visible) return;
        const columns = Math.ceil(width / 32);
        const rows = Math.ceil(height / 32);
        if (cacheValid && cachedFloor === controller.z
                && Math.abs(centerX - cachedX) + columns / 2 <= cachedColumns / 2
                && Math.abs(centerY - cachedY) + rows / 2 <= cachedRows / 2) return;
        const queryColumns = Math.min(512, Math.ceil((columns + padding * 2) / 16) * 16);
        const queryRows = Math.min(512, Math.ceil((rows + padding * 2) / 16) * 16);
        const next = source && typeof source.previewMonsterLabels === "function"
                ? source.previewMonsterLabels(centerX, centerY, controller.z,
                                              queryColumns, queryRows) : [];
        cachedFloor = controller.z; cachedX = centerX; cachedY = centerY;
        cachedColumns = queryColumns; cachedRows = queryRows; cacheValid = true;
        // syncLabels already compares individual roles; serializing PNG data URLs
        // for a second whole-list comparison adds work without avoiding updates.
        creatures = next;
        syncLabels(next);
    }
    onCenterXChanged: scheduleRefresh()
    onCenterYChanged: scheduleRefresh()
    onWidthChanged: scheduleRefresh()
    onHeightChanged: scheduleRefresh()
    onVisibleChanged: { if (visible) refresh(false); }
    onSourceChanged: refresh(true)
    Component.onCompleted: refresh()
    Connections {
        target: root.controller
        function onZChanged() { root.refresh(true) }
    }
    Connections {
        target: root.source
        ignoreUnknownSignals: true
        function onContentUpdated() {
            if (root.source && root.source.otbm) root.scheduleRefresh();
            else root.refresh(true);
        }
        function onViewFlagsChanged() { root.refresh(true) }
        function onReadersChanged() { root.refresh(true) }
        function onAtlasChanged() { root.refresh(true) }
    }
    Connections {
        target: root.source && root.source.otbm ? root.source.otbm : null
        ignoreUnknownSignals: true
        function onMapChanged() { root.refresh(true) }
        function onLoadedChanged() { root.refresh(true) }
    }
    Repeater {
        model: labels
        delegate: CreatureInformation {
            objectName: "monsterInformation"
            required property string labelName
            required property int tileX
            required property int tileY
            required property int elevation
            required property bool isNpc
            required property string bitmapSource
            required property int bitmapWidth
            anchorX: (tileX - root.renderedCenterX) * 32 + root.width / 2 - elevation
            anchorY: (tileY - root.renderedCenterY) * 32 + root.height / 2 - elevation - 16
            viewportWidth: root.width
            viewportHeight: root.height
            viewScale: root.viewScale
            npc: isNpc
            visible: anchorX >= 0 && anchorX <= viewportWidth
                && anchorY >= 0 && anchorY <= viewportHeight
            nameSource: bitmapSource
            nameWidth: bitmapWidth
        }
    }
}
