import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../themes/fluent/Colors.js" as FluentColors

Menu {
    id: root
    readonly property bool fluentTheme: Backend.uiTheme.style === "fluent-dark"
    readonly property bool grayTheme: Backend.uiTheme.style === "gray-dark"
                                      || Backend.uiTheme.style === "gray-modern"
    readonly property bool classicLayout: Backend.uiTheme.style === "classic"
                                          || Backend.uiTheme.style === "windows-classic"
    implicitWidth: Math.max(160, implicitContentWidth + leftPadding + rightPadding)
    padding: root.fluentTheme ? 4 : 1
    palette.mid: root.fluentTheme ? FluentColors.separator : "#808080"
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
        Rectangle { implicitWidth: 150; radius: root.fluentTheme ? 0 : 6; color: root.fluentTheme ? FluentColors.popup : root.grayTheme ? "#202020" : "#10151C"; border.width: 1; border.color: root.fluentTheme ? FluentColors.border : root.grayTheme ? "#424242" : "#2D3743" }
    }
    delegate: DmeMenuItem {}
}
