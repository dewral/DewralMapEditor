import "../../../components"
import QtQuick
import Tibia 1.0
import "../../tibia" as TibiaSkin
import "../Colors.js" as Colors

Item {
    id: root
    signal clicked
    property string text: ""
    property bool checked: false
    property string variant: "default"
    implicitWidth: Math.max(60, label.implicitWidth + 16)
    implicitHeight: 30
    opacity: enabled ? 1 : 0.5
    readonly property bool active: checked || mouseArea.pressed

    Rectangle {
        anchors.fill: parent; radius: Colors.isTibia() ? 0 : 4
        color: {
            if (root.checked) return mouseArea.containsMouse ? Colors.c("selectedHover") : Colors.c("selected");
            if (root.variant === "primary") return mouseArea.pressed ? Qt.darker(Colors.c("accent"), 1.2) : (mouseArea.containsMouse ? Qt.lighter(Colors.c("accent"), 1.15) : Colors.c("accent"));
            if (root.variant === "danger") return mouseArea.pressed ? "#8E1F22" : (mouseArea.containsMouse ? "#DA3633" : "#B62324");
            return mouseArea.pressed ? Colors.c("pressed") : (mouseArea.containsMouse ? Colors.c("hover") : Colors.c("button"));
        }
        border.width: 1
        border.color: root.checked ? Colors.c("selectedBorder") : (root.variant === "primary" ? Colors.c("selectedBorder") : (root.variant === "danger" ? "#DA3633" : (mouseArea.containsMouse ? "#737d84" : Colors.c("border"))))
    }
    TibiaSkin.TibiaSkin { anchors.fill: parent; kind: "button"; pressed: mouseArea.pressed; hovered: mouseArea.containsMouse; selected: root.checked }
    Text { id: label; anchors.centerIn: parent; anchors.verticalCenterOffset: root.active ? 1 : 0; text: root.text; color: root.enabled ? (root.variant === "primary" && Backend.uiTheme.style !== "tibia-ui" ? "#101b23" : Colors.c("buttonText")) : Colors.c("placeholder"); font.weight: Colors.bold() ? Font.Bold : Font.Normal; font.family: Colors.family(); font.pixelSize: Colors.size(12); font.bold: Colors.bold(); renderType: Text.NativeRendering }
    MouseArea { id: mouseArea; anchors.fill: parent; enabled: root.enabled; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.clicked() }
    ColorHighlight { targetItem: root; colorKeys: ["selectedHover", "selected", "selectedBorder", "accent", "pressed", "hover", "button", "border", "buttonText", "placeholder"] }
}
