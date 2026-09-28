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
        anchors.fill: parent; radius: 0
        color: {
            if (root.checked) return "#414447";
            if (root.variant === "primary") return mouseArea.pressed ? "#55595D" : (mouseArea.containsMouse ? "#CDD0D3" : "#B8BDC2");
            if (root.variant === "danger") return mouseArea.pressed ? "#8E1F22" : (mouseArea.containsMouse ? "#DA3633" : "#B62324");
            return mouseArea.pressed ? "#353535" : (mouseArea.containsMouse ? "#303030" : "#2B2D2F");
        }
        border.width: 1
        border.color: root.checked ? "#B8BDC2" : (root.variant === "primary" ? "#CDD0D3" : (root.variant === "danger" ? "#DA3633" : (mouseArea.containsMouse ? "#5A5A5A" : "#3A3A3A")))
    }
    Text { id: label; anchors.centerIn: parent; anchors.verticalCenterOffset: root.active ? 1 : 0; text: root.text; color: root.enabled ? (root.variant === "primary" ? "#17191B" : "#F0F0F0") : "#858585"; font.weight: Font.DemiBold; font.pixelSize: 12 }
    MouseArea { id: mouseArea; anchors.fill: parent; enabled: root.enabled; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.clicked() }
}
