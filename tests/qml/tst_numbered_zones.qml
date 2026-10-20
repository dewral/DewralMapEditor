import QtQuick
import QtQuick.Window
import QtTest
import Tibia 1.0
import "../../editor/qml/dialogs" as Dialogs
Item {
    id: scene; width: 900; height: 800
    QtObject {
        id: reader
        property bool numberedZonesEnabled: false
    }
    QtObject {
        id: map
        property int numberedZoneId: 42
        property bool showNumberedZones: true
        property bool eraseMode: true
        property int activeZone: 0
        property var calls: []
        function findNumberedZones(id, selected) { calls.push([id,selected]); return {results:[{x:100,y:200,z:7,zones:"42"}],total:1}; }
        function centerOnPosition(x,y,z) {}
    }
    Dialogs.NumberedZonesDialog { id: dialog; mapView: map; mapReader: reader }
    TestCase {
        name: "NumberedZones"; when: windowShown
        function locate(name) { return findChild(dialog.contentItem,name); }
        function initTestCase() { scene.Window.window.width=900; scene.Window.window.height=800; }
        function init() { Backend.uiTheme.style="fluent-dark"; reader.numberedZonesEnabled=false; map.numberedZoneId=42; map.activeZone=0; map.calls=[]; dialog.open(); tryCompare(dialog,"visible",true); wait(40); }
        function cleanup() { dialog.close(); tryCompare(dialog,"visible",false); }
        function test_optInAndPaint() {
            compare(locate("paintZone").enabled,false);
            locate("zonesEnabled").clicked(); compare(reader.numberedZonesEnabled,true);
            compare(locate("paintZone").enabled,true);
            locate("paintZone").clicked();
            compare(map.numberedZoneId,42); compare(map.activeZone,64); compare(map.eraseMode,false); compare(dialog.visible,false);
        }
        function test_resultsShown() {
            reader.numberedZonesEnabled = true;
            dialog.search(); wait(60);
            compare(dialog.results.length,1);
        }
        function test_searchScope() {
            locate("zonesSelectionOnly").clicked();
            locate("findZones").clicked();
            compare(map.calls[0][0],42); compare(map.calls[0][1],true); compare(dialog.total,1);
        }
        function test_themes_data() { return [{tag:"Fluent",style:"fluent-dark"},{tag:"Gray",style:"gray-dark"},{tag:"Github",style:"github-dark"},{tag:"Classic",style:"classic"}]; }
        function test_themes(data) {
            Backend.uiTheme.style=data.style; wait(30);
            verify(dialog.contentItem.width<=dialog.width);
            verify(dialog.contentItem.height>400);
            verify(locate("zoneId").visible);
        }
    }
}
