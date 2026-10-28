pragma ComponentBehavior: Bound
import QtQuick
Canvas {
    id: overlay
    required property var mapView
    property var session: null
    property var cells: session&&session.previewCells&&session.previewCells.length?session.previewCells:mapView.generationMask
    property var diagnosticCells: session&&session.diagnosticCells?session.diagnosticCells:[]
    onDiagnosticCellsChanged: requestPaint()
    property var paths: mapView.generationPaths
    property var cellRows: indexCells(cells)
    property var pathRows: indexCells(paths)
    readonly property var cursorCells: {
        const x=mapView.hoverX,y=mapView.hoverY,z=mapView.floor;
        const mode=mapView.generationPaintMode,width=mapView.generationBrushWidth;
        const roughness=mapView.generationBrushRoughness,seed=mapView.generationBrushSeed;
        if(mapView.generationInputBlocked||x<0||y<0)return [];
        if(mode===5&&session){
            const prefab=session.brushPrefabs[session.selectedPrefabIndex];
            return prefab&&prefab.tiles?prefab.tiles.map(t=>({x:x+t.dx,y:y+t.dy,z:z})):[];
        }
        return mode===3||mode===4?mapView.generationCursorFootprint():[];
    }
    property var contour: outline(cells)
    property var cursorContour: outline(cursorCells)
    visible: cells.length>0||cursorCells.length>0
    property real originX: mapView.renderOriginX()
    property real originY: mapView.renderOriginY()
    function indexCells(tiles) {
        const rows={};
        for(const t of tiles){const key=t.z+":"+t.y;if(!rows[key])rows[key]={};rows[key][t.x]=true;}
        return rows;
    }
    function outline(tiles) {
        const rows=indexCells(tiles),edges=[];
        function has(x,y,z){const row=rows[z+":"+y];return !!row&&!!row[x];}
        for(const key of Object.keys(rows)){
            const parts=key.split(":"),z=Number(parts[0]),y=Number(parts[1]);
            for(const value of Object.keys(rows[key])){
                const x=Number(value);
                if(!has(x,y-1,z))edges.push({x:x,y:y,dx:1,dy:0,z:z});
                if(!has(x,y+1,z))edges.push({x:x,y:y+1,dx:1,dy:0,z:z});
                if(!has(x-1,y,z))edges.push({x:x,y:y,dx:0,dy:1,z:z});
                if(!has(x+1,y,z))edges.push({x:x+1,y:y,dx:0,dy:1,z:z});
            }
        }
        return edges;
    }
    function paintOutline(ctx,edges,color,lineWidth) {
        const size=mapView.tileSize,floor=mapView.floor;
        ctx.beginPath();
        for(const e of edges){
            if(e.z!==floor)continue;
            const x=(e.x-originX)*size,y=(e.y-originY)*size;
            if(x+size<0||y+size<0||x>width+size||y>height+size)continue;
            ctx.moveTo(x,y);ctx.lineTo(x+e.dx*size,y+e.dy*size);
        }
        ctx.lineWidth=lineWidth;ctx.strokeStyle=color;ctx.stroke();
    }
    onPathRowsChanged: requestPaint()
    onContourChanged: requestPaint()
    onCursorContourChanged: requestPaint()
    Connections {
        target: overlay.mapView
        function onContentUpdated(){const x=overlay.mapView.renderOriginX(),y=overlay.mapView.renderOriginY();if(x!==overlay.originX||y!==overlay.originY){overlay.originX=x;overlay.originY=y;overlay.requestPaint();}}
        function onTileSizeChanged(){overlay.requestPaint();}
        function onFloorChanged(){overlay.requestPaint();}
        function onGenerationPreviewChanged(){overlay.requestPaint();}
    }
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        const ctx=getContext("2d");ctx.clearRect(0,0,width,height);
        const size=mapView.tileSize,floor=mapView.floor;
        // Only pending terrain gets a faint wash. Generated sprites stay unobscured.
        if(!mapView.generationPreviewActive){
            ctx.fillStyle="rgba(92,191,183,0.07)";
            for(const t of cells)if(t.z===floor){const x=(t.x-originX)*size,y=(t.y-originY)*size;if(x+size>=0&&y+size>=0&&x<width&&y<height)ctx.fillRect(x,y,size,size);}
        }
        if(mapView.generationPaintMode===1||mapView.generationPaintMode===2){
            ctx.fillStyle="rgba(225,199,144,0.8)";
            for(const t of paths)if(t.z===floor){const x=(t.x-originX+.5)*size,y=(t.y-originY+.5)*size;ctx.beginPath();ctx.arc(x,y,Math.max(1.5,Math.min(3,size*.08)),0,Math.PI*2);ctx.fill();}
        }
        for(const t of diagnosticCells)if(t.z===floor){ctx.fillStyle="rgba(230,75,65,0.5)";ctx.fillRect((t.x-originX)*size,(t.y-originY)*size,size,size);}
        paintOutline(ctx,contour,"rgba(12,24,30,0.65)",3);
        paintOutline(ctx,contour,mapView.generationPreviewActive?"#bce5d8":"#e3c68a",1);
        paintOutline(ctx,cursorContour,"rgba(12,24,30,0.7)",3);
        paintOutline(ctx,cursorContour,"rgba(240,251,248,0.9)",1);
        if(cursorCells.length){
            const x=(mapView.hoverX-originX+.5)*size,y=(mapView.hoverY-originY+.5)*size;
            ctx.strokeStyle="rgba(240,251,248,0.8)";ctx.lineWidth=1;ctx.beginPath();ctx.moveTo(x-4,y);ctx.lineTo(x+4,y);ctx.moveTo(x,y-4);ctx.lineTo(x,y+4);ctx.stroke();
        }
    }
}
