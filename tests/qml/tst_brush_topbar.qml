import QtQuick
import QtTest
import Tibia 1.0
import "../../editor/qml/components" as Components
Item {
    width: 1400; height: 400
    QtObject { id: prefs; property string brushSizeDock: "topbar"; property string toolsDock: "palette" }
    QtObject {
        id: map
        property string brushShape: "square"
        property int brushSize: 0
        property bool hasClipboard: false
        property int selectionCount: 0
        property bool showAnimations: false
        property bool torchOn: false
        property int lightAmbient: 40
        property int lightAmbientColor: 215
        function forceActiveFocus() {}
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
        function test_palette_panels_fit_content() {
            tools.compact = false;
            selector.compact = false;
            tools.width = 236;
            selector.width = 236;
            tools.visible = false;
            selector.visible = false;
            wait(20);
            tools.visible = true;
            selector.visible = true;
            wait(50);
            const lastTool = findChild(tools, "fluentToolDoors");
            const toolBottom = lastTool.mapToItem(tools, 0, lastTool.height).y;
            verify(tools.implicitHeight >= toolBottom, "Tools height " + tools.implicitHeight + " < content " + toolBottom);
            const lastBrush = findChild(selector, "brushRadius11");
            const brushBottom = lastBrush.mapToItem(selector, 0, lastBrush.height).y;
            verify(brushBottom > 24);
            verify(selector.implicitHeight >= brushBottom, "Brush height " + selector.implicitHeight + " < content " + brushBottom);
            tools.compact = true;
            selector.compact = true;
            tools.width = 489;
            selector.width = 280;
        }
        function test_lighting_active_indicator_tracks_state() {
            const button = findChild(bar, "fluentLightSwitch");
            const background = findChild(button, "lightingButtonBackground");
            const icon = findChild(button, "lightingButtonIcon");
            map.torchOn = false;
            const inactiveSource = String(icon.source);
            const glow = findChild(button, "lightingButtonGlow");
            compare(glow.visible, false);
            compare(background.border.width, 0);
            map.torchOn = true;
            compare(button.checked, true);
            compare(String(icon.source), inactiveSource);
            compare(glow.visible, true);
            compare(background.border.width, 0);
            map.torchOn = false;
            compare(button.checked, false);
            compare(String(icon.source), inactiveSource);
        }
        function test_animationToggle() {
            const button = findChild(bar, "fluentAnimationButton");
            verify(button !== null);
            const loaded = Backend.otbmReader.loaded;
            Backend.otbmReader.loaded = true;
            map.showAnimations = false;
            mouseClick(button);
            compare(map.showAnimations, true);
            compare(button.checked, true);
            mouseClick(button);
            compare(map.showAnimations, false);
            Backend.otbmReader.loaded = loaded;
        }
        function test_adapts_to_width() {
            tryCompare(bar, "height", 42);
            bar.width = 700;
            tryCompare(bar, "height", 114);
            prefs.brushSizeDock = "palette";
            tryCompare(bar, "height", 78);
            prefs.brushSizeDock = "topbar";
            bar.width = 1400;
        }
        function test_lightingFitsNarrowWindow() {
            prefs.brushSizeDock = "palette";
            for (const width of [720, 860, 1400]) {
                bar.width = width;
                tryCompare(bar, "height", bar.compactLighting ? 78 : 42);
                const color = findChild(bar, "brightnessColorButton");
                verify(color.mapToItem(bar, color.width, 0).x <= width);
                verify(color.mapToItem(bar, 0, color.height).y <= bar.height);
            }
            prefs.brushSizeDock = "topbar";
            bar.width = 1400;
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
