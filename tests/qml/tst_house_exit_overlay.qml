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
    }
    QtObject {
        id: map
        property int floor: 7
        property int tileSize: 32
        property int exitX: 1
        signal contentUpdated()
        function renderOriginX() { return 0; }
        function renderOriginY() { return 0; }
        function mapOverlayData(tooltips, waypoints, houses) {
            return houses ? [{kind: "house_exit", x: exitX, y: 1, name: "House", text: "EXIT"}] : [];
        }
    }
    Components.MapOverlay {
        id: overlay
        anchors.fill: parent
        mapCtrl: map
        settings: prefs
    }
    TestCase {
        name: "HouseExitOverlay"
        when: windowShown
        function init() {
            prefs.showTooltips = false;
            prefs.showHouses = false;
            map.exitX = 1;
            overlay.refreshData(true);
        }
        function test_house_toggle_without_tooltips() {
            compare(overlay.visible, false);
            prefs.showHouses = true;
            compare(overlay.visible, true);
            compare(overlay.entries.length, 1);
            compare(overlay.entries[0].kind, "house_exit");
            prefs.showHouses = false;
            compare(overlay.entries.length, 0);
            prefs.showTooltips = true;
            compare(overlay.entries.length, 0);
        }
        function test_refresh_after_exit_moves() {
            prefs.showHouses = true;
            map.exitX = 2;
            overlay.refreshData(true);
            compare(overlay.entries[0].x, 2);
        }
        function test_exit_label() {
            let labels = [];
            const ctx = {
                strokeText: function(text, x, y) {},
                fillText: function(text, x, y) { labels.push(text); }
            };
            overlay.drawHouseExit(ctx, 16, 16);
            compare(labels.length, 1);
            compare(labels[0], "EXIT");
        }
    }
}
