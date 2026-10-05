import QtQuick
import QtTest
import Tibia 1.0
import "../../editor/qml/dialogs" as Dialogs

Item {
    width: 600
    height: 300

    QtObject {
        id: controller
        property int floor: 7
        property var destination: []
        function renderOriginX() { return 16746 }
        function renderOriginY() { return 16898 }
        function centerOnTile(x, y, z) { destination = [x, y, z] }
    }

    Dialogs.GoToPositionDialog { id: dialog; mapCtrl: controller }

    TestCase {
        name: "GoToPosition"
        when: windowShown
        property string previousTheme: ""

        function init() {
            previousTheme = Backend.uiTheme.style;
            Backend.fileTools.setClipboard("32796, 33213, 12");
            controller.destination = [];
        }

        function cleanup() {
            dialog.close();
            tryCompare(dialog, "opened", false);
            Backend.uiTheme.style = previousTheme;
        }

        function test_paste_data() {
            var rows = [];
            for (const theme of ["classic", "github", "gray-dark", "fluent-dark"]) {
                for (var field = 0; field < 3; ++field) {
                    rows.push({tag: theme + "-ctrl-v-" + field,
                               theme: theme, field: field, button: false});
                }
                rows.push({tag: theme + "-button", theme: theme, field: 0, button: true});
            }
            return rows;
        }

        function test_paste(data) {
            Backend.uiTheme.style = data.theme;
            dialog.open();
            tryCompare(dialog, "opened", true);
            var fields = dialog.contentItem.children[0].children;
            var buttons = dialog.contentItem.children[1].children;
            fields[data.field].focusEditor();
            // A previous edit must not stop the pasted value reaching the editor.
            keyClick(Qt.Key_1);
            if (data.button)
                mouseClick(buttons[0], buttons[0].width / 2, buttons[0].height / 2);
            else
                keyClick(Qt.Key_V, Qt.ControlModifier);

            var expected = [32796, 33213, 12];
            compare(dialog.contentItem.Window.window.activeFocusItem.text,
                    String(expected[data.button ? 0 : data.field]));
            for (var i = 0; i < 3; ++i) {
                compare(fields[i].value, expected[i]);
                fields[i].focusEditor();
                compare(dialog.contentItem.Window.window.activeFocusItem.text, String(expected[i]));
            }
            mouseClick(buttons[1], buttons[1].width / 2, buttons[1].height / 2);
            compare(controller.destination, expected);
        }

        function test_invalidClipboard() {
            dialog.open();
            tryCompare(dialog, "opened", true);
            var fields = dialog.contentItem.children[0].children;
            var original = [fields[0].value, fields[1].value, fields[2].value];
            for (const text of ["", "32796, 33213", "32796, 33213, 16", "-1, 33213, 12"]) {
                Backend.fileTools.setClipboard(text);
                compare(dialog.pastePosition(), false);
                compare([fields[0].value, fields[1].value, fields[2].value], original);
            }
        }
    }
}
