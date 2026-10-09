import QtQuick
import QtQuick.Controls
import "../style"
import "../themes/fluent/Colors.js" as Colors

Rectangle {
    id: root
    required property var mapView
    function setZoneColor(index, color) {
        const values = mapView.zoneColors.slice();
        values[index] = String(color);
        mapView.zoneColors = values;
    }
    DmeColorPicker {
        id: zonePicker
        property int zoneIndex: 0
        title: "Choose zone color"
        onAccepted: root.setZoneColor(zoneIndex, selectedColor)
    }
    color: Colors.c("surface")
    radius: 6
    border.color: Colors.c("border")
    implicitWidth: 236
    implicitHeight: content.implicitHeight + 24
    Flickable {
        anchors.fill: parent
        anchors.margins: 12
        contentHeight: content.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
    Column {
        id: content
        width: parent.width
        spacing: 10
        Text { text: "Zone display"; color: Colors.c("heading"); font { family: Colors.fontFamily; pixelSize: 12; bold: true } }
        Rectangle { width: parent.width; height: 1; color: Colors.c("separator") }
        Repeater {
            model: [ {name:"Protection Zone",bit:1,color:"#399ee8"}, {name:"Non-PvP",bit:4,color:"#48b883"},
                     {name:"No Logout",bit:8,color:"#dfa65a"}, {name:"PvP",bit:16,color:"#d46b79"}, {name:"House",bit:0,color:"#9173be"} ]
            delegate: Column {
                id: zoneRow
                readonly property color displayColor: root.mapView.zoneColors ? root.mapView.zoneColors[index] : modelData.color
                required property var modelData
                required property int index
                width: content.width
                spacing: 2
                Row {
                    spacing: 8
                    Rectangle {
                        objectName: "zoneColor" + index
                        width: 18; height: 18; radius: 4
                        color: zoneRow.displayColor
                        anchors.verticalCenter: parent.verticalCenter
                        border.width: colorMouse.containsMouse ? 2 : 1
                        border.color: colorMouse.containsMouse ? Colors.c("text") : Colors.c("border")
                        MouseArea {
                            id: colorMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                zonePicker.zoneIndex = index;
                                zonePicker.selectedColor = zoneRow.displayColor;
                                zonePicker.title = "Choose " + modelData.name + " color";
                                zonePicker.open();
                            }
                        }
                    }
                    CheckBox {
                        objectName: "zoneVisible" + index
                        text: modelData.name
                        checked: modelData.bit === 0 ? root.mapView.showHouses : (root.mapView.visibleZoneMask & modelData.bit) !== 0
                        palette.windowText: Colors.c("text")
                        palette.highlight: zoneRow.displayColor
                        indicator: Rectangle {
                            x: parent.width - width; y: (parent.height - height) / 2
                            width: 20; height: 12; radius: 8
                            color: "transparent"
                            border.color: parent.checked ? zoneRow.displayColor : Colors.c("disabled")
                            Rectangle { anchors.centerIn: parent; width: 5; height: 5; radius: 3; color: parent.parent.checked ? zoneRow.displayColor : Colors.c("disabled") }
                        }
                        leftPadding: 0; rightPadding: 28
                        width: content.width - 26
                        font { family: Colors.fontFamily; pixelSize: 12 }
                        onClicked: { if (modelData.bit === 0) { root.mapView.showHouses = checked; return; } root.mapView.visibleZoneMask = checked
                            ? root.mapView.visibleZoneMask | modelData.bit : root.mapView.visibleZoneMask & ~modelData.bit; }
                    }
                }
                Row {
                    width: parent.width
                    spacing: 8
                    Slider {
                        objectName: "zoneOpacity" + index
                        width: parent.width - 40
                        from: 0; to: 1; stepSize: 0.05
                        value: index === 4 ? root.mapView.houseOpacity : root.mapView.zoneOpacities[index]
                        palette.highlight: zoneRow.displayColor
                        background: Rectangle {
                            x: parent.leftPadding; y: parent.topPadding + (parent.availableHeight - height) / 2
                            width: parent.availableWidth; height: 3; radius: 2; color: Colors.c("border")
                            Rectangle { width: parent.parent.visualPosition * parent.width; height: parent.height; radius: 2; color: zoneRow.displayColor }
                        }
                        handle: Rectangle {
                            x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                            y: parent.topPadding + (parent.availableHeight - height) / 2
                            width: 10; height: 10; radius: 5; color: zoneRow.displayColor
                        }
                        onMoved: {
                            if (index === 4) { root.mapView.houseOpacity = value; return; }
                            let values = root.mapView.zoneOpacities.slice(); values[index] = value;
                            root.mapView.zoneOpacities = values;
                        }
                    }
                    Text { text: Math.round((index === 4 ? root.mapView.houseOpacity : root.mapView.zoneOpacities[index]) * 100) + "%"; color: Colors.c("muted"); font { family: Colors.fontFamily; pixelSize: 11 } anchors.verticalCenter: parent.verticalCenter }
                }
            }
        }
        Rectangle { width: parent.width; height: 1; color: Colors.c("separator") }
        Text { text: "Map layers"; color: Colors.c("heading"); font { family: Colors.fontFamily; pixelSize: 12; bold: true } }
        Repeater {
            model: [{name:"Tiles",key:"tilesOpacity"}, {name:"Items",key:"itemsOpacity"}]
            delegate: Column {
                required property var modelData
                width: content.width
                Text { text: modelData.name + "  " + Math.round(root.mapView[modelData.key] * 100) + "%"; color: Colors.c("text"); font { family: Colors.fontFamily; pixelSize: 12 } }
                Slider {
                    width: parent.width
                    from: 0; to: 1; stepSize: 0.05
                    value: root.mapView[modelData.key]
                    palette.highlight: Colors.c("accent")
                    background: Rectangle {
                        x: parent.leftPadding; y: parent.topPadding + (parent.availableHeight - height) / 2
                        width: parent.availableWidth; height: 3; radius: 2; color: Colors.c("border")
                        Rectangle { width: parent.parent.visualPosition * parent.width; height: parent.height; radius: 2; color: Colors.c("accent") }
                    }
                    handle: Rectangle {
                        x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                        y: parent.topPadding + (parent.availableHeight - height) / 2
                        width: 10; height: 10; radius: 5; color: Colors.c("accent")
                    }
                    onMoved: root.mapView[modelData.key] = value
                }
            }
        }
        Repeater {
            model: [{name:"Zones overlay",key:"showZonesAlways"},
                    {name:"Spawns",key:"showSpawns"}, {name:"Creatures",key:"showCreatures"}, {name:"Grid",key:"showGrid"}]
            delegate: CheckBox {
                required property var modelData
                text: modelData.name
                checked: root.mapView[modelData.key]
                palette.windowText: Colors.c("text")
                palette.highlight: Colors.c("accent")
                font { family: Colors.fontFamily; pixelSize: 12 }
                width: content.width
                leftPadding: 0; rightPadding: 28
                indicator: Rectangle {
                    x: parent.width - width; y: (parent.height - height) / 2
                    width: 20; height: 12; radius: 8
                    color: "transparent"; border.color: parent.checked ? Colors.c("accent") : Colors.c("disabled")
                    Rectangle { anchors.centerIn: parent; width: 5; height: 5; radius: 3; color: parent.parent.checked ? Colors.c("accent") : Colors.c("disabled") }
                }
                onClicked: root.mapView[modelData.key] = checked
            }
        }
    }
    }
}
