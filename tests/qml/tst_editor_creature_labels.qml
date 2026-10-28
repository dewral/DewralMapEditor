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
        property bool showWaypoints: false
        property bool showHouses: false
        property bool showLightSources: false
    }
    QtObject {
        id: map
        property int floor: 7
        property int tileSize: 32
        property bool showCreatures: true
        property real originX: 0
        signal contentUpdated()
        signal viewFlagsChanged()
        signal atlasChanged()
        onShowCreaturesChanged: viewFlagsChanged()
        function renderOriginX() { return originX; }
        function renderOriginY() { return 0; }
        function mapOverlayData(tooltips, waypoints, houses, lights) { return []; }
        function previewMonsterLabels(x, y, z, columns, rows) {
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
            prefs.showTooltips = false;
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
        function test_creature_toggle_hides_and_restores_names() {
            map.showCreatures = false;
            compare(overlay.visible, false);
            compare(findChild(overlay, "monsterInformation"), null);
            map.showCreatures = true;
            compare(overlay.visible, true);
            verify(findChild(overlay, "monsterInformation") !== null);
        }
    }
}
