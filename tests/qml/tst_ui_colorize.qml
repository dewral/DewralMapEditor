import QtQuick
import QtTest
import Tibia 1.0
import "../../editor/qml/themes/fluent/Colors.js" as Colors
import "../../editor/qml/components" as Components
import "../../editor/qml/dialogs" as Dialogs

Item {
    id: testRoot
    width: 900; height: 800
    Rectangle { id: sample; width: 80; height: 40; color: Colors.c("selected") }
    Components.ColorHighlight { id: highlight; targetItem: sample; colorKeys: ["selected"] }
    Component { id: dialogComponent; Dialogs.UiColorizeDialog {} }
    TestCase {
        name: "UiColorize"
        when: windowShown
        function init() { Backend.uiTheme.style = "fluent-dark"; Backend.uiTheme.resetUiColors(); Backend.uiTheme.highlightedColor = ""; }
        function cleanup() { Backend.uiTheme.resetUiColors(); Backend.uiTheme.highlightedColor = ""; }
        function test_liveColorsAndInheritance() {
            verify(Backend.uiTheme.setUiColor("selected", "#355066"));
            tryCompare(sample, "color", "#355066");
            compare(Colors.c("lightingOn"), "#4a9ec7");
            Backend.uiTheme.setUiColor("accent", "#287CBD");
            compare(Colors.c("lightingOn"), "#287CBD");
            compare(Colors.c("selectedBorder"), "#287CBD");
            compare(Colors.c("selectedCell"), "#355066");
            Backend.uiTheme.setUiColor("lightingOn", "#123456");
            compare(Colors.c("lightingOn"), "#123456");
            Backend.uiTheme.resetUiColor("lightingOn");
            compare(Colors.c("lightingOn"), "#287CBD");
            Backend.uiTheme.resetUiColors();
            tryCompare(sample, "color", "#404040");
        }
        function test_highlightStopsAndDoesNotChangeColor() {
            Backend.uiTheme.highlightedColor = "selected";
            tryCompare(highlight, "visible", true);
            compare(sample.color, "#404040");
            wait(1600);
            compare(highlight.visible, false);
        }
        function test_colorizeDialogOpens() {
            const dialog = createTemporaryObject(dialogComponent, testRoot);
            verify(dialog !== null);
            dialog.open();
            tryCompare(dialog, "visible", true);
            compare(dialog.modal, false);
            const hex = findChild(dialog, "uiColorHexField");
            hex.text = "#123456";
            dialog.selectedKey = "lightingOff";
            compare(hex.text, "#383838", "Changing roles must discard the previous unfinished HEX edit");
            Backend.uiTheme.setUiColor("lightingOff", "#456789");
            compare(hex.text, "#456789", "Saved edits must refresh the displayed HEX");
            Backend.uiTheme.resetUiColor("lightingOff");
            compare(hex.text, "#383838");
            dialog.close();
        }
    }
}
