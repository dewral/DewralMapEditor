import QtQuick
import QtTest
import Tibia 1.0
import "../../editor/qml/dialogs" as Dialogs

Item {
    width: 700
    height: 800

    QtObject {
        id: controller
        property var received: ({})
        function applyContextItemProperties(properties) { received = properties; }
    }

    Dialogs.ItemPropertiesDialog {
        id: dialog
        mapCtrl: controller
        ctx: ({hasItem: true, serverId: 1387, clientId: 100, name: "magic forcefield",
               x: 100, y: 100, z: 7, teleport: true,
               teleportX: 100, teleportY: 200, teleportZ: 7})
    }

    // The mock supplies the paste hook; TextEdit supplies the native fallback.
    TextEdit { id: clipboardSource; visible: false }

    TestCase {
        name: "ItemPropertiesTeleport"
        when: windowShown
        property string previousTheme: ""

        function field(axis) { return findChild(dialog, "teleport" + axis + "Field"); }
        function values() { return [field("X").value, field("Y").value, field("Z").value]; }
        function setClipboard(text) {
            Backend.fileTools.setClipboard(text);
            clipboardSource.text = text;
            clipboardSource.selectAll();
            clipboardSource.copy();
        }
        function openDialog(theme) {
            Backend.uiTheme.style = theme;
            dialog.open();
            tryCompare(dialog, "opened", true);
        }
        function init() {
            previousTheme = Backend.uiTheme.style;
            controller.received = {};
        }
        function cleanup() {
            dialog.close();
            tryCompare(dialog, "opened", false);
            Backend.uiTheme.style = previousTheme;
        }

        function test_pastePosition_data() {
            const rows = [];
            for (const theme of ["fluent-dark", "tibia-ui"]) {
                rows.push({tag: theme + "-example", theme: theme,
                           text: "32004, 31929, 15", expected: [32004, 31929, 15]});
                rows.push({tag: theme + "-bounds", theme: theme,
                           text: " 65535,0, 0\n", expected: [65535, 0, 0]});
            }
            return rows;
        }
        function test_pastePosition(data) {
            openDialog(data.theme);
            // Prior edits must not break updates to any of the three editors.
            for (const axis of ["X", "Y", "Z"]) {
                field(axis).focusEditor();
                keyClick(Qt.Key_1);
            }
            field("X").focusEditor();
            setClipboard(data.text);
            keyClick(Qt.Key_V, Qt.ControlModifier);
            compare(values(), data.expected);
            compare(dialog.contentItem.Window.window.activeFocusItem.text, String(data.expected[0]));
            const axes = ["X", "Y", "Z"];
            for (let i = 0; i < axes.length; ++i) {
                field(axes[i]).focusEditor();
                compare(dialog.contentItem.Window.window.activeFocusItem.text, String(data.expected[i]));
            }
            dialog.applyAndClose();
            compare([controller.received.teleportX, controller.received.teleportY,
                     controller.received.teleportZ], data.expected);
        }

        function test_invalidPosition() {
            openDialog("fluent-dark");
            for (const text of ["", "32004, 31929", "32004, 31929, 16",
                                "65536, 31929, 15", "32004, 65536, 15",
                                "-1, 31929, 15", "32004.5, 31929, 15",
                                "32004, 31929, 15, 1", "text 32004, 31929, 15"]) {
                Backend.fileTools.setClipboard(text);
                compare(dialog.pasteTeleportPosition(), false);
                compare(values(), [100, 200, 7]);
            }
        }

        function test_singleCoordinatePaste_data() {
            return [{tag: "fluent", theme: "fluent-dark"}, {tag: "tibia", theme: "tibia-ui"}];
        }
        function test_singleCoordinatePaste(data) {
            openDialog(data.theme);
            field("X").focusEditor();
            setClipboard("32004");
            keyClick(Qt.Key_V, Qt.ControlModifier);
            compare(values(), [32004, 200, 7]);
            keyClick(Qt.Key_Escape);
            tryCompare(dialog, "opened", false);
            compare(controller.received, {});
        }
    }
}
