import "../controllers/ContextBrushes.js" as ContextBrushes
import QtQuick
import "../themes/tibia" as TibiaSkin
import QtQuick.Controls
import Tibia 1.0
import "../style"
import "../controllers"
import "../dialogs"

Item {
    id: workspace
    readonly property bool tibiaUi: Backend.uiTheme.style === "tibia-ui"
    required property var app
    required property var settings
    required property var propertiesDialog
    required property var browseFieldDialog
    required property var paletteNavigator

    property var workTimerPopupHost
    property var proceduralBrushSession: null
    property Item previewDockHost: null
    property alias mapView: mapView
    property alias mapRenderer: mapRenderer
    property alias weatherRegions: weatherRegionLayer
    property alias context: mapArea.ctx

    MapViewSettings {
        settings: workspace.settings
        mapView: workspace.mapView
    }

    function positionText(format) {
        var x = mapArea.ctx.x;
        var y = mapArea.ctx.y;
        var z = mapArea.ctx.z;
        switch (format) {
        case "tuple":
            return "(" + x + ", " + y + ", " + z + ")";
        case "lua":
            return "{x = " + x + ", y = " + y + ", z = " + z + "}";
        case "position":
            return "Position(" + x + ", " + y + ", " + z + ")";
        case "json":
            return "{\"x\":" + x + ",\"y\":" + y + ",\"z\":" + z + "}";
        default:
            return x + ", " + y + ", " + z;
        }
    }

    function copyPosition(format) {
        Backend.fileTools.setClipboard(positionText(format));
    }



    Loader {
        objectName: "mapChatBackground"
        anchors.fill: parent
        active: workspace.tibiaUi
        sourceComponent: BorderImage {
            source: Qt.resolvedUrl("../themes/tibia/images/2pixel-up-frame-borderimage-upside-down.png")
            border { left: 2; top: 2; right: 2; bottom: 2 }
            horizontalTileMode: BorderImage.Repeat
            verticalTileMode: BorderImage.Repeat
            rotation: 180
            smooth: false
        }
    }
    TibiaSkin.TibiaMapScrollBar {
        objectName: "tibiaMapVerticalScroll"
        visible: workspace.tibiaUi && Backend.otbmReader.loaded
        mapView: workspace.mapView
        anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom
        anchors.margins: 4; anchors.bottomMargin: 16; width: 12
    }
    TibiaSkin.TibiaMapScrollBar {
        objectName: "tibiaMapHorizontalScroll"
        visible: workspace.tibiaUi && Backend.otbmReader.loaded
        mapView: workspace.mapView; horizontal: true
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.margins: 4; anchors.rightMargin: 16; height: 12
    }
    Loader {
        visible: workspace.tibiaUi && Backend.otbmReader.loaded
        active: visible; width: 12; height: 12
        anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 4
        sourceComponent: TibiaSkin.TibiaClassicFrame { kind: "button" }
    }
    Column {
        id: errorArea
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
        anchors.leftMargin: 5
        anchors.rightMargin: 5
        spacing: 2
        topPadding: Backend.otbmReader.errorString ? 4 : 0

        Text {
            visible: Backend.sprReader.errorString.length > 0
            text: "SPR: " + Backend.sprReader.errorString
            color: "#ff6b6b"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            width: parent.width
        }
        Text {
            visible: Backend.datReader.errorString.length > 0
            text: "DAT: " + Backend.datReader.errorString
            color: "#ff6b6b"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            width: parent.width
        }
        Text {
            visible: Backend.otbReader.errorString.length > 0
            text: "OTB: " + Backend.otbReader.errorString
            color: "#ff6b6b"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            width: parent.width
        }
        Text {
            visible: Backend.otbmReader.errorString.length > 0
            text: "OTBM: " + Backend.otbmReader.errorString
            color: "#ff6b6b"
            font.pixelSize: 11
            wrapMode: Text.WordWrap
            width: parent.width
        }
    }

    Item {
        id: mapArea
        anchors {
            top: errorArea.bottom
            bottom: parent.bottom
            left: parent.left
            right: parent.right
            margins: 3
            rightMargin: workspace.tibiaUi ? 17 : 3
            bottomMargin: workspace.tibiaUi ? 17 : 3
        }
        visible: Backend.otbmReader.loaded
        clip: true

        property var brushOptions: ({})
        property var ctx: ({
                singleTile: false,
                hasSelectedItem: false,
                hasItem: false,
                serverId: 0,
                clientId: 0,
                name: "",
                groupName: "",
                x: 0,
                y: 0,
                z: 0,
                creatureName: "",
                creatureSpawntime: 0,
                spawnRadius: 0,
                actionId: 0,
                uniqueId: 0,
                text: "",
                writable: false,
                teleport: false,
                hasTeleportDest: false,
                teleportX: 0,
                teleportY: 0,
                teleportZ: 0,
                canRotate: false,
                door: false,
                doorOpen: false
            })

        MapView {
            id: mapView
            readonly property bool weatherEditing: weatherRegionLayer.editing
            readonly property var weatherDocument: weatherRegionLayer.document
            hotkeyBindings: Backend.hotkeys.activeBindings
            commandHotkeysEnabled: !Backend.hotkeys.capturing
            modernZones: true
            anchors.fill: parent
            focus: true
            otbm: Backend.docMgr.current
            otb: Backend.otbReader
            dat: Backend.datReader
            spr: Backend.sprReader
            floor: 7
            Component.onCompleted: {
                setBrushStore(Backend.brushStore);
                setCreatureStore(Backend.creatureStore);
            }
            onContextMenuRequested: (x, y) => {
                mapArea.ctx = mapView.contextInfo();
                mapArea.brushOptions = ContextBrushes.options(mapArea.ctx, mapView,
                    Backend.tilesetStore, Backend.brushStore);
                contextMenu.popup(x, y);
            }
            onPropertiesRequested: {
                mapArea.ctx = mapView.contextInfo();
                workspace.propertiesDialog.open();
            }
        }

        MapRhiView {
            id: mapRenderer
            anchors.fill: parent
            source: mapView
            lightingMinimumZoom: workspace.settings.lightingMinimumZoom
            Component.onCompleted: {
                if (!workspace.settings.renderMaxFpsConfigured) {
                    workspace.settings.renderMaxFps = 60;
                    workspace.settings.renderMaxFpsConfigured = true;
                }
                maxFps = workspace.settings.renderMaxFps;
            }
        }

        WeatherRegionLayer {
            id: weatherRegionLayer
            anchors.fill: parent
            mapView: workspace.mapView
            mapSettings: workspace.settings
            z: 6
        }

        GenerationMaskOverlay { anchors.fill: parent; mapView: workspace.mapView; session: workspace.proceduralBrushSession; z: 4 }

        WeatherLayer {
            visible: weatherRegionLayer.testingMap || !weatherRegionLayer.previewEnabled
            anchors.fill: parent
            z: 0.5
            mapView: workspace.mapView
            settings: workspace.settings
        }

        ZoneLabels {
            z: 2
            anchors.fill: parent
            visible: true
            mapView: workspace.mapView
        }

        MapOverlay {
            anchors.fill: parent
            z: 5
            mapCtrl: mapView
            settings: workspace.settings
        }

        IngamePreviewPanel {
            dockHost:workspace.previewDockHost
            mapView: mapView
            settings: workspace.settings
        }

        DmeMenu {
            id: contextMenu
            Action {
                text: "Cut"
                enabled: mapView.selectionCount > 0
                onTriggered: mapView.cutSelection()
            }
            Action {
                text: "Copy"
                enabled: mapView.selectionCount > 0
                onTriggered: mapView.copySelection()
            }
            Action {
                objectName: "createCollectionAction"
                text: "Create Collection..."
                enabled: mapView.selectionCount > 0
                onTriggered: collectionDialog.openForSelection()
            }
            Action {
                text: "Add Prefab..."
                enabled: mapView.selectionCount > 0
                onTriggered: prefabDialog.openForSelection()
            }
            DmeMenu {
                title: "Rotate Selection"
                enabled: mapView.selectionCount > 0
                Action {
                    text: "90° clockwise"
                    onTriggered: mapView.rotateSelection(1)
                }
                Action {
                    text: "180°"
                    onTriggered: mapView.rotateSelection(2)
                }
                Action {
                    text: "90° counterclockwise"
                    onTriggered: mapView.rotateSelection(3)
                }
            }
            Action {
                text: "Generate Path from Prefabs..."
                enabled: Backend.otbmReader.loaded
                onTriggered: pathBuilderDialog.openForMap()
            }
            Action {
                text: "Copy Position"
                onTriggered: workspace.copyPosition("plain")
            }
            DmeMenu {
                title: "Copy Position As"
                DmeMenuItem {
                    text: "Plain: x, y, z"
                    onTriggered: workspace.copyPosition("plain")
                }
                DmeMenuItem {
                    text: "Tuple: (x, y, z)"
                    onTriggered: workspace.copyPosition("tuple")
                }
                DmeMenuItem {
                    text: "OTClient: Position(x, y, z)"
                    onTriggered: workspace.copyPosition("position")
                }
                DmeMenuItem {
                    text: "Lua table: {x = ..., y = ..., z = ...}"
                    onTriggered: workspace.copyPosition("lua")
                }
                DmeMenuItem {
                    text: "JSON: {\"x\":...,\"y\":...,\"z\":...}"
                    onTriggered: workspace.copyPosition("json")
                }
            }
            Action {
                text: "Browse Field"
                enabled: mapArea.ctx.hasItem
                onTriggered: workspace.browseFieldDialog.open()
            }
            Action {
                text: "Paste"
                enabled: mapView.hasClipboard
                onTriggered: mapView.startPasting()
            }
            Action {
                text: "Delete"
                enabled: mapView.selectionCount > 0
                onTriggered: mapView.deleteSelectedTop()
            }
            MenuSeparator {}
            Action {
                text: "Copy Item Server Id"
                enabled: mapArea.ctx.hasItem
                onTriggered: Backend.fileTools.setClipboard("" + mapArea.ctx.serverId)
            }
            Action {
                text: "Copy Item Client Id"
                enabled: mapArea.ctx.hasItem
                onTriggered: Backend.fileTools.setClipboard("" + mapArea.ctx.clientId)
            }
            Action {
                text: "Copy Item Name"
                enabled: mapArea.ctx.hasItem
                onTriggered: Backend.fileTools.setClipboard(mapArea.ctx.name)
            }
            MenuSeparator {}
            DmeMenuItem {
                objectName: "selectGroundBrush"
                text: "Select Ground Brush"
                visible: !!mapArea.brushOptions.ground
                height: visible ? implicitHeight : 0
                onTriggered: workspace.paletteNavigator.selectContextBrush(mapArea.brushOptions.ground)
            }
            DmeMenuItem {
                objectName: "selectWallBrush"
                text: "Select Wall Brush"
                visible: !!mapArea.brushOptions.wall
                height: visible ? implicitHeight : 0
                onTriggered: workspace.paletteNavigator.selectContextBrush(mapArea.brushOptions.wall)
            }
            DmeMenuItem {
                objectName: "selectCarpetBrush"
                text: "Select Carpetbrush"
                visible: !!mapArea.brushOptions.carpet
                height: visible ? implicitHeight : 0
                onTriggered: workspace.paletteNavigator.selectContextBrush(mapArea.brushOptions.carpet)
            }
            DmeMenuItem {
                objectName: "selectTableBrush"
                text: "Select Tablebrush"
                visible: !!mapArea.brushOptions.table
                height: visible ? implicitHeight : 0
                onTriggered: workspace.paletteNavigator.selectContextBrush(mapArea.brushOptions.table)
            }
            DmeMenuItem {
                objectName: "selectDoodadBrush"
                text: "Select Doodadbrush"
                visible: !!mapArea.brushOptions.doodad
                height: visible ? implicitHeight : 0
                onTriggered: workspace.paletteNavigator.selectContextBrush(mapArea.brushOptions.doodad)
            }
            DmeMenuItem {
                objectName: "selectDoorBrush"
                text: "Select Doorbrush"
                visible: !!mapArea.brushOptions.door
                height: visible ? implicitHeight : 0
                onTriggered: workspace.paletteNavigator.selectContextBrush(mapArea.brushOptions.door)
            }
            DmeMenuItem {
                objectName: "selectCollectionBrush"
                text: "Select Collection"
                visible: !!mapArea.brushOptions.collection
                height: visible ? implicitHeight : 0
                onTriggered: workspace.paletteNavigator.selectContextBrush(mapArea.brushOptions.collection)
            }
            DmeMenuItem {
                objectName: "selectRawBrush"
                text: "Select RAW"
                visible: mapArea.ctx.singleTile && mapArea.ctx.hasSelectedItem
                height: visible ? implicitHeight : 0
                onTriggered: workspace.paletteNavigator.selectRaw(mapArea.ctx.serverId)
            }
            DmeMenuItem {
                text: "Select Creature"
                visible: mapArea.ctx.singleTile && mapArea.ctx.creatureName !== ""
                height: visible ? implicitHeight : 0
                onTriggered: workspace.paletteNavigator.selectCreature(
                                 mapArea.ctx.creatureName,
                                 mapArea.ctx.creatureIsNpc === true)
            }
            DmeMenuItem {
                objectName: "selectSpawnBrush"
                text: "Select Spawn"
                visible: mapArea.ctx.singleTile && mapArea.ctx.spawnRadius > 0
                height: visible ? implicitHeight : 0
                onTriggered: workspace.paletteNavigator.selectSpawn()
            }
            DmeMenuItem {
                text: "Select House"
                visible: mapArea.ctx.singleTile && mapArea.ctx.houseId > 0
                height: visible ? implicitHeight : 0
                onTriggered: {
                    mapView.clearSelection();
                    workspace.paletteNavigator.selectHouse(mapArea.ctx.houseId);
                }
            }
            DmeMenuItem {
                text: "Go To Destination"
                visible: mapArea.ctx.teleport === true && mapArea.ctx.hasTeleportDest === true
                height: visible ? implicitHeight : 0
                onTriggered: mapView.centerOnPosition(mapArea.ctx.teleportX, mapArea.ctx.teleportY, mapArea.ctx.teleportZ)
            }
            DmeMenuItem {
                text: "Rotate Item"
                visible: mapArea.ctx.canRotate === true
                height: visible ? implicitHeight : 0
                onTriggered: mapView.rotateContextItem()
            }
            DmeMenuItem {
                text: mapArea.ctx.doorOpen === true ? "Close Door" : "Open Door"
                visible: mapArea.ctx.door === true
                height: visible ? implicitHeight : 0
                onTriggered: mapView.switchContextDoor()
            }
            Action {
                text: mapArea.ctx.creatureName !== ""
                      ? "Creature properties - " + mapArea.ctx.creatureSpawntime + " s"
                      : (mapArea.ctx.spawnRadius > 0
                         ? "Spawn properties"
                         : "Properties")
                enabled: mapArea.ctx.hasItem || mapArea.ctx.creatureName !== "" || mapArea.ctx.spawnRadius > 0
                onTriggered: workspace.propertiesDialog.open()
            }
        }

        CreateCollectionDialog {
            id: collectionDialog
            mapCtrl: mapView
            tilesetStore: Backend.tilesetStore
            onCollectionCreated: (name, firstServerId) =>
                workspace.paletteNavigator.showItemLocation("collection", name, firstServerId)
        }

        DmeDialog {
            id: prefabDialog
            title: "Add Prefab"
            property var paletteNames: {
                const revision = Backend.tilesetStore.revision;
                return Backend.tilesetStore.namesFor("doodad");
            }

            function openForSelection() {
                prefabName.text = "";
                newPrefabPalette.text = "";
                prefabError.text = "";
                prefabPalette.currentIndex = paletteNames.length > 0 ? 0 : paletteNames.length;
                open();
                Qt.callLater(function() { prefabName.forceActiveFocus(); });
            }

            function commit() {
                const creatingPalette = prefabPalette.currentIndex >= paletteNames.length;
                const palette = creatingPalette ? newPrefabPalette.text.trim()
                                                : prefabPalette.currentText;
                if (prefabName.text.trim() === "") {
                    prefabError.text = "Enter a prefab name.";
                    return;
                }
                if (palette === "") {
                    prefabError.text = "Select or enter a doodad category.";
                    return;
                }
                if (creatingPalette && !Backend.tilesetStore.newTileset("doodad", palette)) {
                    prefabError.text = Backend.tilesetStore.errorString || "Could not create the doodad category.";
                    return;
                }
                const result = mapView.saveSelectionAsPrefab(prefabName.text.trim(), palette);
                if (!result.success) {
                    if (creatingPalette)
                        Backend.tilesetStore.deleteTileset("doodad", palette);
                    prefabError.text = result.error || "Could not save the prefab.";
                    return;
                }
                close();
                workspace.paletteNavigator.selectPrefabPalette(palette, prefabName.text.trim());
            }

            contentItem: Column {
                width: 300
                spacing: 8

                Text {
                    text: "Name"
                    color: (("#D0D0D0"))
                    font.pixelSize: 11
                }
                DmeTextField {
                    id: prefabName
                    width: parent.width
                    placeholderText: "Prefab name"
                    onAccepted: prefabDialog.commit()
                }
                Text {
                    text: "Doodad category"
                    color: (("#D0D0D0"))
                    font.pixelSize: 11
                }
                DmeComboBox {
                    id: prefabPalette
                    width: parent.width
                    model: prefabDialog.paletteNames.concat(["New category..."])
                }
                DmeTextField {
                    id: newPrefabPalette
                    width: parent.width
                    visible: prefabPalette.currentIndex >= prefabDialog.paletteNames.length
                    height: visible ? implicitHeight : 0
                    placeholderText: "New doodad category"
                    onAccepted: prefabDialog.commit()
                }
                Text {
                    id: prefabError
                    width: parent.width
                    color: "#F85149"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
                Text {
                    width: parent.width
                    text: "All item stacks in the selected tiles will be stored. Creatures and spawns are not included."
                    color: (("#A0A0A0"))
                    font.pixelSize: 10
                    wrapMode: Text.WordWrap
                }
                Row {
                    spacing: 8
                    anchors.horizontalCenter: parent.horizontalCenter
                    DmeButton {
                        text: "Add"
                        width: 90
                        onClicked: prefabDialog.commit()
                    }
                    DmeButton {
                        text: "Cancel"
                        width: 90
                        onClicked: prefabDialog.close()
                    }
                }
            }
        }

        DmeDialog {
            id: pathBuilderDialog
            title: "Prefab Path Generator"
            property var paletteNames: []
            property var prefabNames: []

            function loadPrefabs() {
                const rows = pathPalette.currentIndex >= 0
                           ? Backend.brushStore.prefabsForPalette(pathPalette.currentText) : [];
                const names = [];
                for (let i = 0; i < rows.length; ++i)
                    names.push(rows[i].name);
                prefabNames = names;
                straightPrefab.currentIndex = names.length > 0 ? 0 : -1;
                cornerPrefab.currentIndex = names.length > 1 ? 1 : (names.length > 0 ? 0 : -1);
                endPrefab.currentIndex = 0;
            }

            function openForMap() {
                pathError.text = "";
                const revision = Backend.brushStore.revision;
                paletteNames = Backend.brushStore.prefabPaletteNames();
                pathPalette.currentIndex = paletteNames.length > 0 ? 0 : -1;
                loadPrefabs();
                open();
            }

            function start() {
                if (prefabNames.length === 0) {
                    pathError.text = "This category does not contain any prefabs.";
                    return;
                }
                const ending = endPrefab.currentIndex > 0
                             ? prefabNames[endPrefab.currentIndex - 1] : "";
                mapView.pathPreserveGround = preservePathGround.checked;
                const result = mapView.startPathBuilder(straightPrefab.currentText,
                                                        cornerPrefab.currentText,
                                                        ending,
                                                        pathSpacing.value);
                if (!result.success) {
                    pathError.text = result.error || "Could not start the path generator.";
                    return;
                }
                close();
            }

            contentItem: Column {
                width: 360
                spacing: 8

                Text {
                    width: parent.width
                    text: "Choose base prefabs in their canonical orientation. The generator rotates the tile layout and rotatable items automatically."
                    color: (("#A0A0A0"))
                    font.pixelSize: 10
                    wrapMode: Text.WordWrap
                }
                Text { text: "Doodad category"; color: "#D0D0D0"; font.pixelSize: 11 }
                DmeCheckBox {
                    id: preservePathGround
                    text: "Preserve existing ground (borders and objects only)"
                    checked: true
                    width: parent.width
                }
                DmeComboBox {
                    id: pathPalette
                    width: parent.width
                    model: pathBuilderDialog.paletteNames
                    onActivated: pathBuilderDialog.loadPrefabs()
                }
                Text { text: "Straight prefab (west - east)"; color: "#D0D0D0"; font.pixelSize: 11 }
                DmeComboBox { id: straightPrefab; width: parent.width; model: pathBuilderDialog.prefabNames }
                Text { text: "Corner prefab (west - north)"; color: "#D0D0D0"; font.pixelSize: 11 }
                DmeComboBox { id: cornerPrefab; width: parent.width; model: pathBuilderDialog.prefabNames }
                Text { text: "Optional end prefab (opening to west)"; color: "#D0D0D0"; font.pixelSize: 11 }
                DmeComboBox {
                    id: endPrefab
                    width: parent.width
                    model: ["No separate ending"].concat(pathBuilderDialog.prefabNames)
                }
                Row {
                    spacing: 8
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Segment spacing (tiles)"
                        color: "#D0D0D0"
                        font.pixelSize: 11
                    }
                    DmeSpinBox { id: pathSpacing; from: 1; to: 64; value: 4; width: 90 }
                }
                Text {
                    id: pathError
                    width: parent.width
                    color: "#F85149"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
                Row {
                    spacing: 8
                    anchors.horizontalCenter: parent.horizontalCenter
                    DmeButton { text: "Start drawing"; width: 120; onClicked: pathBuilderDialog.start() }
                    DmeButton { text: "Cancel"; width: 90; onClicked: pathBuilderDialog.close() }
                }
            }
        }

        Item {
            z: 30
            visible: mapView.pathBuilderActive
            width: 430
            height: 72
            anchors {
                top: parent.top
                horizontalCenter: parent.horizontalCenter
                topMargin: 12
            }

            DmePanel { anchors.fill: parent }
            Column {
                anchors.centerIn: parent
                spacing: 6
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: mapView.pathPreviewCount > 0
                          ? "Path preview: " + mapView.pathPreviewCount + " segments"
                          : "Drag on the map to draw a path"
                    color: "#E0E0E0"
                    font.pixelSize: 11
                }
                Row {
                    spacing: 8
                    DmeButton {
                        text: "Apply"
                        width: 90
                        enabled: mapView.pathPreviewCount > 0 && !mapView.pathBuilderDrawing
                        onClicked: mapView.commitPathPreview()
                    }
                    DmeButton {
                        text: "Redraw"
                        width: 90
                        enabled: mapView.pathPreviewCount > 0
                        onClicked: mapView.clearPathPreview()
                    }
                    DmeButton { text: "Close"; width: 90; onClicked: mapView.cancelPathBuilder() }
                }
            }
        }

        Item {
            z: 20
            visible: mapView.minimapOn
            width: 236
            height: 262
            anchors {
                right: parent.right
                top: parent.top
                margins: 10
            }

            DmePanel {
                anchors.fill: parent
            }
            Text {
                id: minimapTitle
                anchors {
                    left: parent.left
                    top: parent.top
                    leftMargin: 8
                    topMargin: 5
                }
                text: "Minimap  -  floor " + mapView.floor
                color: "#ddd"
                font.pixelSize: 12
                font.bold: true
            }
            Text {
                anchors {
                    right: parent.right
                    top: parent.top
                    rightMargin: 8
                    topMargin: 4
                }
                text: "x"
                color: closeMinimapArea.containsMouse ? "#fff" : "#999"
                font.pixelSize: 13
                font.bold: true
                MouseArea {
                    id: closeMinimapArea
                    anchors.fill: parent
                    anchors.margins: -4
                    hoverEnabled: true
                    onClicked: mapView.minimapOn = false
                }
            }
            MinimapView {
                source: mapView
                anchors {
                    left: parent.left
                    right: parent.right
                    bottom: parent.bottom
                    top: minimapTitle.bottom
                    margins: 6
                    topMargin: 4
                }
            }
        }


        Rectangle {
            anchors {
                horizontalCenter: parent.horizontalCenter
                top: parent.top
                topMargin: 8
            }
            visible: workspace.app.savedToast.length > 0
            width: toastLabel.implicitWidth + 20
            height: 26
            radius: 5
            color: "#E622432f"
            Text {
                id: toastLabel
                anchors.centerIn: parent
                text: workspace.app.savedToast
                color: "#eaffea"
                font.pixelSize: 12
            }
        }

    }



    Rectangle {
        id: loadingOverlay
        visible: Backend.otbmReader.loading
        anchors.centerIn: parent
        width: Math.min(420, parent.width - 40)
        height: 112
        z: 100
        radius: 2
        color: (("#F02A2A2A"))
        border.width: 1
        border.color: (("#777"))

        Column {
            anchors { fill: parent; margins: 16 }
            spacing: 10

            Text {
                text: "Loading map in background"
                color: (("#E0E0E0"))
                font.pixelSize: 13
                font.bold: true
            }
            Text {
                width: parent.width
                text: Backend.otbmReader.loadingStage
                color: (("#C0C0C0"))
                font.pixelSize: 11
                elide: Text.ElideRight
            }
            Rectangle {
                width: parent.width
                height: 7
                radius: 4
                color: (("#151515"))
                Rectangle {
                    width: parent.width * Backend.otbmReader.loadingProgress / 100
                    height: parent.height
                    radius: parent.radius
                    color: (("#4FAE62"))

                    Behavior on width {
                        NumberAnimation { duration: 25; easing.type: Easing.OutCubic }
                    }
                }
            }
        }
    }
}
