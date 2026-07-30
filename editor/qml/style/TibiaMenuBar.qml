import Tibia 1.0
import QtQuick
import QtQuick.Controls

MenuBar {
    id: root
    property int githubMenuCount: 7
    implicitHeight: Backend.uiTheme.style === "github-dark" ? 40 : 26
    leftPadding: Backend.uiTheme.style === "github-dark" ? 0 : 0
    rightPadding: Backend.uiTheme.style === "github-dark" ? 0 : 0
    background: Rectangle {
        radius: 0
        color: "transparent"
        border {
            width: 0
            color: "transparent"
        }
    }
    delegate: MenuBarItem {
        id: menuBarItem
        focusPolicy: Qt.NoFocus
        width: Backend.uiTheme.style === "github-dark"
               ? Math.max(48, (root.width - root.leftPadding - root.rightPadding) / root.githubMenuCount)
               : implicitWidth
        implicitHeight: Backend.uiTheme.style === "github-dark" ? 40 : 26
        contentItem: Text {
            anchors.fill: parent
            text: menuBarItem.text
            color: Backend.uiTheme.style === "github-dark"
                   ? (menuBarItem.highlighted ? "#FFFFFF" : "#C9D1D9")
                   : (menuBarItem.highlighted ? "#eaffea" : "#c0c0c0")
            font.pixelSize: Backend.uiTheme.style === "github-dark" ? 13 : 12
            font.bold: menuBarItem.highlighted
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: Backend.uiTheme.style === "github-dark" ? 4 : 0
            color: menuBarItem.highlighted
                   ? (Backend.uiTheme.style === "github-dark" ? "#161B22" : "#1fffffff")
                   : "transparent"
        }
    }
}
