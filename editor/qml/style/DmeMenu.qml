import QtQuick
import "../themes/tibia" as TibiaSkin
import QtQuick.Controls
import Tibia 1.0
import "../themes/fluent/Colors.js" as FluentColors

Menu {
    id: root
    readonly property bool tibiaTheme: Backend.uiTheme.style === "tibia-ui"
    readonly property bool fluentTheme: (Backend.uiTheme.style === "fluent-dark" || Backend.uiTheme.style === "tibia-ui")
    readonly property bool grayTheme: Backend.uiTheme.style === "gray-dark"
                                      || Backend.uiTheme.style === "gray-modern"
    readonly property bool classicLayout: Backend.uiTheme.style === "classic"
                                          || Backend.uiTheme.style === "windows-classic"
    readonly property real widestEntry: {
        if (!tibiaTheme) return 0;
        let widest = 0;
        for (let index = 0; index < count; ++index) {
            const entry = itemAt(index);
            if (entry && entry.visible) widest = Math.max(widest, entry.implicitWidth);
        }
        return widest;
    }
    implicitWidth: Math.max(160, implicitContentWidth + leftPadding + rightPadding,
                            widestEntry + leftPadding + rightPadding)
    padding: root.tibiaTheme ? 6 : root.fluentTheme ? 4 : 1
    spacing: 0
    margins: root.tibiaTheme ? 6 : 0
    palette.mid: root.fluentTheme ? FluentColors.c("separator") : "#808080"
    overlap: 0
    background: Loader {
        sourceComponent: root.tibiaTheme ? tibiaMenuBackground : root.classicLayout ? classicMenuBackground : githubMenuBackground
    }
    Component {
        id: tibiaMenuBackground
        BorderImage {
            source: Qt.resolvedUrl("../themes/tibia/images/3pixel-frame-borderimage.png")
            border { left: 3; top: 3; right: 3; bottom: 3 }
            horizontalTileMode: BorderImage.Repeat
            verticalTileMode: BorderImage.Repeat
            smooth: false
        }
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
        Rectangle { implicitWidth: 150; radius: root.fluentTheme ? 4 : 6; color: root.fluentTheme ? FluentColors.c("popup") : root.grayTheme ? "#202020" : "#10151C"; border.width: 1; border.color: root.fluentTheme ? FluentColors.c("border") : root.grayTheme ? "#424242" : "#2D3743"; TibiaSkin.TibiaSkin { anchors.fill: parent } }
    }
    delegate: DmeMenuItem {}
}
