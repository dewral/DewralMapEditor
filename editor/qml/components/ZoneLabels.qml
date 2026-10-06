import QtQuick
import QtQuick.Controls
import "../themes/fluent/Colors.js" as Colors

Item {
    id: root
    required property var mapView
    property var regions: []
    property bool dirty: true
    property real originX: mapView.renderOriginX()
    property real originY: mapView.renderOriginY()
    Connections {
        target: root.mapView
        function onContentUpdated() {
            root.originX = root.mapView.renderOriginX()
            root.originY = root.mapView.renderOriginY()
            root.dirty = true
        }
        function onTileSizeChanged() { root.dirty = true }
        function onViewFlagsChanged() { root.dirty = true }
    }
    onWidthChanged: dirty = true
    onHeightChanged: dirty = true
    function refreshRegions() {
        const next = mapView.visibleZoneLabels();
        next.sort((a,b) => a.worldX - b.worldX || a.worldY - b.worldY || a.name.localeCompare(b.name));
        const keys = ["worldX", "worldY", "name", "color", "flags"];
        let changed = next.length !== regions.length;
        for (let i = 0; !changed && i < next.length; ++i)
            for (const key of keys)
                if (next[i][key] !== regions[i][key]) { changed = true; break; }
        // Camera and pointer updates do not replace the model. Existing labels
        // already follow the camera through their world-coordinate bindings.
        if (changed) regions = next;
        dirty = false;
    }
    Timer {
        interval: 150; running: root.visible; repeat: true; triggeredOnStart: true
        onTriggered: if (root.dirty && !root.mapView.zoneLabelEditInProgress()) root.refreshRegions()
    }
    Repeater {
        model: root.regions
        delegate: Rectangle {
            id: label
            objectName: "zoneLabel" + index
            required property int index
            required property var modelData
            x: (modelData.worldX - root.originX) * root.mapView.tileSize - width / 2
            y: (modelData.worldY - root.originY) * root.mapView.tileSize - height / 2
            width: caption.implicitWidth + 30; height: 24; radius: 4
            color: "#e6191d1f"
            border { width: hover.hovered || (root.mapView.activeZone & modelData.flags) ? 2 : 1; color: modelData.color }
            HoverHandler { id: hover }
            ToolTip.visible: hover.hovered
            ToolTip.delay: 250
            ToolTip.text: modelData.name + "\n" + ((modelData.flags & 1) ? "Protection Zone\n" : "")
                + ((modelData.flags & 4) ? "Non-PvP\n" : "") + ((modelData.flags & 8) ? "No Logout\n" : "")
                + ((modelData.flags & 16) ? "PvP\n" : "")
            Rectangle { x: 9; anchors.verticalCenter: parent.verticalCenter; width: 6; height: 6; radius: label.modelData.name.startsWith("Spawn") ? 0 : 3; color: label.modelData.color }
            Text {
                id: caption
                x: 21; anchors.verticalCenter: parent.verticalCenter
                text: label.modelData.name
                color: Colors.c("text")
                font { family: Colors.fontFamily; pixelSize: 12 }
            }
        }
    }
}
