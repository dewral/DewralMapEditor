import "../../../components"
import "../Colors.js" as Colors
import QtQuick
import QtQuick.Controls
Item {
    id: root; property var model: []; property int currentIndex: -1
    readonly property string currentText: currentIndex >= 0 && currentIndex < model.length ? model[currentIndex] : ""
    readonly property bool open: popup.visible; signal activated(int index); implicitWidth: 140; implicitHeight: 23
    Rectangle { anchors.fill: parent; radius: 4; color: Colors.c("base"); border.width: 1; border.color: root.open || mouse.containsMouse ? "#737d84" : Colors.c("border") }
    Text { anchors.left: parent.left; anchors.leftMargin: 6; anchors.right: arrow.left; anchors.verticalCenter: parent.verticalCenter; text: root.currentText; color: Colors.c("text"); font.family: "Segoe UI"; font.pixelSize: 12; elide: Text.ElideRight }
    Text { id: arrow; anchors.right: parent.right; anchors.rightMargin: 7; anchors.verticalCenter: parent.verticalCenter; text: "\u25BE"; color: Colors.c("placeholder"); font.pixelSize: 11 }
    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: popup.visible = !popup.visible }
    Popup { id: popup; y: root.height; width: root.width; height: Math.min(200, list.contentHeight + 2); padding: 1; closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape; modal: true; dim: false
        background: Rectangle { radius: 4; color: Colors.c("surface"); border.width: 1; border.color: Colors.c("border") }
        contentItem: ListView { id: list; model: root.model; clip: true
            delegate: Rectangle { width: list.width; height: 22; radius: 4; color: entry.containsMouse ? Colors.c("hover") : "transparent"
                Text { anchors.left: parent.left; anchors.leftMargin: 6; anchors.verticalCenter: parent.verticalCenter; text: modelData; color: Colors.c("text"); font.family: "Segoe UI"; font.pixelSize: 12 }
                MouseArea { id: entry; anchors.fill: parent; hoverEnabled: true; onClicked: { root.currentIndex = index; root.activated(index); popup.close(); } }
            }
        }
    }
    ColorHighlight { targetItem: root; colorKeys: ["base", "border", "text", "placeholder", "surface", "hover"] }
}
