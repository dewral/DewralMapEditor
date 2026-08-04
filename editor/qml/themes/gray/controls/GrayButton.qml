import QtQuick

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
        anchors.fill: parent; radius: 6
        color: {
            if (root.checked) return "#4A3A1F";
            if (root.variant === "primary") return mouseArea.pressed ? "#A87928" : (mouseArea.containsMouse ? "#DDB24F" : "#C79A3B");
            if (root.variant === "danger") return mouseArea.pressed ? "#8E1F22" : (mouseArea.containsMouse ? "#DA3633" : "#B62324");
            return mouseArea.pressed ? "#353535" : (mouseArea.containsMouse ? "#303030" : "#262626");
        }
        border.width: 1
        border.color: root.checked ? "#D8AA48" : (root.variant === "primary" ? "#E3BC62" : (root.variant === "danger" ? "#DA3633" : (mouseArea.containsMouse ? "#5A5A5A" : "#3A3A3A")))
    }
    Text { id: label; anchors.centerIn: parent; anchors.verticalCenterOffset: root.active ? 1 : 0; text: root.text; color: root.enabled ? "#F0F0F0" : "#858585"; font.weight: Font.DemiBold; font.pixelSize: 12 }
    MouseArea { id: mouseArea; anchors.fill: parent; enabled: root.enabled; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.clicked() }
}
