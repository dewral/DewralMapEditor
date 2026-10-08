import QtQuick
import QtTest
import Tibia 1.0
import "../../editor/qml/components" as Components
Item {
    width: 1000; height: 400
    QtObject { id: prefs; property string brushSizeDock: "topbar"; property string toolsDock: "palette" }
    QtObject {
        id: map
        property string brushShape: "square"
        property int brushSize: 0
        property bool hasClipboard: false
        property int selectionCount: 0
        property bool torchOn: false
        property bool selectionMode: false
        property bool lassoMode: false
        property bool optionalBorderMode: false
        property bool eraseMode: false
        property bool automagic: true
        property int activeZone: 0
    }
    Components.FluentFileBar { id: bar; width: parent.width; height: implicitHeight; mapView: map; settings: prefs }
    Components.PaletteBrushSizeSelector { id: selector; y: 120; width: 280; compact: true; githubUi: false; mapCtrl: map }
    Components.FluentTools { id: tools; y: 220; width: 489; compact: true; mapView: map }
    SignalSpy { id: toolsDropped; target: tools; signalName: "dockDragFinished" }
    SignalSpy { id: dropped; target: selector; signalName: "dockDragFinished" }
    TestCase {
        name: "BrushTopbar"
        when: windowShown
        function initTestCase() { Backend.uiTheme.style = "fluent-dark"; }
        function test_adapts_to_width() {
            compare(bar.height, 42);
            bar.width = 700;
            compare(bar.height, 78);
            prefs.brushSizeDock = "palette";
            compare(bar.height, 42);
            prefs.brushSizeDock = "topbar";
            bar.width = 1000;
        }
        function test_tools_controls_and_docking() {
            prefs.toolsDock = "topbar";
            compare(bar.secondRow, true);
            prefs.brushSizeDock = "palette";
            compare(bar.dockedWidth, 489);
            prefs.toolsDock = "palette";
            prefs.brushSizeDock = "topbar";
            const pz = findChild(tools, "fluentToolPZ");
            mouseClick(pz);
            compare(map.activeZone, 1);
            const lasso = findChild(tools, "fluentToolLasso");
            mouseClick(lasso);
            compare(map.selectionMode, true);
            compare(map.lassoMode, true);
            const grip = findChild(tools, "brushDockHandle");
            mousePress(grip, 8, 10);
            mouseMove(grip, 50, 20, 20);
            mouseRelease(grip, 50, 20);
            compare(toolsDropped.count, 1);
        }
        function test_brush_controls_and_drag() {
            compare(selector.implicitHeight, 30);
            const radius = findChild(selector, "brushRadius4");
            mouseClick(radius);
            compare(map.brushSize, 4);
            const grip = findChild(selector, "brushDockHandle");
            mousePress(grip, 8, 10);
            mouseMove(grip, 50, 20, 20);
            mouseRelease(grip, 50, 20);
            compare(dropped.count, 1);
        }
    }
}
