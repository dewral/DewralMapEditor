import QtQuick
import QtQuick.Controls
import Tibia 1.0

MenuBar {
    id: root
    readonly property bool modernTheme: Backend.uiTheme.style !== "classic"
    readonly property bool grayTheme: Backend.uiTheme.style === "gray-dark"
    implicitHeight: modernTheme ? 40 : 26
    leftPadding: 0
    rightPadding: 0
    spacing: 0
    background: Item {}
    delegate: MenuBarItem {
        id: menuItem
        focusPolicy: Qt.NoFocus
        width: root.modernTheme ? Math.max(56, label.implicitWidth + 24) : Math.max(44, label.implicitWidth + 16)
        implicitHeight: root.modernTheme ? 40 : 26
        contentItem: Text {
            id: label
            text: menuItem.text
            color: root.modernTheme ? (menuItem.highlighted ? "#FFFFFF" : (root.grayTheme ? "#E0E0E0" : "#C9D1D9")) : "#dcdcdc"
            font.pixelSize: root.modernTheme ? 13 : 12
            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: root.modernTheme ? 4 : 0
            color: menuItem.highlighted ? (root.modernTheme ? (root.grayTheme ? "#292929" : "#161B22") : "#1fffffff") : "transparent"
        }
    }
}
