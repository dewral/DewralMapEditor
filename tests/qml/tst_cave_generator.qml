import QtQuick
import QtTest
import "../../editor/qml/dialogs" as Dialogs
Item {
    width: 1000; height: 900
    QtObject {
        id: controller
        property bool dungeonPreviewActive: false
        property var captured: ({})
        function clearDungeonPreview() {}
        function generateDungeonPreview(options) {
            captured = options;
            return {success: false, error: "Test preview"};
        }
    }
    Item {
        width: 0; height: 0
        Dialogs.DungeonGeneratorDialog { id: dialog; mapCtrl: controller }
    }
    TestCase {
        name: "CaveGenerator"
        when: windowShown
        function test_noWalls() {
            dialog.open(); tryCompare(dialog, "opened", true);
            const layout = findChild(dialog, "generatorLayout");
            verify(layout !== null);
            verify(dialog.height > 300, "Dialog must use window height, not its zero-sized owner");
            verify(dialog.contentItem.height > 200);
            verify(layout.visible);
            const scroll = findChild(dialog, "generatorScroll");
            verify(scroll.contentHeight > 300);
            layout.currentIndex = 1;
            compare(dialog.caveMode, true);
            dialog.caveDecorations = ["rocks"];
            dialog.generatePreview();
            compare(controller.captured.wall, "");
            compare(controller.captured.bossDoodad, "");
            compare(controller.captured.caveDecorations[0], "rocks");
            verify(dialog.height <= 820);
            dialog.close();
        }
    }
}
