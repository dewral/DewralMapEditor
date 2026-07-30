pragma ComponentBehavior: Bound

import QtQuick

Canvas {
    id: overlay

    required property var mapCtrl
    required property var settings

    property var entries: []
    property string dataKey: ""

    function refreshData(force) {
        if (!mapCtrl)
            return;
        const tileSize = Math.max(1, mapCtrl.tileSize);
        const key = mapCtrl.floor + ":"
                + Math.floor(mapCtrl.glOriginX()) + ":"
                + Math.floor(mapCtrl.glOriginY()) + ":"
                + Math.ceil(width / tileSize) + ":"
                + Math.ceil(height / tileSize) + ":"
                + tileSize + ":"
                + settings.showTooltips + ":"
                + settings.showWaypoints;
        if (force || key !== dataKey) {
            dataKey = key;
            entries = mapCtrl.mapOverlayData(settings.showTooltips,
                                             settings.showWaypoints);
        }
        requestPaint();
    }

    function drawClientBox(ctx) {
        const tileSize = Math.max(1, mapCtrl.tileSize);
        const originX = mapCtrl.glOriginX();
        const originY = mapCtrl.glOriginY();
        const playerX = Math.floor(originX + width / tileSize / 2);
        const playerY = Math.floor(originY + height / tileSize / 2);
        const startX = (playerX - 8 - originX) * tileSize;
        const startY = (playerY - 6 - originY) * tileSize;
        const boxWidth = 17 * tileSize;
        const boxHeight = 13 * tileSize;
        const endX = startX + boxWidth;
        const endY = startY + boxHeight;

        ctx.fillStyle = "rgba(0, 0, 0, 0.66)";
        ctx.fillRect(0, 0, width, Math.max(0, startY));
        ctx.fillRect(0, Math.min(height, endY), width,
                     Math.max(0, height - endY));
        ctx.fillRect(0, Math.max(0, startY), Math.max(0, startX),
                     Math.min(height, endY) - Math.max(0, startY));
        ctx.fillRect(Math.min(width, endX), Math.max(0, startY),
                     Math.max(0, width - endX),
                     Math.min(height, endY) - Math.max(0, startY));

        ctx.lineWidth = 1.5;
        ctx.strokeStyle = "#f85149";
        ctx.strokeRect(startX + 0.5, startY + 0.5,
                       boxWidth - 1, boxHeight - 1);
        ctx.strokeStyle = "#3fb950";
        ctx.strokeRect(startX + tileSize + 0.5,
                       startY + tileSize + 0.5,
                       boxWidth - tileSize * 2 - 1,
                       boxHeight - tileSize * 2 - 1);
        ctx.strokeRect((playerX - originX) * tileSize + 0.5,
                       (playerY - originY) * tileSize + 0.5,
                       tileSize - 1, tileSize - 1);
    }

    function drawWaypoint(ctx, centerX, centerY) {
        const radius = Math.max(5, Math.min(11, mapCtrl.tileSize * 0.32));
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

        const boxWidth = Math.min(width - 4, textWidth + 10);
        const boxHeight = lines.length * lineHeight + 7;
        let x = anchorX - boxWidth / 2;
        let y = anchorY - boxHeight - 6;
        x = Math.max(2, Math.min(width - boxWidth - 2, x));
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

    onPaint: {
        const ctx = getContext("2d");
        ctx.reset();
        ctx.clearRect(0, 0, width, height);
        if (!mapCtrl)
            return;

        if (settings.showClientBox)
            drawClientBox(ctx);

        const tileSize = Math.max(1, mapCtrl.tileSize);
        const originX = mapCtrl.glOriginX();
        const originY = mapCtrl.glOriginY();
        for (let i = 0; i < entries.length; ++i) {
            const entry = entries[i];
            const centerX = (entry.x + 0.5 - originX) * tileSize;
            const centerY = (entry.y + 0.5 - originY) * tileSize;
            if (entry.kind === "waypoint" && settings.showWaypoints)
                drawWaypoint(ctx, centerX, centerY);
            if (settings.showTooltips && entry.text.length > 0)
                drawTooltip(ctx, entry.text, centerX,
                            (entry.y - originY) * tileSize,
                            entry.kind === "waypoint");
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
        function onShowClientBoxChanged() { overlay.refreshData(false); }
        function onShowTooltipsChanged() { overlay.refreshData(true); }
        function onShowWaypointsChanged() { overlay.refreshData(true); }
    }

    Component.onCompleted: refreshData(true)
}
