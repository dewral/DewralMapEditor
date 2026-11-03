import QtQuick
import QtQuick.Controls
import Tibia 1.0
import "../themes/tibia" as TibiaSkin
import "../themes/fluent/Colors.js" as Colors

Column {
    id: root
    required property var mapView
    readonly property bool tibiaUi: Backend.uiTheme.style === "tibia-ui"
    property bool paletteOpen: false
    property bool paletteOnly: false
    spacing: 12
    function paletteColor(index) {
        return Qt.rgba(Math.floor(index / 36) / 5, Math.floor(index / 6) % 6 / 5, index % 6 / 5, 1);
    }
    Row {
        visible: !root.paletteOnly
        width: parent.width; spacing: 8
        Text { text: "Brightness"; color: Colors.c("text"); font.pixelSize: 12 }
        Text { text: root.mapView.lightAmbient + " / 255"; color: Colors.c("muted"); font.pixelSize: 12 }
    }
    LightingSlider {
        visible: !root.paletteOnly
        width: parent.width
        from: 0; to: 255; stepSize: 1
        value: root.mapView.lightAmbient
        onMoved: root.mapView.lightAmbient = Math.round(value)
    }
    Button {
        visible: !root.paletteOnly
        width: parent.width
        text: "Light color · " + root.mapView.lightAmbientColor
        onClicked: root.paletteOpen = !root.paletteOpen
        contentItem: Row {
            spacing: 10
            Rectangle { width: 20; height: 20; color: root.paletteColor(root.mapView.lightAmbientColor); border.color: Colors.c("border") }
            Text { text: parent.parent.text; color: Colors.c("text"); font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
        }
        background: Rectangle { color: Colors.c("button"); border.color: Colors.c("border") }
    }
    Row {
        visible: root.tibiaUi && root.paletteOnly
        spacing: 8
        Rectangle { width: 18; height: 18; color: root.paletteColor(root.mapView.lightAmbientColor); border.color: "#b4b4ad" }
        Text { anchors.verticalCenter: parent.verticalCenter; text: "Color " + root.mapView.lightAmbientColor; color: "#d5d5ce"; font.family: "Verdana"; font.pixelSize: 11 }
    }
    Grid {
        visible: root.paletteOpen
        columns: 18; spacing: 2
        Repeater {
            model: 216
            delegate: Rectangle {
                required property int index
                objectName: "lightColorCell_" + index
                width: root.tibiaUi ? 16 : 14; height: width
                color: root.paletteColor(index)
                border.width: root.mapView.lightAmbientColor === index ? 2 : 1
                border.color: root.mapView.lightAmbientColor === index ? "white" : Colors.c("border")
                Accessible.name: "Light color " + index
                Accessible.role: Accessible.Button
                Accessible.onPressAction: root.mapView.lightAmbientColor = index
                Rectangle { anchors.fill: parent; anchors.margins: 2; color: "transparent"; border.color: "#202020"; visible: root.tibiaUi && root.mapView.lightAmbientColor === parent.index }
                MouseArea { id: cellMouse; anchors.fill: parent; hoverEnabled: true; onClicked: root.mapView.lightAmbientColor = parent.index }
                ToolTip.visible: cellMouse.containsMouse
                ToolTip.delay: 650
                ToolTip.text: "Color " + index
            }
        }
    }
}