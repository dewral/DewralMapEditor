import QtQuick
import QtTest
import "../../editor/qml/components" as Components
Item {
    width: 200; height: 200
    QtObject { id: view; property real contentY: 0; property real height: 200 }
    Rectangle { id: cell; width: 60; height: 60; y: 80 }
    Components.PaletteCellMotion { id: motion; view: view; targetCell: cell }
    TestCase {
        name: "PaletteCellMotion"
        when: windowShown
        function init() { view.contentY=0;cell.y=80;motion.enabled=true; }
        function test_edges_reveal_in_both_directions() {
            compare(motion.reveal,1);
            cell.y=200;compare(motion.reveal,0);
            cell.y=195;const entering=motion.reveal;verify(entering>0 && entering<1);
            cell.y=180;verify(motion.reveal>entering);
            cell.y=160;compare(motion.reveal,1);
            cell.y=-60;compare(motion.reveal,0);
            cell.y=-55;verify(motion.reveal>0 && motion.reveal<1);
            cell.y=0;compare(motion.reveal,1);
        }
        function test_jump_and_reused_cell_have_no_delayed_animation() {
            view.contentY=1000;compare(motion.reveal,0);
            cell.y=1080;compare(motion.reveal,1);
            motion.enabled=false;cell.y=3000;compare(motion.reveal,1);
        }
    }
}
