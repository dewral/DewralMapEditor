import QtQuick
import QtQuick.Controls
import QtTest
import Tibia 1.0
import "../../editor/qml" as Editor
import "../../editor/qml/components" as Components
import "../../editor/qml/controllers" as Controllers
import "../../editor/qml/controllers/ContextBrushes.js" as ContextBrushes

Item {
    id: testRoot
    width: 900
    height: 800

    QtObject {
        id: prefs
        property string paletteViewMode: "grid"
        property int iconSize: 50
        property bool hideInvisibleSprites: false
        property bool hideNamedItems: false
        property string customPalettesJson: "{}"
        property string lastPaletteKind: ""
        property string lastRawTileset: ""
    }
    Controllers.PaletteController { id: palettes; settings: prefs }
    QtObject {
        id: mockApp
        property var settings: prefs
        property int iconSizePx: prefs.iconSize
        property var customPalettes: palettes.customPalettes
        property var customPaletteNames: palettes.customPaletteNames
        property var favoriteBrushIds: []
        property var recentBrushIds: []
        function addCustomPalette(name) { return palettes.addCustomPalette(name); }
        function addItemsToPalette(name, ids) { palettes.addItemsToPalette(name, ids); }
        function removeItemFromPalette(name, id) { palettes.removeItemFromPalette(name, id); }
        function isFavoriteBrush(id) { return favoriteBrushIds.indexOf(id) >= 0; }
        function toggleFavoriteBrush(id) {
            const copy = favoriteBrushIds.slice();
            const index = copy.indexOf(id);
            if (index < 0) copy.push(id);
            else copy.splice(index, 1);
            favoriteBrushIds = copy;
        }
    }
    QtObject {
        id: mockMap
        property int brushServerId: 0
        property bool eraseMode: false
        property bool lassoMode: false
        property bool selectionMode: false
        property string doodadBrush: ""
        property string creatureBrush: ""
        property bool creatureBrushIsNpc: false
        property bool spawnBrush: false
        property int creatureSpawntime: 60
        property int spawnBrushRadius: 1
        property int houseBrush: 0
        property bool houseExitMode: false
        property string brushShape: "square"
        property int brushSize: 0
        property string selectedBrushKind: ""
        signal brushChanged()
        signal brushUsed(int serverId)
        function useGroundBrush(id) { brushServerId = id; }
        function typedBrushName(kind, id) { return id === 506 ? "stone" : ""; }
        function useTypedBrush(kind, id) {
            selectedBrushKind = kind;
            brushServerId = id;
            selectionMode = false;
        }
        function doodadPreviewSource(id) { return ""; }
        function doodadPreviewSourceForName(name) { return ""; }
        function selectCreatureBrush(name, isNpc) {
            creatureBrush = name;
            creatureBrushIsNpc = isNpc;
        }
    }
    ListModel {
        id: items
        property bool loaded: true
        property string searchText: "filtered"
        property bool hideInvisibleSprites: false
        property bool hideNamedItems: false
        property var detailsProbe: ({reads: 0})
        // Exercise model metadata reads through detailsAt.
        function detailsAt(row) {
            detailsProbe.reads++;
            const item = get(row);
            return {serverId: item.serverId, name: item.itemName};
        }
        function rowForServerId(id) {
            for (let i = 0; i < count; ++i)
                if (get(i).serverId === id) return i;
            return -1;
        }
        function itemHasVisibleSprite(id) { return true; }
        function doodadHasVisibleSprite(name) { return true; }
    }
    QtObject {
        id: tilesets
        property int revision: 0
        property var saved: ({Walls: []})
        property var rawNames: ["Others"]
        property var collectionNames: []
        property var rawOnlyNames: []
        function namesFor(category) {
            return category === "raw" ? rawNames : category === "terrain"
                ? Object.keys(saved).filter(name => rawOnlyNames.indexOf(name) < 0)
                : category === "collection" ? collectionNames : [];
        }
        function itemsFor(category, name) {
            if (category === "raw" && name === "Others") {
                const namedIds = rawNames.filter(n => n !== "Others")
                        .reduce((ids, n) => ids.concat(saved[n] || []), []);
                const ids=[]; for (let row=0; row<items.count; ++row) {
                    const id = items.get(row).serverId;
                    if (namedIds.indexOf(id) < 0) ids.push(id);
                }
                return ids;
            }
            return saved[name] || [];
        }
        function tilesetForItem(category, id) {
            return namesFor(category).find(name => itemsFor(category, name).indexOf(id) >= 0) || "";
        }
        function isCustomOnly(category, name) { return true; }
        function newTileset(category, name) {
            const copy = Object.assign({}, saved);
            copy[name] = [];
            saved = copy;
            ++revision;
            return true;
        }
        function addItems(category, name, ids) {
            const copy = Object.assign({}, saved);
            copy[name] = (copy[name] || []).slice();
            for (const id of ids)
                if (copy[name].indexOf(id) < 0) copy[name].push(id);
            saved = copy;
            ++revision;
            return true;
        }
        function removeItem(category, name, id) {
            const copy = Object.assign({}, saved);
            copy[name] = (copy[name] || []).filter(value => value !== id);
            saved = copy;
            ++revision;
            return true;
        }
    }
    Component {
        id: gridComponent
        Components.PaletteItemGrid {
            width: 300; height: 150
            app: mockApp; mapCtrl: mockMap; filterModel: items
            currentKind: "RAW Palette"; githubUi: false
        }
    }
    Component {
        id: panelComponent
        Editor.PalettePanel { width: 340; height: 780; app: mockApp; mapCtrl: mockMap }
    }
    Component {
        id: settingsComponent
        Controllers.AppSettings {}
    }
    Component {
        id: doodadComponent
        Components.DoodadPaletteGrid {
            width: 300; height: 150
            app: mockApp; mapCtrl: mockMap; filterModel: items
            itemIds: [500, 503, 506, 509, 512, 515]
            categoryName: ""; searchText: ""; githubUi: false
        }
    }

    TestCase {
        name: "PaletteMultiSelection"
        when: windowShown
        property var oldReader
        property var oldTilesets

        function init() {
            oldReader = Backend.otbReader;
            oldTilesets = Backend.tilesetStore;
            Backend.otbReader = items;
            Backend.tilesetStore = tilesets;
            Backend.uiTheme.style = "gray-dark";
            prefs.paletteViewMode = "grid";
            prefs.hideInvisibleSprites = false;
            prefs.hideNamedItems = false;
            prefs.lastPaletteKind = "";
            prefs.lastRawTileset = "";
            mockApp.settings = prefs;
            items.hideNamedItems = false;
            mockMap.brushServerId = 0;
            mockMap.creatureBrush = "";
            mockMap.creatureBrushIsNpc = false;
            palettes.customPalettes = ({});
            mockApp.favoriteBrushIds = [];
            tilesets.saved = ({Walls: []});
            tilesets.rawNames = ["Others"];
            tilesets.collectionNames = [];
            tilesets.rawOnlyNames = [];
            ++tilesets.revision;
            items.clear();
            // IDs are deliberately nonconsecutive: ranges follow displayed rows.
            for (let i = 0; i < 80; ++i)
                items.append({serverId: 500 + i * 3, itemName: "Test item " + i});
        }
        function cleanup() {
            mockApp.settings = prefs;
            Backend.otbReader = oldReader;
            Backend.tilesetStore = oldTilesets;
        }
        function clickRow(panel, row, modifiers, button) {
            const grid = findChild(panel, "paletteGrid");
            grid.positionViewAtIndex(row, GridView.Center);
            tryVerify(() => grid.itemAtIndex(row) !== null);
            mouseClick(grid.itemAtIndex(row), 8, 8, button || Qt.LeftButton,
                       modifiers || Qt.NoModifier);
        }
        function test_tibiaWindowFramePersistsWhenDocked() {
            Backend.uiTheme.style="tibia-ui";
            const panel=createTemporaryObject(panelComponent,testRoot);
            verify(panel);
            const background=findChild(panel,"floatingPaletteBackground");
            verify(background!==null); compare(background.active,true);
            panel.floating=true;
            tryCompare(background,"active",true);
            tryVerify(()=>background.item!==null);
            const image=findChild(background,"tibiaDialogFrame");
            tryCompare(image,"status",Image.Ready);
            compare(background.width,panel.width); verify(background.height<panel.height);
            const toolsFrame=findChild(panel,"toolsSectionFrame");
            const brushFrame=findChild(panel,"brushSectionFrame");
            verify(toolsFrame.active && brushFrame.active);
            verify(toolsFrame.mapToItem(panel,0,0).y>=background.height);
            verify(brushFrame.mapToItem(panel,0,0).y>=toolsFrame.mapToItem(panel,0,toolsFrame.height).y);
            compare(image.border.left,4); compare(image.border.top,17);
            verify(String(image.source).indexOf("dialog-frame-borderimage.png")>=0);
            compare(image.caption,panel.currentKind); compare(image.contentTopMargin,25);
            image.captionActivated();
            const kindPicker=findChild(panel,"paletteKindCombo");
            tryCompare(kindPicker,"open",true);
            keyClick(Qt.Key_Escape); tryCompare(kindPicker,"open",false);

            verify(findChild(image,"tibiaWindowMouseShield")!==null);
            wait(250);
            grabImage(panel).save("build/tibia-palette-window.png");
            for(const width of [260,340,480]) {
                panel.width=width; panel.height=600; wait(250);
                compare(background.width,panel.width);
                const toolsTop=toolsFrame.mapToItem(panel,0,0);
                const brushTop=brushFrame.mapToItem(panel,0,0);
                verify(toolsTop.y>=background.height);
                verify(brushTop.y>=toolsTop.y+toolsFrame.height);
                verify(brushTop.y+brushFrame.height<=panel.height);
                const doors=findChild(panel,"fluentToolDoors");
                verify(doors.mapToItem(panel,doors.width,0).x<=panel.width);
            }
            panel.floating=false;
            tryCompare(background,"active",true); compare(background.item,image);
            Backend.uiTheme.style="fluent-dark";
            tryCompare(background,"active",false); compare(background.item,null);
        }
        function test_rawTilesetSurvivesRestart_data() {
            return ["fluent-dark", "gray-dark", "gray-modern", "github", "classic", "windows-classic"]
                    .map(theme => ({tag: theme, theme: theme}));
        }
        function test_rawTilesetSurvivesRestart(data) {
            Backend.uiTheme.style = data.theme;
            const location = Qt.resolvedUrl("../../build/tmp/raw-palette-"
                                           + Date.now() + "-" + Math.random() + ".ini");
            let settings = createTemporaryObject(settingsComponent, testRoot, {location: location});
            verify(settings);
            mockApp.settings = settings;
            tilesets.rawNames = ["Architecture", "Stones / Rocks", "Others"];
            ++tilesets.revision;
            let panel = createTemporaryObject(panelComponent, testRoot);
            verify(panel);
            const kind = findChild(panel, "paletteKindCombo");
            if (data.theme !== "fluent-dark")
                kind.handleActivation(kind.model.indexOf("RAW Palette"));
            if (panel.githubUi)
                findChild(panel, "githubPaletteTilesetCombo").activated(1);
            else
                findChild(panel, "paletteTilesetCombo").handleActivation(1);
            compare(settings.lastPaletteKind, "RAW Palette");
            compare(settings.lastRawTileset, "Stones / Rocks");
            panel.destroy();
            mockApp.settings = prefs;
            settings.destroy();
            wait(0); // Destroy Settings and flush writes before restarting.

            // The panel starts before client definitions load on a real restart.
            tilesets.rawNames = [];
            ++tilesets.revision;
            settings = createTemporaryObject(settingsComponent, testRoot, {location: location});
            verify(settings);
            mockApp.settings = settings;
            panel = createTemporaryObject(panelComponent, testRoot);
            verify(panel);
            tryCompare(panel, "currentKind", "RAW Palette");
            compare(settings.lastRawTileset, "Stones / Rocks");
            tilesets.rawNames = ["Others", "Architecture", "Stones / Rocks"];
            ++tilesets.revision;
            const tileset = findChild(panel, "paletteTilesetCombo");
            tryCompare(tileset, "currentText", "Stones / Rocks");
            compare(tileset.currentIndex, 2, "Restore by name after tilesets change order");

            panel.selectKind("Terrain Palette");
            panel.selectKind("RAW Palette");
            tryCompare(tileset, "currentText", "Stones / Rocks");
            compare(settings.lastRawTileset, "Stones / Rocks");

            // A different profile or a deleted tileset must leave a valid fallback.
            tilesets.rawNames = ["Architecture", "Others"];
            ++tilesets.revision;
            tryCompare(tileset, "currentText", "Architecture");
            compare(settings.lastRawTileset, "Stones / Rocks");
            tilesets.rawNames = ["Stones / Rocks", "Others"];
            ++tilesets.revision;
            tryCompare(tileset, "currentText", "Stones / Rocks");

            panel.selectRaw(503);
            tryCompare(tileset, "currentText", "Others");
            compare(settings.lastRawTileset, "Others", "Programmatic RAW navigation must be saved too");
            panel.destroy();
            mockApp.settings = prefs;
            settings.destroy();
            wait(0);
        }
        function test_rawOthers_does_not_build_hidden_doodads() {
            Backend.uiTheme.style = "fluent-dark";
            const panel = createTemporaryObject(panelComponent, testRoot);
            verify(panel);
            wait(50);
            items.detailsProbe.reads = 0;
            panel.selectKind("RAW Palette");
            tryCompare(findChild(panel, "paletteItemGrid"), "count", 80);
            wait(50);
            compare(items.detailsProbe.reads, 0,
                    "RAW Others must not build full item details for the hidden Doodad grid");
            compare(findChild(panel, "doodadPaletteGrid").count, 0);
        }
        function test_selectRawOpensOthers_data() {
            return ["classic", "windows-classic", "github", "gray-dark", "gray-modern", "fluent-dark"]
                    .map(theme => ({tag: theme, theme: theme}));
        }
        function test_selectRawOpensOthers(data) {
            Backend.uiTheme.style = data.theme;
            tilesets.rawNames = ["Architecture", "Others"];
            tilesets.saved = ({Walls: [], Architecture: [500]});
            ++tilesets.revision;
            const panel = createTemporaryObject(panelComponent, testRoot);
            verify(panel);
            panel.revealRequested.connect(function () { panel.visible = true; });
            const tileset = findChild(panel, "paletteTilesetCombo");
            const grid = findChild(panel, "paletteItemGrid");
            // Exercise both uncategorized items and items listed in a named RAW tileset.
            for (const serverId of [725, 500]) {
                prefs.lastRawTileset = "Architecture";
                panel.selectKind("Terrain Palette");
                panel.visible = false;
                grid.filterModel.searchText = "missing item";
                mockMap.houseBrush = 42;
                mockMap.eraseMode = true;
                mockMap.lassoMode = true;
                panel.selectRaw(serverId);
                verify(panel.visible, "Select RAW must reveal a hidden palette");
                compare(panel.currentKind, "RAW Palette");
                tryCompare(tileset, "currentText", "Others");
                compare(prefs.lastRawTileset, "Others");
                compare(grid.filterModel.searchText, "");
                compare(mockMap.brushServerId, serverId);
                compare(mockMap.houseBrush, 0);
                verify(!mockMap.eraseMode && !mockMap.lassoMode);
                if (serverId === 725)
                    tryCompare(grid, "currentIndex", grid.filterModel.rowForServerId(serverId));
            }
        }
        function test_drawReplacesPreviousPaletteSelection_data() {
            const cases = [];
            for (const theme of ["classic", "windows-classic", "github", "gray-dark", "gray-modern", "fluent-dark"])
                for (const view of ["grid", "list"])
                    cases.push({tag: theme + "-" + view, theme, view});
            return cases;
        }
        function test_drawReplacesPreviousPaletteSelection(data) {
            Backend.uiTheme.style = data.theme;
            prefs.paletteViewMode = data.view;
            items.clear();
            for (let id = 13700; id <= 13740; ++id)
                items.append({serverId: id, itemName: ""});
            const panel = createTemporaryObject(panelComponent, testRoot);
            verify(panel);
            panel.selectKind("RAW Palette");
            const itemGrid = findChild(panel, "paletteItemGrid");
            const view = findChild(itemGrid, "paletteGrid");
            tryCompare(itemGrid, "count", 41);
            const targetRow = itemGrid.filterModel.rowForServerId(13717);
            const previousRow = itemGrid.filterModel.rowForServerId(13722);

            // Stay in RAW Others: no category change or model reset clears the
            // old selection for us. Exercise both single and multiple selections.
            for (const multiple of [false, true]) {
                clickRow(itemGrid, previousRow);
                if (multiple)
                    clickRow(itemGrid, previousRow + 1, Qt.ControlModifier);
                compare(itemGrid.selectedServerIds, multiple ? [13722, 13723] : [13722]);

                // This is the same entry point used by the map's RAW/Draw action.
                panel.selectRaw(13717);
                compare(mockMap.brushServerId, 13717);
                tryCompare(itemGrid, "currentIndex", targetRow);
                tryVerify(() => view.itemAtIndex(targetRow) !== null);
                verify(view.itemAtIndex(targetRow).selected,
                       "Draw must highlight server ID 13717, not the previous palette selection");
                compare(itemGrid.selectedServerIds, []);
                if (view.itemAtIndex(previousRow))
                    verify(!view.itemAtIndex(previousRow).selected);
            }
        }
        function test_hideNamedItems_data() {
            return ["classic", "windows-classic", "github", "gray-dark",
                    "gray-modern", "fluent-dark"].map(theme => ({tag: theme, theme}));
        }
        function test_hideNamedItems(data) {
            Backend.uiTheme.style = data.theme;
            items.setProperty(1, "itemName", "");
            items.setProperty(3, "itemName", "  ");
            const panel = createTemporaryObject(panelComponent, testRoot);
            verify(panel);
            panel.width = 440;
            panel.selectKind("RAW Palette");
            const itemGrid = findChild(panel, "paletteItemGrid");

            tryCompare(itemGrid, "count", 80);
            const prefix = panel.fluentUi ? "fluent" : panel.githubUi ? "github" : "palette";
            const hideNamed = findChild(panel, prefix + "HideNamedItems");
            const hideInvisible = findChild(panel, prefix + "HideInvisibleSprites");
            if (data.theme === "fluent-dark") {
                verify(!findChild(panel, "paletteHideNamedItems").visible);
                verify(!findChild(panel, "paletteHideInvisibleSprites").visible);
                wait(50);
                mouseClick(findChild(panel, "fluentFilterButton"));
                tryCompare(findChild(panel, "fluentFilterMenu"), "visible", true);
            } else {
                tryVerify(() => hideNamed.mapToItem(panel, 0, 0).x >= hideInvisible.mapToItem(panel, hideInvisible.width, 0).x);
                compare(hideNamed.mapToItem(panel, 0, 0).y, hideInvisible.mapToItem(panel, 0, 0).y, "Filters must appear side by side");
            }
            mouseClick(hideNamed, 7, 7);
            compare(prefs.hideNamedItems, true);

            tryCompare(itemGrid, "count", 2);
            compare(itemGrid.filterModel.serverIdAtRow(0), 503);
            compare(itemGrid.filterModel.serverIdAtRow(1), 509);
            itemGrid.filterModel.searchText = "509";
            tryCompare(itemGrid, "count", 1);
            compare(itemGrid.filterModel.serverIdAtRow(0), 509);
            itemGrid.filterModel.searchText = "";
            if (panel.fluentUi) {
                mouseClick(findChild(panel, "fluentFilterButton"));
                tryCompare(findChild(panel, "fluentFilterMenu"), "visible", true);
            }
            mouseClick(hideNamed, 7, 7);
            compare(prefs.hideNamedItems, false);
            tryCompare(itemGrid, "count", 80);

            panel.width = 225;
            tryVerify(() => hideNamed.y > hideInvisible.y);
            verify(hideNamed.x + hideNamed.width <= hideNamed.parent.width,
                   "Narrow palettes must keep the checkbox within the panel");
            panel.selectKind("Creature Palette");
            verify(!hideNamed.visible);
            panel.selectKind("House Palette");
            verify(!hideNamed.visible);
        }
        function test_hideNamedDoodads() {
            items.setProperty(1, "itemName", "");
            items.setProperty(3, "itemName", "  ");
            const panel = createTemporaryObject(doodadComponent, testRoot);
            verify(panel);
            panel.categoryName = "Structures";
            compare(panel.count, 7);
            items.hideNamedItems = true;
            compare(panel.count, 2, "Hide named doodad items and named prefabs");
            compare(panel.entries.map(entry => entry.serverId), [503, 509]);
            panel.searchText = "509";
            compare(panel.count, 1);
            panel.searchText = "";
            items.hideNamedItems = false;
            compare(panel.count, 7);
        }
        function test_shiftCtrlAndRightClick() {
            const panel = createTemporaryObject(gridComponent, testRoot);
            verify(panel);
            clickRow(panel, 2);
            compare(mockMap.brushServerId, 506);
            clickRow(panel, 5, Qt.ShiftModifier);
            compare(panel.selectedServerIds, [506, 509, 512, 515]);
            compare(mockMap.brushServerId, 506, "Range clicks must preserve the drawing brush");
            const grid = findChild(panel, "paletteGrid");
            for (let row = 2; row <= 5; ++row)
                verify(grid.itemAtIndex(row).selected, "Every selected item must be highlighted");
            verify(!grid.itemAtIndex(6).selected);
            clickRow(panel, 1, Qt.ShiftModifier);
            compare(panel.selectedServerIds, [503, 506], "The initial anchor survives range changes");
            clickRow(panel, 5, Qt.ControlModifier);
            compare(panel.selectedServerIds, [503, 506, 515]);
            clickRow(panel, 2, Qt.ControlModifier);
            compare(panel.selectedServerIds, [503, 515]);
            clickRow(panel, 5, Qt.NoModifier, Qt.RightButton);
            compare(panel.selectedServerIds, [503, 515]);
            clickRow(panel, 6, Qt.NoModifier, Qt.RightButton);
            compare(panel.selectedServerIds, [518]);
        }
        function test_offscreenRangeAndListMode() {
            const panel = createTemporaryObject(gridComponent, testRoot);
            clickRow(panel, 1);
            clickRow(panel, 70, Qt.ShiftModifier);
            compare(panel.selectedServerIds.length, 70);
            compare(panel.selectedServerIds[0], 503);
            compare(panel.selectedServerIds[69], 710);
            prefs.paletteViewMode = "list";
            clickRow(panel, 5);
            clickRow(panel, 2, Qt.ShiftModifier);
            compare(panel.selectedServerIds, [506, 509, 512, 515]);
        }
        function test_additiveRangeAndModelChanges() {
            const panel = createTemporaryObject(gridComponent, testRoot);
            clickRow(panel, 1);
            clickRow(panel, 4, Qt.ControlModifier);
            clickRow(panel, 6, Qt.ControlModifier | Qt.ShiftModifier);
            compare(panel.selectedServerIds, [503, 512, 515, 518]);
            items.remove(0, 3);
            compare(panel.selectedServerIds, []);
            clickRow(panel, 2, Qt.ShiftModifier);
            compare(panel.selectedServerIds, [515], "Removed rows must invalidate the anchor");
            panel.currentKind = "Item Palette";
            compare(panel.selectedServerIds, []);
        }
        function test_doodadItemRange() {
            const panel = createTemporaryObject(doodadComponent, testRoot);
            clickRow(panel, 1);
            clickRow(panel, 4, Qt.ShiftModifier);
            compare(panel.selectedServerIds, [503, 506, 509, 512]);
            clickRow(panel, 2, Qt.NoModifier, Qt.RightButton);
            compare(panel.selectedServerIds, [503, 506, 509, 512]);
            panel.searchText = "Test item 4";
            compare(panel.selectedServerIds, []);
        }
        function test_contextMenuAddsRangeToWalls() {
            const panel = createTemporaryObject(panelComponent, testRoot);
            verify(panel);
            panel.selectKind("RAW Palette");
            const itemGrid = findChild(panel, "paletteItemGrid");
            clickRow(itemGrid, 2);
            clickRow(itemGrid, 5, Qt.ShiftModifier);
            clickRow(itemGrid, 3, Qt.NoModifier, Qt.RightButton);
            const menu = findChild(panel, "paletteItemMenu");
            tryCompare(menu, "opened", true);
            compare(menu.serverIds, [506, 509, 512, 515]);
            const terrain = findChild(panel, "paletteAdd_terrain");
            terrain.itemAt(0).triggered();
            compare(tilesets.saved.Walls, [506, 509, 512, 515]);
            menu.close();
        }
        function test_selectCreatureAfterPaletteDropdown_data() {
            const rows = [];
            for (const theme of ["classic", "windows-classic", "github", "gray-dark",
                                 "gray-modern", "fluent-dark"]) {
                for (const isNpc of [false, true])
                    rows.push({tag: theme + (isNpc ? "-npc" : "-monster"), theme, isNpc});
            }
            return rows;
        }
        function test_selectCreatureAfterPaletteDropdown(data) {
            Backend.uiTheme.style = data.theme;
            const panel = createTemporaryObject(panelComponent, testRoot);
            verify(panel);
            const combo = findChild(panel, "paletteKindCombo");
            const control = combo.children[0].item;
            // The themed dropdown updates its own index before emitting activated.
            // Exercise that user selection before the context-menu navigation.
            const terrainIndex = combo.model.indexOf("Terrain Palette");
            control.currentIndex = terrainIndex;
            control.activated(terrainIndex);
            compare(panel.currentKind, "Terrain Palette");

            const name = data.isNpc ? "Captain Bluebear" : "Rat";
            panel.selectCreature(name, data.isNpc);
            compare(panel.currentKind, "Creature Palette");
            compare(combo.currentText, "Creature Palette");
            compare(control.currentText, "Creature Palette");
            const creatures = findChild(panel, "creaturePaletteView");
            verify(creatures.visible);
            compare(creatures.filterMode, data.isNpc ? "npc" : "monster");
            compare(mockMap.creatureBrush, name);
            compare(mockMap.creatureBrushIsNpc, data.isNpc);

            panel.selectKind("Terrain Palette");
            compare(panel.currentKind, "Terrain Palette");
            verify(!creatures.visible);
            panel.selectCreature(name, data.isNpc);
            compare(panel.currentKind, "Creature Palette");
            compare(mockMap.creatureBrush, name);
        }
        function test_contextBrushActivatesOriginalVariant() {
            tilesets.saved = ({Walls: [500]});
            const panel = createTemporaryObject(panelComponent, testRoot);
            verify(panel);
            panel.selectContextBrush({kind: "wall", name: "stone", serverId: 506,
                                      displayedServerId: 500, category: "terrain", tileset: "Walls"});
            compare(panel.currentKind, "Terrain Palette");
            compare(mockMap.selectedBrushKind, "wall");
            compare(mockMap.brushServerId, 506);
            panel.selectContextBrush({kind: "wall", name: "removed", serverId: 509,
                                      displayedServerId: 500, category: "terrain", tileset: "Walls"});
            compare(mockMap.brushServerId, 506);
        }
        function test_contextBrushActivatesUnlistedBrush_data() {
            const rows = [];
            for (const kind of ["ground", "wall"]) {
                rows.push({tag: kind + "-named-raw-tileset", kind: kind, named: true});
                rows.push({tag: kind + "-raw-others", kind: kind, named: false});
            }
            return rows;
        }
        function test_contextBrushActivatesUnlistedBrush(data) {
            tilesets.saved = ({Walls: [], Architecture: data.named ? [506] : [500]});
            tilesets.rawNames = ["Architecture", "Others"];
            tilesets.rawOnlyNames = ["Architecture"];
            ++tilesets.revision;
            const panel = createTemporaryObject(panelComponent, testRoot);
            verify(panel);
            const brushes = ({});
            brushes[data.kind] = {kind: data.kind, name: "stone", serverId: 506};
            const option = ContextBrushes.options({singleTile: true, brushes: brushes},
                mockMap, tilesets, Backend.brushStore)[data.kind];
            verify(option);
            mockMap.selectionMode = true;
            panel.selectContextBrush(option);
            tryCompare(panel, "currentKind", "RAW Palette");
            const grid = findChild(panel, "paletteItemGrid");
            tryVerify(() => grid.filterModel.rowForServerId(506) >= 0
                      && grid.currentIndex === grid.filterModel.rowForServerId(506));
            compare(mockMap.selectedBrushKind, data.kind);
            compare(mockMap.brushServerId, 506);
            verify(!mockMap.selectionMode);
        }
        function test_reselectSpawnLeavesSelectionMode() {
            const panel = createTemporaryObject(panelComponent, testRoot);
            verify(panel);
            panel.selectSpawn();
            mockMap.selectionMode = true;
            mockMap.eraseMode = true;
            mockMap.lassoMode = true;
            panel.selectSpawn();
            compare(panel.currentKind, "Creature Palette");
            verify(mockMap.spawnBrush);
            verify(!mockMap.selectionMode);
            verify(!mockMap.eraseMode);
            verify(!mockMap.lassoMode);
        }
        function test_filteredRangeUsesVisibleRows() {
            const panel = createTemporaryObject(panelComponent, testRoot);
            verify(panel);
            panel.selectKind("RAW Palette");
            const itemGrid = findChild(panel, "paletteItemGrid");
            clickRow(itemGrid, 0);
            clickRow(itemGrid, 3, Qt.ShiftModifier);
            itemGrid.filterModel.searchText = "Test item 1";
            compare(itemGrid.selectedServerIds, []);

            clickRow(itemGrid, 0);
            clickRow(itemGrid, 3, Qt.ShiftModifier);
            compare(itemGrid.selectedServerIds, [503, 530, 533, 536]);
            clickRow(itemGrid, 2, Qt.NoModifier, Qt.RightButton);
            const terrain = findChild(panel, "paletteAdd_terrain");
            terrain.itemAt(0).triggered();
            compare(tilesets.saved.Walls, [503, 530, 533, 536]);
            findChild(panel, "paletteItemMenu").close();
        }
        function test_newTilesetKeepsSelectionSnapshot() {
            const panel = createTemporaryObject(panelComponent, testRoot);
            verify(panel);
            panel.selectKind("RAW Palette");
            const itemGrid = findChild(panel, "paletteItemGrid");
            clickRow(itemGrid, 1);
            clickRow(itemGrid, 3, Qt.ShiftModifier);
            clickRow(itemGrid, 2, Qt.NoModifier, Qt.RightButton);
            const menu = findChild(panel, "paletteItemMenu");
            const terrain = findChild(panel, "paletteAdd_terrain");
            terrain.itemAt(terrain.count - 1).triggered();
            const dialog = findChild(panel, "newPaletteDialog");
            tryCompare(dialog, "opened", true);
            itemGrid.clearSelection();
            findChild(panel, "newPaletteName").text = "New walls";
            dialog.commit();
            compare(tilesets.saved["New walls"], [503, 506, 509]);
            menu.close();
        }
        function test_customPaletteBulkSave() {
            palettes.addCustomPalette("Test");
            palettes.addItemsToPalette("Test", [500, 503, 500, 0, -1]);
            compare(palettes.customPalettes.Test, [500, 503]);
            compare(JSON.parse(prefs.customPalettesJson).Test, [500, 503]);
        }
        function test_obdExportUsesContextItem() {
            const panel = createTemporaryObject(panelComponent, testRoot);
            verify(panel);
            panel.selectKind("RAW Palette");
            const grid = findChild(panel, "paletteItemGrid");
            clickRow(grid, 2, Qt.NoModifier, Qt.RightButton);
            const menu = findChild(panel, "paletteItemMenu");
            const action = findChild(panel, "exportItemObdAction");
            verify(action.enabled);
            compare(action.text, "Export item as OBD...");
            const dialog = findChild(panel, "obdExportDialog");
            compare(dialog.defaultSuffix, "obd");
            compare(dialog.nameFilters, ["ObjectBuilder data (*.obd)"]);
            // Exercise acceptance without opening an interactive native file picker.
            dialog.serverId = menu.sid;
            dialog.selectedFile = Backend.obdExportUrl(menu.sid);
            Backend.obdExportCalls = [];
            dialog.accepted();
            compare(Backend.obdExportCalls.length, 1);
            compare(Backend.obdExportCalls[0].serverId, 506);
            compare(Backend.obdExportCalls[0].destination, String(dialog.selectedFile));
            menu.close();
        }
        function menuItem(menu, text) {
            for (let i = 0; i < menu.count; ++i) {
                const item = menu.itemAt(i);
                if (item && item.text === text) return item;
            }
            fail("Menu item not found: " + text);
            return null;
        }
        function test_collectionNavigationAndRemoval() {
            tilesets.saved = ({Cave: [500, 503, 506]});
            tilesets.collectionNames = ["Cave"];
            ++tilesets.revision;
            const panel = createTemporaryObject(panelComponent, testRoot);
            verify(panel);
            panel.showItemLocation("collection", "Cave", 500);
            compare(panel.currentKind, "Collection Palette");
            const itemGrid = findChild(panel, "paletteItemGrid");
            tryCompare(itemGrid, "count", 3);
            clickRow(itemGrid, 1, Qt.NoModifier, Qt.RightButton);
            const menu = findChild(panel, "paletteItemMenu");
            menuItem(menu, 'Remove from "Cave"').triggered();
            compare(tilesets.saved.Cave, [500, 506]);
            tryCompare(itemGrid, "count", 2);
            compare(items.count, 80, "Collection removal must retain the source catalogue");
            menu.close();
        }
        function test_bulkFavoritesAndRemoval() {
            const panel = createTemporaryObject(panelComponent, testRoot);
            verify(panel);
            panel.selectKind("RAW Palette");
            const itemGrid = findChild(panel, "paletteItemGrid");
            clickRow(itemGrid, 0);
            clickRow(itemGrid, 2, Qt.ShiftModifier);
            clickRow(itemGrid, 1, Qt.NoModifier, Qt.RightButton);
            const menu = findChild(panel, "paletteItemMenu");
            menuItem(menu, "Add to Favorites").triggered();
            compare(mockApp.favoriteBrushIds, [500, 503, 506]);
            menuItem(menu, "Remove from Favorites").triggered();
            compare(mockApp.favoriteBrushIds, []);
            menu.close();

            mockApp.addCustomPalette("Test");
            mockApp.addItemsToPalette("Test", [500, 503, 506, 509]);
            panel.selectKind("My Palettes");
            tryCompare(itemGrid, "count", 4);
            clickRow(itemGrid, 0);
            clickRow(itemGrid, 2, Qt.ShiftModifier);
            clickRow(itemGrid, 1, Qt.NoModifier, Qt.RightButton);
            menuItem(menu, 'Remove from "Test"').triggered();
            compare(palettes.customPalettes.Test, [509],
                    "All IDs must be removed even while the source model changes");
            menu.close();
        }
    }
}
