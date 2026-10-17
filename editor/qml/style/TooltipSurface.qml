import QtQuick
import Tibia 1.0
import "../themes/fluent/Colors.js" as Colors

Rectangle {
    readonly property color accentColor: Colors.c("accent")
    readonly property color textColor: Backend.uiTheme.style === "fluent-dark"
        ? Colors.c("text") : (Backend.uiTheme.style === "gray-dark" || Backend.uiTheme.style === "gray-modern")
        ? "#F0F0F0" : "#E6EDF3"
    readonly property int contentLeftPadding: 21
    readonly property int contentRightPadding: 10
    readonly property int contentVerticalPadding: 6
    radius: 4
    color: "#e6191d1f"
    border.width: 1
    border.color: accentColor
    Rectangle {
        x: 9
        anchors.verticalCenter: parent.verticalCenter
        width: 6; height: 6; radius: 3
        color: parent.accentColor
    }
}
