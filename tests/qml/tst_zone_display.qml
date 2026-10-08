import QtQuick
import QtTest
import "../../editor/qml/components" as Components
Item {
    width: 300; height: 620
    QtObject {
        id: map
        property int visibleZoneMask: 29
        property var zoneColors: ["#399ee8", "#48b883", "#dfa65a", "#d46b79", "#9173be"]
        property var zoneOpacities: [0.25,0.25,0.25,0.25]
        property bool showZonesAlways: true
        property bool showHouses: true
        property bool showSpawns: true
        property bool showCreatures: true
        property bool showGrid: false
        property double houseOpacity: 0.25
        property double tilesOpacity: 1.0
        property double itemsOpacity: 1.0
    }
    Components.ZoneDisplayPanel { id: panel; width: 236; height: parent.height; mapView: map }
    TestCase {
        name: "ZoneDisplay"; when: windowShown
        function test_visibilityDoesNotRemoveOtherFlags() {
            map.visibleZoneMask = 29;
            const pz = findChild(panel,"zoneVisible0");
            mouseClick(pz);
            compare(map.visibleZoneMask,28);
            map.visibleZoneMask = 5;
            tryCompare(pz,"checked",true);
            mouseClick(pz);
            compare(map.visibleZoneMask,4);
        }
        function test_opacityTracksModel() {
            map.zoneOpacities = [0.75,0.15,0.4,0.6];
            tryCompare(findChild(panel,"zoneOpacity0"),"value",0.75);
            tryCompare(findChild(panel,"zoneOpacity1"),"value",0.15);
        }
        function test_colorChangesOnlySelectedZone() {
            const previous = map.zoneColors.slice();
            panel.setZoneColor(0, "#ff8800");
            compare(map.zoneColors[0], "#ff8800");
            compare(map.zoneColors[1], previous[1]);
            tryCompare(findChild(panel, "zoneColor0"), "color", "#ff8800");
            panel.setZoneColor(4, "#00aaee");
            tryCompare(findChild(panel, "zoneColor4"), "color", "#00aaee");
            map.zoneColors = previous;
        }
        function test_houseControls() {
            const house = findChild(panel,"zoneVisible4");
            map.showHouses = true;
            house.checked = false;
            house.clicked();
            compare(map.showHouses,false);
            const opacity = findChild(panel,"zoneOpacity4");
            opacity.value = 0.6;
            opacity.moved();
            compare(map.houseOpacity,0.6);
        }
    }
}
