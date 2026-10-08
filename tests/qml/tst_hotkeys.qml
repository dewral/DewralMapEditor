import QtQuick
import QtQuick.Controls
import QtTest
import Tibia 1.0
import "../../editor/qml/components" as Components
import "../../editor/qml/controllers" as Controllers
import "../../editor/qml/dialogs" as Dialogs

Item {
    id: root
    width: 900
    height: 650
    property int activations: 0

    QtObject {
        id: testHotkeys
        property bool capturing: false
        property var bindings: ({go_to_position: "Ctrl+G", save: "Ctrl+S", move_left: "Left"})
        readonly property var commands: [
            {id: "go_to_position", label: "Go to Position", category: "Edit", shortcut: bindings.go_to_position,
             defaultShortcut: "Ctrl+G", editable: true},
            {id: "save", label: "Save", category: "File", shortcut: bindings.save,
             defaultShortcut: "Ctrl+S", editable: true},
            {id: "move_left", label: "Pan left", category: "Fixed controls", shortcut: bindings.move_left,
             defaultShortcut: "Left", editable: false}
        ]
        function shortcutFromKey(key, modifiers) {
            if (key < Qt.Key_A || key > Qt.Key_Z) return "";
            return (modifiers & Qt.ControlModifier ? "Ctrl+" : "")
                + (modifiers & Qt.AltModifier ? "Alt+" : "") + String.fromCharCode(key);
        }
        function setShortcut(id, shortcut) {
            for (const other of Object.keys(bindings))
                if (other !== id && shortcut.length && bindings[other] === shortcut)
                    return shortcut + " is already used by " + other;
            const updated = Object.assign({}, bindings);
            updated[id] = shortcut;
            bindings = updated;
            return "";
        }
        function resetShortcut(id) { return setShortcut(id, id === "save" ? "Ctrl+S" : "Ctrl+G"); }
        function resetAll() { bindings = ({go_to_position: "Ctrl+G", save: "Ctrl+S", move_left: "Left"}); }
    }

    Components.HotkeysPage { id: page; anchors.fill: parent; hotkeys: testHotkeys }
    Controllers.AppSettings {
        id: preferences
        location: Qt.resolvedUrl("../../build/tmp/hotkey-preferences-" + Date.now() + ".ini")
    }
    QtObject {
        id: map
        property int maxFps: 60
        property int visibleZoneMask: 29
        property var zoneOpacities: [0.25, 0.25, 0.25, 0.25]
        property bool showZonesAlways: true
        property bool showHouses: true
        property bool showSpawns: true
        property bool showCreatures: true
        property bool showGrid: false
        property double houseOpacity: 0.25
        property double tilesOpacity: 1.0
        property double itemsOpacity: 1.0
    }
    Component {
        id: preferencesComponent
        Dialogs.PreferencesDialog { settings: preferences; mapView: map; mapRenderer: map }
    }
    Action {
        shortcut: testHotkeys.capturing ? "" : testHotkeys.bindings.go_to_position
        onTriggered: root.activations++
    }

    TestCase {
        name: "Hotkeys"
        when: windowShown
        function init() {
            testHotkeys.resetAll();
            page.feedback = "";
            findChild(page, "hotkeySearch").text = "Go to Position";
            root.activations = 0;
            wait(0);
        }
        function openRecorder() {
            const button = findChild(page, "changeHotkey_go_to_position");
            verify(button !== null);
            mouseClick(button);
            const dialog = findChild(page, "hotkeyCaptureDialog");
            tryCompare(dialog, "opened", true);
            compare(testHotkeys.capturing, true);
            return dialog;
        }
        function applyRecorder(dialog) {
            mouseClick(findChild(page, "applyHotkey"));
            tryCompare(dialog, "opened", false);
            tryCompare(testHotkeys, "capturing", false);
        }
        function visualChild(item, name) {
            if (item.objectName === name) return item;
            for (const child of item.children || []) {
                const match = visualChild(child, name);
                if (match) return match;
            }
            return null;
        }
        function test_captureRebindsCommandAndDisablesFormerShortcut() {
            keyClick(Qt.Key_G, Qt.ControlModifier);
            compare(root.activations, 1);
            const dialog = openRecorder();
            keyClick(Qt.Key_G, Qt.ControlModifier | Qt.AltModifier);
            compare(dialog.candidate, "Ctrl+Alt+G");
            compare(root.activations, 1);
            applyRecorder(dialog);
            compare(testHotkeys.bindings.go_to_position, "Ctrl+Alt+G");
            keyClick(Qt.Key_G, Qt.ControlModifier);
            compare(root.activations, 1);
            keyClick(Qt.Key_G, Qt.ControlModifier | Qt.AltModifier);
            compare(root.activations, 2);
        }
        function test_conflictAndEscapeKeepOriginalBinding() {
            const dialog = openRecorder();
            keyClick(Qt.Key_S, Qt.ControlModifier);
            mouseClick(findChild(page, "applyHotkey"));
            verify(dialog.error.includes("already used"));
            compare(dialog.opened, true);
            compare(testHotkeys.bindings.go_to_position, "Ctrl+G");
            findChild(page, "hotkeyCaptureTarget").forceActiveFocus();
            keyClick(Qt.Key_Escape);
            tryCompare(dialog, "opened", false);
            tryCompare(testHotkeys, "capturing", false);
        }
        function test_clearResetAndSearch() {
            const dialog = openRecorder();
            mouseClick(findChild(page, "clearHotkey"));
            applyRecorder(dialog);
            compare(testHotkeys.bindings.go_to_position, "");
            mouseClick(findChild(page, "resetHotkey_go_to_position"));
            compare(testHotkeys.bindings.go_to_position, "Ctrl+G");
            findChild(page, "hotkeySearch").text = "Ctrl+S";
            tryCompare(findChild(page, "hotkeyList"), "count", 1);
            findChild(page, "hotkeySearch").text = "Fixed controls";
            tryCompare(findChild(page, "hotkeyList"), "count", 1);
            verify(!findChild(page, "changeHotkey_move_left").enabled);
            findChild(page, "hotkeySearch").text = "nonexistent";
            tryCompare(findChild(page, "hotkeyList"), "count", 0);
        }
        function test_restoreAllDefaults() {
            testHotkeys.setShortcut("go_to_position", "Ctrl+Alt+G");
            testHotkeys.setShortcut("save", "Ctrl+Alt+S");
            mouseClick(findChild(page, "resetAllHotkeys"));
            compare(testHotkeys.bindings.go_to_position, "Ctrl+G");
            compare(testHotkeys.bindings.save, "Ctrl+S");
        }
        function test_preferencesHasHotkeysTab() {
            Backend.hotkeys = testHotkeys;
            const dialog = createTemporaryObject(preferencesComponent, root);
            verify(dialog !== null);
            dialog.open();
            tryCompare(dialog, "opened", true);
            const tab = visualChild(dialog.contentItem, "preferencesTabHotkeys");
            verify(tab !== null);
            mouseClick(tab);
            compare(dialog.page, 5);
            const hotkeysPage = findChild(dialog, "preferencesHotkeys");
            verify(hotkeysPage.visible);
            compare(findChild(hotkeysPage, "hotkeyList").count, 3);
            waitForRendering(dialog.contentItem);
            verify(tab.y + tab.height <= tab.parent.height + 1);
            verify(hotkeysPage.y + hotkeysPage.height <= dialog.contentItem.height + 1);
            verify(hotkeysPage.x + hotkeysPage.width <= dialog.contentItem.width + 1);
            grabImage(root).save("build/tmp/hotkey-preferences-preview.png");
            dialog.close();
        }
    }
}
