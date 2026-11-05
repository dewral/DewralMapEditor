import QtQuick
import QtQuick.Controls
import "../themes/tibia" as TibiaSkin
import Tibia 1.0
import "../themes/fluent/Colors.js" as FluentColors

MenuBar {
    id: root
    readonly property bool tibiaTheme: Backend.uiTheme.style === "tibia-ui"
    implicitHeight: tibiaTheme ? 28 : 32
    // Keep heading backgrounds inside the MenuBox 3px border.
    topPadding: tibiaTheme ? 3 : 0
    bottomPadding: tibiaTheme ? 3 : 0
    leftPadding: tibiaTheme ? 1 : 0
    rightPadding: 0
    spacing: 0
    background: Item {}
    delegate: MenuBarItem {
        id: menuItem
        objectName: "menuHeading_" + text
        readonly property bool expanded: menu !== null && menu.opened
        readonly property bool pointerInMenu: expanded && (hovered || menu.pointerInMenuTree === true)
        property bool pointerVisited: false

        onClicked: {
            if (hovered)
                pointerVisited = true;
        }
        onExpandedChanged: {
            if (!expanded)
                pointerVisited = false;
            else if (hovered)
                pointerVisited = true;
        }
        onPointerInMenuChanged: {
            if (pointerInMenu)
                pointerVisited = true;
        }

        Timer {
            interval: 200
            running: root.tibiaTheme && menuItem.expanded
                     && menuItem.pointerVisited && !menuItem.pointerInMenu
            onTriggered: menuItem.menu.close()
        }
        focusPolicy: Qt.NoFocus
        hoverEnabled: true
        leftPadding: 12
        rightPadding: root.tibiaTheme ? 12 : 16
        topPadding: root.tibiaTheme ? 0 : 6
        bottomPadding: root.tibiaTheme ? 0 : 6
        height: root.availableHeight
        implicitWidth: root.tibiaTheme ? Math.ceil(label.implicitWidth) + 24 : (label.implicitWidth + 28)
        implicitHeight: root.tibiaTheme ? 28 : 32
        contentItem: Text {
            id: label
            text: menuItem.text
            color: root.tibiaTheme ? (menuItem.hovered || menuItem.expanded ? "#ffffff" : "#dfdfdf") : (FluentColors.c("buttonText"))
            font.family: (FluentColors.family())
            font.bold: false
            font.pixelSize: root.tibiaTheme ? 11 : 13
            renderType: root.tibiaTheme ? Text.NativeRendering : Text.QtRendering
            font.hintingPreference: Font.PreferFullHinting
            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            objectName: "menuHeadingBackground"
            radius: root.tibiaTheme ? 0 : 4
            color: root.tibiaTheme ? "transparent" : ((menuItem.highlighted ? FluentColors.c("selected") : "transparent"))
            border.width: 0
            border.color: "#99c9ef"
            Loader {
                objectName: "menuHeadingPressedFrame"
                anchors.fill: parent
                active: root.tibiaTheme && (menuItem.hovered || menuItem.expanded)
                sourceComponent: TibiaSkin.TibiaClassicFrame { kind: "button"; pressed: true }
            }
        }
    }
}
