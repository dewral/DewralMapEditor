import QtQuick
import QtTest
import "../../editor/qml/components" as Components

Item {
    width: 320
    height: 320
    QtObject {
        id: prefs
        property bool showClientBox: false
        property bool showTooltips: false
        property bool showWaypoints: false
        property bool showHouses: false
        property bool showLightSources: false
    }
    QtObject {
        id: map
        property int floor: 7
        property int tileSize: 32
        property int exitX: 1
        signal contentUpdated()
        function renderOriginX() { return 0; }
        function renderOriginY() { return 0; }
        function mapOverlayData(tooltips, waypoints, houses, lights) {
            return lights ? [{kind: "light_source", x: exitX, y: 1, name: "", text: "", intensity: 7, red: 255, green: 153, blue: 0}] : [];
        }
    }
    Components.MapOverlay {
        id: overlay
        anchors.fill: parent
        mapCtrl: map
        settings: prefs
    }
    TestCase {
        name: "LightSourceOverlay"
        when: windowShown
        function init() {
            prefs.showTooltips = false;
            prefs.showLightSources = false;
            map.exitX = 1;
            overlay.refreshData(true);
        }
        function test_light_toggle_without_tooltips() {
            compare(overlay.visible, false);
            prefs.showLightSources = true;
            compare(overlay.visible, true);
            compare(overlay.entries.length, 1);
            compare(overlay.entries[0].kind, "light_source");
            prefs.showLightSources = false;
            compare(overlay.entries.length, 0);
            prefs.showTooltips = true;
            compare(overlay.entries.length, 0);
        }
        function test_refresh_after_source_moves() {
            prefs.showLightSources = true;
            map.exitX = 2;
            overlay.refreshData(true);
            compare(overlay.entries[0].x, 2);
        }
        function test_source_badge() {
            let labels = [];
            let fills = [];
            const ctx = {
                strokeRect: function() {},
                fillRect: function() { fills.push(this.fillStyle); },
                strokeText: function() {},
                fillText: function(text) { labels.push(text); }
            };
            overlay.drawLightSource(ctx, 1, 1, 7, 255, 153, 0);
            compare(labels.length, 1);
            compare(labels[0], "7");
            compare(fills.length, 1);
            compare(fills[0], Qt.rgba(1, 0.6, 0, 1));
        }
    }
}
