import QtQuick
import QtTest
import "../../editor/qml/components" as Components
Item {
    width: 300; height: 220
    GridView {
        id: view
        width: 200; height: 200
        cellWidth: 100; cellHeight: 50
        model: 100
        delegate: Rectangle { width: 98; height: 48; color: "gray" }
    }
    Components.PaletteSmoothScroll { id: scroll; view: view; step: 100 }
    TestCase {
        name: "PaletteSmoothScroll"
        when: windowShown
        function init() { scroll.cancel(); view.contentY = view.originY; }
        function test_steps_accumulate_and_reverse() {
            scroll.scrollBy(100, false);
            scroll.scrollBy(100, false);
            compare(scroll.destination, view.originY + 200);
            tryCompare(view, "contentY", view.originY + 200);
            scroll.scrollBy(-100, false);
            tryCompare(view, "contentY", view.originY + 100);
        }
        function test_clamps_to_bounds() {
            scroll.scrollBy(100000, false);
            tryCompare(view, "contentY", view.originY + view.contentHeight - view.height);
            scroll.scrollBy(-100000, false);
            tryCompare(view, "contentY", view.originY);
        }
        function test_direct_input_cancels_animation() {
            scroll.scrollBy(200, false);
            scroll.cancel();
            view.contentY = view.originY + 37;
            wait(200);
            compare(view.contentY, view.originY + 37);
            scroll.scrollBy(13, true);
            compare(view.contentY, view.originY + 50);
        }
        function test_wheel_reaches_destination() {
            mouseWheel(scroll, 80, 80, 0, -120);
            tryCompare(view, "contentY", view.originY + 100);
        }
    }
}
