import QtQuick
import QtQuick.Controls
import Tibia 1.0

MenuBar {
    id: root
    implicitHeight: Backend.uiTheme.style === "github-dark" ? 40 : 26
    leftPadding: 0
    rightPadding: 0
    spacing: 0
    background: Item {}
    delegate: MenuBarItem {
        id: menuItem
        focusPolicy: Qt.NoFocus
        width: Backend.uiTheme.style === "github-dark" ? Math.max(56, label.implicitWidth + 24) : Math.max(44, label.implicitWidth + 16)
        implicitHeight: Backend.uiTheme.style === "github-dark" ? 40 : 26
        contentItem: Text {
            id: label
            text: menuItem.text
            color: Backend.uiTheme.style === "github-dark" ? (menuItem.highlighted ? "#FFFFFF" : "#C9D1D9") : "#dcdcdc"
            font.pixelSize: Backend.uiTheme.style === "github-dark" ? 13 : 12
            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: Backend.uiTheme.style === "github-dark" ? 4 : 0
            color: menuItem.highlighted ? (Backend.uiTheme.style === "github-dark" ? "#161B22" : "#1fffffff") : "transparent"
        }
    }
}
