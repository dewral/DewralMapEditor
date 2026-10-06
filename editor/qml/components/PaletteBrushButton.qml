import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../themes/fluent/Colors.js" as Colors

Item {
    id: root
    readonly property bool fluentUi: Backend.uiTheme.style === "fluent-dark"
    readonly property bool grayTheme: Backend.uiTheme.style === "gray-dark"
                                      || Backend.uiTheme.style === "gray-modern"

    property bool active: false
    property bool round: false
    property bool githubStyle: false
    property int iconSize: 14
    property alias hovered: mouseArea.containsMouse
    property alias hoverArea: mouseArea

    signal clicked

    width: githubStyle ? 32 : 26
    height: githubStyle ? 32 : 26

    BorderImage {
        anchors.fill: parent
        visible: !root.githubStyle && !root.fluentUi
        source: Backend.uiTheme.tex + "panel_side.png"
        smooth: false
        border.left: 1
        border.right: 1
        border.top: 1
        border.bottom: 1
        horizontalTileMode: BorderImage.Repeat
        verticalTileMode: BorderImage.Repeat
    }

    Rectangle {
        anchors.fill: parent
        visible: !root.githubStyle && !root.fluentUi
        color: root.active ? (root.fluentUi ? "#335F6265" : "#992f6f4f")
                           : (mouseArea.containsMouse ? "#28ffffff" : "transparent")
        border.width: root.active ? 1 : 0
        border.color: root.fluentUi ? "#B8BDC2" : "#7fdc8f"
    }
    Rectangle {
        anchors.fill: parent; visible: root.fluentUi
        radius: 4; color: root.active ? (mouseArea.containsMouse ? Colors.c("selectedHover") : Colors.c("selected")) : mouseArea.containsMouse ? Colors.c("hover") : Colors.c("button")
        border.color: root.active ? Colors.c("selectedBorder") : Colors.c("border")
        border.width: 1
    }

    Rectangle {
        anchors.fill: parent
        visible: root.githubStyle
        radius: 5
        color: mouseArea.containsMouse ? (root.grayTheme ? "#303030" : "#171E27") : (root.grayTheme ? "#242424" : "#111820")
        border.width: root.active ? 2 : 1
        border.color: root.active ? (root.fluentUi ? "#CDD0D3" : root.grayTheme ? "#C79A3B" : "#2EA043") : (root.grayTheme ? "#484848" : "#242D38")
    }

    Rectangle {
        anchors.centerIn: parent
        width: root.iconSize
        height: root.iconSize
        radius: root.round ? width / 2 : 0
        color: root.fluentUi ? (root.round ? "transparent" : Colors.c("text")) : root.active ? (root.grayTheme ? "#C79A3B" : "#3FB950") : (root.grayTheme ? "#858585" : "#7D8590")
        border.color: root.fluentUi ? Colors.c("text") : root.active ? (root.grayTheme ? "#F0CD78" : "#7EE787") : (root.grayTheme ? "#B0B0B0" : "#A7B1BC")
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
