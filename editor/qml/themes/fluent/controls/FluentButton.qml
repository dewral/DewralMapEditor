import "../../../components"
import QtQuick
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
        anchors.fill: parent; radius: 4
        color: {
            if (root.checked) return mouseArea.containsMouse ? Colors.c("selectedHover") : Colors.c("selected");
            if (root.variant === "primary") return mouseArea.pressed ? Qt.darker(Colors.c("accent"), 1.2) : (mouseArea.containsMouse ? Qt.lighter(Colors.c("accent"), 1.15) : Colors.c("accent"));
            if (root.variant === "danger") return mouseArea.pressed ? "#8E1F22" : (mouseArea.containsMouse ? "#DA3633" : "#B62324");
            return mouseArea.pressed ? Colors.c("pressed") : (mouseArea.containsMouse ? Colors.c("hover") : Colors.c("button"));
        }
        border.width: 1
        border.color: root.checked ? Colors.c("selectedBorder") : (root.variant === "primary" ? Colors.c("selectedBorder") : (root.variant === "danger" ? "#DA3633" : (mouseArea.containsMouse ? "#737d84" : Colors.c("border"))))
    }
    Text { id: label; anchors.centerIn: parent; anchors.verticalCenterOffset: root.active ? 1 : 0; text: root.text; color: root.enabled ? (root.variant === "primary" ? "#101b23" : Colors.c("buttonText")) : Colors.c("placeholder"); font.weight: Font.Normal; font.family: "Segoe UI"; font.pixelSize: 12 }
    MouseArea { id: mouseArea; anchors.fill: parent; enabled: root.enabled; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.clicked() }
    ColorHighlight { targetItem: root; colorKeys: ["selectedHover", "selected", "selectedBorder", "accent", "pressed", "hover", "button", "border", "buttonText", "placeholder"] }
}
