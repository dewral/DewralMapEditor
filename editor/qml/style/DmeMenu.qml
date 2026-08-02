import QtQuick
import QtQuick.Controls
import Tibia 1.0

Menu {
    id: root
    implicitWidth: Math.max(160, implicitContentWidth + leftPadding + rightPadding)
    padding: 1
    overlap: 0
    background: Loader {
        sourceComponent: Backend.uiTheme.style === "github-dark" ? githubMenuBackground : classicMenuBackground
    }
    Component {
        id: classicMenuBackground
        Item {
            implicitWidth: 150
            Image { anchors.fill: parent; source: Backend.uiTheme.tex + "texture.png"; fillMode: Image.Tile; smooth: false }
            Rectangle { anchors.fill: parent; color: "transparent"; border.width: 1; border.color: "#6e6e6e" }
        }
    }
    Component {
        id: githubMenuBackground
        Rectangle { implicitWidth: 150; radius: 6; color: "#10151C"; border.width: 1; border.color: "#2D3743" }
    }
    delegate: DmeMenuItem {}
}
