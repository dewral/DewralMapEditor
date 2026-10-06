import QtQuick
import QtTest
import Tibia 1.0
import "../../editor/qml/components" as Components
Item {
    width: 800; height: 400
    QtObject {
        id: map
        property bool torchOn: false
        property bool selectionMode: false
        property bool lassoMode: false
        property bool eraseMode: false
        property bool optionalBorderMode: false
        property bool automagic: false
        property int activeZone: 0
        property int selectionCount: 0
        property bool hasClipboard: false
        property string brushShape: "square"
        property int brushSize: 0
    }
    Components.FluentFileBar { id: bar; width: parent.width; height: 40; mapView: map }
    Components.FluentTools { id: tools; y: 60; width: 265; mapView: map }
    Components.PaletteBrushSizeSelector { id: brush; y: 200; width: 265; mapCtrl: map; githubUi: false }
    TestCase {
        name: "FluentControls"
        when: windowShown
        function init() { Backend.uiTheme.style = "fluent-dark"; Backend.otbmReader.loaded = true; wait(50); }
        function test_lightTracksRendererInBothDirections() {
            const toggle = findChild(bar, "fluentLightSwitch");
            verify(toggle !== null);
            map.torchOn = false;
            tryCompare(toggle, "checked", false);
            mouseClick(toggle);
            compare(map.torchOn, true);
            map.torchOn = false;
            tryCompare(toggle, "checked", false);
            mouseClick(toggle);
            compare(map.torchOn, true);
            Backend.otbmReader.loaded = false;
            tryCompare(toggle, "enabled", false);
        }
        function test_toolsReturnToDrawingAndResize() {
            compare(findChild(tools, "fluentToolDraw"), null);
            compare(findChild(tools, "fluentToolSelect"), null);
            compare(findChild(tools, "fluentToolErase"), null);
            const lasso = findChild(tools, "fluentToolLasso");
            const border = findChild(tools, "fluentToolBorder");
            mouseClick(lasso);
            compare(map.selectionMode, true);
            mouseClick(lasso);
            compare(map.selectionMode, false);
            mouseClick(lasso);
            mouseClick(border);
            compare(map.selectionMode, false);
            compare(map.optionalBorderMode, true);
            mouseClick(border);
            compare(map.optionalBorderMode, false);
            let narrowHeight = 0;
            for (const width of [210, 265, 420, 520]) {
                tools.width = width;
                wait(30);
                const np = findChild(tools, "fluentToolNP");
                verify(np.x + np.width <= tools.width);
                compare(lasso.width, np.width);
                compare(lasso.width, 62, "Tools must keep their width when the panel resizes");
                compare(lasso.height, 38);
                const doors = findChild(tools, "fluentToolDoors");
                verify(doors.x + doors.width <= tools.width);
                if (width === 210) {
                    verify(np.y > lasso.y, "The fourth tool must wrap in a narrow panel");
                    narrowHeight = tools.height;
                } else {
                    compare(np.y, lasso.y);
                    verify(tools.height < narrowHeight, "Wider panels must fit tools in fewer rows");
                }
            }
        }
        function test_brushKeepsRadiusAndBounds() {
            compare(findChild(brush, "fluentBrushIncrease"), null);
            for (const radius of [0, 1, 2, 4, 6, 8, 11]) {
                const button = findChild(brush, "brushRadius" + radius);
                verify(button !== null);
                mouseClick(button);
                compare(map.brushSize, radius);
                compare(button.active, true);
            }
            mouseClick(findChild(brush, "fluentBrushcircle"));
            compare(map.brushShape, "circle");
        }
    }
}
