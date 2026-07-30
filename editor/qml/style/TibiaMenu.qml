import Tibia 1.0
import QtQuick
import QtQuick.Controls

Menu {
    id: root

    implicitWidth: Math.max(160, implicitContentWidth + leftPadding + rightPadding)
    padding: 1
    overlap: 0

    background: Item {
        implicitWidth: 150
        Image {
            anchors.fill: parent
            visible: Backend.uiTheme.style !== "github-dark"
            source: (Backend.uiTheme.tex + "texture.png")
            fillMode: Image.Tile
            smooth: false
        }
        Rectangle {
            anchors.fill: parent
            radius: Backend.uiTheme.style === "github-dark" ? 6 : 0
            color: Backend.uiTheme.style === "github-dark" ? "#10151C" : "transparent"
            border.width: 1
            border.color: Backend.uiTheme.style === "github-dark" ? "#2D3743" : "#6e6e6e"
        }
    }

    delegate: MenuItem {
        id: menuItem
        implicitHeight: 24
        padding: 0
        spacing: 0

        contentItem: Item {
            implicitWidth: itemText.implicitWidth + (menuItem.checkable ? 30 : 10)
                           + (menuItem.subMenu !== null ? 22 : 10)
            implicitHeight: 24
            Text {
                id: itemText
                anchors.left: parent.left
                anchors.leftMargin: menuItem.checkable ? 30 : 10
                anchors.verticalCenter: parent.verticalCenter
                text: menuItem.text
                color: Backend.uiTheme.style === "github-dark"
                       ? (!menuItem.enabled ? "#6E7681" : (menuItem.highlighted ? "#FFFFFF" : "#C9D1D9"))
                       : (!menuItem.enabled ? "#777" : (menuItem.highlighted ? "#eaffea" : "#dcdcdc"))
                font.pixelSize: 12
                verticalAlignment: Text.AlignVCenter
            }
        }
        indicator: Text {
            visible: menuItem.checkable
            width: 22
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            text: menuItem.checked ? "\u2713" : ""
            horizontalAlignment: Text.AlignHCenter
            color: Backend.uiTheme.style === "github-dark"
                   ? (menuItem.highlighted ? "#FFFFFF" : "#3FB950")
                   : (menuItem.highlighted ? "#eaffea" : "#80c080")
            font.pixelSize: 13
            font.bold: true
        }
        arrow: Text {
            visible: menuItem.subMenu !== null
            text: ">"
            color: Backend.uiTheme.style === "github-dark"
                   ? (menuItem.highlighted ? "#FFFFFF" : "#8B949E")
                   : (menuItem.highlighted ? "#eaffea" : "#999")
            font.pixelSize: 10
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
        }
        background: Rectangle {
            radius: Backend.uiTheme.style === "github-dark" ? 4 : 0
            color: menuItem.highlighted
                   ? (Backend.uiTheme.style === "github-dark" ? "#1B2632" : "#807a7d82")
                   : "transparent"
            border.width: menuItem.highlighted ? 1 : 0
            border.color: Backend.uiTheme.style === "github-dark" ? "#3A4655" : "#9a9a9a"
        }
    }
}
