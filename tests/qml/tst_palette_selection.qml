import QtQuick
import QtQuick.Controls
import QtTest
import Tibia 1.0
import "../../editor/qml" as Editor
import "../../editor/qml/components" as Components
import "../../editor/qml/controllers" as Controllers

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
        signal brushChanged()
        signal brushUsed(int serverId)
        function useGroundBrush(id) { brushServerId = id; }
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
        // Exercise the direct All Items path, whose reader exposes detailsAt.
        function detailsAt(row) {
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
        function namesFor(category) { return category === "terrain" ? Object.keys(saved) : []; }
        function itemsFor(category, name) { return saved[name] || []; }
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
            items.hideNamedItems = false;
            mockMap.brushServerId = 0;
            mockMap.creatureBrush = "";
            mockMap.creatureBrushIsNpc = false;
            palettes.customPalettes = ({});
            mockApp.favoriteBrushIds = [];
            tilesets.saved = ({Walls: []});
            items.clear();
            // IDs are deliberately nonconsecutive: ranges follow displayed rows.
            for (let i = 0; i < 80; ++i)
                items.append({serverId: 500 + i * 3, itemName: "Test item " + i});
        }
        function cleanup() {
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
            panel.selectKind("All Items");
            const itemGrid = findChild(panel, "paletteItemGrid");
            verify(itemGrid.directAllItems);
            compare(itemGrid.count, 80);
            const prefix = panel.githubUi ? "github" : "palette";
            const hideNamed = findChild(panel, prefix + "HideNamedItems");
            const hideInvisible = findChild(panel, prefix + "HideInvisibleSprites");
            verify(hideNamed.visible);
            tryVerify(() => hideNamed.x >= hideInvisible.x + hideInvisible.width);
            compare(hideNamed.y, hideInvisible.y, "Filters must appear side by side");
            mouseClick(hideNamed, 7, 7);
            compare(prefs.hideNamedItems, true);
            verify(!itemGrid.directAllItems, "All Items must use the filter when names are hidden");
            tryCompare(itemGrid, "count", 2);
            compare(itemGrid.filterModel.serverIdAtRow(0), 503);
            compare(itemGrid.filterModel.serverIdAtRow(1), 509);
            itemGrid.filterModel.searchText = "509";
            tryCompare(itemGrid, "count", 1);
            compare(itemGrid.filterModel.serverIdAtRow(0), 509);
            itemGrid.filterModel.searchText = "";
            mouseClick(hideNamed, 7, 7);
            compare(prefs.hideNamedItems, false);
            tryCompare(itemGrid, "count", 80);
            verify(itemGrid.directAllItems);

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
            panel.currentKind = "All Items";
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
            panel.selectKind("All Items");
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
        function test_filteredRangeUsesVisibleRows() {
            const panel = createTemporaryObject(panelComponent, testRoot);
            verify(panel);
            panel.selectKind("All Items");
            const itemGrid = findChild(panel, "paletteItemGrid");
            clickRow(itemGrid, 0);
            clickRow(itemGrid, 3, Qt.ShiftModifier);
            itemGrid.filterModel.searchText = "Test item 1";
            compare(itemGrid.selectedServerIds, []);
            verify(!itemGrid.directAllItems);
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
            panel.selectKind("All Items");
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
        function menuItem(menu, text) {
            for (let i = 0; i < menu.count; ++i) {
                const item = menu.itemAt(i);
                if (item && item.text === text) return item;
            }
            fail("Menu item not found: " + text);
            return null;
        }
        function test_bulkFavoritesAndRemoval() {
            const panel = createTemporaryObject(panelComponent, testRoot);
            verify(panel);
            panel.selectKind("All Items");
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
