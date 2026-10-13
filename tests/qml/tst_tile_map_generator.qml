import QtQuick
import QtQuick.Controls
import QtTest
import Tibia 1.0
import "../../editor/qml/dialogs" as Dialogs

Item {
    id: root
    width: 1000; height: 820
    QtObject { id: mapStub; property int floor: 7 }
    Dialogs.TileMapGenDialog { id: dialog; mapCtrl: mapStub }
    TestCase {
        name: "TileMapGenerator"
        when: windowShown
        function field(name) { const item = findChild(dialog, name); verify(item !== null, name); return item; }
        function init() {
            Backend.uiTheme.style = "fluent-dark";
            Backend.tileMapGen.busy = false;
            Backend.tileMapGen.previewReady = false;
            Backend.tileMapGen.available = true;
            Backend.tileMapGen.applied = 0;
            Backend.tileMapGenProfiles.profiles = [];
            Backend.tileMapGen.materialChoices = {grounds:[{name:"cave floor",label:"cave floor",serverId:100},{name:"rock",label:"rock",serverId:101}], floors:["cave floor"], backgrounds:["rock"], walls:[], decorations:[], monsters:["Rat","Orc","Troll","Wolf","Dragon","Demon"]};
            dialog.open();
            dialog.monsters = []; dialog.activeProfileId = "";
            field("tileMapProfileName").text = "";
            field("tileMapFloor").currentIndex = 0;
            field("tileMapBackground").currentIndex = 0;
            field("tileMapWidth").value = 96; field("tileMapHeight").value = 80;
            field("tileMapFloors").value = 1; field("tileMapDownStair").value = 0; field("tileMapUpStair").value = 0;
            field("tileMapDestination").currentIndex = 0; field("tileMapStyle").currentIndex = 0;
            field("tileMapSeed").text = "fixed-seed";
            field("tileMapWorkflow").currentIndex = 0;
            wait(20);
        }
        function cleanup() { dialog.close(); Backend.tileMapGen.cancel(); Backend.sprReader.generatorPreviewSprites = false; Backend.otbReader.generatorPreviewMetadata = false; }
        function test_generatePreviewApply() {
            const apply = field("applyTileMap"); verify(!apply.enabled);
            field("tileMapStyle").currentIndex = 1;
            field("tileMapX").value = 1200; field("tileMapY").value = 1300;
            mouseClick(field("generateTileMap"));
            verify(Backend.tileMapGen.busy);
            const options = Backend.tileMapGen.lastOptions;
            compare(options.generator, "cave"); compare(options.width, 96); compare(options.height, 80);
            compare(options.seed, "fixed-seed"); compare(options.palette.floor, "cave floor");
            compare(options.x, 1200); compare(options.y, 1300); compare(options.z, 7); verify(options.placement);
            compare(options.settings.cave_density, 0.48);
            verify(!apply.enabled); verify(!field("tileMapWidth").enabled);
            Backend.tileMapGen.busy = false; Backend.tileMapGen.previewReady = true;
            tryCompare(apply, "enabled", true);
            mouseClick(apply); compare(Backend.tileMapGen.applied, 1); verify(!apply.enabled);
        }
        function test_editDiscardsPreview() {
            Backend.tileMapGen.previewReady = true;
            field("tileMapX").value += 1;
            tryCompare(Backend.tileMapGen, "previewReady", false);
            verify(!field("applyTileMap").enabled);
        }
        function test_cancelAndClose() {
            mouseClick(field("generateTileMap")); verify(Backend.tileMapGen.busy);
            mouseClick(field("cancelTileMap")); verify(!Backend.tileMapGen.busy);
            mouseClick(field("generateTileMap")); verify(Backend.tileMapGen.busy);
            dialog.close(); verify(!Backend.tileMapGen.busy); verify(!Backend.tileMapGen.previewReady);
        }
        function test_largeMapNeedsNewDocument() {
            field("tileMapWidth").value = 1000; field("tileMapHeight").value = 1000;
            verify(!field("generateTileMap").enabled);
            field("tileMapDestination").currentIndex = 1;
            verify(field("generateTileMap").enabled);
            dialog.generate(); verify(!Backend.tileMapGen.lastOptions.placement);
        }
        function test_multipleFloorsNeedStairsAndSpace() {
            field("tileMapFloors").value = 2;
            verify(!field("generateTileMap").enabled);
            field("tileMapDownStair").value = 100; field("tileMapUpStair").value = 101;
            verify(field("generateTileMap").enabled);
            field("tileMapZ").value = 15;
            verify(!field("generateTileMap").enabled);
        }
        function test_emptyMaterialsAndMissingGenerator() {
            Backend.tileMapGen.available = false;
            verify(!field("generateTileMap").enabled);
            Backend.tileMapGen.available = true;
            Backend.tileMapGen.materialChoices = {grounds:[],floors: [], backgrounds: [], walls: [], decorations: [], monsters: []};
            dialog.refreshMaterials();
            field("tileMapFloor").currentIndex = -1;
            verify(!field("generateTileMap").enabled);
        }
        function test_groundChoicesAndSprites() {
            const floor = field("tileMapFloor"), background = field("tileMapBackground");
            compare(floor.count, 2); compare(background.count, 3);
            compare(floor.currentName, "cave floor");
            background.currentIndex = 2;
            compare(dialog.generationOptions().palette.background, "rock");
            Backend.otbReader.generatorPreviewMetadata = true;
            Backend.sprReader.generatorPreviewSprites = true;
            tryVerify(() => String(field("tileMapFloorSelectedSprite").source).length > 0);
            floor.popup.open(); wait(50);
            const firstRow = floor.popup.contentItem.itemAtIndex(0);
            verify(firstRow !== null);
            verify(String(firstRow.contentItem.children[0].source).length > 0);
            mouseClick(floor.popup.contentItem.itemAtIndex(1));
            tryCompare(floor, "currentIndex", 1);
            compare(dialog.generationOptions().palette.floor, "rock");
            floor.popup.close();
        }
        function test_liveBrushRefreshPreservesSelection() {
            field("tileMapFloor").currentIndex = 1;
            Backend.tileMapGen.materialChoices = {grounds:[{name:"grass",label:"grass",serverId:100},{name:"rock",label:"rock",serverId:101}], floors:[],backgrounds:[],walls:[],decorations:[],monsters:[]};
            Backend.brushStore.brushesChanged();
            compare(field("tileMapFloor").currentName, "rock");
            compare(field("tileMapFloor").model[0].name, "grass");
        }
        function test_weightedMonstersLimitAndRemove() {
            for (let i = 0; i < 6; ++i) dialog.addMonster();
            compare(dialog.monsters.length, 5);
            verify(!field("addTileMapMonster").enabled);
            dialog.editMonster(0, "weight", 80); dialog.editMonster(1, "weight", 20);
            const list = dialog.generationOptions().palette.creatures;
            compare(list.length, 5); compare(list[0].weight, 80); compare(list[1].weight, 20);
            compare(new Set(list.map(entry => entry.name)).size, 5);
            dialog.removeMonster(2); compare(dialog.monsters.length, 4);
            verify(field("addTileMapMonster").enabled);
            verify(dialog.monsterChoices(1).indexOf(dialog.monsters[0].name) < 0);
        }
        function test_profileRestoresCompleteRecipeAndUpdates() {
            field("tileMapProfileName").text = "Hunting profile";
            field("tileMapBackground").currentIndex = 2;
            dialog.addMonster(); dialog.addMonster(); dialog.editMonster(0,"weight",75);
            const recipe = JSON.stringify(dialog.generationOptions());
            dialog.saveProfile(false); const id = dialog.activeProfileId;
            verify(id.length > 0); compare(Backend.tileMapGenProfiles.profiles.length, 1);
            field("tileMapWidth").value = 200; dialog.monsters = [];
            dialog.loadProfile(id);
            compare(JSON.stringify(dialog.generationOptions()), recipe);
            const stored = JSON.parse(JSON.stringify(Backend.tileMapGenProfiles.load(id)));
            Object.assign(stored.options.settings, {rooms:18,room_min:8,room_max:23,corridor_width:5,loops:0.45,
                cave_density:0.6,smoothing:7,chamber_size:19,terrain_scale:256,water:0.2,lava:0.1,
                decoration_density:0.08,spawn_density:0.012,spawn_spacing:16,spawn_radius:5,respawn_seconds:180,safe_radius:12});
            Backend.tileMapGenProfiles.save(stored,id); dialog.loadProfile(id);
            compare(JSON.stringify(dialog.generationOptions().settings), JSON.stringify(stored.options.settings));
            field("tileMapWidth").value = 120; dialog.saveProfile(false);
            compare(Backend.tileMapGenProfiles.profiles.length, 1);
            compare(Backend.tileMapGenProfiles.load(id).options.width, 120);
            dialog.saveProfile(true); compare(Backend.tileMapGenProfiles.profiles.length, 2);
        }
        function test_themeLayout_data() {
            return ["fluent-dark", "github", "gray-dark", "gray-modern", "classic", "windows-classic"].map(style => ({tag: style, style: style}));
        }
        function test_themeLayout(data) {
            Backend.uiTheme.style = data.style;
            wait(50);
            const button = field("generateTileMap");
            verify(button.width > 20); verify(button.height > 15);
            const bottom = button.mapToItem(dialog.contentItem, 0, button.height).y;
            verify(bottom <= dialog.contentItem.height + 1, "Footer must fit in the dialog");
            if (data.style === "fluent-dark") {
                grabImage(root).save("tmp/tile-map-generator.png");
            }
        }
    }
}
