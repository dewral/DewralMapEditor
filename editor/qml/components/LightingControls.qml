import QtQuick
import QtQuick.Controls
import "../themes/fluent/Colors.js" as Colors

Column {
    id: root
    required property var mapView
    property bool paletteOpen: false
    spacing: 12
    function paletteColor(index) {
        return Qt.rgba(Math.floor(index / 36) / 5, Math.floor(index / 6) % 6 / 5, index % 6 / 5, 1);
    }
    Row {
        width: parent.width; spacing: 8
        Text { text: "Brightness"; color: Colors.c("text"); font.pixelSize: 12 }
        Text { text: root.mapView.lightAmbient + " / 255"; color: Colors.c("muted"); font.pixelSize: 12 }
    }
    Slider {
        width: parent.width
        from: 0; to: 255; stepSize: 1
        value: root.mapView.lightAmbient
        onMoved: root.mapView.lightAmbient = Math.round(value)
    }
    Button {
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
    Grid {
        visible: root.paletteOpen
        columns: 18; spacing: 2
        Repeater {
            model: 216
            delegate: Rectangle {
                required property int index
                width: 14; height: 14
                color: root.paletteColor(index)
                border.width: root.mapView.lightAmbientColor === index ? 2 : 1
                border.color: root.mapView.lightAmbientColor === index ? "white" : Colors.c("border")
                MouseArea { anchors.fill: parent; onClicked: root.mapView.lightAmbientColor = parent.index }
            }
        }
    }
}