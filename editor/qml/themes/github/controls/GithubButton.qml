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
        anchors.fill: parent
        radius: 6
        color: {
            if (root.checked)
                return "#174D2B";
            if (root.variant === "primary")
                return mouseArea.pressed ? "#1F6F35" : (mouseArea.containsMouse ? "#2EA043" : "#238636");
            if (root.variant === "danger")
                return mouseArea.pressed ? "#8E1F22" : (mouseArea.containsMouse ? "#DA3633" : "#B62324");
            return mouseArea.pressed ? "#30363D" : (mouseArea.containsMouse ? "#252C35" : "#21262D");
        }
        border.width: 1
        border.color: {
            if (root.checked)
                return "#3FB950";
            if (root.variant === "primary")
                return mouseArea.containsMouse ? "#56D364" : "#2EA043";
            if (root.variant === "danger")
                return mouseArea.containsMouse ? "#F85149" : "#DA3633";
            return mouseArea.containsMouse ? "#8B949E" : "#30363D";
        }
    }

    Text {
        id: label
        anchors.centerIn: parent
        anchors.verticalCenterOffset: root.active ? 1 : 0
        text: root.text
        color: root.enabled ? "#F0F6FC" : "#7D8590"
        font.weight: Font.DemiBold
        font.pixelSize: 12
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        enabled: root.enabled
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
