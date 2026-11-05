import QtQuick
import "../themes/tibia" as TibiaSkin
import QtQuick.Controls
import Tibia 1.0
import "../themes/fluent/Colors.js" as Colors

Item {
    id: root

    property bool active: false
    property bool round: false
    property int iconSize: 14
    property alias hovered: mouseArea.containsMouse
    property alias hoverArea: mouseArea

    signal clicked

    width: 26
    height: 26


    Rectangle {
        anchors.fill: parent; visible: true
        radius: Backend.uiTheme.style === "tibia-ui" ? 0 : 4; color: root.active ? (mouseArea.containsMouse ? Colors.c("selectedHover") : Colors.c("selected")) : mouseArea.containsMouse ? Colors.c("hover") : Colors.c("button")
        border.color: root.active ? Colors.c("selectedBorder") : Colors.c("border")
        border.width: 1
    }


    TibiaSkin.TibiaSkin { anchors.fill: parent; kind: "button"; selected: root.active; hovered: mouseArea.containsMouse; pressed: mouseArea.pressed }
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(root.iconSize, root.width - 6)
        height: Math.min(root.iconSize, root.height - 6)
        radius: root.round ? width / 2 : 0
        color: ((root.round ? "transparent" : Colors.c("text")))
        border.color: (Colors.c("text"))
        border.width: 1
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
    ColorHighlight { targetItem: root; colorKeys: ["button", "hover", "border", "text", "selected", "selectedHover", "selectedBorder"] }
}
