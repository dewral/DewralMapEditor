import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../themes/rme/Colors.js" as RmeColors

Menu {
    id: root
    readonly property bool rmeTheme: Backend.uiTheme.style === "rme-fluent"
    readonly property bool grayTheme: Backend.uiTheme.style === "gray-dark"
                                      || Backend.uiTheme.style === "gray-modern"
    readonly property bool classicLayout: Backend.uiTheme.style === "classic"
                                          || Backend.uiTheme.style === "windows-classic"
    implicitWidth: Math.max(160, implicitContentWidth + leftPadding + rightPadding)
    padding: root.rmeTheme ? 4 : 1
    palette.mid: root.rmeTheme ? RmeColors.separator : "#808080"
    overlap: 0
    background: Loader {
        sourceComponent: root.classicLayout ? classicMenuBackground : githubMenuBackground
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
        Rectangle { implicitWidth: 150; radius: root.rmeTheme ? 0 : 6; color: root.rmeTheme ? RmeColors.popup : root.grayTheme ? "#202020" : "#10151C"; border.width: 1; border.color: root.rmeTheme ? RmeColors.border : root.grayTheme ? "#424242" : "#2D3743" }
    }
    delegate: DmeMenuItem {}
}
