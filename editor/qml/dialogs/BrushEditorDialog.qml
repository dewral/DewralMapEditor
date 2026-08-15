import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../style"

DmeDialog {
    id: root
    property var mapCtrl: null

    title: "Tileset & Brush Manager"

    property string tab: "tilesets"
    property string curGround: ""
    property string curWall: ""
    property string curDoodad: ""
    property int selectedServerId: 0
    property var selectedServerIds: []
    property int pickerSelectionAnchor: -1
    property int pickerCellSize: 52

    readonly property bool grayUi: Backend.uiTheme.style === "gray-dark"
    readonly property bool modernUi: Backend.uiTheme.style !== "classic"
    readonly property color textColor: grayUi ? "#F0F0F0" : (modernUi ? "#F0F6FC" : "#D0D0D0")
    readonly property color mutedColor: grayUi ? "#A0A0A0" : (modernUi ? "#8B949E" : "#999999")
    readonly property color panelColor: grayUi ? "#242424" : (modernUi ? "#0D1117" : "#252525")
    readonly property color cellColor: grayUi ? "#2D2D2D" : (modernUi ? "#161B22" : "#252525")
    readonly property color borderColor: grayUi ? "#494949" : (modernUi ? "#30363D" : "#3A3A3A")
    readonly property color accentColor: grayUi ? "#C79A3B" : (modernUi ? "#2EA043" : "#7FDC8F")

    property var tilesetCategoryCodes: ["terrain", "doodad", "item", "raw", "collection", "door"]
    property var tilesetCategoryLabels: ["Terrain", "Doodads", "Items", "RAW", "Collections", "Doors"]
    property string tilesetCategory: "terrain"
    property var tilesetNames: []
    property var tilesetItems: []
    property string curTileset: ""
    property int selectedTilesetItem: 0

    property var doodadPaletteNames: []
    property var doodadNames: []
    property int doodadWidth: 2
    property int doodadHeight: 2
    property var doodadCellItems: [[], [], [], []]
    property string pendingDeleteCategory: ""
    property string pendingDeleteTileset: ""

    property var borderSets: ({
            "": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
        })
    property string borderTarget: ""

    readonly property var borderIds: borderSets[borderTarget] || [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
    property var wallIds: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]

    function setBorderTile(bt, sid) {
        var sets = JSON.parse(JSON.stringify(borderSets));
        if (!sets[borderTarget])
            sets[borderTarget] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
        sets[borderTarget][bt] = sid;
        borderSets = sets;
    }

    function borderTargetKeys() {
        var keys = ["", "*"];
        var names = Backend.brushStore.groundBrushNames();
        for (var i = 0; i < names.length; ++i)
            if (names[i] !== groundNameField.text.trim())
                keys.push(names[i]);
        return keys;
    }
    function borderTargetLabel(key) {
        if (key === "")
            return "Empty (no neighbor)";
        if (key === "*")
            return "Any other brush";
        return key;
    }

    function iconSrc(id) {
        if (id <= 0)
            return "";
        var row = Backend.otbReader.rowForServerId(id);
        if (row < 0)
            return "";
        var d = Backend.otbReader.detailsAt(row);
        return Backend.sprReader.itemImageSource(d.spriteIds, d.itemWidth, d.itemHeight, d.layers);
    }

    function itemName(id) {
        var row = Backend.otbReader.rowForServerId(id);
        if (row < 0)
            return "Unknown item";
        var details = Backend.otbReader.detailsAt(row);
        return details.name || "Unnamed item";
    }

    function refreshTilesets(preferredName) {
        var names = Backend.tilesetStore.namesFor(tilesetCategory);
        tilesetNames = names;
        var wanted = preferredName === undefined ? curTileset : preferredName;
        var index = names.indexOf(wanted);
        if (index < 0 && names.length > 0)
            index = 0;
        tilesetCombo.currentIndex = index;
        loadTileset(index >= 0 ? names[index] : "");
    }

    function loadTileset(name) {
        curTileset = name || "";
        tilesetNameField.text = curTileset;
        tilesetItems = curTileset === "" ? []
                                           : Backend.tilesetStore.itemsFor(tilesetCategory, curTileset);
        selectedTilesetItem = 0;
    }

    function saveTilesetName() {
        var name = tilesetNameField.text.trim();
        if (name === "")
            return;
        if (curTileset === "") {
            if (Backend.tilesetStore.newTileset(tilesetCategory, name))
                refreshTilesets(name);
        } else if (Backend.tilesetStore.renameTileset(tilesetCategory, curTileset, name)) {
            if (tilesetCategory === "doodad")
                Backend.brushStore.renamePrefabPalette(curTileset, name);
            refreshTilesets(name);
        }
    }

    function addSelectedToTileset() {
        if (curTileset === "" || selectedServerIds.length === 0)
            return;
        if (Backend.tilesetStore.addItems(tilesetCategory, curTileset, selectedServerIds))
            loadTileset(curTileset);
    }

    function pickerItemSelected(serverId) {
        return selectedServerIds.indexOf(serverId) >= 0;
    }

    function selectPickerItem(serverId, row, modifiers) {
        if (serverId <= 0)
            return;
        var ctrl = (modifiers & Qt.ControlModifier) !== 0;
        var shift = (modifiers & Qt.ShiftModifier) !== 0;
        var selection = (ctrl || shift) ? selectedServerIds.slice() : [];

        if (shift && pickerSelectionAnchor >= 0) {
            if (!ctrl)
                selection = [];
            var first = Math.min(pickerSelectionAnchor, row);
            var last = Math.max(pickerSelectionAnchor, row);
            for (var i = first; i <= last; ++i) {
                var rangeId = pf.serverIdAtRow(i);
                if (rangeId > 0 && selection.indexOf(rangeId) < 0)
                    selection.push(rangeId);
            }
        } else if (ctrl) {
            var existing = selection.indexOf(serverId);
            if (existing >= 0)
                selection.splice(existing, 1);
            else
                selection.push(serverId);
            pickerSelectionAnchor = row;
        } else {
            selection = [serverId];
            pickerSelectionAnchor = row;
        }

        selectedServerIds = selection;
        selectedServerId = selection.indexOf(serverId) >= 0
                           ? serverId : (selection.length > 0 ? selection[selection.length - 1] : 0);
    }

    function removeSelectedFromTileset() {
        if (curTileset === "" || selectedTilesetItem <= 0)
            return;
        if (Backend.tilesetStore.removeItem(tilesetCategory, curTileset, selectedTilesetItem))
            loadTileset(curTileset);
    }

    function refreshDoodads(preferredPalette, preferredName) {
        var palettes = Backend.tilesetStore.namesFor("doodad");
        doodadPaletteNames = palettes;
        var paletteIndex = palettes.indexOf(preferredPalette || doodadPaletteCombo.currentText);
        if (paletteIndex < 0 && palettes.length > 0)
            paletteIndex = 0;
        doodadPaletteCombo.currentIndex = paletteIndex;
        var palette = paletteIndex >= 0 ? palettes[paletteIndex] : "";
        var entries = palette === "" ? [] : Backend.brushStore.prefabsForPalette(palette);
        var names = [];
        for (var i = 0; i < entries.length; ++i)
            names.push(entries[i].name);
        doodadNames = names;
        var nameIndex = names.indexOf(preferredName || curDoodad);
        doodadCombo.currentIndex = nameIndex;
        if (nameIndex >= 0)
            loadDoodad(names[nameIndex]);
        else
            newDoodad();
    }

    function resizeDoodadGrid(widthValue, heightValue) {
        var width = Math.max(1, Math.min(12, widthValue));
        var height = Math.max(1, Math.min(12, heightValue));
        var oldWidth = doodadWidth;
        var oldHeight = doodadHeight;
        var oldCells = doodadCellItems;
        var next = [];
        for (var y = 0; y < height; ++y) {
            for (var x = 0; x < width; ++x) {
                var oldIndex = y * oldWidth + x;
                next.push(x < oldWidth && y < oldHeight && oldCells[oldIndex]
                          ? oldCells[oldIndex].slice() : []);
            }
        }
        doodadWidth = width;
        doodadHeight = height;
        doodadCellItems = next;
        doodadWidthField.value = width;
        doodadHeightField.value = height;
    }

    function setDoodadCell(index, serverId, append) {
        if (index < 0 || index >= doodadWidth * doodadHeight)
            return;
        var cells = [];
        for (var i = 0; i < doodadCellItems.length; ++i)
            cells.push(doodadCellItems[i] ? doodadCellItems[i].slice() : []);
        if (serverId <= 0) {
            cells[index] = [];
        } else if (append) {
            if (cells[index].indexOf(serverId) < 0)
                cells[index].push(serverId);
        } else {
            cells[index] = [serverId];
        }
        doodadCellItems = cells;
    }

    function newDoodad() {
        curDoodad = "";
        doodadCombo.currentIndex = -1;
        doodadNameField.text = "";
        doodadWidth = 2;
        doodadHeight = 2;
        doodadWidthField.value = 2;
        doodadHeightField.value = 2;
        doodadCellItems = [[], [], [], []];
    }

    function loadDoodad(name) {
        var data = Backend.brushStore.prefabEdit(name);
        curDoodad = name;
        doodadNameField.text = name;
        var width = Math.max(1, Number(data.width || 1));
        var height = Math.max(1, Number(data.height || 1));
        doodadWidth = width;
        doodadHeight = height;
        doodadWidthField.value = width;
        doodadHeightField.value = height;
        var cells = [];
        for (var i = 0; i < width * height; ++i)
            cells.push([]);
        var tiles = data.tiles || [];
        for (var t = 0; t < tiles.length; ++t) {
            var index = Number(tiles[t].dy) * width + Number(tiles[t].dx);
            if (index >= 0 && index < cells.length)
                cells[index] = (tiles[t].items || []).slice();
        }
        doodadCellItems = cells;
    }

    function saveDoodad() {
        var name = doodadNameField.text.trim();
        var palette = doodadPaletteCombo.currentText;
        if (name === "" || palette === "")
            return;
        var tiles = [];
        var originX = Math.floor(doodadWidth / 2);
        var originY = Math.floor(doodadHeight / 2);
        for (var y = 0; y < doodadHeight; ++y) {
            for (var x = 0; x < doodadWidth; ++x) {
                var items = doodadCellItems[y * doodadWidth + x] || [];
                if (items.length > 0)
                    tiles.push({ dx: x - originX, dy: y - originY, dz: 0,
                                 items: items.slice() });
            }
        }
        if (tiles.length === 0)
            return;
        var oldName = curDoodad;
        if (Backend.brushStore.savePrefab(name, palette, tiles)) {
            if (oldName !== "" && oldName !== name)
                Backend.brushStore.deletePrefab(oldName);
            refreshDoodads(palette, name);
        }
    }

    function loadGround(name) {
        curGround = name;
        var d = Backend.brushStore.groundBrushEdit(name);
        zorderField.value = d.zorder;
        gItems.clear();
        for (var i = 0; i < d.items.length; ++i)
            gItems.append({
                sid: d.items[i].id,
                chance: d.items[i].chance
            });

        var sets = {
            "": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
        };
        for (var b = 0; b < d.borders.length; ++b)
            sets[d.borders[b].to] = d.borders[b].tiles.slice();
        borderSets = sets;
        borderTarget = "";
        groundNameField.text = name;
        targetCombo.syncFromApp();
    }
    function newGround() {
        curGround = "";
        groundNameField.text = "";
        zorderField.value = 3500;
        gItems.clear();
        borderSets = ({
                "": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
            });
        borderTarget = "";
        targetCombo.syncFromApp();
    }
    function saveGround() {
        var name = groundNameField.text.trim();
        if (name === "" || gItems.count === 0)
            return;
        var items = [];
        for (var i = 0; i < gItems.count; ++i)
            items.push({
                id: gItems.get(i).sid,
                chance: gItems.get(i).chance
            });

        var blocks = [];
        for (var key in borderSets)
            blocks.push({
                to: key,
                tiles: borderSets[key]
            });
        if (Backend.brushStore.saveGroundBrush(name, zorderField.value, items, blocks))
            loadGround(name);
    }

    function loadWall(name) {
        curWall = name;
        wallIds = Backend.brushStore.wallBrushEdit(name);
        wallNameField.text = name;
    }
    function newWall() {
        curWall = "";
        wallNameField.text = "";
        wallIds = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    }
    function saveWall() {
        var name = wallNameField.text.trim();
        if (name === "")
            return;
        if (Backend.brushStore.saveWallBrush(name, wallIds))
            loadWall(name);
    }

    Connections {
        target: Backend.brushStore
        function onBrushesChanged() {
            groundCombo.model = Backend.brushStore.groundBrushNames();
            wallCombo.model = Backend.brushStore.wallBrushNames();
            root.refreshDoodads(doodadPaletteCombo.currentText, root.curDoodad);
        }
    }
    Connections {
        target: Backend.tilesetStore
        function onTilesetsChanged() {
            root.refreshTilesets(root.curTileset);
            root.refreshDoodads(doodadPaletteCombo.currentText, root.curDoodad);
        }
    }
    onOpened: {
        groundCombo.model = Backend.brushStore.groundBrushNames();
        wallCombo.model = Backend.brushStore.wallBrushNames();
        newGround();
        newWall();
        refreshTilesets("");
        refreshDoodads("", "");
        selectedServerIds = [];
        selectedServerId = 0;
        pickerSelectionAnchor = -1;
    }

    DmeDialog {
        id: newDoodadPaletteDialog
        title: "New Doodad Palette"
        contentItem: Column {
            width: 320
            spacing: 10
            Text {
                width: parent.width
                text: "Create a category for your custom doodads and multi-tile compositions."
                color: root.mutedColor
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
            DmeTextField {
                id: newDoodadPaletteName
                width: parent.width
                placeholderText: "Palette name"
                onAccepted: createPaletteButton.clicked()
            }
            Text {
                id: newDoodadPaletteError
                width: parent.width
                color: "#F85149"
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8
                DmeButton {
                    id: createPaletteButton
                    text: "Create"
                    width: 90
                    onClicked: {
                        var name = newDoodadPaletteName.text.trim();
                        if (name === "")
                            return;
                        if (!Backend.tilesetStore.newTileset("doodad", name)) {
                            newDoodadPaletteError.text = Backend.tilesetStore.errorString
                                    || "Could not create this palette.";
                            return;
                        }
                        newDoodadPaletteError.text = "";
                        newDoodadPaletteDialog.close();
                        root.refreshDoodads(name, "");
                    }
                }
                DmeButton { text: "Cancel"; width: 90; onClicked: newDoodadPaletteDialog.close() }
            }
        }
        onOpened: {
            newDoodadPaletteName.text = "";
            newDoodadPaletteError.text = "";
        }
    }

    DmeConfirmDialog {
        id: deleteTilesetDialog
        title: "Delete tileset"
        message: root.pendingDeleteCategory === "doodad"
                 ? "Delete '" + root.pendingDeleteTileset + "' and all custom doodads stored in this palette?"
                 : "Delete tileset '" + root.pendingDeleteTileset + "'? The source items are not deleted."
        onAccepted: {
            var category = root.pendingDeleteCategory;
            var name = root.pendingDeleteTileset;
            if (category === "doodad")
                Backend.brushStore.deletePrefabsForPalette(name);
            if (Backend.tilesetStore.deleteTileset(category, name)) {
                root.curTileset = "";
                root.refreshTilesets("");
            }
            root.pendingDeleteCategory = "";
            root.pendingDeleteTileset = "";
        }
    }

    PaletteFilter {
        id: pf
        sourceModel: Backend.otbReader
        mode: "all"
    }

    contentItem: Item {
        id: body
        implicitWidth: 1010
        implicitHeight: 650

        Column {
            id: pickerCol
            width: 248
            anchors {
                left: parent.left
                top: parent.top
                bottom: parent.bottom
            }
            spacing: 6

            DmeTextField {
                width: parent.width
                placeholderText: "Search by name or ID..."
                onTextChanged: {
                    pf.searchText = text;
                    root.pickerSelectionAnchor = -1;
                }
            }

            DmePanel {
                id: pickerPanel
                width: parent.width
                height: Math.max(140, pickerCol.height - 76)

                GridView {
                    id: pickerGrid
                    anchors.fill: parent
                    anchors.margins: 3
                    clip: true
                    cellWidth: root.pickerCellSize
                    cellHeight: root.pickerCellSize
                    model: pf

                    delegate: Rectangle {
                        readonly property int sid: typeof serverId !== "undefined" ? serverId : 0
                        readonly property bool selected: root.pickerItemSelected(sid)
                        width: root.pickerCellSize - 4
                        height: root.pickerCellSize - 4
                        color: cellMa.containsMouse ? Qt.lighter(root.cellColor, 1.18) : root.cellColor
                        border.color: selected ? root.accentColor : root.borderColor
                        border.width: selected ? 2 : 1
                        radius: root.modernUi ? 4 : 0

                        Image {
                            anchors.centerIn: parent
                            width: Math.max(32, parent.width - 10)
                            height: Math.max(32, parent.height - 10)
                            fillMode: Image.PreserveAspectFit
                            smooth: false
                            cache: false
                            source: (typeof spriteIds !== "undefined" && spriteIds.length > 0) ? Backend.sprReader.itemImageSource(spriteIds, typeof itemWidth !== "undefined" ? itemWidth : 1, typeof itemHeight !== "undefined" ? itemHeight : 1, typeof layers !== "undefined" ? layers : 1) : ""
                        }
                        Rectangle {
                            anchors {
                                right: parent.right
                                bottom: parent.bottom
                                margins: 2
                            }
                            width: idLabel.implicitWidth + 6
                            height: idLabel.implicitHeight + 2
                            radius: 2
                            color: root.grayUi ? "#CC242424" : "#CC0D1117"
                            Text {
                                id: idLabel
                                anchors.centerIn: parent
                                text: typeof serverId !== "undefined" ? serverId : ""
                                color: root.textColor
                                font.pixelSize: 9
                            }
                        }

                        MouseArea {
                            id: cellMa
                            anchors.fill: parent
                            hoverEnabled: true
                            drag.target: dragGhost
                            onPressed: mouse => {
                                root.selectPickerItem(parent.sid, index, mouse.modifiers);
                                dragGhost.sid = parent.sid;
                                dragGhost.source = parent.children[0].source;
                                var p = mapToItem(body, mouse.x, mouse.y);
                                dragGhost.x = p.x - 16;
                                dragGhost.y = p.y - 16;
                            }
                            drag.onActiveChanged: dragGhost.visible = drag.active
                            onReleased: {
                                if (dragGhost.visible)
                                    dragGhost.Drag.drop();
                                dragGhost.visible = false;
                            }
                            onDoubleClicked: {
                                if (root.tab === "tilesets")
                                    root.addSelectedToTileset();
                            }
                        }
                    }
                }
                DmeScrollBar {
                    anchors {
                        right: parent.right
                        top: parent.top
                        bottom: parent.bottom
                    }
                    anchors.margins: 2
                    flickable: pickerGrid
                }
            }

            Row {
                width: parent.width
                height: 32
                spacing: 7

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Size"
                    color: root.mutedColor
                    font.pixelSize: 11
                }
                Slider {
                    id: pickerScaleSlider
                    anchors.verticalCenter: parent.verticalCenter
                    width: 135
                    from: 40
                    to: 76
                    stepSize: 4
                    value: root.pickerCellSize
                    snapMode: Slider.SnapAlways
                    onMoved: root.pickerCellSize = Math.round(value)
                    background: Rectangle {
                        x: pickerScaleSlider.leftPadding
                        y: pickerScaleSlider.topPadding + pickerScaleSlider.availableHeight / 2 - height / 2
                        width: pickerScaleSlider.availableWidth
                        height: 4
                        radius: 2
                        color: root.borderColor
                        Rectangle {
                            width: pickerScaleSlider.visualPosition * parent.width
                            height: parent.height
                            radius: 2
                            color: root.accentColor
                        }
                    }
                    handle: Rectangle {
                        x: pickerScaleSlider.leftPadding + pickerScaleSlider.visualPosition
                           * (pickerScaleSlider.availableWidth - width)
                        y: pickerScaleSlider.topPadding + pickerScaleSlider.availableHeight / 2 - height / 2
                        width: 14
                        height: 14
                        radius: 7
                        color: root.textColor
                        border.width: 2
                        border.color: root.accentColor
                    }
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.selectedServerIds.length > 1
                          ? root.selectedServerIds.length + " selected" : root.pickerCellSize + " px"
                    color: root.selectedServerIds.length > 1 ? root.accentColor : root.mutedColor
                    font.pixelSize: 10
                }
            }
        }

        Column {
            id: editorColumn
            anchors {
                left: pickerCol.right
                right: parent.right
                top: parent.top
            }
            anchors.leftMargin: 10
            spacing: 8

            Row {
                spacing: 6
                DmeButton {
                    text: "Tilesets"
                    width: 100
                    checked: root.tab === "tilesets"
                    opacity: root.tab === "tilesets" ? 1.0 : 0.65
                    onClicked: root.tab = "tilesets"
                }
                DmeButton {
                    text: "Ground brush"
                    width: 110
                    checked: root.tab === "ground"
                    opacity: root.tab === "ground" ? 1.0 : 0.65
                    onClicked: root.tab = "ground"
                }
                DmeButton {
                    text: "Wall brush"
                    width: 110
                    checked: root.tab === "wall"
                    opacity: root.tab === "wall" ? 1.0 : 0.65
                    onClicked: root.tab = "wall"
                }
                DmeButton {
                    text: "Doodad composer"
                    width: 130
                    checked: root.tab === "doodad"
                    opacity: root.tab === "doodad" ? 1.0 : 0.65
                    onClicked: root.tab = "doodad"
                }
            }

            DmeSeparator { width: parent.width }

            Column {
                visible: root.tab === "tilesets"
                spacing: 10
                width: parent.width

                Text {
                    text: "Tileset Manager"
                    color: root.textColor
                    font { pixelSize: 17; bold: true }
                }
                Text {
                    width: parent.width
                    text: "Organize the categories shown in every palette. Use Ctrl to select individual items and Shift to select a range, then double-click or use Add selected."
                    color: root.mutedColor
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }

                Row {
                    spacing: 8
                    Text { text: "Palette"; color: root.mutedColor; anchors.verticalCenter: parent.verticalCenter }
                    DmeComboBox {
                        id: tilesetCategoryCombo
                        width: 150
                        model: root.tilesetCategoryLabels
                        currentIndex: 0
                        onActivated: index => {
                            root.tilesetCategory = root.tilesetCategoryCodes[index];
                            root.curTileset = "";
                            root.refreshTilesets("");
                        }
                    }
                    Text { text: "Tileset"; color: root.mutedColor; anchors.verticalCenter: parent.verticalCenter }
                    DmeComboBox {
                        id: tilesetCombo
                        width: 220
                        model: root.tilesetNames
                        onActivated: index => root.loadTileset(index >= 0 ? root.tilesetNames[index] : "")
                    }
                    DmeButton {
                        text: "New"
                        width: 70
                        onClicked: {
                            root.curTileset = "";
                            tilesetCombo.currentIndex = -1;
                            tilesetNameField.text = "";
                            root.tilesetItems = [];
                            tilesetNameField.forceActiveFocus();
                        }
                    }
                }

                Row {
                    spacing: 8
                    Text { text: root.curTileset === "" ? "New name" : "Name"; color: root.mutedColor; anchors.verticalCenter: parent.verticalCenter }
                    DmeTextField {
                        id: tilesetNameField
                        width: 260
                        placeholderText: "Tileset name"
                        onAccepted: root.saveTilesetName()
                    }
                    DmeButton {
                        text: root.curTileset === "" ? "Create" : "Rename"
                        width: 90
                        enabled: tilesetNameField.text.trim() !== ""
                        onClicked: root.saveTilesetName()
                    }
                    DmeButton {
                        text: "Delete tileset"
                        width: 110
                        variant: "danger"
                        enabled: root.curTileset !== ""
                        onClicked: {
                            root.pendingDeleteCategory = root.tilesetCategory;
                            root.pendingDeleteTileset = root.curTileset;
                            deleteTilesetDialog.open();
                        }
                    }
                }

                DmePanel {
                    width: parent.width
                    height: 430

                    Item {
                        anchors.fill: parent
                        anchors.margins: 10

                        Text {
                            id: emptyTilesetText
                            anchors.centerIn: parent
                            visible: root.curTileset === "" || root.tilesetItems.length === 0
                            text: root.curTileset === "" ? "Create or select a tileset"
                                                         : "This tileset is empty\nDrop or add items from the picker"
                            color: root.mutedColor
                            horizontalAlignment: Text.AlignHCenter
                        }

                        GridView {
                            id: tilesetGrid
                            anchors.fill: parent
                            anchors.bottomMargin: 42
                            clip: true
                            cellWidth: 76
                            cellHeight: 82
                            model: root.tilesetItems

                            delegate: Rectangle {
                                required property var modelData
                                required property int index
                                width: 70
                                height: 76
                                radius: root.modernUi ? 5 : 0
                                color: root.selectedTilesetItem === Number(modelData)
                                       ? (root.grayUi ? "#4A3A1F" : "#163B2C") : root.cellColor
                                border.width: root.selectedTilesetItem === Number(modelData) ? 2 : 1
                                border.color: root.selectedTilesetItem === Number(modelData)
                                              ? root.accentColor : root.borderColor
                                Image {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    anchors.top: parent.top
                                    anchors.topMargin: 7
                                    width: 48
                                    height: 48
                                    fillMode: Image.PreserveAspectFit
                                    smooth: false
                                    cache: true
                                    source: root.iconSrc(Number(modelData))
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    anchors.bottom: parent.bottom
                                    anchors.bottomMargin: 4
                                    text: Number(modelData)
                                    color: root.mutedColor
                                    font.pixelSize: 10
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: root.selectedTilesetItem = Number(modelData)
                                    ToolTip.visible: containsMouse
                                    ToolTip.delay: 500
                                    ToolTip.text: root.itemName(Number(modelData)) + " (sid " + Number(modelData) + ")"
                                }
                            }
                        }

                        DropArea {
                            anchors.fill: tilesetGrid
                            onDropped: drop => {
                                if (root.curTileset !== "" && drop.source.sid > 0
                                        && Backend.tilesetStore.addItem(root.tilesetCategory, root.curTileset, drop.source.sid))
                                    root.loadTileset(root.curTileset);
                            }
                        }

                        Row {
                            anchors.left: parent.left
                            anchors.bottom: parent.bottom
                            spacing: 8
                            DmeButton {
                                text: root.selectedServerIds.length > 1
                                      ? "Add selected (" + root.selectedServerIds.length + ")"
                                      : "Add selected"
                                width: root.selectedServerIds.length > 1 ? 135 : 110
                                enabled: root.curTileset !== "" && root.selectedServerIds.length > 0
                                onClicked: root.addSelectedToTileset()
                            }
                            DmeButton {
                                text: "Remove selected"
                                width: 130
                                enabled: root.selectedTilesetItem > 0
                                onClicked: root.removeSelectedFromTileset()
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: root.curTileset === "" ? "" : root.tilesetItems.length + " items"
                                color: root.mutedColor
                            }
                        }
                    }
                }
            }

            Column {
                visible: root.tab === "doodad"
                spacing: 10
                width: parent.width

                Text {
                    text: "Doodad Composer"
                    color: root.textColor
                    font { pixelSize: 17; bold: true }
                }
                Text {
                    width: parent.width
                    text: "Build reusable multi-tile doodads. Set the footprint, then drop item pieces into the grid. Dropping several items on one cell creates a stack."
                    color: root.mutedColor
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }

                Row {
                    spacing: 8
                    Text { text: "Doodad palette"; color: root.mutedColor; anchors.verticalCenter: parent.verticalCenter }
                    DmeComboBox {
                        id: doodadPaletteCombo
                        width: 210
                        model: root.doodadPaletteNames
                        onActivated: root.refreshDoodads(currentText, "")
                    }
                    DmeButton {
                        text: "New palette"
                        width: 100
                        onClicked: newDoodadPaletteDialog.open()
                    }
                    Text { text: "Doodad"; color: root.mutedColor; anchors.verticalCenter: parent.verticalCenter }
                    DmeComboBox {
                        id: doodadCombo
                        width: 190
                        model: root.doodadNames
                        onActivated: index => {
                            if (index >= 0)
                                root.loadDoodad(root.doodadNames[index]);
                        }
                    }
                    DmeButton { text: "New"; width: 64; onClicked: root.newDoodad() }
                }

                Row {
                    spacing: 8
                    Text { text: "Name"; color: root.mutedColor; anchors.verticalCenter: parent.verticalCenter }
                    DmeTextField {
                        id: doodadNameField
                        width: 220
                        placeholderText: "Custom doodad name"
                    }
                    Text { text: "Width"; color: root.mutedColor; anchors.verticalCenter: parent.verticalCenter }
                    DmeSpinBox {
                        id: doodadWidthField
                        width: 72
                        from: 1
                        to: 12
                        value: 2
                        onValueModified: root.resizeDoodadGrid(value, root.doodadHeight)
                    }
                    Text { text: "Height"; color: root.mutedColor; anchors.verticalCenter: parent.verticalCenter }
                    DmeSpinBox {
                        id: doodadHeightField
                        width: 72
                        from: 1
                        to: 12
                        value: 2
                        onValueModified: root.resizeDoodadGrid(root.doodadWidth, value)
                    }
                    DmeButton {
                        text: "Clear grid"
                        width: 90
                        onClicked: {
                            var cells = [];
                            for (var i = 0; i < root.doodadWidth * root.doodadHeight; ++i)
                                cells.push([]);
                            root.doodadCellItems = cells;
                        }
                    }
                }

                DmePanel {
                    width: parent.width
                    height: 440

                    Flickable {
                        id: doodadFlick
                        anchors.fill: parent
                        anchors.margins: 12
                        clip: true
                        contentWidth: Math.max(width, doodadGrid.width)
                        contentHeight: Math.max(height, doodadGrid.height)

                        Grid {
                            id: doodadGrid
                            x: Math.max(0, (doodadFlick.width - width) / 2)
                            y: Math.max(0, (doodadFlick.height - height) / 2)
                            columns: root.doodadWidth
                            spacing: 5
                            width: root.doodadWidth * 70 + Math.max(0, root.doodadWidth - 1) * spacing
                            height: root.doodadHeight * 70 + Math.max(0, root.doodadHeight - 1) * spacing

                            Repeater {
                                model: root.doodadWidth * root.doodadHeight
                                delegate: Rectangle {
                                    required property int index
                                    readonly property var ids: root.doodadCellItems[index] || []
                                    width: 70
                                    height: 70
                                    radius: root.modernUi ? 5 : 0
                                    clip: true
                                    color: doodadDrop.containsDrag ? (root.grayUi ? "#5A4721" : "#163B2C")
                                                                      : root.cellColor
                                    border.width: doodadDrop.containsDrag ? 2 : 1
                                    border.color: doodadDrop.containsDrag ? root.accentColor : root.borderColor

                                    Image {
                                        anchors.centerIn: parent
                                        width: 56
                                        height: 56
                                        fillMode: Image.PreserveAspectFit
                                        smooth: false
                                        cache: true
                                        source: parent.ids.length > 0
                                                ? root.iconSrc(parent.ids[parent.ids.length - 1]) : ""
                                    }
                                    Text {
                                        anchors.centerIn: parent
                                        visible: parent.ids.length === 0
                                        text: (index % root.doodadWidth) + "," + Math.floor(index / root.doodadWidth)
                                        color: root.mutedColor
                                        font.pixelSize: 10
                                    }
                                    Rectangle {
                                        visible: parent.ids.length > 1
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.margins: 4
                                        width: 20
                                        height: 18
                                        radius: 9
                                        color: root.accentColor
                                        Text {
                                            anchors.centerIn: parent
                                            text: parent.parent.ids.length
                                            color: "white"
                                            font { pixelSize: 10; bold: true }
                                        }
                                    }
                                    DropArea {
                                        id: doodadDrop
                                        anchors.fill: parent
                                        onDropped: drop => root.setDoodadCell(index, drop.source.sid, true)
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        acceptedButtons: Qt.RightButton
                                        hoverEnabled: true
                                        onClicked: root.setDoodadCell(index, 0, false)
                                        ToolTip.visible: containsMouse && parent.ids.length > 0
                                        ToolTip.delay: 500
                                        ToolTip.text: parent.ids.map(function(id) {
                                            return root.itemName(id) + " (" + id + ")";
                                        }).join("\n") + "\nRight-click to clear"
                                    }
                                }
                            }
                        }
                    }
                }

                Row {
                    spacing: 8
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.doodadWidth + " x " + root.doodadHeight + " tiles ("
                              + (root.doodadWidth * 32) + " x " + (root.doodadHeight * 32) + " px)"
                        color: root.mutedColor
                    }
                    Item { width: 12; height: 1 }
                    DmeButton {
                        text: root.curDoodad === "" ? "Create doodad" : "Save doodad"
                        width: 120
                        enabled: doodadNameField.text.trim() !== "" && doodadPaletteCombo.currentIndex >= 0
                        onClicked: root.saveDoodad()
                    }
                    DmeButton {
                        text: "Delete"
                        width: 80
                        variant: "danger"
                        enabled: root.curDoodad !== ""
                        onClicked: {
                            var palette = doodadPaletteCombo.currentText;
                            Backend.brushStore.deletePrefab(root.curDoodad);
                            root.refreshDoodads(palette, "");
                        }
                    }
                }
            }

            Column {
                visible: root.tab === "ground"
                spacing: 6
                width: parent.width

                Row {
                    spacing: 6
                    DmeComboBox {
                        id: groundCombo
                        width: 150
                        height: 23
                        onActivated: root.loadGround(model[currentIndex])
                    }
                    DmeButton {
                        text: "New"
                        width: 60
                        onClicked: root.newGround()
                    }
                    DmeButton {
                        text: "Remove"
                        width: 60
                        enabled: root.curGround !== ""
                        onClicked: {
                            Backend.brushStore.deleteGroundBrush(root.curGround);
                            root.newGround();
                        }
                    }
                }
                Row {
                    spacing: 6
                    Text {
                        text: "Name"
                        color: root.mutedColor
                        font.pixelSize: 11
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    DmeTextField {
                        id: groundNameField
                        width: 150
                        height: 22
                    }
                    Text {
                        text: "Z-order"
                        color: root.mutedColor
                        font.pixelSize: 11
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    DmeSpinBox {
                        id: zorderField
                        width: 80
                        from: 0
                        to: 65535
                        value: 3500
                    }
                }

                Text {
                    text: "Ground items (drop an item; weight = chance)"
                    color: root.mutedColor
                    font.pixelSize: 11
                }
                Rectangle {
                    width: parent.width
                    height: 66
                    color: gDrop.containsDrag ? Qt.darker(root.accentColor, 2.3) : root.cellColor
                    border.color: gDrop.containsDrag ? root.accentColor : root.borderColor
                    border.width: 1

                    DropArea {
                        id: gDrop
                        anchors.fill: parent
                        onDropped: drop => {
                            var sid = drop.source.sid;
                            if (sid > 0)
                                gItems.append({
                                    sid: sid,
                                    chance: 10
                                });
                        }
                    }
                    ListView {
                        anchors.fill: parent
                        anchors.margins: 4
                        orientation: ListView.Horizontal
                        spacing: 4
                        clip: true
                        model: ListModel {
                            id: gItems
                        }
                        delegate: Column {
                            spacing: 1
                            Image {
                                width: 32
                                height: 32
                                smooth: false
                                cache: false
                                fillMode: Image.PreserveAspectFit
                                source: root.iconSrc(sid)
                                MouseArea {
                                    anchors.fill: parent
                                    acceptedButtons: Qt.RightButton
                                    onClicked: gItems.remove(index)
                                }
                            }
                            DmeSpinBox {
                                width: 44
                                height: 18
                                from: 1
                                to: 1000
                                value: chance
                                onValueModified: gItems.setProperty(index, "chance", value)
                            }
                        }
                    }
                }

                Text {
                    text: "Borders (drop tiles; right click clears a slot)"
                    color: root.mutedColor
                    font.pixelSize: 11
                }

                Row {
                    spacing: 6
                    Text {
                        text: "Border target:"
                        color: root.mutedColor
                        font.pixelSize: 11
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    DmeComboBox {
                        id: targetCombo
                        width: 190
                        height: 23
                        property var keys: []
                        function syncFromApp() {
                            keys = root.borderTargetKeys();
                            model = keys.map(function (k) {
                                return root.borderTargetLabel(k);
                            });
                            var idx = keys.indexOf(root.borderTarget);
                            currentIndex = idx >= 0 ? idx : 0;
                        }
                        onActivated: root.borderTarget = keys[currentIndex]
                    }
                }
                Item {

                    width: 5 * 50 - 6
                    height: 5 * 50 - 6

                    Rectangle {
                        x: 2 * 50
                        y: 2 * 50
                        width: 44
                        height: 44
                        color: root.panelColor
                        border.color: root.borderColor
                        border.width: 1
                        Image {
                            anchors.centerIn: parent
                            width: 32
                            height: 32
                            smooth: false
                            cache: false
                            fillMode: Image.PreserveAspectFit
                            source: gItems.count > 0 ? root.iconSrc(gItems.get(0).sid) : ""
                        }
                        Text {
                            anchors.centerIn: parent
                            visible: gItems.count === 0
                            text: "Ground"
                            color: root.mutedColor
                            font.pixelSize: 9
                        }
                    }

                    Repeater {
                        model: [
                            {
                                bt: 9,
                                lab: "DNW",
                                cx: 0,
                                cy: 0
                            },
                            {
                                bt: 1,
                                lab: "N",
                                cx: 2,
                                cy: 0
                            },
                            {
                                bt: 10,
                                lab: "DNE",
                                cx: 4,
                                cy: 0
                            },
                            {
                                bt: 5,
                                lab: "CNW",
                                cx: 1,
                                cy: 1
                            },
                            {
                                bt: 6,
                                lab: "CNE",
                                cx: 3,
                                cy: 1
                            },
                            {
                                bt: 4,
                                lab: "W",
                                cx: 0,
                                cy: 2
                            },
                            {
                                bt: 2,
                                lab: "E",
                                cx: 4,
                                cy: 2
                            },
                            {
                                bt: 7,
                                lab: "CSW",
                                cx: 1,
                                cy: 3
                            },
                            {
                                bt: 8,
                                lab: "CSE",
                                cx: 3,
                                cy: 3
                            },
                            {
                                bt: 12,
                                lab: "DSW",
                                cx: 0,
                                cy: 4
                            },
                            {
                                bt: 3,
                                lab: "S",
                                cx: 2,
                                cy: 4
                            },
                            {
                                bt: 11,
                                lab: "DSE",
                                cx: 4,
                                cy: 4
                            }
                        ]
                        delegate: Rectangle {
                            required property var modelData
                            x: modelData.cx * 50
                            y: modelData.cy * 50
                            width: 44
                            height: 44
                            color: slotDrop.containsDrag ? Qt.darker(root.accentColor, 2.3) : root.cellColor
                            border.color: slotDrop.containsDrag ? root.accentColor : root.borderColor
                            border.width: 1

                            Image {
                                anchors.centerIn: parent
                                width: 32
                                height: 32
                                smooth: false
                                cache: false
                                fillMode: Image.PreserveAspectFit
                                source: root.iconSrc(root.borderIds[modelData.bt])
                            }
                            Text {
                                anchors {
                                    left: parent.left
                                    top: parent.top
                                    margins: 1
                                }
                                text: modelData.lab
                                color: root.mutedColor
                                font.pixelSize: 8
                            }
                            DropArea {
                                id: slotDrop
                                anchors.fill: parent
                                onDropped: drop => root.setBorderTile(modelData.bt, drop.source.sid)
                            }
                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.RightButton
                                onClicked: root.setBorderTile(modelData.bt, 0)
                            }
                        }
                    }
                }

                Row {
                    spacing: 6
                    DmeButton {
                        text: "Save"
                        width: 90
                        onClicked: root.saveGround()
                    }
                    DmeButton {
                        text: "Test on map"
                        width: 120
                        enabled: gItems.count > 0 && root.curGround !== ""
                        onClicked: if (root.mapCtrl)
                            root.mapCtrl.useGroundBrush(gItems.get(0).sid)
                    }
                }
            }

            Column {
                visible: root.tab === "wall"
                spacing: 6
                width: parent.width

                Row {
                    spacing: 6
                    DmeComboBox {
                        id: wallCombo
                        width: 150
                        height: 23
                        onActivated: root.loadWall(model[currentIndex])
                    }
                    DmeButton {
                        text: "New"
                        width: 60
                        onClicked: root.newWall()
                    }
                    DmeButton {
                        text: "Remove"
                        width: 60
                        enabled: root.curWall !== ""
                        onClicked: {
                            Backend.brushStore.deleteWallBrush(root.curWall);
                            root.newWall();
                        }
                    }
                }
                Row {
                    spacing: 6
                    Text {
                        text: "Name"
                        color: root.mutedColor
                        font.pixelSize: 11
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    DmeTextField {
                        id: wallNameField
                        width: 150
                        height: 22
                    }
                }

                Text {
                    text: "Wall slots by connection (drop; right click clears)"
                    color: root.mutedColor
                    font.pixelSize: 11
                }
                Grid {
                    columns: 6
                    spacing: 3
                    Repeater {

                        model: ["•", "╵", "╴", "┘", "╶", "└", "─", "┴", "╷", "│", "┐", "┤", "┌", "├", "┬", "┼", "✦"]
                        delegate: Rectangle {
                            required property string modelData
                            required property int index
                            width: 44
                            height: 52
                            color: wDrop.containsDrag ? Qt.darker(root.accentColor, 2.3) : root.cellColor
                            border.color: wDrop.containsDrag ? root.accentColor : root.borderColor
                            border.width: 1
                            Image {
                                anchors {
                                    horizontalCenter: parent.horizontalCenter
                                    top: parent.top
                                    topMargin: 2
                                }
                                width: 32
                                height: 32
                                smooth: false
                                cache: false
                                fillMode: Image.PreserveAspectFit
                                source: root.iconSrc(root.wallIds[index])
                            }
                            Text {
                                anchors {
                                    horizontalCenter: parent.horizontalCenter
                                    bottom: parent.bottom
                                    bottomMargin: 1
                                }
                                text: modelData
                                color: root.mutedColor
                                font.pixelSize: 12
                                font.bold: true
                            }
                            DropArea {
                                id: wDrop
                                anchors.fill: parent
                                onDropped: drop => {
                                    var w = root.wallIds.slice();
                                    w[index] = drop.source.sid;
                                    root.wallIds = w;
                                }
                            }
                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.RightButton
                                onClicked: {
                                    var w = root.wallIds.slice();
                                    w[index] = 0;
                                    root.wallIds = w;
                                }
                            }
                        }
                    }
                }

                DmeButton {
                    text: "Save"
                    width: 90
                    onClicked: root.saveWall()
                }
            }

            DmeButton {
                text: "Close"
                width: 90
                onClicked: root.close()
            }
        }

        Image {
            id: dragGhost
            width: 32
            height: 32
            visible: false
            z: 1000
            smooth: false
            cache: false
            fillMode: Image.PreserveAspectFit
            opacity: 0.85
            property int sid: 0
            Drag.active: visible
            Drag.source: dragGhost
            Drag.hotSpot.x: 16
            Drag.hotSpot.y: 16
        }
    }
}
