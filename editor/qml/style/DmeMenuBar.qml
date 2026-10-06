import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../themes/fluent/Colors.js" as FluentColors

MenuBar {
    id: root
    readonly property bool fluentTheme: Backend.uiTheme.style === "fluent-dark"
    readonly property bool windowsClassic: Backend.uiTheme.style === "windows-classic"
    readonly property bool modernTheme: Backend.uiTheme.style !== "classic" && !windowsClassic
    readonly property bool grayTheme: Backend.uiTheme.style === "gray-dark"
                                      || Backend.uiTheme.style === "gray-modern"
    implicitHeight: fluentTheme ? 32 : modernTheme ? 40 : 26
    leftPadding: 0
    rightPadding: 0
    spacing: 0
    background: Item {}
    delegate: MenuBarItem {
        id: menuItem
        focusPolicy: Qt.NoFocus
        width: root.fluentTheme ? label.implicitWidth + 28 : root.modernTheme ? Math.max(56, label.implicitWidth + 24) : Math.max(44, label.implicitWidth + 16)
        implicitHeight: root.fluentTheme ? 32 : modernTheme ? 40 : 26
        contentItem: Text {
            id: label
            text: menuItem.text
            color: root.fluentTheme ? FluentColors.c("buttonText") : root.windowsClassic ? "#202020" : (root.modernTheme ? (menuItem.highlighted ? "#FFFFFF" : (root.grayTheme ? "#E0E0E0" : "#C9D1D9")) : "#dcdcdc")
            font.family: root.fluentTheme ? "Segoe UI" : Qt.application.font.family
            font.pixelSize: root.fluentTheme ? 13 : root.modernTheme ? 13 : 12
            font.bold: false
            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: root.fluentTheme ? 4 : root.modernTheme ? 4 : 0
            color: root.fluentTheme ? (menuItem.highlighted ? FluentColors.c("selected") : "transparent") : menuItem.highlighted ? (root.windowsClassic ? "#cce8ff" : (root.modernTheme ? (root.grayTheme ? "#292929" : "#161B22") : "#1fffffff")) : "transparent"
            border.width: root.windowsClassic && menuItem.highlighted ? 1 : 0
            border.color: "#99c9ef"
        }
    }
}
