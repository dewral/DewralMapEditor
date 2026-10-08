pragma ComponentBehavior: Bound

import QtQuick
import Tibia 1.0
import "../themes/fluent/Colors.js" as Colors

Item {
    id: overlay

    required property var mapCtrl
    required property var settings

    property var entries: []
    property string dataKey: ""
    property string entriesKey: ""
    property bool refreshPending: true
    property bool forcePending: true
    function scheduleRefresh(force) { refreshPending = true; forcePending = forcePending || force; }
    property real currentOriginX: 0
    property real currentOriginY: 0
    property real paintedOriginX: 0
    property real paintedOriginY: 0
    readonly property real currentTileSize: mapCtrl ? Math.max(1, mapCtrl.tileSize) : 1
    readonly property int tooltipMinimumZoom: typeof settings.tooltipMinimumZoom === "number" ? settings.tooltipMinimumZoom : 0
    readonly property bool tooltipsVisible: settings.showTooltips && currentTileSize / 32 * 100 >= tooltipMinimumZoom
    onTooltipsVisibleChanged: scheduleRefresh(true)
    readonly property real canvasMargin: 64

    clip: true
    visible: settings.showClientBox || settings.showTooltips || settings.showWaypoints
             || settings.showHouses || settings.showLightSources

    function refreshData(force) {
        if (!mapCtrl)
            return;

        const originX = mapCtrl.renderOriginX();
        const originY = mapCtrl.renderOriginY();
        currentOriginX = originX;
        currentOriginY = originY;

        const key = mapCtrl.floor + ":"
                + Math.floor(originX) + ":" + Math.floor(originY) + ":"
                + Math.ceil(width / currentTileSize) + ":"
                + Math.ceil(height / currentTileSize) + ":"
                + currentTileSize + ":" + tooltipsVisible + ":"
                + settings.showWaypoints + ":" + settings.showHouses + ":"
                + settings.showLightSources;
        if (!force && key === dataKey)
            return;

        dataKey = key;
        const next = mapCtrl.mapOverlayData(tooltipsVisible,
                                         settings.showWaypoints, settings.showHouses,
                                         settings.showLightSources);
        const nextKey = JSON.stringify(next);
        const moved = Math.abs(originX - paintedOriginX) * currentTileSize > canvasMargin / 2
                   || Math.abs(originY - paintedOriginY) * currentTileSize > canvasMargin / 2;
        if (!force && nextKey === entriesKey && !moved) return;
        if (nextKey !== entriesKey) { entriesKey = nextKey; entries = next; }
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

    function drawHouseExit(ctx, centerX, centerY) {
        ctx.font = "bold " + Math.max(8, Math.min(12, Math.round(currentTileSize * 0.28))) + "px sans-serif";
        ctx.textAlign = "center";
        ctx.textBaseline = "middle";
        ctx.lineWidth = 3;
        ctx.strokeStyle = "#202020";
        ctx.strokeText("EXIT", centerX, centerY);
        ctx.fillStyle = "#ffd966";
        ctx.fillText("EXIT", centerX, centerY);
    }

    function drawLightSource(ctx, tileX, tileY, intensity, red, green, blue) {
        const tileSize = currentTileSize;
        const x = (tileX * tileSize) + canvasMargin;
        const y = (tileY * tileSize) + canvasMargin;
        const inset = Math.max(1, Math.round(tileSize * 0.12));
        const outerSize = Math.max(2, tileSize - inset * 2);
        const badgeSize = Math.max(9, Math.min(18, Math.round(tileSize * 0.46)));
        const badgeX = x + tileSize - badgeSize + 2;
        const badgeY = y + tileSize - badgeSize + 2;

        ctx.lineWidth = Math.max(1, Math.round(tileSize / 16));
        ctx.strokeStyle = "white";
        ctx.strokeRect(x + inset + 0.5, y + inset + 0.5,
                       outerSize - 1, outerSize - 1);

        ctx.fillStyle = Qt.rgba(red / 255, green / 255, blue / 255, 1.0);
        ctx.fillRect(badgeX, badgeY, badgeSize, badgeSize);
        ctx.lineWidth = 1;
        ctx.strokeStyle = "black";
        ctx.strokeRect(badgeX + 0.5, badgeY + 0.5, badgeSize - 1, badgeSize - 1);
        ctx.font = "bold " + Math.max(7, Math.min(11, Math.round(badgeSize * 0.62))) + "px sans-serif";
        ctx.textAlign = "center";
        ctx.textBaseline = "middle";
        ctx.fillStyle = "white";
        ctx.strokeStyle = "black";
        ctx.lineWidth = 2;
        ctx.strokeText(String(intensity), badgeX + badgeSize / 2,
                       badgeY + badgeSize / 2 + 0.5);
        ctx.fillText(String(intensity), badgeX + badgeSize / 2,
                     badgeY + badgeSize / 2 + 0.5);
    }

    function containerImageSource(item) {
        if (!item || !item.spriteIds || item.spriteIds.length === 0)
            return "";
        return Backend.sprReader.itemImageSource(item.spriteIds,
                                                 item.itemWidth || 1,
                                                 item.itemHeight || 1,
                                                 item.layers || 1);
    }

    readonly property var containerEntries: entries.filter(function(entry) {
        return entry.kind === "container";
    })

    Canvas {
        id: worldCanvas

        width: overlay.width + overlay.canvasMargin * 2
        height: overlay.height + overlay.canvasMargin * 2
        x: -overlay.canvasMargin
           + (overlay.paintedOriginX - overlay.currentOriginX) * overlay.currentTileSize
        y: -overlay.canvasMargin
           + (overlay.paintedOriginY - overlay.currentOriginY) * overlay.currentTileSize
        visible: overlay.settings.showTooltips || overlay.settings.showWaypoints
                 || overlay.settings.showHouses || overlay.settings.showLightSources

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
                if (entry.kind === "house_exit" && overlay.settings.showHouses && !overlay.mapCtrl.modernZones)
                    overlay.drawHouseExit(ctx, centerX, centerY);
                if (entry.kind === "light_source" && overlay.settings.showLightSources)
                    overlay.drawLightSource(ctx, entry.x - overlay.paintedOriginX,
                                            entry.y - overlay.paintedOriginY,
                                            entry.intensity, entry.red, entry.green,
                                            entry.blue);

            }
        }
    }

    readonly property var textEntries: entries.filter(function(entry) {
        return entry.kind !== "container" && entry.kind !== "house_exit" && entry.text.length > 0;
    })
    Repeater {
        model: overlay.tooltipsVisible ? overlay.textEntries : []
        delegate: Rectangle {
            required property var modelData
            readonly property real anchorX: (modelData.x + 0.5 - overlay.currentOriginX) * overlay.currentTileSize
            readonly property real anchorY: (modelData.y - overlay.currentOriginY) * overlay.currentTileSize
            x: anchorX - width / 2
            y: anchorY - height - 6
            width: Math.min(360, caption.implicitWidth + 18)
            height: caption.contentHeight + 10
            radius: 4
            color: "#e6191d1f"
            border.width: 1
            border.color: modelData.kind === "waypoint" ? "#3fb950" : Colors.c("accent")
            Text {
                id: caption
                anchors.fill: parent
                anchors.margins: 5
                text: modelData.text
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                color: Colors.c("text")
                font.family: Colors.fontFamily
                font.pixelSize: 12
            }
        }
    }

    Repeater {
        model: overlay.tooltipsVisible ? overlay.containerEntries : []

        delegate: Rectangle {
            id: containerTip
            required property var modelData
            readonly property int columns: Math.min(4, Math.max(1, modelData.items.length))
            readonly property int rows: Math.ceil(modelData.items.length / columns)
            // Tooltips stay compact when the map is zoomed out and never enlarge
            // Tibia sprites beyond their natural 32x32 presentation size.
            readonly property int slotSize: Math.max(22, Math.min(32,
                                                        Math.round(overlay.currentTileSize * 0.72)))
            readonly property real gridWidth: columns * slotSize + Math.max(0, columns - 1) * 2
            readonly property real gridHeight: rows * slotSize + Math.max(0, rows - 1) * 2
            readonly property bool hasOverflow: modelData.itemCount > modelData.items.length
            readonly property real anchorX: (modelData.x + 0.5 - overlay.currentOriginX)
                                                 * overlay.currentTileSize
            readonly property real anchorY: (modelData.y - overlay.currentOriginY)
                                                 * overlay.currentTileSize

            x: Math.max(2, Math.min(overlay.width - width - 2, anchorX - width / 2))
            y: anchorY - height - 7 >= 2 ? anchorY - height - 7 : anchorY + overlay.currentTileSize + 5
            width: Math.max(containerHeader.implicitWidth + 10, gridWidth + 10)
            height: 10 + containerHeader.implicitHeight + 3 + gridHeight
                    + (hasOverflow ? overflowText.implicitHeight + 3 : 0)
            radius: 4
            color: "#FA0D1117"
            border.width: 1
            border.color: "#59636E"
            z: 20

            Rectangle {
                x: 2
                y: 3
                width: parent.width
                height: parent.height
                radius: parent.radius
                color: "#62000000"
                z: -1
            }

            Rectangle {
                width: parent.width - 2
                height: 2
                anchors { top: parent.top; horizontalCenter: parent.horizontalCenter }
                radius: 1
                color: "#C89B3C"
                opacity: 0.85
            }

            Column {
                anchors { fill: parent; margins: 5 }
                spacing: 3

                Text {
                    id: containerHeader
                    width: parent.width
                    text: containerTip.modelData.text
                    color: "#F0F3F6"
                    font.pixelSize: 10
                    font.bold: true
                    lineHeight: 0.9
                }

                Grid {
                    columns: containerTip.columns
                    spacing: 2

                    Repeater {
                        model: containerTip.modelData.items
                        delegate: Rectangle {
                            id: containerSlot
                            required property var modelData
                            width: containerTip.slotSize
                            height: containerTip.slotSize
                            radius: 2
                            color: "#161B22"
                            border.width: 1
                            border.color: "#3D4650"

                            Image {
                                anchors.centerIn: parent
                                width: Math.min(32, parent.width - 4)
                                height: Math.min(32, parent.height - 4)
                                source: overlay.containerImageSource(containerSlot.modelData)
                                fillMode: Image.PreserveAspectFit
                                smooth: false
                                cache: true
                            }

                            Text {
                                visible: containerSlot.modelData.count > 1
                                anchors { right: parent.right; bottom: parent.bottom; margins: 2 }
                                text: containerSlot.modelData.count
                                color: "white"
                                font.pixelSize: 9
                                font.bold: true
                                style: Text.Outline
                                styleColor: "black"
                            }
                        }
                    }
                }

                Text {
                    id: overflowText
                    visible: containerTip.hasOverflow
                    text: "+" + (containerTip.modelData.itemCount - containerTip.modelData.items.length)
                          + " more"
                    color: "#8B949E"
                    font.pixelSize: 9
                }
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

    onWidthChanged: scheduleRefresh(false)
    onHeightChanged: scheduleRefresh(false)

    Connections {
        target: overlay.mapCtrl
        function onContentUpdated() {
            overlay.currentOriginX = overlay.mapCtrl.renderOriginX();
            overlay.currentOriginY = overlay.mapCtrl.renderOriginY();
            overlay.scheduleRefresh(false);
        }
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
        function onShowHousesChanged() { overlay.refreshData(true); }
        function onShowLightSourcesChanged() { overlay.refreshData(true); }
    }

    Timer {
        interval: 100
        running: overlay.visible
        repeat: true
        onTriggered: {
            if (!overlay.refreshPending) return;
            const force = overlay.forcePending;
            overlay.refreshPending = false;
            overlay.forcePending = false;
            overlay.refreshData(force);
        }
    }
    Component.onCompleted: refreshData(true)
}
