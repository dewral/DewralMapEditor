pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import Tibia 1.0
import "../style"
import "../themes/fluent/Colors.js" as Colors

DmeDialog {
    id: dialog
    objectName: "animationPreviewDialog"
    title: "Effects & Missiles"
    modal: false
    dim: false
    width: Math.min(1000, Controls.Overlay.overlay ? Controls.Overlay.overlay.width - 24 : 1000)
    height: Math.min(760, Controls.Overlay.overlay ? Controls.Overlay.overlay.height - 24 : 760)

    property var datReader: Backend.datReader
    property var sprReader: Backend.sprReader
    readonly property bool ready: datReader.loaded && sprReader.loaded
    property bool missile: false
    property int catalogRevision: 0
    property var assetIds: []
    property var groundIds: []
    property int selectedId: 1
    property int groundId: 0
    property int direction: 2
    property int distance: 5
    property int travelMs: 600
    property bool playing: true
    property real elapsedMs: 0
    property real lastTick: 0
    readonly property var preview: ready && catalogRevision > 0 ? datReader.animationPreview(missile, selectedId, elapsedMs, direction, -1) : ({})
    readonly property var directionX: [0, 1, 1, 1, 0, -1, -1, -1]
    readonly property var directionY: [-1, -1, 0, 1, 1, 1, 0, -1]
    readonly property real flight: (elapsedMs % (travelMs + 400)) / travelMs
    readonly property bool clockRunning: visible && playing && ready && assetIds.length > 0
                                              && (!viewport.Window.window || viewport.Window.window.active)

    function imageSource(info) {
        return ready && info.ids ? sprReader.itemImageSource(info.ids, info.width, info.height, info.layers) : "";
    }
    function restart() { elapsedMs = 0; lastTick = Date.now(); }
    function refreshCatalog() {
        assetIds = ready ? datReader.animationIds(missile) : [];
        groundIds = ready ? datReader.previewGroundIds() : [];
        if (assetIds.indexOf(selectedId) < 0) selectedId = assetIds.length ? assetIds[0] : 1;
        if (groundIds.indexOf(groundId) < 0) groundId = groundIds.length ? groundIds[0] : 0;
        ++catalogRevision;
        restart();
    }
    function selectAdjacent(delta) {
        if (!assetIds.length) return;
        const index = Math.max(0, Math.min(assetIds.length - 1, assetIds.indexOf(selectedId) + delta));
        selectedId = assetIds[index];
        assets.positionViewAtIndex(index, ListView.Contain);
    }
    function stepFrame() {
        playing = false;
        const next = ((preview.frame || 0) + 1) % Math.max(1, preview.frames || 1);
        elapsedMs = datReader.animationPreview(missile, selectedId, 0, direction, next).frameStartMs || 0;
    }
    onSelectedIdChanged: restart()
    onMissileChanged: refreshCatalog()
    onDirectionChanged: restart()
    onDistanceChanged: restart()
    onTravelMsChanged: restart()
    onReadyChanged: refreshCatalog()
    Component.onCompleted: refreshCatalog()
    Connections {
        target: dialog.datReader
        function onLoadedChanged() { dialog.refreshCatalog(); }
    }
    Connections {
        target: dialog.sprReader
        function onLoadedChanged() { dialog.refreshCatalog(); }
    }
    Timer {
        interval: 16
        repeat: true
        running: dialog.clockRunning
        onRunningChanged: dialog.lastTick = Date.now()
        onTriggered: {
            const now = Date.now();
            dialog.elapsedMs += Math.max(0, now - dialog.lastTick);
            dialog.lastTick = now;
        }
    }

    contentItem: ColumnLayout {
        spacing: 10
        RowLayout {
            Layout.fillWidth: true
            DmeComboBox {
                objectName: "animationCategory"
                model: ["Effect", "Missile"]
                currentIndex: dialog.missile ? 1 : 0
                onActivated: index => dialog.missile = index === 1
                Layout.preferredWidth: 150
            }
            Text { text: "Client ID"; color: Colors.c("muted") }
            DmeSpinBox {
                objectName: "animationId"
                from: 1; to: dialog.assetIds.length ? dialog.assetIds[dialog.assetIds.length - 1] : 1
                value: dialog.selectedId
                enabled: dialog.assetIds.length > 0
                onValueModified: dialog.selectedId = value
            }
            Text { text: dialog.assetIds.length + " available"; color: Colors.c("muted"); Layout.fillWidth: true }
            DmeButton { text: "Previous"; enabled: dialog.assetIds.length > 0; onClicked: dialog.selectAdjacent(-1) }
            DmeButton { text: "Next"; enabled: dialog.assetIds.length > 0; onClicked: dialog.selectAdjacent(1) }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10
            ListView {
                id: assets
                objectName: "animationAssets"
                Layout.preferredWidth: 145
                Layout.fillHeight: true
                clip: true
                model: dialog.assetIds
                currentIndex: dialog.assetIds.indexOf(dialog.selectedId)
                keyNavigationEnabled: false
                Keys.onUpPressed: dialog.selectAdjacent(-1)
                Keys.onDownPressed: dialog.selectAdjacent(1)
                Controls.ScrollBar.vertical: Controls.ScrollBar {}
                delegate: Rectangle {
                    id: row
                    required property int modelData
                    width: assets.width
                    height: 54
                    color: dialog.selectedId === modelData ? Colors.c("selectedCell") : Colors.c("cell")
                    border.color: Colors.c("cellBorder")
                    Image {
                        x: 6; y: 5; width: 44; height: 44
                        fillMode: Image.PreserveAspectFit
                        smooth: false
                        source: dialog.ready && dialog.catalogRevision > 0 ? dialog.imageSource(dialog.datReader.animationPreview(dialog.missile, row.modelData, 0, dialog.direction, 0)) : ""
                    }
                    Text { x: 59; anchors.verticalCenter: parent.verticalCenter; text: "ID " + row.modelData; color: Colors.c("text") }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: { dialog.selectedId = row.modelData; assets.forceActiveFocus(); }
                    }
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Colors.c("field")
                border.color: Colors.c("border")
                Flickable {
                    id: viewport
                    objectName: "animationViewport"
                    anchors.fill: parent; anchors.margins: 1
                    clip: true
                    contentWidth: Math.max(width, board.width)
                    contentHeight: Math.max(height, board.height)
                    Controls.ScrollBar.horizontal: Controls.ScrollBar {}
                    Controls.ScrollBar.vertical: Controls.ScrollBar {}
                    Item {
                        id: board
                        readonly property int tileSize: 32 * (zoom.currentIndex + 1)
                        readonly property int columns: Math.max(13, 2 * (dialog.preview.width || 1) + 8)
                        readonly property int rows: Math.max(11, 2 * (dialog.preview.height || 1) + 8)
                        readonly property real centerX: Math.floor(columns / 2)
                        readonly property real centerY: Math.floor(rows / 2)
                        readonly property real startX: centerX - (dialog.missile ? dialog.directionX[dialog.direction] * Math.floor(dialog.distance / 2) : 0)
                        readonly property real startY: centerY - (dialog.missile ? dialog.directionY[dialog.direction] * Math.floor(dialog.distance / 2) : 0)
                        width: columns * tileSize; height: rows * tileSize
                        x: Math.max(0, (viewport.width - width) / 2)
                        y: Math.max(0, (viewport.height - height) / 2)
                        Repeater {
                            model: board.columns * board.rows
                            delegate: Rectangle {
                                id: ground
                                required property int index
                                readonly property int tileX: index % board.columns
                                readonly property int tileY: Math.floor(index / board.columns)
                                x: tileX * board.tileSize; y: tileY * board.tileSize
                                width: board.tileSize; height: board.tileSize
                                color: (tileX + tileY) % 2 ? "#444950" : "#383d44"
                                Image {
                                    anchors.fill: parent
                                    smooth: false
                                    source: dialog.ready && dialog.catalogRevision > 0 && dialog.groundId ? dialog.imageSource(dialog.datReader.groundTilePreview(dialog.groundId, ground.tileX, ground.tileY)) : ""
                                }
                                Rectangle { anchors.fill: parent; color: "transparent"; border.color: "#40000000"; visible: grid.checked }
                            }
                        }
                        Rectangle {
                            x: board.startX * board.tileSize; y: board.startY * board.tileSize
                            width: board.tileSize; height: board.tileSize
                            color: "transparent"; border.color: "#ffe07a"; border.width: 2
                        }
                        Rectangle {
                            visible: dialog.missile
                            x: (board.startX + dialog.directionX[dialog.direction] * dialog.distance) * board.tileSize
                            y: (board.startY + dialog.directionY[dialog.direction] * dialog.distance) * board.tileSize
                            width: board.tileSize; height: board.tileSize
                            color: "transparent"; border.color: "#82e5c4"; border.width: 2
                        }
                        Image {
                            objectName: "animationSprite"
                            visible: !!dialog.preview.ids && (!dialog.missile || dialog.flight <= 1)
                            readonly property real progress: dialog.missile ? Math.min(1, dialog.flight) : 0
                            x: (board.startX + dialog.directionX[dialog.direction] * dialog.distance * progress + 1 - (dialog.preview.width || 1)) * board.tileSize - (dialog.preview.offsetX || 0) * board.tileSize / 32
                            y: (board.startY + dialog.directionY[dialog.direction] * dialog.distance * progress + 1 - (dialog.preview.height || 1)) * board.tileSize - (dialog.preview.offsetY || 0) * board.tileSize / 32
                            width: (dialog.preview.width || 1) * board.tileSize
                            height: (dialog.preview.height || 1) * board.tileSize
                            smooth: false
                            source: dialog.imageSource(dialog.preview)
                        }
                    }
                    Text {
                        visible: !dialog.ready || !dialog.preview.ids
                        anchors.centerIn: parent
                        text: !dialog.ready ? "Load a client profile to preview its assets." : "No sprites available for this ID."
                        color: "#ffffff"
                        style: Text.Outline; styleColor: "#000000"
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Text { text: "Ground"; color: Colors.c("muted") }
            DmeComboBox {
                objectName: "animationGround"
                Layout.preferredWidth: 180
                model: dialog.groundIds.map(id => "Client ID " + id)
                currentIndex: dialog.groundIds.indexOf(dialog.groundId)
                enabled: dialog.groundIds.length > 0
                onActivated: index => dialog.groundId = dialog.groundIds[index]
            }
            DmeCheckBox { id: grid; text: "Tile grid"; checked: true; onClicked: checked = !checked }
            Item { Layout.fillWidth: true }
            Text { text: "Zoom"; color: Colors.c("muted") }
            DmeComboBox { id: zoom; model: ["100%", "200%", "300%", "400%"]; currentIndex: 0; Layout.preferredWidth: 100 }
        }
        RowLayout {
            visible: dialog.missile
            Layout.fillWidth: true
            Text { text: "Direction"; color: Colors.c("muted") }
            DmeComboBox {
                objectName: "missileDirection"
                model: ["North", "North-east", "East", "South-east", "South", "South-west", "West", "North-west"]
                currentIndex: dialog.direction
                onActivated: index => dialog.direction = index
                Layout.preferredWidth: 140
            }
            Text { text: "Tiles"; color: Colors.c("muted") }
            DmeSpinBox { from: 1; to: 6; value: dialog.distance; onValueModified: dialog.distance = value }
            Text { text: "Flight ms"; color: Colors.c("muted") }
            DmeSpinBox { from: 100; to: 5000; stepSize: 100; value: dialog.travelMs; onValueModified: dialog.travelMs = value }
            Item { Layout.fillWidth: true }
        }
        RowLayout {
            Layout.fillWidth: true
            DmeButton {
                objectName: "animationPlayPause"
                text: dialog.playing ? "Pause" : "Play"
                enabled: !!dialog.preview.ids
                onClicked: {
                    dialog.playing = !dialog.playing;
                }
            }
            DmeButton { objectName: "animationRestart"; text: "Restart"; onClicked: dialog.restart() }
            DmeButton { objectName: "animationStep"; text: "Next frame"; enabled: !!dialog.preview.ids; onClicked: dialog.stepFrame() }
            Text {
                text: dialog.preview.ids ? "Frame " + (dialog.preview.frame + 1) + "/" + dialog.preview.frames + "  ·  " + dialog.preview.width + " × " + dialog.preview.height + " tiles" : ""
                color: Colors.c("muted"); Layout.fillWidth: true
            }
            DmeButton { text: "Close"; onClicked: dialog.close() }
        }
        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Colors.c("muted")
            text: "1 tile = 32 pixels. Yellow: origin; green: missile target. Preview only; maps are unchanged.\n"
                  + (dialog.missile ? "Flight time is illustrative and adjustable; assets define the directional sprites, not the travel speed."
                                    : "Playback repeats for inspection. DAT timing uses duration midpoints; older assets use 100 ms per frame.")
        }
    }
}
