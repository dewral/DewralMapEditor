import QtQuick
import QtTest
import "../../editor/qml/controllers"

TestCase {
    name: "PaletteDocking"
    QtObject {
        id: prefs
        property string paletteDockSide: "left"
        property bool palettePositionLocked: false
        property real paletteFloatingX: 40
        property real paletteFloatingY: 20
        property int paletteFloatingHeight: 400
    }
    PaletteDockController { id: dock; settings: prefs; availableWidth: 1000; availableHeight: 600; panelWidth: 285 }
    function init() { dock.cancel(); prefs.paletteDockSide = "left"; prefs.palettePositionLocked = false; }
    function test_dockRightAndLeft() {
        dock.begin(30, 10); dock.move(980, 30); compare(dock.target, "right"); dock.finish();
        compare(prefs.paletteDockSide, "right"); compare(dock.x, 715); compare(dock.panelHeight, 600);
        dock.begin(750, 10); dock.move(10, 50); dock.finish(); compare(prefs.paletteDockSide, "left"); compare(dock.x, 0);
    }
    function test_floatAndRestore() {
        dock.begin(30, 10); dock.move(400, 130); dock.finish();
        compare(prefs.paletteDockSide, "floating"); compare(dock.x, 370); compare(dock.y, 120);
        compare(dock.panelWidth, 285); compare(dock.panelHeight, 400);
        dock.availableWidth = 500; verify(dock.x <= 215); dock.availableWidth = 1000;
    }
    function test_lockAndCancel() {
        prefs.palettePositionLocked = true; dock.begin(20, 10); dock.move(980, 10); dock.finish();
        compare(prefs.paletteDockSide, "left"); verify(!dock.dragging);
        prefs.palettePositionLocked = false; dock.begin(20, 10); dock.move(980, 10); dock.cancel();
        compare(prefs.paletteDockSide, "left"); compare(dock.target, "");
    }
    function test_clickDoesNotUndock() {
        prefs.paletteDockSide = "right";
        dock.begin(750, 10); dock.move(752, 11); dock.finish();
        compare(prefs.paletteDockSide, "right"); verify(!dock.dragging);
    }
    function test_panelEdgeTriggersPreview() {
        prefs.paletteDockSide = "floating";
        prefs.paletteFloatingX = 350; prefs.paletteFloatingY = 20;
        // Grab well inside the header: the pointer never needs to reach the edge.
        dock.begin(530, 30); dock.move(220, 30);
        compare(dock.x, 40); compare(dock.target, "left");
        dock.move(240, 30); compare(dock.target, "");
        dock.move(865, 30); compare(dock.target, "right");
        dock.finish(); compare(prefs.paletteDockSide, "right");
    }
}
