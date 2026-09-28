import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../themes/rme/Colors.js" as RmeColors

MenuBar {
    id: root
    readonly property bool rmeTheme: Backend.uiTheme.style === "rme-fluent"
    readonly property bool windowsClassic: Backend.uiTheme.style === "windows-classic"
    readonly property bool modernTheme: Backend.uiTheme.style !== "classic" && !windowsClassic
    readonly property bool grayTheme: Backend.uiTheme.style === "gray-dark"
                                      || Backend.uiTheme.style === "gray-modern"
    implicitHeight: rmeTheme ? 27 : modernTheme ? 40 : 26
    leftPadding: 0
    rightPadding: 0
    spacing: 0
    background: Item {}
    delegate: MenuBarItem {
        id: menuItem
        focusPolicy: Qt.NoFocus
        width: root.rmeTheme ? label.implicitWidth + 20 : root.modernTheme ? Math.max(56, label.implicitWidth + 24) : Math.max(44, label.implicitWidth + 16)
        implicitHeight: root.rmeTheme ? 27 : modernTheme ? 40 : 26
        contentItem: Text {
            id: label
            text: menuItem.text
            color: root.rmeTheme ? RmeColors.text : root.windowsClassic ? "#202020" : (root.modernTheme ? (menuItem.highlighted ? "#FFFFFF" : (root.grayTheme ? "#E0E0E0" : "#C9D1D9")) : "#dcdcdc")
            font.pixelSize: root.rmeTheme ? 12 : root.modernTheme ? 13 : 12
            font.bold: root.rmeTheme
            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: root.rmeTheme ? 0 : root.modernTheme ? 4 : 0
            color: root.rmeTheme ? (menuItem.highlighted ? RmeColors.hover : "transparent") : menuItem.highlighted ? (root.windowsClassic ? "#cce8ff" : (root.modernTheme ? (root.grayTheme ? "#292929" : "#161B22") : "#1fffffff")) : "transparent"
            border.width: root.windowsClassic && menuItem.highlighted ? 1 : 0
            border.color: "#99c9ef"
        }
    }
}
