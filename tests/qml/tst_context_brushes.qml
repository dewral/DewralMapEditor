import QtQuick
import QtTest
import "../../editor/qml/controllers/ContextBrushes.js" as ContextBrushes

TestCase {
    name: "ContextBrushes"
    property var definitions: ({
        ground: {100: "grass", 101: "grass"},
        wall: {200: "stone", 201: "stone", 202: "stone", 203: "unlisted wall"},
        carpet: {300: "rug", 301: "rug"},
        table: {400: "table", 401: "table"},
        doodad: {200: "rock", 500: "rock", 501: "rock"},
        door: {201: "201", 202: "202"}
    })
    property var palettes: ({})
    function typedBrushName(kind, id) { return (definitions[kind] || {})[id] || ""; }
    function namesFor(category) { return Object.keys(palettes[category] || {}); }
    function itemsFor(category, name) { return palettes[category][name]; }
    function tilesetForItem(category, id) {
        return namesFor(category).find(name => itemsFor(category, name).indexOf(id) >= 0) || "";
    }
    function canSwitchDoor(id) { return id === 201 || id === 202; }
    function switchedDoorItem(id) { return id === 201 ? 202 : 201; }
    function candidate(kind, id) { return {kind, serverId: id, name: typedBrushName(kind, id)}; }
    function options(brushes, collections, singleTile) {
        return ContextBrushes.options({singleTile: singleTile !== false, brushes,
                                      collectionBrushes: collections || []}, this, this, this);
    }
    function init() {
        palettes = ({terrain: {Grounds: [100], Walls: [200]},
                     doodad: {Rocks: [500]}, door: {Doors: [201]},
                     collection: {Furniture: [300, 400], Nature: [100, 500]}});
    }
    function test_resolvesVariantsToPaletteEntries() {
        const result = options({ground: candidate("ground", 101),
                                wall: candidate("wall", 202),
                                carpet: candidate("carpet", 301),
                                table: candidate("table", 401),
                                doodad: candidate("doodad", 501),
                                door: candidate("door", 202)});
        compare(result.ground.serverId, 101);
        compare(result.ground.displayedServerId, 100);
        compare(result.ground.tileset, "Grounds");
        compare(result.wall.kind, "wall");
        compare(result.wall.displayedServerId, 200);
        compare(result.carpet.tileset, "Furniture");
        compare(result.table.displayedServerId, 400);
        compare(result.doodad.displayedServerId, 500);
        compare(result.door.serverId, 202);
        compare(result.door.displayedServerId, 201);
    }
    function test_sharedIdKeepsRequestedFamily() {
        const result = options({wall: candidate("wall", 200), doodad: candidate("doodad", 200)});
        compare(result.wall.name, "stone");
        compare(result.wall.category, "terrain");
        compare(result.doodad.name, "rock");
        compare(result.doodad.category, "doodad");
    }
    function test_hiddenWallDoesNotHideAnotherVisibleWall() {
        const hidden = {kind: "wall", serverId: 999, name: "hidden wall"};
        const result = options({wall: hidden}, [hidden, candidate("wall", 200)]);
        compare(result.wall.name, "stone");
    }
    function test_unlistedBrushIsHidden() {
        palettes = ({terrain: {Empty: []}});
        compare(Object.keys(options({ground: candidate("ground", 101)})).length, 0);
    }
    function test_unlistedWallRemainsSelectable() {
        palettes = ({raw: {"Wall sprites": [202]}});
        const wall = options({wall: candidate("wall", 202)}).wall;
        compare(wall.kind, "wall");
        compare(wall.name, "stone");
        compare(wall.serverId, 202);
        compare(wall.displayedServerId, 202);
        compare(wall.category, "raw");
        compare(wall.tileset, "Wall sprites");
    }
    function test_unlistedWallWithoutRawEntry() {
        palettes = ({});
        const wall = options({wall: candidate("wall", 200)}).wall;
        compare(wall.name, "stone");
        compare(wall.serverId, 200);
        compare(wall.tileset, "");
    }
    function test_unknownWallIsHidden() {
        palettes = ({});
        verify(!options({wall: {kind: "wall", name: "missing wall", serverId: 999}}).wall);
    }
    function test_listedWallWinsOverUnlistedWallInStack() {
        const result = options({wall: candidate("wall", 203)},
                               [candidate("wall", 203), candidate("wall", 200)]);
        compare(result.wall.serverId, 200);
        compare(result.wall.category, "terrain");
    }
    function test_brushCanBeListedInAnotherBrushPalette() {
        palettes = ({terrain: {Carpets: [300]}, doodad: {Walls: [200]}});
        const result = options({wall: candidate("wall", 201), carpet: candidate("carpet", 301)});
        compare(result.wall.category, "doodad");
        compare(result.carpet.category, "terrain");
    }
    function test_multipleSelectionHasNoBrushOptions() {
        compare(Object.keys(options({ground: candidate("ground", 101)},
                                    [candidate("ground", 101)], false)).length, 0);
    }
    function test_collectionUsesStackPriorityThenGround() {
        let result = options({}, [candidate("wall", 200), candidate("table", 401),
                                  candidate("ground", 101)]);
        compare(result.collection.kind, "table");
        compare(result.collection.tileset, "Furniture");
        result = options({}, [candidate("wall", 200), candidate("ground", 101)]);
        compare(result.collection.kind, "ground");
    }
    function test_rawCollectionFallbackRequiresExactItem() {
        palettes = ({collection: {Items: [777]}});
        compare(options({}, [{kind: "raw", serverId: 777}]).collection.displayedServerId, 777);
        verify(!options({}, [{kind: "raw", serverId: 778}]).collection);
    }
}
