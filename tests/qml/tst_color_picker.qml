import QtQuick
import QtTest
import "../../editor/qml/style" as Style
Item {
    id: root
    width: 800; height: 720
    Component { id: pickerComponent; Style.DmeColorPicker {} }
    TestCase {
        name: "ColorPicker"
        when: windowShown
        function test_hex_and_reopen() {
            const picker = createTemporaryObject(pickerComponent, root);
            verify(picker !== null);
            picker.selectedColor = "#48b883";
            picker.open();
            tryCompare(picker, "visible", true);
            const hex = findChild(picker, "colorHexField");
            compare(hex.text, "#48B883");
            picker.choose("#ff8800");
            compare(hex.text, "#FF8800");
            compare(picker.selectedColor, "#ff8800");
            hex.text = "#12";
            compare(findChild(picker, "colorApplyButton").enabled, false);
            picker.reject();
            picker.selectedColor = "#9173be";
            picker.open();
            compare(hex.text, "#9173BE");
            verify(findChild(picker, "colorApplyButton").enabled);
            verify(findChild(picker, "colorSaturationField").height > 0);
            picker.close();
        }
    }
}
