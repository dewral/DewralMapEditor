import QtQuick
import Tibia 1.0
import "../themes/tibia" as TibiaSkin
import QtQuick.Controls
import "../themes/fluent/Colors.js" as Colors
Slider {
    id: control
    property bool classicValueFill: false
    readonly property bool tibiaUi: Backend.uiTheme.style === "tibia-ui"
    from: 0; to: 255; stepSize: 1
    focusPolicy: Qt.NoFocus
    implicitHeight: 32
    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        width: control.availableWidth; height: control.tibiaUi ? 12 : 4; radius: control.tibiaUi ? 0 : 2
        border.width: control.tibiaUi ? 1 : 0; border.color: "#121212"
        color: Colors.c("border")
        Image { anchors.fill: parent; visible: control.tibiaUi; source: Qt.resolvedUrl("../themes/tibia/images/slider-texture.png"); fillMode: Image.Tile; smooth: false }
        Rectangle { objectName: "brightnessValueFill"; visible: !control.tibiaUi || control.classicValueFill; x: control.tibiaUi ? 2 : 0; y: control.tibiaUi ? 3 : 0; width: Math.max(0,control.visualPosition * (parent.width-(control.tibiaUi ? 4 : 0))); height: control.tibiaUi ? 6 : parent.height; radius: control.tibiaUi ? 0 : 2; color: control.tibiaUi ? "#249ec7" : Colors.c("accent") }
    }
    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + (control.availableHeight - height) / 2
        width: control.tibiaUi ? 12 : 16; height: control.tibiaUi ? (control.classicValueFill ? 22 : 12) : 16; radius: control.tibiaUi ? 0 : 8
        color: control.tibiaUi ? "transparent" : Colors.c("accent")
        border.width: control.tibiaUi ? 0 : 3; border.color: Colors.c("surface")
        BorderImage {
            anchors.fill: parent; visible: control.tibiaUi
            source: Qt.resolvedUrl("../themes/tibia/images/scrollbar-handle-horizontal.png")
            border { left: 6; right: 6; top: 0; bottom: 0 } smooth: false
        }
    }
    ColorHighlight { targetItem: control; colorKeys: ["border", "accent", "surface"] }
}