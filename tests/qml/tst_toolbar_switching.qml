import QtQuick
import QtTest
import "../../editor/qml/components" as Components

Item {
    width: 600
    height: 200
    property bool mapLoaded: true

    Component { id: compact; Item { implicitHeight: mapLoaded ? 40 : 0 } }
    Component { id: modern; Item { implicitHeight: mapLoaded ? 56 : 0 } }
    Components.ImplicitHeightLoader { id: toolbar; width: parent.width }
    Item { id: tabs; anchors.top: toolbar.bottom; height: 30 }

    TestCase {
        name: "ToolbarThemeSwitching"
        when: windowShown

        function test_switchOnceInBothDirections() {
            for (let cycle = 0; cycle < 3; ++cycle) {
                for (const entry of [[compact, 40], [modern, 56], [null, 0],
                                     [modern, 56], [compact, 40], [null, 0]]) {
                    toolbar.sourceComponent = entry[0];
                    tryCompare(toolbar, "height", entry[1]);
                    compare(tabs.y, entry[1]);
                    if (toolbar.item)
                        compare(toolbar.item.height, entry[1]);
                }
            }
        }

        function test_mapLoadedWithoutRecreatingToolbar() {
            toolbar.sourceComponent = modern;
            mapLoaded = false;
            tryCompare(toolbar, "height", 0);
            mapLoaded = true;
            tryCompare(toolbar, "height", 56);
            compare(tabs.y, 56);
        }
    }
}
