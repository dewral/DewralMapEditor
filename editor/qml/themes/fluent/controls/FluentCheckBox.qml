import "../../../components"
import "../Colors.js" as Colors
import QtQuick
Item {
    id: root; signal clicked; property bool checked: false; property string text: ""
    implicitWidth: box.width + (text.length > 0 ? label.implicitWidth + 8 : 0); implicitHeight: Math.max(box.height, label.implicitHeight)
    Item { id: box; width: 14; height: 14; anchors.verticalCenter: parent.verticalCenter
        Rectangle { anchors.fill: parent; radius: 2; color: root.checked ? Colors.c("accent") : Colors.c("base"); border.width: 1; border.color: root.checked ? Colors.c("selectedBorder") : "#737d84" }
        Text { anchors.centerIn: parent; visible: root.checked; text: "\u2713"; color: "#FFFFFF"; font.pixelSize: 11; font.weight: Font.DemiBold }
    }
    Text { id: label; anchors.left: box.right; anchors.leftMargin: 8; anchors.verticalCenter: parent.verticalCenter; text: root.text; color: Colors.c("text"); font.family: "Segoe UI"; font.pixelSize: 12 }
    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.clicked() }
    ColorHighlight { targetItem: root; colorKeys: ["accent", "base", "selectedBorder", "text"] }
}
