pragma ComponentBehavior: Bound

import QtQuick
import Tibia 1.0

Item {
    id: overlay

    required property var mapCtrl
    required property var settings

    property var entries: []
    property string dataKey: ""
    property real currentOriginX: 0
    property real currentOriginY: 0
    property real paintedOriginX: 0
    property real paintedOriginY: 0
    readonly property real currentTileSize: mapCtrl ? Math.max(1, mapCtrl.tileSize) : 1
    readonly property real canvasMargin: 64

    clip: true
    visible: settings.showClientBox || settings.showTooltips || settings.showWaypoints

    function refreshData(force) {
        if (!mapCtrl)
            return;

        const originX = mapCtrl.glOriginX();
        const originY = mapCtrl.glOriginY();
        currentOriginX = originX;
        currentOriginY = originY;

        const key = mapCtrl.floor + ":"
                + Math.floor(originX) + ":" + Math.floor(originY) + ":"
                + Math.ceil(width / currentTileSize) + ":"
                + Math.ceil(height / currentTileSize) + ":"
                + currentTileSize + ":" + settings.showTooltips + ":"
                + settings.showWaypoints;
        if (!force && key === dataKey)
            return;

        dataKey = key;
        entries = mapCtrl.mapOverlayData(settings.showTooltips,
                                         settings.showWaypoints);
        paintedOriginX = originX;
        paintedOriginY = originY;
        worldCanvas.requestPaint();
    }

    function drawWaypoint(ctx, centerX, centerY) {
        const radius = Math.max(5, Math.min(11, currentTileSize * 0.32));
        ctx.beginPath();
        ctx.moveTo(centerX, centerY + radius * 1.35);
        ctx.lineTo(centerX - radius * 0.55, centerY + radius * 0.35);
        ctx.lineTo(centerX + radius * 0.55, centerY + radius * 0.35);
        ctx.closePath();
        ctx.fillStyle = "#2388ff";
        ctx.fill();

        ctx.beginPath();
        ctx.arc(centerX, centerY, radius, 0, Math.PI * 2);
        ctx.fillStyle = "#2388ff";
        ctx.fill();
        ctx.lineWidth = 1.5;
        ctx.strokeStyle = "#8dccff";
        ctx.stroke();

        ctx.font = "bold " + Math.max(8, Math.round(radius)) + "px sans-serif";
        ctx.textAlign = "center";
        ctx.textBaseline = "middle";
        ctx.fillStyle = "#ffffff";
        ctx.fillText("W", centerX, centerY + 0.5);
    }

    function drawTooltip(ctx, text, anchorX, anchorY, waypoint) {
        if (!text || text.length === 0)
            return;

        const lines = text.split("\n");
        const lineHeight = 13;
        ctx.font = "11px sans-serif";
        ctx.textAlign = "left";
        ctx.textBaseline = "top";

        let textWidth = 0;
        for (let i = 0; i < lines.length; ++i)
            textWidth = Math.max(textWidth, ctx.measureText(lines[i]).width);

        const boxWidth = Math.min(worldCanvas.width - 4, textWidth + 10);
        const boxHeight = lines.length * lineHeight + 7;
        let x = anchorX - boxWidth / 2;
        let y = anchorY - boxHeight - 6;
        x = Math.max(2, Math.min(worldCanvas.width - boxWidth - 2, x));
        if (y < 2)
            y = anchorY + 8;

        ctx.fillStyle = "rgba(12, 14, 18, 0.92)";
        ctx.fillRect(x, y, boxWidth, boxHeight);
        ctx.lineWidth = 1;
        ctx.strokeStyle = waypoint ? "#3fb950" : "#8b949e";
        ctx.strokeRect(x + 0.5, y + 0.5, boxWidth - 1, boxHeight - 1);
        ctx.fillStyle = "#f0f0f0";
        for (let line = 0; line < lines.length; ++line)
            ctx.fillText(lines[line], x + 5, y + 4 + line * lineHeight);
    }

    Canvas {
        id: worldCanvas

        width: overlay.width + overlay.canvasMargin * 2
        height: overlay.height + overlay.canvasMargin * 2
        x: -overlay.canvasMargin
           + (overlay.paintedOriginX - overlay.currentOriginX) * overlay.currentTileSize
        y: -overlay.canvasMargin
           + (overlay.paintedOriginY - overlay.currentOriginY) * overlay.currentTileSize
        visible: overlay.settings.showTooltips || overlay.settings.showWaypoints

        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            ctx.clearRect(0, 0, width, height);

            const tileSize = overlay.currentTileSize;
            const margin = overlay.canvasMargin;
            for (let i = 0; i < overlay.entries.length; ++i) {
                const entry = overlay.entries[i];
                const centerX = (entry.x + 0.5 - overlay.paintedOriginX) * tileSize + margin;
                const centerY = (entry.y + 0.5 - overlay.paintedOriginY) * tileSize + margin;
                if (entry.kind === "waypoint" && overlay.settings.showWaypoints)
                    overlay.drawWaypoint(ctx, centerX, centerY);
                if (overlay.settings.showTooltips && entry.text.length > 0)
                    overlay.drawTooltip(ctx, entry.text, centerX,
                                        (entry.y - overlay.paintedOriginY) * tileSize + margin,
                                        entry.kind === "waypoint");
            }
        }
    }

    Item {
        id: clientBox

        readonly property int playerX: Math.floor(overlay.currentOriginX
                                                  + overlay.width / overlay.currentTileSize / 2)
        readonly property int playerY: Math.floor(overlay.currentOriginY
                                                  + overlay.height / overlay.currentTileSize / 2)
        readonly property real boxX: (playerX - 8 - overlay.currentOriginX)
                                             * overlay.currentTileSize
        readonly property real boxY: (playerY - 6 - overlay.currentOriginY)
                                             * overlay.currentTileSize
        readonly property real boxWidth: 17 * overlay.currentTileSize
        readonly property real boxHeight: 13 * overlay.currentTileSize

        anchors.fill: parent
        visible: overlay.settings.showClientBox

        Rectangle {
            x: 0
            y: 0
            width: parent.width
            height: Math.max(0, clientBox.boxY)
            color: "#a8000000"
        }
        Rectangle {
            x: 0
            y: Math.min(parent.height, clientBox.boxY + clientBox.boxHeight)
            width: parent.width
            height: Math.max(0, parent.height - y)
            color: "#a8000000"
        }
        Rectangle {
            x: 0
            y: Math.max(0, clientBox.boxY)
            width: Math.max(0, clientBox.boxX)
            height: Math.max(0, Math.min(parent.height, clientBox.boxY + clientBox.boxHeight) - y)
            color: "#a8000000"
        }
        Rectangle {
            x: Math.min(parent.width, clientBox.boxX + clientBox.boxWidth)
            y: Math.max(0, clientBox.boxY)
            width: Math.max(0, parent.width - x)
            height: Math.max(0, Math.min(parent.height, clientBox.boxY + clientBox.boxHeight) - y)
            color: "#a8000000"
        }
        Rectangle {
            x: clientBox.boxX + 0.5
            y: clientBox.boxY + 0.5
            width: clientBox.boxWidth - 1
            height: clientBox.boxHeight - 1
            color: "transparent"
            border.width: 1
            border.color: "#f85149"
        }
        Rectangle {
            x: clientBox.boxX + overlay.currentTileSize + 0.5
            y: clientBox.boxY + overlay.currentTileSize + 0.5
            width: clientBox.boxWidth - overlay.currentTileSize * 2 - 1
            height: clientBox.boxHeight - overlay.currentTileSize * 2 - 1
            color: "transparent"
            border.width: 1
            border.color: "#3fb950"
        }
        Rectangle {
            x: (clientBox.playerX - overlay.currentOriginX) * overlay.currentTileSize + 0.5
            y: (clientBox.playerY - overlay.currentOriginY) * overlay.currentTileSize + 0.5
            width: overlay.currentTileSize - 1
            height: overlay.currentTileSize - 1
            color: "transparent"
            border.width: 1
            border.color: "#3fb950"
        }
    }

    onWidthChanged: refreshData(false)
    onHeightChanged: refreshData(false)

    Connections {
        target: overlay.mapCtrl
        function onContentUpdated() { overlay.refreshData(false); }
        function onFloorChanged() { overlay.refreshData(true); }
        function onTileSizeChanged() { overlay.refreshData(true); }
    }

    Connections {
        target: Backend.otbmReader
        function onMapChanged() { overlay.refreshData(true); }
        function onLoadedChanged() { overlay.refreshData(true); }
    }

    Connections {
        target: overlay.settings
        function onShowTooltipsChanged() { overlay.refreshData(true); }
        function onShowWaypointsChanged() { overlay.refreshData(true); }
    }

    Component.onCompleted: refreshData(true)
}
