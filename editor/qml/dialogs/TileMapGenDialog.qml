pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import Tibia 1.0
import "../style"
import "../components"

DmeDialog {
    id: dialog
    required property var mapCtrl
    title: "Tile Map Generator"
    modal: false; dim: false
    width: Math.min(800, (Overlay.overlay ? Overlay.overlay.width : 832) - 32)
    height: Math.min(720, (Overlay.overlay ? Overlay.overlay.height : 752) - 32)
    property var materials: ({grounds: [], floors: [], backgrounds: [], walls: [], decorations: [], monsters: []})
    property var monsters: []
    property string activeProfileId: ""
    readonly property var profileEntries: Backend.tileMapGenProfiles.profiles
    property string message: "Choose your materials and settings, then generate a preview on the map."
    readonly property color textColor: Backend.uiTheme.style === "windows-classic" ? "#202020" : "#c9d1d9"
    readonly property bool busy: Backend.tileMapGen.busy
    readonly property bool fitsPlacement: mapWidth.value * mapHeight.value * floors.value <= 100000
    readonly property string recipeKey: JSON.stringify(generationOptions())
    onRecipeKeyChanged: Backend.tileMapGen.clearPreview()

    Settings {
        category: "tileMapGenerator"
        property alias width: mapWidth.value
        property alias height: mapHeight.value
        property alias floorCount: floors.value
        property alias seed: seed.text
        property alias style: style.currentIndex
        property alias x: destinationX.value
        property alias y: destinationY.value
        property alias downId: downId.value
        property alias upId: upId.value
    }
    function optional(combo) { return combo.currentIndex > 0 ? combo.currentText : ""; }
    function groundEntries(noneLabel, walking) {
        const rows = noneLabel ? [{name: "", label: noneLabel, clientId: 0, usable: true}] : [];
        const grounds = materials.grounds || [];
        for (let i = 0; i < grounds.length; ++i) {
            if (walking ? grounds[i].walking : grounds[i].blocking)
                rows.push(Object.assign({}, grounds[i], {usable: true}));
        }
        return rows;
    }
    function selectGround(combo, name) {
        for (let i = 0; i < combo.model.length; ++i) if (combo.model[i].name === name) { combo.currentIndex = i; return; }
        if (name) { const copy = Array.from(combo.model); copy.push({name: name, label: name + " (unavailable)", clientId: 0, usable: false}); combo.model = copy; combo.currentIndex = copy.length - 1; }
        else combo.currentIndex = -1;
    }
    function selectChoice(combo, name) {
        const names = Array.from(combo.model);
        let index = name ? names.indexOf(name) : 0;
        if (index < 0) { names.push(name); combo.model = names; index = names.length - 1; }
        combo.currentIndex = index;
    }
    function refreshMaterials() {
        const selected = [floorBrush.currentName, backgroundBrush.currentName, secondaryBrush.currentName, waterBrush.currentName, lavaBrush.currentName];
        materials = Backend.tileMapGen.generationMaterials();
        const combos = [floorBrush, backgroundBrush, secondaryBrush, waterBrush, lavaBrush];
        const labels = ["", "Void", "None", "None", "None"];
        for (let i = 0; i < combos.length; ++i) {
            combos[i].model = groundEntries(labels[i], i === 0 || i === 2);
            if (selected[i]) selectGround(combos[i], selected[i]);
            else combos[i].currentIndex = combos[i].model.length ? 0 : -1;
        }
    }
    function monsterChoices(index) {
        const selected = monsters[index] ? monsters[index].name : "";
        const names = [];
        for (let i = 0; i < materials.monsters.length; ++i) {
            const name = materials.monsters[i];
            if (name === selected || !monsters.some(entry => entry.name === name)) names.push(name);
        }
        if (selected && names.indexOf(selected) < 0) names.push(selected);
        return names;
    }
    function addMonster() {
        if (monsters.length >= 5) return;
        const available = monsterChoices(-1);
        if (available.length) monsters = monsters.concat([{name: available[0], weight: 1}]);
    }
    function editMonster(index, field, value) {
        const copy = monsters.slice(); copy[index] = Object.assign({}, copy[index]); copy[index][field] = value;
        monsters = copy;
    }
    function removeMonster(index) { const copy = monsters.slice(); copy.splice(index, 1); monsters = copy; }
    function profileNames() {
        const names = ["New profile"];
        for (let i = 0; i < profileEntries.length; ++i) names.push(profileEntries[i].name);
        return names;
    }
    function syncProfileSelection() {
        let index = 0;
        for (let i = 0; i < profileEntries.length; ++i) if (profileEntries[i].id === activeProfileId) index = i + 1;
        savedProfiles.currentIndex = index;
    }
    function saveProfile(asNew) {
        const result = Backend.tileMapGenProfiles.save({version: 1, name: profileName.text,
            clientKey: Backend.docMgr.currentProfileKey || "", options: generationOptions()}, asNew ? "" : activeProfileId);
        if (!result.success) { message = result.error; return; }
        activeProfileId = result.profile.id; profileName.text = result.profile.name; syncProfileSelection();
        message = "Saved profile: " + result.profile.name;
    }
    function loadProfile(id) {
        const p = Backend.tileMapGenProfiles.load(id);
        if (!p || !p.options) { message = "Profile no longer exists."; return; }
        Backend.tileMapGen.clearPreview();
        const o = p.options, palette = o.palette, settings = o.settings;
        style.currentIndex = ["hunting", "cave", "linear"].indexOf(o.generator);
        mapWidth.value = o.width; mapHeight.value = o.height; floors.value = o.floors; seed.text = o.seed;
        destination.currentIndex = o.placement ? 0 : 1;
        destinationX.value = o.x; destinationY.value = o.y; destinationZ.value = o.z;
        downId.value = o.downId; upId.value = o.upId;
        selectGround(floorBrush, palette.floor); selectGround(backgroundBrush, palette.background);
        selectGround(secondaryBrush, palette.secondary); selectGround(waterBrush, palette.water); selectGround(lavaBrush, palette.lava);
        selectChoice(wallBrush, palette.wall); selectChoice(decoration, palette.decorations.length ? palette.decorations[0] : "");
        monsters = palette.creatures.map(entry => ({name: entry.name, weight: entry.weight}));
        rooms.value = settings.rooms; roomMin.value = settings.room_min; roomMax.value = settings.room_max;
        corridorWidth.value = settings.corridor_width; loops.value = Math.round(settings.loops * 100);
        caveDensity.value = Math.round(settings.cave_density * 100); smoothing.value = settings.smoothing;
        chamberSize.value = settings.chamber_size; terrainScale.value = settings.terrain_scale;
        waterAmount.value = Math.round(settings.water * 100); lavaAmount.value = Math.round(settings.lava * 100);
        decorationDensity.value = Math.round(settings.decoration_density * 1000); spawnDensity.value = Math.round(settings.spawn_density * 1000);
        spawnSpacing.value = settings.spawn_spacing; spawnRadius.value = settings.spawn_radius;
        respawn.value = settings.respawn_seconds; safeRadius.value = settings.safe_radius;
        activeProfileId = id; profileName.text = p.name; syncProfileSelection();
        message = "Loaded profile: " + p.name;
    }
    function generationOptions() {
        return {
            generator: ["hunting", "cave", "linear"][style.currentIndex],
            width: mapWidth.value, height: mapHeight.value, floors: floors.value,
            seed: seed.text, placement: destination.currentIndex === 0,
            x: destinationX.value, y: destinationY.value, z: destinationZ.value,
            downId: downId.value, upId: upId.value,
            palette: {
                floor: floorBrush.currentName, background: backgroundBrush.currentName,
                secondary: secondaryBrush.currentName, water: waterBrush.currentName, lava: lavaBrush.currentName,
                wall: optional(wallBrush), decorations: decoration.currentIndex > 0 ? [decoration.currentText] : [],
                creatures: monsters.map(entry => ({name: entry.name, weight: entry.weight}))
            },
            settings: {
                rooms: rooms.value, room_min: roomMin.value, room_max: roomMax.value,
                corridor_width: corridorWidth.value, loops: loops.value / 100,
                cave_density: caveDensity.value / 100, smoothing: smoothing.value,
                chamber_size: chamberSize.value, terrain_scale: terrainScale.value,
                water: waterAmount.value / 100, lava: lavaAmount.value / 100,
                decoration_density: decorationDensity.value / 1000,
                spawn_density: spawnDensity.value / 1000, spawn_spacing: spawnSpacing.value,
                spawn_radius: spawnRadius.value, respawn_seconds: respawn.value, safe_radius: safeRadius.value
            }
        };
    }
    function generate() { Backend.tileMapGen.generateMap(generationOptions(), mapCtrl); }
    onOpened: {
        refreshMaterials();
        destinationZ.value = mapCtrl.floor;
    }
    onClosed: Backend.tileMapGen.cancel()
    Connections {
        target: Backend.tileMapGen
        function onChanged() { if (Backend.tileMapGen.status) dialog.message = Backend.tileMapGen.status; }
    }
    Connections {
        target: Backend.docMgr
        function onCurrentChanged() {
            Backend.tileMapGen.cancel();
            dialog.refreshMaterials();
        }
    }
    FileDialog {
        id: packPicker; title: "Export Tile Map Generator materials"
        fileMode: FileDialog.SaveFile
        nameFilters: ["TileMapGen materials (*.tmgpack)"]; defaultSuffix: "tmgpack"
        onAccepted: {
            const error = Backend.tileMapGen.exportAssets(selectedFile, downId.value, upId.value);
            dialog.message = error || "Materials exported. Open the pack in TileMapGen's Materials tab.";
        }
    }
    Connections { target: Backend.brushStore; function onBrushesChanged() { if (dialog.opened) dialog.refreshMaterials(); } }
    Connections { target: Backend.otbReader; ignoreUnknownSignals: true; function onLoadedChanged() { if (dialog.opened) dialog.refreshMaterials(); } }
    Connections { target: Backend.datReader; ignoreUnknownSignals: true; function onLoadedChanged() { if (dialog.opened) dialog.refreshMaterials(); } }
    Connections { target: Backend.tileMapGenProfiles; function onProfilesChanged() { dialog.syncProfileSelection(); } }
    FileDialog {
        id: mapPicker; title: "Import a generated Tile Map Generator map"
        nameFilters: ["TileMapGen maps (*.tmg)"]
        onAccepted: Backend.tileMapGen.importMap(selectedFile, destination.currentIndex === 0, dialog.mapCtrl,
                                                destinationX.value, destinationY.value, destinationZ.value)
    }
    component Caption: Text { color: dialog.textColor; font.pixelSize: 12 }
    component NumberField: DmeSpinBox { Layout.fillWidth: true; Layout.preferredHeight: 30 }
    component Choice: DmeComboBox { Layout.fillWidth: true; Layout.preferredHeight: 30; currentIndex: 0 }
    component GroundChoice: GroundBrushPicker { Layout.fillWidth: true; Layout.preferredHeight: 38 }
    contentItem: ColumnLayout {
        spacing: 10
        Caption { Layout.fillWidth: true; text: "Connected to " + Backend.tileMapGen.generatorDirectory; elide: Text.ElideMiddle }
        Choice { id: workflow; objectName: "tileMapWorkflow"; enabled: !dialog.busy; model: ["Generate in DME", "Export materials for the studio", "Import a generated .tmg map"] }
        RowLayout {
            visible: workflow.currentIndex === 0; enabled: !dialog.busy; Layout.fillWidth: true
            Choice {
                id: savedProfiles; objectName: "tileMapProfiles"; model: dialog.profileNames()
                onActivated: index => {
                    if (index > 0) dialog.loadProfile(dialog.profileEntries[index - 1].id);
                    else { dialog.activeProfileId = ""; profileName.text = ""; }
                }
            }
            DmeTextField { id: profileName; objectName: "tileMapProfileName"; placeholderText: "Profile name"; Layout.fillWidth: true; Layout.preferredHeight: 30 }
            DmeButton { objectName: "saveTileMapProfile"; text: "Save"; enabled: profileName.text.trim().length > 0; onClicked: dialog.saveProfile(false) }
            DmeButton { objectName: "saveNewTileMapProfile"; text: "Save as new"; enabled: profileName.text.trim().length > 0; onClicked: dialog.saveProfile(true) }
        }
        ScrollView {
            id: scroll; Layout.fillWidth: true; Layout.fillHeight: true; clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ColumnLayout {
                width: scroll.availableWidth; spacing: 14; enabled: !dialog.busy
                GridLayout {
                    visible: workflow.currentIndex === 0; Layout.fillWidth: true
                    columns: 4; columnSpacing: 12; rowSpacing: 8
                    Caption { text: "Style" }
                    Choice { id: style; objectName: "tileMapStyle"; model: ["Hunting grounds", "Cave", "Linear dungeon"] }
                    Caption { text: "Seed" }
                    DmeTextField { id: seed; objectName: "tileMapSeed"; text: "dewral-forest"; Layout.fillWidth: true; Layout.preferredHeight: 30 }
                    Caption { text: "Width" }
                    NumberField { id: mapWidth; objectName: "tileMapWidth"; from: 24; to: 65535; value: 96 }
                    Caption { text: "Height" }
                    NumberField { id: mapHeight; objectName: "tileMapHeight"; from: 24; to: 65535; value: 96 }
                    Caption { text: "Floors" }
                    NumberField { id: floors; objectName: "tileMapFloors"; from: 1; to: 16; value: 1 }
                    DmeButton { text: "New seed"; Layout.columnSpan: 2; Layout.fillWidth: true; onClicked: seed.text = String(Date.now()) + "-" + String(Math.floor(Math.random() * 1000000)) }
                }
                Caption { visible: workflow.currentIndex === 0; text: "Materials from the active client"; font.bold: true }
                GridLayout {
                    visible: workflow.currentIndex === 0; Layout.fillWidth: true
                    columns: 4; columnSpacing: 12; rowSpacing: 8
                    Caption { text: "Walking floor" }
                    GroundChoice { id: floorBrush; objectName: "tileMapFloor" }
                    Caption { text: "Background" }
                    GroundChoice { id: backgroundBrush; objectName: "tileMapBackground" }
                    Caption { text: "Secondary floor" }
                    GroundChoice { id: secondaryBrush }
                    Caption { text: "Walls" }
                    Choice { id: wallBrush; model: ["None"].concat(Array.from(dialog.materials.walls)) }
                }
                Caption { visible: workflow.currentIndex !== 2; Layout.fillWidth: true; wrapMode: Text.WordWrap; text: "For multiple floors, enter matching down and up stair server IDs from this client." }
                ColumnLayout {
                    visible: workflow.currentIndex === 0; Layout.fillWidth: true; spacing: 6
                    RowLayout {
                        Layout.fillWidth: true
                        Caption { text: "Monsters"; font.bold: true; Layout.fillWidth: true }
                        DmeButton { objectName: "addTileMapMonster"; text: "+ Add monster"; enabled: dialog.monsters.length < 5 && dialog.monsterChoices(-1).length > 0; onClicked: dialog.addMonster() }
                    }
                    Repeater {
                        model: dialog.monsters
                        delegate: RowLayout {
                            id: monsterRow
                            required property int index
                            required property var modelData
                            Layout.fillWidth: true
                            Choice {
                                objectName: "tileMapMonster" + monsterRow.index
                                model: dialog.monsterChoices(monsterRow.index)
                                currentIndex: model.indexOf(monsterRow.modelData.name)
                                onActivated: dialog.editMonster(monsterRow.index, "name", currentText)
                            }
                            Caption { text: "Weight" }
                            DmeSpinBox {
                                objectName: "tileMapMonsterWeight" + monsterRow.index
                                Layout.preferredWidth: 100; Layout.preferredHeight: 30
                                from: 1; to: 10000; value: monsterRow.modelData.weight
                                onValueModified: dialog.editMonster(monsterRow.index, "weight", value)
                            }
                            DmeButton { objectName: "removeTileMapMonster" + monsterRow.index; text: "×"; onClicked: dialog.removeMonster(monsterRow.index) }
                        }
                    }
                    Caption { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: "Choose up to five monsters. Weights control their relative spawn frequency." }
                }
                GridLayout {
                    visible: workflow.currentIndex !== 2; Layout.fillWidth: true; columns: 4; columnSpacing: 12
                    Caption { text: "Down stair ID" }
                    NumberField { id: downId; objectName: "tileMapDownStair"; from: 0; to: 65535 }
                    Caption { text: "Up stair ID" }
                    NumberField { id: upId; objectName: "tileMapUpStair"; from: 0; to: 65535 }
                }
                DmeCheckBox { id: advanced; visible: workflow.currentIndex === 0; text: "Advanced generation settings"; onClicked: checked = !checked }
                GridLayout {
                    visible: workflow.currentIndex === 0 && advanced.checked; Layout.fillWidth: true
                    columns: 4; columnSpacing: 12; rowSpacing: 8
                    Caption { text: "Water" }
                    GroundChoice { id: waterBrush }
                    Caption { text: "Lava" }
                    GroundChoice { id: lavaBrush }
                    Caption { text: "Decoration" }
                    Choice { id: decoration; model: ["None"].concat(Array.from(dialog.materials.decorations)) }
                }
                GridLayout {
                    visible: workflow.currentIndex === 0 && advanced.checked; Layout.fillWidth: true
                    columns: 4; columnSpacing: 12; rowSpacing: 8
                    Caption { text: "Rooms" }
                    NumberField { id: rooms; from: 2; to: 512; value: 12 }
                    Caption { text: "Corridor width" }
                    NumberField { id: corridorWidth; from: 1; to: 15; value: 3 }
                    Caption { text: "Min. room size" }
                    NumberField { id: roomMin; from: 5; to: 64; value: 7 }
                    Caption { text: "Max. room size" }
                    NumberField { id: roomMax; from: 5; to: 128; value: 17 }
                    Caption { text: "Route loops (%)" }
                    NumberField { id: loops; from: 0; to: 100; value: 25 }
                    Caption { text: "Terrain scale" }
                    NumberField { id: terrainScale; from: 8; to: 512; value: 64 }
                    Caption { text: "Cave density (%)" }
                    NumberField { id: caveDensity; from: 20; to: 70; value: 48 }
                    Caption { text: "Smoothing steps" }
                    NumberField { id: smoothing; from: 0; to: 12; value: 4 }
                    Caption { text: "Chamber size" }
                    NumberField { id: chamberSize; from: 3; to: 128; value: 12 }
                    Caption { text: "Safe radius" }
                    NumberField { id: safeRadius; from: 2; to: 64; value: 6 }
                    Caption { text: "Water (%)" }
                    NumberField { id: waterAmount; from: 0; to: 45; value: 8 }
                    Caption { text: "Lava (%)" }
                    NumberField { id: lavaAmount; from: 0; to: 30; value: 0 }
                    Caption { text: "Decorations / 1,000" }
                    NumberField { id: decorationDensity; from: 0; to: 300; value: 35 }
                    Caption { text: "Spawns / 1,000" }
                    NumberField { id: spawnDensity; from: 0; to: 100; value: 4 }
                    Caption { text: "Spawn spacing" }
                    NumberField { id: spawnSpacing; from: 2; to: 128; value: 8 }
                    Caption { text: "Spawn radius" }
                    NumberField { id: spawnRadius; from: 1; to: 50; value: 3 }
                    Caption { text: "Respawn (seconds)" }
                    NumberField { id: respawn; from: 1; to: 86400; value: 60 }
                }
                Caption { Layout.fillWidth: true; wrapMode: Text.WordWrap; visible: workflow.currentIndex === 1; text: "Export the active client's materials and sprite previews for the external TileMapGen studio." }
                DmeButton { visible: workflow.currentIndex === 1; text: "Export .tmgpack…"; onClicked: packPicker.open() }
                Item { Layout.fillWidth: true; implicitHeight: 1 }
            }
        }
        GridLayout {
            visible: workflow.currentIndex !== 1; enabled: !dialog.busy
            Layout.fillWidth: true; columns: 6; columnSpacing: 8; rowSpacing: 6
            Caption { text: "Destination" }
            Choice { id: destination; objectName: "tileMapDestination"; Layout.columnSpan: 5; model: ["Current map (preview, then Apply)", "New map document"] }
            Caption { text: "Origin X" }
            NumberField { id: destinationX; objectName: "tileMapX"; from: 0; to: 65535; value: 1000; enabled: destination.currentIndex === 0 }
            Caption { text: "Y" }
            NumberField { id: destinationY; objectName: "tileMapY"; from: 0; to: 65535; value: 1000; enabled: destination.currentIndex === 0 }
            Caption { text: "Floor Z" }
            NumberField { id: destinationZ; objectName: "tileMapZ"; from: 0; to: 15; value: 7 }
        }
        Caption {
            visible: workflow.currentIndex === 0 && destination.currentIndex === 0
            Layout.fillWidth: true; wrapMode: Text.WordWrap
            text: dialog.fitsPlacement ? "Placement needs an empty area and can be undone in one step."
                                      : "This size exceeds 100,000 cells. Choose New map document or reduce the size."
        }
        ProgressBar { Layout.fillWidth: true; visible: dialog.busy; from: 0; to: 100; value: Backend.tileMapGen.progress }
        Caption { objectName: "tileMapStatus"; Layout.fillWidth: true; wrapMode: Text.Wrap; text: dialog.message }
        RowLayout {
            visible: Backend.tileMapGen.errorDetails.length > 0
            DmeCheckBox { id: showError; objectName: "tileMapShowError"; text: "Show error details"; onClicked: checked = !checked }
            DmeButton { objectName: "tileMapCopyError"; text: "Copy error"; onClicked: Backend.fileTools.setClipboard(Backend.tileMapGen.errorDetails) }
        }
        ScrollView {
            visible: showError.checked && Backend.tileMapGen.errorDetails.length > 0
            Layout.fillWidth: true; Layout.preferredHeight: 130; clip: true
            TextArea {
                objectName: "tileMapErrorDetails"; text: Backend.tileMapGen.errorDetails
                readOnly: true; selectByMouse: true; wrapMode: TextEdit.Wrap
                color: dialog.textColor; font.pixelSize: 12
            }
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            DmeButton {
                objectName: "generateTileMap"; visible: workflow.currentIndex === 0; text: "Generate"
                enabled: !dialog.busy && Backend.tileMapGen.available && floorBrush.currentName.length > 0
                    && floorBrush.currentUsable && backgroundBrush.currentUsable && secondaryBrush.currentUsable
                    && waterBrush.currentUsable && lavaBrush.currentUsable
                    && seed.text.trim().length > 0 && destinationZ.value + floors.value <= 16
                    && (destination.currentIndex !== 0 || dialog.fitsPlacement)
                    && (floors.value === 1 || (downId.value > 0 && upId.value > 0))
                onClicked: dialog.generate()
            }
            DmeButton { visible: workflow.currentIndex === 2; text: "Import .tmg…"; enabled: !dialog.busy; onClicked: mapPicker.open() }
            DmeButton { objectName: "applyTileMap"; visible: workflow.currentIndex !== 1; text: "Apply"; enabled: !dialog.busy && Backend.tileMapGen.previewReady; onClicked: { const error = Backend.tileMapGen.applyPreview(); if (error) dialog.message = error; } }
            DmeButton { objectName: "cancelTileMap"; text: "Cancel"; enabled: dialog.busy || Backend.tileMapGen.previewReady; onClicked: Backend.tileMapGen.cancel() }
            DmeButton { text: "Close"; onClicked: dialog.close() }
        }
    }
}
