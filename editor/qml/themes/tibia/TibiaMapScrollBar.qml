import QtQuick
import QtQuick.Controls

Item {
    id: root
    required property var mapView
    property bool horizontal: false
    property real originX: mapView.renderOriginX()
    property real originY: mapView.renderOriginY()
    readonly property real viewTiles: (horizontal ? mapView.width : mapView.height) / Math.max(1,mapView.tileSize)
    readonly property real mapTiles: Math.max(viewTiles+1, horizontal ? mapView.otbm.width : mapView.otbm.height)
    readonly property real limit: Math.max(1,mapTiles-viewTiles)
    function moveTo(value) {
        const x=horizontal ? value : originX;
        const y=horizontal ? originY : value;
        mapView.centerOnTile(Math.round(x+mapView.width/(2*mapView.tileSize)),
                             Math.round(y+mapView.height/(2*mapView.tileSize)),mapView.floor);
    }
    function step(direction) { moveTo(Math.max(0,Math.min(limit,(horizontal ? originX : originY)+direction*3))); }
    Connections {
        target: root.mapView
        function onContentUpdated() { root.originX=root.mapView.renderOriginX(); root.originY=root.mapView.renderOriginY(); }
    }
    TibiaClassicFrame { anchors.fill: parent; kind: "field" }
    ToolButton {
        id: previous
        objectName: "mapScrollPrevious"
        x: 0; y: 0; width: 12; height: 12
        autoRepeat: true; onClicked: root.step(-1)
        background: Image { smooth: false; source: Qt.resolvedUrl("images/scrollbar-button"+(root.horizontal ? "left" : "up")+"-"+(previous.down ? "down" : "up")+".png") }
    }
    ToolButton {
        id: next
        objectName: "mapScrollNext"
        x: root.horizontal ? root.width-width : 0; y: root.horizontal ? 0 : root.height-height
        width: 12; height: 12
        autoRepeat: true; onClicked: root.step(1)
        background: Image { smooth: false; source: Qt.resolvedUrl("images/scrollbar-button"+(root.horizontal ? "right" : "down")+"-"+(next.down ? "down" : "up")+".png") }
    }
    Slider {
        id: slider
        objectName: "mapScrollSlider"
        x: root.horizontal ? 12 : 0; y: root.horizontal ? 0 : 12
        width: root.horizontal ? root.width-24 : 12; height: root.horizontal ? 12 : root.height-24
        orientation: root.horizontal ? Qt.Horizontal : Qt.Vertical
        from: 0; to: root.limit; padding: 0
        Binding { target: slider; property: "value"; value: root.horizontal ? Math.max(0,Math.min(root.limit,root.originX)) : root.limit-Math.max(0,Math.min(root.limit,root.originY)); when: !slider.pressed }
        onMoved: root.moveTo(root.horizontal ? value : root.limit-value)
        background: Image {
            source: Qt.resolvedUrl("images/scrollbar-texture-"+(root.horizontal ? "horizontal" : "vertical")+".png")
            fillMode: Image.Tile; smooth: false
        }
        handle: BorderImage {
            width: root.horizontal ? Math.max(14,slider.availableWidth*Math.min(1,root.viewTiles/root.mapTiles)) : 12
            height: root.horizontal ? 12 : Math.max(14,slider.availableHeight*Math.min(1,root.viewTiles/root.mapTiles))
            x: root.horizontal ? slider.visualPosition*(slider.availableWidth-width) : 0
            y: root.horizontal ? 0 : slider.visualPosition*(slider.availableHeight-height)
            source: Qt.resolvedUrl("images/scrollbar-handle-"+(root.horizontal ? "horizontal" : "vertical")+".png")
            border { left: root.horizontal ? 6 : 0; right: root.horizontal ? 6 : 0; top: root.horizontal ? 0 : 6; bottom: root.horizontal ? 0 : 6 } smooth: false
            horizontalTileMode: BorderImage.Stretch; verticalTileMode: BorderImage.Stretch
        }
    }
}
