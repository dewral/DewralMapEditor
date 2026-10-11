import QtQuick
import QtQuick.Controls
import "../themes/fluent/Colors.js" as Colors
Slider {
    id: control
    from: 0; to: 255; stepSize: 1
    focusPolicy: Qt.NoFocus
    implicitHeight: 32
    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        width: control.availableWidth; height: 4; radius: 2
        color: Colors.c("border")
        Rectangle { width: control.visualPosition * parent.width; height: parent.height; radius: 2; color: Colors.c("accent") }
    }
    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + (control.availableHeight - height) / 2
        width: 16; height: 16; radius: 8
        color: Colors.c("accent")
        border.width: 3; border.color: Colors.c("surface")
    }
    ColorHighlight { targetItem: control; colorKeys: ["border", "accent", "surface"] }
}