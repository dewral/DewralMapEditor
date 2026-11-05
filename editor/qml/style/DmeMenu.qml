import QtQuick
import "../themes/tibia" as TibiaSkin
import QtQuick.Controls
import Tibia 1.0
import "../themes/fluent/Colors.js" as FluentColors

Menu {
    id: root
    readonly property bool tibiaTheme: Backend.uiTheme.style === "tibia-ui"
    readonly property bool pointerInMenuTree: {
        if (!visible)
            return false;
        if (menuPointer.hovered)
            return true;
        for (let index = 0; index < count; ++index) {
            const entry = itemAt(index);
            if (entry && entry.subMenu && entry.subMenu.pointerInMenuTree)
                return true;
        }
        return false;
    }
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
    padding: root.tibiaTheme ? 6 : 4
    spacing: 0
    margins: root.tibiaTheme ? 6 : 0
    palette.mid: (FluentColors.c("separator"))
    overlap: 0
    background: Loader {
        id: menuBackground
        HoverHandler {
            id: menuPointer
            parent: menuBackground.parent || menuBackground
            enabled: root.tibiaTheme && root.visible
        }
        sourceComponent: root.tibiaTheme ? tibiaMenuBackground : fluentMenuBackground
    }
    Component {
        id: tibiaMenuBackground
        BorderImage {
            source: Qt.resolvedUrl("../themes/tibia/images/3pixel-frame-borderimage.png")
            border { left: 3; top: 3; right: 3; bottom: 3 }
            horizontalTileMode: BorderImage.Repeat
            verticalTileMode: BorderImage.Repeat
            smooth: false
            Rectangle {
                anchors.fill:parent;anchors.margins:3
                color:"#18000000"
            }
        }
    }
    Component {
        id: fluentMenuBackground
        Rectangle { implicitWidth: 150; radius: 4; color: (FluentColors.c("popup")); border.width: 1; border.color: (FluentColors.c("border")); TibiaSkin.TibiaSkin { anchors.fill: parent } }
    }
    delegate: DmeMenuItem {}
}
