import QtQuick
import QtTest
import "../../editor/qml/components/PreviewMovement.js" as Movement

TestCase {
    name: "PreviewMovement"
    QtObject {
        id: hotkeys
        property bool capturing: false
        property var bindings: ({preview_left: [Qt.Key_A, 0], preview_right: [Qt.Key_D, 0],
                                 preview_up: [Qt.Key_W, 0], preview_down: [Qt.Key_S, 0]})
        function matches(id, key, modifiers) {
            const binding = bindings[id];
            return !capturing && binding && binding[0] === key && binding[1] === modifiers;
        }
    }
    function init() {
        hotkeys.capturing = false;
        hotkeys.bindings = ({preview_left: [Qt.Key_A, 0], preview_right: [Qt.Key_D, 0],
                             preview_up: [Qt.Key_W, 0], preview_down: [Qt.Key_S, 0]});
    }
    function test_heldDirectionsCombineAndRelease() {
        const held = {};
        verify(Movement.press(held, Qt.Key_W, 0, hotkeys, false));
        verify(Movement.press(held, Qt.Key_D, 0, hotkeys, false));
        compare(Movement.vector(held), {dx: 1, dy: -1});
        verify(Movement.release(held, Qt.Key_W, true));
        compare(Movement.vector(held), {dx: 1, dy: -1});
        verify(Movement.release(held, Qt.Key_W, false));
        compare(Movement.vector(held), {dx: 1, dy: 0});
        Movement.release(held, Qt.Key_D, false);
        compare(Movement.vector(held), {dx: 0, dy: 0});
    }
    function test_customCombinationReplacesDefaultAndReleasesPhysicalKey() {
        hotkeys.bindings = ({preview_left: [Qt.Key_G, Qt.ControlModifier]});
        const held = {};
        verify(!Movement.press(held, Qt.Key_A, 0, hotkeys, false));
        verify(!Movement.press(held, Qt.Key_G, 0, hotkeys, false));
        verify(Movement.press(held, Qt.Key_G, Qt.ControlModifier, hotkeys, false));
        compare(Movement.vector(held), {dx: -1, dy: 0});
        // The modifier may be released before G; release still clears this direction.
        verify(Movement.release(held, Qt.Key_G, false));
        compare(Movement.vector(held), {dx: 0, dy: 0});
    }
    function test_fixedArrowsAndDiagonalsRemainAvailable() {
        hotkeys.bindings = ({});
        const held = {};
        verify(Movement.press(held, Qt.Key_Left, Qt.KeypadModifier, hotkeys, false));
        compare(Movement.vector(held), {dx: -1, dy: 0});
        verify(Movement.press(held, Qt.Key_PageDown, 0, hotkeys, false));
        compare(Movement.vector(held), {dx: 1, dy: 1});
        Movement.release(held, Qt.Key_PageDown, false);
        compare(Movement.vector(held), {dx: -1, dy: 0});
        verify(!Movement.press(held, Qt.Key_Left, Qt.ControlModifier, hotkeys, false));
    }
    function test_recordingSuppressesAllMovement() {
        hotkeys.capturing = true;
        const held = {};
        verify(!Movement.press(held, Qt.Key_W, 0, hotkeys, false));
        verify(!Movement.press(held, Qt.Key_Left, 0, hotkeys, false));
        verify(!Movement.press(held, Qt.Key_Home, 0, hotkeys, false));
        compare(Movement.vector(held), {dx: 0, dy: 0});
    }
}
