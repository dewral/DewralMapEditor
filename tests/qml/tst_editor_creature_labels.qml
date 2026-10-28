import QtQuick
import QtTest
import "../../editor/qml/components" as Components

Item {
    width: 640; height: 400
    QtObject {
        id: prefs
        property bool showClientBox: false
        property bool showTooltips: false
        property int tooltipMinimumZoom: 100
        property int creatureNamesMinimumZoom: 25
        property bool showWaypoints: false
        property bool showHouses: false
        property bool showLightSources: false
    }
    QtObject {
        id: map
        property int floor: 7
        property int tileSize: 32
        property bool showCreatures: true
        property int labelQueries: 0
        property real originX: 0
        signal contentUpdated()
        signal viewFlagsChanged()
        signal atlasChanged()
        onShowCreaturesChanged: viewFlagsChanged()
        function renderOriginX() { return originX; }
        function renderOriginY() { return 0; }
        function mapOverlayData(tooltips, waypoints, houses, lights) { return []; }
        function previewMonsterLabels(x, y, z, columns, rows) {
            labelQueries++;
            return showCreatures ? [
                {name: "Rat", x: 4, y: 4, elevation: 0, npc: false},
                {name: "Trader", x: 6, y: 4, elevation: 0, npc: true}
            ] : [];
        }
    }
    Components.MapOverlay {
        id: overlay
        anchors.fill: parent
        mapCtrl: map
        settings: prefs
    }
    TestCase {
        name: "EditorCreatureLabels"
        when: windowShown
        function init() {
            map.showCreatures = true; map.tileSize = 32;
            prefs.showTooltips = false; prefs.creatureNamesMinimumZoom = 25;
            overlay.refreshData(true);
        }
        function test_names_visible_with_all_other_overlays_disabled() {
            compare(overlay.visible, true);
            verify(findChild(overlay, "monsterInformation") !== null);
            compare(findChild(overlay, "editorCreatureLabels").creatures.length, 2);
        }
        function test_health_bar_matches_ingame_preview() {
            const information = findChild(overlay, "monsterInformation");
            const health = findChild(information, "monsterHealthFill");
            compare(health.color, "#00c000");
            compare(health.width, 25); compare(health.height, 2);
            compare(findChild(information, "creatureNameImage").smooth, false);
        }
        function test_names_ignore_tooltip_zoom_threshold() {
            prefs.showTooltips = true;
            map.tileSize = 8;
            compare(overlay.tooltipsVisible, false);
            compare(overlay.visible, true);
            verify(findChild(overlay, "monsterInformation") !== null);
        }
        function test_zoom_cutoff_hides_and_restores_delegates() {
            map.tileSize = 7;
            compare(overlay.creaturesVisible,false);
            verify(!findChild(overlay,"monsterInformation") || !findChild(overlay,"monsterInformation").visible);
            map.tileSize = 8;
            compare(overlay.creaturesVisible,true);
            verify(findChild(overlay,"monsterInformation")!==null);
            prefs.creatureNamesMinimumZoom = 50;
            verify(!findChild(overlay,"monsterInformation") || !findChild(overlay,"monsterInformation").visible);
            map.tileSize = 16;
            verify(findChild(overlay,"monsterInformation")!==null);
        }
        function test_zero_keeps_names_at_low_zoom_but_respects_toggle() {
            map.tileSize = 2;
            verify(!findChild(overlay,"monsterInformation") || !findChild(overlay,"monsterInformation").visible);
            prefs.creatureNamesMinimumZoom = 0;
            verify(findChild(overlay,"monsterInformation")!==null);
            map.showCreatures = false;
            verify(!findChild(overlay,"monsterInformation") || !findChild(overlay,"monsterInformation").visible);
        }
        function test_hidden_names_do_not_query_creature_list() {
            map.tileSize = 2;
            wait(0); map.labelQueries = 0;
            map.originX += 1; map.contentUpdated(); overlay.refreshData(true);
            compare(map.labelQueries,0);
            compare(findChild(overlay,"editorCreatureLabels").visible,false);
        }
        function test_creature_toggle_hides_and_restores_names() {
            map.showCreatures = false;
            compare(overlay.visible, false);
            verify(!findChild(overlay, "monsterInformation") || !findChild(overlay, "monsterInformation").visible);
            map.showCreatures = true;
            compare(overlay.visible, true);
            verify(findChild(overlay, "monsterInformation") !== null);
        }
    }
}
