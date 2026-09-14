import QtQuick
import QtTest
import "../../editor/qml/dialogs" as Dialogs

Item {
    width: 1000; height: 900
    QtObject {
        id: controller
        property bool groundClusterStampActive: false
        property int groundClusterStampSeed: 1
        property var captured: ({})
        function cancelGroundClusterStamp() { groundClusterStampActive = false; }
        function createGroundClusterStamp(options) {
            captured = options;
            groundClusterStampSeed = options.seed;
            groundClusterStampActive = true;
            return {success: true, clusterCount: 2, count: 10,
                    seed: options.seed, decorationCount: 0,
                    groundCounts: [{name: "cave floor", count: 10}]};
        }
    }
    Dialogs.GroundClusterGeneratorDialog { id: dialog; mapCtrl: controller }
    TestCase {
        name: "GroundClusterGenerator"
        when: windowShown
        function test_options() {
            dialog.open(); tryCompare(dialog, "opened", true);
            verify(findChild(dialog, "clusterGeneratorScroll").contentHeight > 300);
            verify(findChild(dialog, "clusterGroundA") !== null);
            dialog.createStamp();
            compare(controller.captured.grounds.length, 1);
            compare(controller.captured.grounds[0].name, "cave floor");
            compare(controller.captured.minimumRadius, 2);
            compare(controller.captured.maximumRadius, 7);
            verify(dialog.opened);
            dialog.close();
            tryCompare(controller, "groundClusterStampActive", false);
        }
    }
}
