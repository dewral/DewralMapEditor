pragma ComponentBehavior: Bound
import QtQuick
import Tibia 1.0
import "../style"
import "../dialogs"

Item {
    id: layer
    required property var mapView
    required property var mapSettings
    property bool testingMap: false
    readonly property var document: store.document
    property bool editing: false
    property bool showRegions: true
    property bool previewEnabled: false
    property var previewOverride: null
    readonly property var previewWeather: previewOverride || (document ? document.preview : ({enabled:false}))
    property string tool: "square"
    property int radius: 2
    property real originX: 0
    property real originY: 0
    property var rectangles: []
    property int startX: 0
    property int startY: 0
    property int lastX: 0
    property int lastY: 0
    property bool rectangleStroke: false
    property bool eraseStroke: false
    property bool painting: false
    clip: true

    function refresh() {
        originX=mapView.renderOriginX(); originY=mapView.renderOriginY();
        rectangles=document ? document.visibleRects(Math.floor(originX),Math.floor(originY),
            Math.ceil(originX+width/mapView.tileSize),Math.ceil(originY+height/mapView.tileSize),mapView.floor) : [];
        canvas.requestPaint();
    }
    function openPanel(mode) { panel.openMode(mode || (testingMap?"test":"region")); }
    function closePanel() { panel.close(); }
    function smokePreview() { return panel.smokePreview(); }
    function startTimedSmoke() { panel.startTimedSmoke(); }
    function verifyTimedSmoke() { const ok=panel.verifyTimedSmoke();panel.x=16;panel.y=16;return ok; }
    function stop(cancel) {
        if(!painting || !document)return;
        if(cancel)document.cancelStroke();
        else {
            if(rectangleStroke)document.paintRectangle(startX,startY,lastX,lastY,mapView.floor,eraseStroke);
            document.endStroke();
        }
        painting=false; refresh();
    }
    onShowRegionsChanged: refresh()
    onEditingChanged: { if(!editing)stop(true); }
    onWidthChanged: refresh()
    onHeightChanged: refresh()
    Component.onCompleted: refresh()
    WeatherRegions {
        id: store
        map: layer.mapView.otbm
        onDocumentChanged: { layer.editing=false; layer.painting=false; layer.previewEnabled=false; layer.previewOverride=null; layer.refresh(); }
    }
    Connections {
        target: layer.document
        function onChanged() { layer.refresh(); }
        function onSelectionChanged() { layer.previewOverride=null; layer.refresh(); }
    }
    Connections {
        target: layer.mapView
        function onContentUpdated() { layer.refresh(); }
        function onFloorChanged() { layer.stop(true); layer.refresh(); }
        function onTileSizeChanged() { layer.refresh(); }
    }
    WeatherPreview {
        anchors.fill: parent
        visible: layer.previewEnabled && !layer.testingMap
        map: layer.mapView.otbm
        originX: layer.originX; originY: layer.originY
        tileSize: layer.mapView.tileSize; floor: layer.mapView.floor
        configuration: layer.document && layer.previewEnabled ? Object.assign({},layer.previewWeather,
            {regionMask:layer.rectangles.filter(r=>r.selected)}) : ({enabled:false})
    }
    Canvas {
        id: canvas
        anchors.fill: parent
        onPaint: {
            const ctx=getContext("2d");ctx.clearRect(0,0,width,height);
            const size=layer.mapView.tileSize;
            const labels={};
            for(const r of (layer.showRegions?layer.rectangles:[])) {
                const x=(r.x-layer.originX)*size,y=(r.y-layer.originY)*size;
                ctx.globalAlpha=layer.previewEnabled ? 0.04 : (r.selected ? 0.18 : 0.08);ctx.fillStyle=r.color;ctx.fillRect(x,y,r.w*size,r.h*size);
                if(r.selected && !labels[r.label] && r.w*size>80 && r.h*size>18) {ctx.globalAlpha=1;ctx.fillStyle="#ffffff";ctx.font="12px sans-serif";ctx.fillText(r.label,x+4,y+14);labels[r.label]=true;}
            }
            ctx.globalAlpha=1;
            if(layer.painting && layer.rectangleStroke) {
                ctx.strokeStyle=layer.eraseStroke?"#ef8b88":"#ffffff";
                ctx.strokeRect((Math.min(layer.startX,layer.lastX)-layer.originX)*size,(Math.min(layer.startY,layer.lastY)-layer.originY)*size,
                    (Math.abs(layer.lastX-layer.startX)+1)*size,(Math.abs(layer.lastY-layer.startY)+1)*size);
            }
        }
    }
    MouseArea {
        id: input
        anchors.fill: parent
        enabled: layer.editing && !!layer.document && layer.document.selectedId.length>0
        acceptedButtons: Qt.LeftButton
        cursorShape: Qt.CrossCursor
        hoverEnabled: false
        onPressed: mouse => {
            forceActiveFocus();
            layer.startX=layer.lastX=Math.floor(layer.originX+mouse.x/layer.mapView.tileSize);
            layer.startY=layer.lastY=Math.floor(layer.originY+mouse.y/layer.mapView.tileSize);
            layer.eraseStroke=layer.tool==="erase" || !!(mouse.modifiers & Qt.ControlModifier);
            layer.rectangleStroke=layer.tool==="rectangle" || !!(mouse.modifiers & Qt.ShiftModifier);
            layer.painting=true;layer.document.beginStroke();
            if(!layer.rectangleStroke)layer.document.paint(layer.startX,layer.startY,layer.mapView.floor,layer.radius,layer.tool==="circle",layer.eraseStroke);
            else layer.document.paintRectangle(layer.startX,layer.startY,layer.startX,layer.startY,layer.mapView.floor,layer.eraseStroke);
        }
        onPositionChanged: mouse => {
            if(!layer.painting)return;
            const x=Math.floor(layer.originX+mouse.x/layer.mapView.tileSize),y=Math.floor(layer.originY+mouse.y/layer.mapView.tileSize);
            if(!layer.rectangleStroke)layer.document.paintLine(layer.lastX,layer.lastY,x,y,layer.mapView.floor,layer.radius,layer.tool==="circle",layer.eraseStroke);
            if(layer.rectangleStroke){
                // Replace this uncommitted stroke; one release still creates one map action.
                layer.document.cancelStroke();layer.document.beginStroke();
                layer.document.paintRectangle(layer.startX,layer.startY,x,y,layer.mapView.floor,layer.eraseStroke);
            }
            layer.lastX=x;layer.lastY=y;canvas.requestPaint();
        }
        onReleased: layer.stop(false)
        onCanceled: layer.stop(true)
        onWheel: wheel => { wheel.accepted=false; }
        Keys.onEscapePressed: { layer.stop(true);layer.editing=false; }
        Keys.onShortcutOverride: event => {
            if((event.modifiers & Qt.ControlModifier) && (event.key===Qt.Key_Z || event.key===Qt.Key_Y))event.accepted=true;
        }
        Keys.onPressed: event => {
            if((event.modifiers & Qt.ControlModifier) && event.key===Qt.Key_Z){layer.document.undoGeometry();event.accepted=true;}
            else if((event.modifiers & Qt.ControlModifier) && event.key===Qt.Key_Y){layer.document.redoGeometry();event.accepted=true;}
        }
    }
    Row {
        visible: layer.editing
        anchors.top: parent.top; anchors.right: parent.right; anchors.margins: 8
        spacing: 5
        DmeButton { text: "Weather: " + (layer.document?layer.document.selectedId:""); width: 180; onClicked: layer.openPanel() }
        DmeButton { text: "Finish"; width: 70; onClicked: { layer.stop(false);layer.editing=false; } }
    }
    WeatherRegionsDialog {
        id: panel
        document: layer.document
        regionLayer: layer
        mapView: layer.mapView
        mapSettings: layer.mapSettings
    }
}
