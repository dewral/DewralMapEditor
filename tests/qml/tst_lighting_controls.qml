import QtQuick
import QtTest
import "../../editor/qml/components" as Components
Item {
    width: 360; height: 400
    QtObject { id: map; property int lightAmbient: 40; property int lightAmbientColor: 215 }
    Components.LightingControls { id: controls; width: 300; mapView: map }
    TestCase {
        name: "LightingControls"; when: windowShown
        function test_palette() {
            compare(controls.paletteColor(215), Qt.rgba(1,1,1,1));
            compare(controls.paletteColor(180), Qt.rgba(1,0,0,1));
            compare(controls.paletteColor(0), Qt.rgba(0,0,0,1));
            controls.paletteOpen = true;
            tryVerify(() => controls.implicitHeight > 250);
        }
    }
}