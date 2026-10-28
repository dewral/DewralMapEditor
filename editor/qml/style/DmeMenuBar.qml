import QtQuick
import QtQuick.Controls
import "../themes/tibia" as TibiaSkin
import Tibia 1.0
import "../themes/fluent/Colors.js" as FluentColors

MenuBar {
    id: root
    readonly property bool tibiaTheme: Backend.uiTheme.style === "tibia-ui"
    readonly property bool fluentTheme: (Backend.uiTheme.style === "fluent-dark" || Backend.uiTheme.style === "tibia-ui")
    readonly property bool windowsClassic: Backend.uiTheme.style === "windows-classic"
    readonly property bool modernTheme: Backend.uiTheme.style !== "classic" && !windowsClassic
    readonly property bool grayTheme: Backend.uiTheme.style === "gray-dark"
                                      || Backend.uiTheme.style === "gray-modern"
    implicitHeight: tibiaTheme ? 28 : fluentTheme ? 32 : modernTheme ? 40 : 26
    leftPadding: 0
    rightPadding: 0
    spacing: 0
    background: Item {}
    delegate: MenuBarItem {
        id: menuItem
        objectName: "menuHeading_" + text
        readonly property bool expanded: menu !== null && menu.opened
        focusPolicy: Qt.NoFocus
        hoverEnabled: true
        leftPadding: 12
        rightPadding: root.tibiaTheme ? 12 : 16
        topPadding: root.tibiaTheme ? 0 : 6
        bottomPadding: root.tibiaTheme ? 0 : 6
        height: root.availableHeight
        implicitWidth: root.tibiaTheme ? Math.ceil(label.implicitWidth) + 24 : root.fluentTheme ? label.implicitWidth + 28 : root.modernTheme ? Math.max(56, label.implicitWidth + 24) : Math.max(44, label.implicitWidth + 16)
        implicitHeight: root.tibiaTheme ? 28 : root.fluentTheme ? 32 : modernTheme ? 40 : 26
        contentItem: Text {
            id: label
            text: menuItem.text
            color: root.tibiaTheme ? (menuItem.hovered || menuItem.expanded ? "#ffffff" : "#dfdfdf") : root.fluentTheme ? FluentColors.c("buttonText") : root.windowsClassic ? "#202020" : (root.modernTheme ? (menuItem.highlighted ? "#FFFFFF" : (root.grayTheme ? "#E0E0E0" : "#C9D1D9")) : "#dcdcdc")
            font.family: root.fluentTheme ? FluentColors.family() : Qt.application.font.family
            font.bold: false
            font.pixelSize: root.tibiaTheme ? 11 : root.fluentTheme ? 13 : root.modernTheme ? 13 : 12
            renderType: root.tibiaTheme ? Text.NativeRendering : Text.QtRendering
            font.hintingPreference: Font.PreferFullHinting
            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            objectName: "menuHeadingBackground"
            radius: root.tibiaTheme ? 0 : root.fluentTheme ? 4 : root.modernTheme ? 4 : 0
            color: root.tibiaTheme ? (menuItem.hovered || menuItem.highlighted ? "#606060" : "transparent") : root.fluentTheme ? (menuItem.highlighted ? FluentColors.c("selected") : "transparent") : menuItem.highlighted ? (root.windowsClassic ? "#cce8ff" : (root.modernTheme ? (root.grayTheme ? "#292929" : "#161B22") : "#1fffffff")) : "transparent"
            border.width: root.windowsClassic && menuItem.highlighted ? 1 : 0
            border.color: "#99c9ef"
            Loader {
                objectName: "menuHeadingPressedFrame"
                anchors.fill: parent
                active: root.tibiaTheme && (menuItem.expanded || menuItem.down)
                sourceComponent: TibiaSkin.TibiaClassicFrame { kind: "button"; pressed: true }
            }
        }
    }
}
