import QtQuick
import QtTest
import "../../editor/qml/components" as Components
Item {
 width: 200; height: 60
 Components.OtclientName { id: label; text: "Dewral" }
 TestCase {
  name: "OtclientName"
  when: windowShown
  function test_bitmap_font() {
   tryVerify(function() { return label.isImageLoaded(label.atlas); });
   compare(label.height, 16);
   compare(label.width, label.glyphs.reduce((sum, code) => sum + label.widths[code] - 1, 1));
   compare(label.codes["D"], 68);
  }
  function test_polish_and_fallback() {
   label.text = "Łukasz";
   compare(label.glyphs[0], 163);
   label.text = "😀";
   compare(label.glyphs[0], 63);
  }
 }
}
