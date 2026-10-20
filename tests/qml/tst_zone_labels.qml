import QtQuick
import QtTest
import "../../editor/qml/components" as Components
Item {
    width: 320; height: 320
    QtObject {
        id: map
        property real ox: 100
        property real oy: 100
        property int tileSize: 32
        property int activeZone: 0
        property int numberedZoneId: 42
        property bool numbered: false
        property bool editing: false
        property int rebuilds: 0
        signal contentUpdated()
        signal viewFlagsChanged()
        function renderOriginX() { return ox }
        function renderOriginY() { return oy }
        function zoneLabelEditInProgress() { return editing }
        function visibleZoneLabels() { ++rebuilds; return numbered ? [{worldX:105.5,worldY:105.5,name:"Zone 7, 42",color:"#c6dc70",flags:0,zoneIds:"7,42"}] : [{worldX:105.5,worldY:105.5,name:"PvP",color:"#d46b79",flags:16}] }
    }
    Components.ZoneLabels { id: labels; anchors.fill: parent; mapView: map }
    TestCase {
        name: "ZoneLabelAnchoring"; when: windowShown
        function test_numberedZoneHighlight() {
            map.numbered=true; map.activeZone=64; map.numberedZoneId=42;
            labels.refreshRegions();
            const label=findChild(labels,"zoneLabel0");
            verify(label.activeNumberedZone);
            compare(label.border.width,2);
            map.numberedZoneId=1;
            compare(label.activeNumberedZone,false);
            compare(label.border.width,1);
            map.numbered=false; map.activeZone=0; labels.refreshRegions();
        }
        function test_panImmediatelyTracksWorldPosition() {
            tryVerify(() => findChild(labels,"zoneLabel0") !== null);
            const label = findChild(labels,"zoneLabel0");
            const initial = label.x;
            map.ox += 0.25; map.contentUpdated();
            compare(label.x,initial - 8);
            map.ox = 120; map.contentUpdated();
            verify(label.x + label.width < 0,"Offscreen labels must not stick to the viewport edge");
            map.ox = 100; map.contentUpdated();
            compare(label.x,initial);
        }
        function test_pointerAndCameraUpdatesRetainLabelInstance() {
            tryVerify(() => findChild(labels,"zoneLabel0") !== null);
            const label = findChild(labels,"zoneLabel0");
            map.contentUpdated();
            wait(220);
            compare(findChild(labels,"zoneLabel0"),label);
            map.ox += 0.25; map.contentUpdated();
            wait(220);
            compare(findChild(labels,"zoneLabel0"),label);
            map.ox -= 0.25; map.contentUpdated();
        }
        function test_editStrokeDefersRegionRebuild() {
            labels.refreshRegions();
            const before = map.rebuilds;
            map.editing = true;
            for (let i = 0; i < 3; ++i) { map.contentUpdated(); wait(180); }
            compare(map.rebuilds,before,"Painting must not recompute region centers during a stroke");
            map.editing = false; map.contentUpdated();
            tryVerify(() => map.rebuilds > before);
            compare(map.rebuilds,before + 1);
        }
    }
}
