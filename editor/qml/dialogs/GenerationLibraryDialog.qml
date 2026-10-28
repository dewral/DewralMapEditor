pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Tibia 1.0
import "../style"
import "../themes/fluent/Colors.js" as Colors
DmeDialog {
    id: dialog
    required property var mapCtrl
    required property string clientKey
    readonly property var library: Backend.generationLibrary
    property var selectedExamples: []
    property var selectedPrefabs: []
    property var prefabModel: []
    property var pins: []
    property var previewCells: []
    property var diagnosticCells: []
    property var frozenStyle: ({})
    property int structureSpan: 1
    property var recipeParts: []
    property var recipeConnections: []
    readonly property var recipes: library.entries.filter(e => e.kind === "recipe")
    property string blueprintId: ""
    property string message: "Capture examples, learn a style, then paint its generation mask."
    property var requestRevision: 0
    property bool waitingForPreview: false
    property bool profileReady: false
    property bool previewDirty: false
    property bool proceduralPlan: true
    property bool mapSessionActive: false
    property bool commitAfterStroke: false
    property bool quickLearning: false
    property bool selectionBeforeMap: false
    readonly property bool sessionActive: visible || mapSessionActive
    readonly property int selectedStyleIndex: styleChoice.currentIndex
    readonly property int selectedPrefabIndex: stampPrefabChoice.currentIndex
    readonly property int brushWidthValue: brushWidth.value
    readonly property int roughnessValue: roughness.value
    readonly property bool replaceTerrain: replace.checked
    readonly property var brushPrefabs: {
        const resources=library.entries;
        const entry=library.load(styleId);
        if(!entry.model)return [];
        const prefabs=entry.model.prefabs||[],recipes=entry.model.recipes||[];
        return prefabs.concat(recipes.map(recipe=>{
            const tiles=[];
            for(const p of recipe.parts||[]){const prefab=prefabs.find(v=>v.name===p.prefab);if(!prefab)continue;
                const count=p.repeat?structureSpan:1;
                for(let n=0;n<count;++n){const shift=p.afterSpan?structureSpan-1:n;for(const t of prefab.tiles||[])tiles.push({dx:t.dx+(p.dx||0)+shift*(p.stepX||0),dy:t.dy+(p.dy||0)+shift*(p.stepY||0),dz:t.dz+(p.dz||0)+shift*(p.stepZ||0),items:t.items});}
            }
            return {name:recipe.name,recipe:true,tiles:tiles};
        }));
    }
    readonly property var examples: library.entries.filter(e => e.kind === "example")
    readonly property var styles: library.entries.filter(e => e.kind === "style")
    readonly property var blueprints: library.entries.filter(e => e.kind === "blueprint")
    readonly property string styleId: styleChoice.currentIndex >= 0 && styleChoice.currentIndex < styles.length ? styles[styleChoice.currentIndex].id : ""
    palette.window: Colors.c("popup")
    palette.base: Colors.c("field")
    palette.button: Colors.c("button")
    palette.text: Colors.c("text")
    palette.windowText: Colors.c("text")
    palette.buttonText: Colors.c("buttonText")
    palette.highlight: Colors.c("accent")
    palette.highlightedText: Colors.c("text")
    font.family: "Segoe UI"
    font.pixelSize: 12
    title: "Procedural Brush & Library"
    modal: false; dim: false
    width: Math.min(590, parent ? parent.width - 24 : 590)
    height: Math.min(730, parent ? parent.height - 24 : 730)
    closePolicy: Popup.NoAutoClose
    function styleIcon(id) {
        const entry=library.load(id),examples=entry.examples||[];
        if(!examples.length)return "";
        const tiles=library.load(examples[0]).tiles||[];
        let minX=65535,minY=65535,maxX=0,maxY=0;
        for(const t of tiles){minX=Math.min(minX,t.x);minY=Math.min(minY,t.y);maxX=Math.max(maxX,t.x);maxY=Math.max(maxY,t.y);}
        let nearest=Infinity,ground=0;
        for(const t of tiles){
            const g=(t.items||[]).find(i=>i.ground);
            if(!g)continue;
            const dx=2*t.x-minX-maxX,dy=2*t.y-minY-maxY,d=dx*dx+dy*dy;
            if(d<nearest){nearest=d;ground=g.id;}
        }
        const row=Backend.otbReader.rowForServerId(ground);
        if(row<0)return "";
        const item=Backend.otbReader.detailsAt(row);
        return item.spriteIds?Backend.sprReader.itemImageSource(item.spriteIds,item.itemWidth,item.itemHeight,item.layers):"";
    }
    function startOnMap() {
        if(!mapSessionActive)selectionBeforeMap=!!mapCtrl.selectionMode;
        mapSessionActive=true;commitAfterStroke=false;mapCtrl.generationInputBlocked=false;
        profileReady=library.selectProfile(clientKey);
        prefabModel=mapCtrl.generationPrefabs();
        replace.checked=true;
        livePreview.checked=true;
        if(visible)close();
        if(profileReady&&styleId){chooseStyle(styleChoice.currentIndex);if(!quickLearning)armBrush(3);message="Maluj LPM na mapie. Po puszczeniu myszy zapiszę pociągnięcie. Ctrl+Z cofa.";}
        else{mapCtrl.generationPaintMode=0;message=profileReady?"Zaznacz gotowy fragment mapy (minimum 3×3) i kliknij Zapamiętaj.":library.status;}
    }
    function endOnMap() {
        mapSessionActive=false;commitAfterStroke=false;quickLearning=false;
        liveTimer.stop();previewDirty=false;waitingForPreview=false;library.cancel();
        mapCtrl.generationInputBlocked=false;mapCtrl.generationPaintMode=0;
        mapCtrl.setGenerationMask([],[]);pins=[];previewCells=[];diagnosticCells=[];frozenStyle=({});
        mapCtrl.selectionMode=selectionBeforeMap;
    }
    function openLibrary() {open();}
    function chooseStyle(index) {
        if(index<0||index>=styles.length||mapCtrl.generationInputBlocked)return;
        frozenStyle=({});styleChoice.currentIndex=index;
        const entry=library.load(styleId),model=entry.model;spacing.value=model?model.decorationSpacing||1:1;
        if(mapSessionActive&&model&&(model.algorithm==="raw-patch-v1"||model.patterns)&&!library.busy) {
            const converted=styles.findIndex(s=>s.parameters&&s.parameters.sourceStyleId===entry.id);
            if(converted>=0){chooseStyle(converted);return;}
            quickLearning=true;mapCtrl.generationPaintMode=0;message="Przygotowuję proceduralną wersję zapisanego wzorca…";
            if(!library.learnStyle(entry.name+" · proceduralny",entry.examples||[],model.prefabs||[],{procedural:true,sourceStyleId:entry.id,recipes:model.recipes||[]})){quickLearning=false;message=library.status;}
            return;
        }
        mapCtrl.invalidateGeneration();
        if(mapSessionActive)armBrush(3);
    }
    function choosePrefab(index) {stampPrefabChoice.currentIndex=index;}
    function setMapWidth(value) {brushWidth.value=value;mapCtrl.generationBrushWidth=value;}
    function setMapRoughness(value) {roughness.value=value;mapCtrl.generationBrushRoughness=value;}
    function setReplaceTerrain(value) {replace.checked=value;}
    function pickMapExample() {
        if(library.busy||mapCtrl.generationInputBlocked)return;
        discardPending();mapCtrl.generationPaintMode=0;mapCtrl.selectionMode=true;
        message="Zaznacz gotowy fragment mapy (minimum 3×3), następnie kliknij Zapamiętaj.";
    }
    function learnMapSelection() {
        if(!profileReady||library.busy||mapCtrl.generationInputBlocked)return;
        const snapshot=mapCtrl.generationExample();
        if(!snapshot.success){message=snapshot.error;return;}
        const name="Wzorzec z mapy "+(styles.length+1);
        const saved=library.capture(name,snapshot);
        if(!saved.success){message=saved.error;return;}
        quickLearning=true;message="Pobieram teren i dekoracje z zaznaczenia…";
        if(!library.learnStyle(name,[saved.id],mapCtrl.generationPrefabs(),{procedural:true,recipes:recipes.map(r=>library.load(r.id).recipe)})){quickLearning=false;message=library.status;}
    }
    function finishMapStroke() {
        if(!mapSessionActive){queueLivePreview();return;}
        if(!profileReady||!styleId){unlockMapStroke();mapCtrl.generationPaintMode=0;message="Najpierw pobierz lub wybierz wzorzec.";return;}
        if(mapCtrl.generationPaintMode<3||!mapCtrl.generationMask.length)return;
        commitAfterStroke=true;mapCtrl.generationInputBlocked=true;
        if(mapCtrl.generationPreviewActive&&!library.busy){applyPending();return;}
        message="Kończę pociągnięcie…";
        if(!waitingForPreview){previewDirty=true;liveTimer.restart();}
    }
    function unlockMapStroke() {commitAfterStroke=false;mapCtrl.generationInputBlocked=false;}
    function toggle(array, id, checked) { return checked ? array.concat([id]) : array.filter(v => v !== id); }
    function report(result) { message = result.success ? "Saved to the current client library." : result.error; }
    function blueprint() {
        return {version:2, styleId:styleId, mask:mapCtrl.generationMask, paths:mapCtrl.generationPaths,
            pins:pins, styleSnapshot:Object.keys(frozenStyle).length?frozenStyle:undefined, seed:seed.value, replace:replace.checked, monsters:monsters.checked, spacing:spacing.value,
            brush:{width:brushWidth.value,roughness:roughness.value,connectRooms:connectRooms.checked,walkable:proceduralPlan,liveMap:mapSessionActive,terrainPaint:false,rawLayout:false,procedural:mapSessionActive,closeArea:mapCtrl.generationPaintMode===6}};
    }
    function generate(refine = false, keepPainting = false) {
        if(!profileReady)return;
        if(library.busy){if(keepPainting)previewDirty=true;return;}
        previewDirty=false;
        mapCtrl.generationBrushSeed=seed.value;
        mapCtrl.generationBrushWidth=brushWidth.value;
        mapCtrl.generationBrushRoughness=roughness.value;
        mapCtrl.generationConnectRooms=connectRooms.checked;
        if(!keepPainting)mapCtrl.generationPaintMode = 0;
        const context = mapCtrl.generationContext(refine);
        if (!context.success) {unlockMapStroke();message=context.error;return;}
        requestRevision = context.revision;
        const options = blueprint();
        if(refine) {
            options.mask = context.targetMask;
            const keys = new Set(options.mask.map(t => t.x + "," + t.y + "," + t.z));
            options.paths = options.paths.filter(t => keys.has(t.x + "," + t.y + "," + t.z));
            options.pins = options.pins.filter(t => keys.has(t.x + "," + t.y + "," + t.z));
        }
        waitingForPreview = library.generate(styleId, options, context);
        if (!waitingForPreview) {unlockMapStroke();message=library.status;}
    }
    function armBrush(mode) {
        if(mapCtrl.generationInputBlocked)return;
        if(mode>=3){proceduralPlan=true;mapCtrl.selectionMode=false;}
        mapCtrl.generationBrushWidth=brushWidth.value;
        mapCtrl.generationBrushRoughness=roughness.value;
        mapCtrl.generationBrushSeed=seed.value;
        mapCtrl.generationConnectRooms=connectRooms.checked;
        mapCtrl.generationPaintMode=mode;
        message=mapSessionActive
            ? (mode===5?"Kliknij na mapie, aby wstawić cały prefab. Ctrl+Z cofa.":"Maluj LPM na mapie. Puszczenie myszy zapisuje pociągnięcie. Ctrl+Z cofa.")
            : (mode===5?"Click to stamp a complete prefab; right-click removes a pending stamp.":"Left drag builds the area; right drag erases. Preview updates after a short pause. Apply commits one undo step.");
    }
    function queueLivePreview() {
        if(!sessionActive||!livePreview.checked||!profileReady||!styleId||mapCtrl.generationPaintMode<3)return;
        if((waitingForPreview&&requestRevision===mapCtrl.generationRevision)||mapCtrl.generationPreviewActive)return;
        previewDirty=mapCtrl.generationMask.length>0;
        if(previewDirty&&!liveTimer.running)liveTimer.start();
    }
    function discardPending() {
        unlockMapStroke();
        liveTimer.stop();previewDirty=false;waitingForPreview=false;library.cancel();
        const mode=mapCtrl.generationPaintMode;mapCtrl.generationPaintMode=0;
        mapCtrl.setGenerationMask([],[]);pins=[];previewCells=[];diagnosticCells=[];frozenStyle=({});blueprintId="";
        mapCtrl.generationPaintMode=mode;
        message="Pending area discarded. The committed map is unchanged.";
    }
    function applyPending() {
        liveTimer.stop();previewDirty=false;
        const mode=mapCtrl.generationPaintMode;
        const result=mapCtrl.applyGenerationPreview();
        if(!result.success){unlockMapStroke();message=result.error;return;}
        mapCtrl.generationPaintMode=0;
        mapCtrl.setGenerationMask([],[]);pins=[];previewCells=[];diagnosticCells=[];frozenStyle=({});blueprintId="";
        mapCtrl.generationPaintMode=mode;
        unlockMapStroke();
        message=mapSessionActive?"Zapisano pociągnięcie. Maluj dalej. Ctrl+Z cofa.":"Applied. Continue painting with the same style. Undo/Redo restores the whole committed area.";
    }
    function stampPrefab(x,y,z,erase) {
        if(!profileReady||!styleId)return;
        if(erase){
            const remaining=pins.filter(pin=>{
                const prefab=brushPrefabs.find(p=>p.name===pin.name);
                if(!prefab||pin.z!==z)return true;
                return !prefab.tiles.some(t=>pin.x+t.dx===x&&pin.y+t.dy===y);
            });
            if(remaining.length!==pins.length){pins=remaining;mapCtrl.invalidateGeneration();queueLivePreview();}
            return;
        }
        const prefab=brushPrefabs[stampPrefabChoice.currentIndex];
        if(!prefab||!prefab.tiles||!prefab.tiles.length){message="Choose a style learned with allowed prefabs, then choose its prefab.";return;}
        let minX=65535,minY=65535,maxX=-65535,maxY=-65535;
        for(const t of prefab.tiles){minX=Math.min(minX,t.dx);minY=Math.min(minY,t.dy);maxX=Math.max(maxX,t.dx);maxY=Math.max(maxY,t.dy);}
        if((maxX-minX+1)*(maxY-minY+1)>100000||x+minX<0||y+minY<0||x+maxX>65535||y+maxY>65535){message="The whole prefab does not fit these coordinates.";return;}
        const mask=mapCtrl.generationMask.slice();const keys=new Set(mask.map(t=>t.x+","+t.y+","+t.z));
        for(let yy=minY;yy<=maxY;++yy)for(let xx=minX;xx<=maxX;++xx){const key=(x+xx)+","+(y+yy)+","+z;if(!keys.has(key)){keys.add(key);mask.push({x:x+xx,y:y+yy,z:z});}}
        if(mask.length>100000){message="The pending area exceeds 100000 cells.";return;}
        if(pins.some(p=>p.name===prefab.name&&p.x===x&&p.y===y&&p.z===z))return;
        pins=pins.concat([{name:prefab.name,x:x,y:y,z:z,recipe:!!prefab.recipe,span:structureSpan}]);
        const paths=mapCtrl.generationPaths.filter(t=>t.z!==z||t.x<x+minX||t.x>x+maxX||t.y<y+minY||t.y>y+maxY);
        mapCtrl.setGenerationMask(mask,paths);queueLivePreview();
    }
    Timer {
        id: liveTimer; interval: 180; repeat: false
        onTriggered: {
            if(!dialog.sessionActive||!livePreview.checked||!dialog.profileReady||!dialog.styleId||!dialog.mapCtrl.generationMask.length){dialog.previewDirty=false;if(dialog.commitAfterStroke)dialog.unlockMapStroke();return;}
            if(dialog.library.busy){dialog.previewDirty=true;return;}
            dialog.generate(false,true);
        }
    }
    function loadBlueprint(entry) {
        const plan = library.load(entry.id).blueprint;
        frozenStyle=plan.styleSnapshot||({});
        const index = styles.findIndex(s => s.id === plan.styleId);
        if (index < 0) { message = "The style for this blueprint is unavailable in this profile."; return; }
        styleChoice.currentIndex = index;
        seed.value = plan.seed; replace.checked = !!plan.replace; monsters.checked = !!plan.monsters;
        spacing.value = plan.spacing === undefined ? 1 : plan.spacing;
        proceduralPlan=!!plan.brush && plan.brush.walkable!==false;
        if(plan.brush){brushWidth.value=plan.brush.width||7;roughness.value=plan.brush.roughness||0;connectRooms.checked=plan.brush.connectRooms!==false;}
        pins = plan.pins || []; blueprintId = entry.id; resourceName.text = entry.name;
        advanced.checked=true;
        mapCtrl.setGenerationMask(plan.mask, plan.paths || []);
        if (plan.mask.length) mapCtrl.centerOnTile(plan.mask[0].x, plan.mask[0].y, plan.mask[0].z);
        tab.currentIndex = 2;
        message = "Saved plan loaded. Continue with the brush or edit its area and prefab placements.";
    }
    onOpened: {
        profileReady=library.selectProfile(clientKey);
        if (!profileReady) { library.cancel();message = library.status; }
        prefabModel = mapCtrl.generationPrefabs();
        if(!mapSessionActive)tab.currentIndex=styles.length?2:0;
    }
    onClosed: {
        if(!mapSessionActive){liveTimer.stop();previewDirty=false;mapCtrl.generationPaintMode=0;library.cancel();waitingForPreview=false;}
    }
    onClientKeyChanged: {
        if(mapSessionActive)endOnMap();
        if (visible) { library.cancel(); waitingForPreview = false; mapCtrl.clearGenerationPreview(); message = "Client changed. Reopen the library to select its resources."; close(); }
    }
    Connections {
        target: dialog.library
        function onStateChanged() {
            if(!dialog.library.busy&&dialog.previewDirty&&dialog.sessionActive)liveTimer.restart();
        }
        function onResultChanged() {
            const result = dialog.library.result;
            if (!result || result.success === undefined) return;
            if(dialog.sessionActive && !dialog.profileReady) {dialog.profileReady=dialog.library.selectProfile(dialog.clientKey);return;}
            if(dialog.quickLearning&&!dialog.library.busy){
                dialog.quickLearning=false;
                if(result.success){
                    const index=dialog.styles.findIndex(e=>e.id===result.id);
                    if(index>=0)dialog.chooseStyle(index);
                    dialog.message="Wzorzec gotowy. Wybierz korytarz lub komnatę i maluj LPM na mapie.";
                }else dialog.message=String(result.error).indexOf("3 x 3")>=0
                    ? "Zaznacz większy fragment podłoża — wzorzec potrzebuje co najmniej pełnego obszaru 3×3."
                    : result.error;
                return;
            }
            if (dialog.waitingForPreview && result.operation === "generate") {
                dialog.waitingForPreview = false;
                if (result.success) {
                    const preview = dialog.mapCtrl.setGenerationPreview(result.patch, dialog.requestRevision);
                    dialog.previewCells=preview.success?result.patch.tiles||[]:[];dialog.diagnosticCells=[];
                    dialog.message = preview.success ? "Preview ready. Apply commits all changes as one undo step." : preview.error;
                    if (result.diagnostics && result.diagnostics.length) dialog.message += "\n" + result.diagnostics.slice(0, 5).join("\n");
                    if(dialog.mapSessionActive&&dialog.commitAfterStroke){
                        if(preview.success)dialog.applyPending();else dialog.unlockMapStroke();
                    }
                } else {dialog.unlockMapStroke();dialog.message=result.error;
                    dialog.diagnosticCells=(result.diagnostics||[]).map(s=>{const p=s.split(",");return {x:Number(p[0]),y:Number(p[1]),z:Number(p[2])};});
                    if(result.diagnostics&&result.diagnostics.length)dialog.message+=" · "+result.diagnostics.join("; ");
                }
            } else {
                if(!result.success&&!dialog.library.busy&&dialog.commitAfterStroke&&!dialog.previewDirty)dialog.unlockMapStroke();
                dialog.message=result.success?"Resource saved. You can now use it from the library.":result.error;
            }
        }
    }
    Connections {
        target: dialog.mapCtrl
        function onGenerationMaskChanged() { dialog.queueLivePreview(); }
        function onGenerationStrokeFinished() { dialog.finishMapStroke(); }
        function onGenerationBrushStopped() {if(dialog.mapSessionActive)dialog.endOnMap();}
        function onGenerationPrefabRequested(x,y,z,erase) { dialog.stampPrefab(x,y,z,erase); }
        function onGenerationInvalidated() {
            dialog.previewCells=[];dialog.diagnosticCells=[];
            if (dialog.waitingForPreview) {dialog.library.cancel();dialog.waitingForPreview=false;dialog.message="Mapa się zmieniła — odświeżam wynik.";}
            if(dialog.mapSessionActive&&dialog.commitAfterStroke&&!dialog.previewDirty&&!dialog.library.busy)dialog.unlockMapStroke();
        }
        function onReadersChanged() {if(dialog.mapSessionActive)dialog.endOnMap();liveTimer.stop();dialog.previewDirty=false;dialog.library.cancel();dialog.mapCtrl.generationPaintMode=0;dialog.mapCtrl.clearGenerationPreview(); }
    }
    Connections {
        target: Backend.brushStore
        function onBrushesChanged() {
            dialog.library.cancel();dialog.waitingForPreview = false;
            dialog.mapCtrl.invalidateGeneration(); dialog.prefabModel = dialog.mapCtrl.generationPrefabs();
        }
    }
    contentItem: ColumnLayout {
        spacing: 8
        RowLayout {
            Layout.fillWidth: true
            DmeTextField { id: resourceName; Layout.fillWidth: true; placeholderText: "Resource name"; text: "My map style" }
            DmeButton { text: "Close"; onClicked: dialog.close() }
        }
        TabBar {
            id: tab; Layout.fillWidth: true
            TabButton { text: "Examples" }
            TabButton { text: "Styles" }
            TabButton { text: "Brush" }
            TabButton { text: "Prefabs" }
            TabButton { text: "Legacy" }
            TabButton { text: "Struktury" }
        }
        StackLayout {
            currentIndex: tab.currentIndex; enabled: dialog.profileReady; Layout.fillWidth: true; Layout.fillHeight: true
            ColumnLayout {
                Label { text: "Select a finished map fragment, then save a copied example."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                DmeButton { text: "Capture selected map fragment"; enabled: !dialog.library.busy && dialog.mapCtrl.selectionCount > 0
                    onClicked: dialog.report(dialog.library.capture(resourceName.text, dialog.mapCtrl.generationExample())) }
                ScrollView {
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                    Column {
                        width: parent.width
                        Repeater {
                            model: dialog.examples
                            delegate: RowLayout {
                                required property var modelData
                                width: parent.width
                                CheckBox {
                                    Layout.fillWidth: true; text: modelData.name + " · " + modelData.tileCount + " cells"
                                    checked: dialog.selectedExamples.indexOf(modelData.id) >= 0
                                    onToggled: dialog.selectedExamples = dialog.toggle(dialog.selectedExamples, modelData.id, checked)
                                }
                                DmeButton { text: "Delete"; enabled: !dialog.library.busy; onClicked: dialog.report(dialog.library.remove(modelData.id)) }
                            }
                        }
                    }
                }
                Label { text: "Import only a chosen OTBM fragment:" }
                RowLayout {
                    Label { text: "X" } SpinBox { id: importX; from: 0; to: 65535; value: 100; editable: true; Layout.preferredWidth: 100 }
                    Label { text: "Y" } SpinBox { id: importY; from: 0; to: 65535; value: 100; editable: true; Layout.preferredWidth: 100 }
                    Label { text: "Z" } SpinBox { id: importZ; from: 0; to: 15; value: 7; editable: true; Layout.preferredWidth: 85 }
                }
                RowLayout {
                    Label { text: "Width" } SpinBox { id: importW; from: 3; to: 200; value: 32; editable: true; Layout.preferredWidth: 95 }
                    Label { text: "Height" } SpinBox { id: importH; from: 3; to: 200; value: 32; editable: true; Layout.preferredWidth: 95 }
                    Label { text: "Floors" } SpinBox { id: importFloors; from: 1; to: 16; value: 1; Layout.preferredWidth: 75 }
                }
                DmeButton { text: "Choose OTBM and import fragment…"; enabled: !dialog.library.busy; onClicked: sourceFile.open() }
            }
            ColumnLayout {
                Label { text: "Check examples and allowed prefabs, then learn a reusable local style. Buildings remain complete prefabs."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                DmeButton {
                    text: "Learn style from checked examples"; enabled: !dialog.library.busy && dialog.selectedExamples.length > 0
                    onClicked: {
                        const prefabs = dialog.prefabModel.filter(p => dialog.selectedPrefabs.indexOf(p.name) >= 0);
                        if (!dialog.library.learnStyle(resourceName.text, dialog.selectedExamples, prefabs, {procedural:true,recipes:dialog.recipes.map(r=>dialog.library.load(r.id).recipe)})) dialog.message = dialog.library.status;
                    }
                }
                ListView {
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true; model: dialog.styles
                    delegate: RowLayout {
                        required property var modelData; width: ListView.view.width
                        Label { text: modelData.name; Layout.fillWidth: true }
                        DmeButton { text: "Use"; onClicked: {dialog.mapCtrl.invalidateGeneration();styleChoice.currentIndex = dialog.styles.findIndex(e => e.id === modelData.id);const model=dialog.library.load(modelData.id).model;spacing.value=model ? model.decorationSpacing || 1 : 1;tab.currentIndex = 2;} }
                        DmeButton { text: "Delete"; enabled: !dialog.library.busy; onClicked: dialog.report(dialog.library.remove(modelData.id)) }
                    }
                }
                Label { text: "Styles and samples belong to the loaded client profile. No internet or external generator is required."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                ColumnLayout {
                    width: dialog.availableWidth - 16; spacing: 8
                    Label { text: "Choose a style, paint corridors or rooms, then Apply. Prefabs and saved plans use the same preview."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    ComboBox { id: styleChoice; Layout.fillWidth: true; model: dialog.styles; textRole: "name"; onActivated: {dialog.mapCtrl.invalidateGeneration();const model=dialog.library.load(dialog.styleId).model;spacing.value=model ? model.decorationSpacing || 1 : 1;dialog.queueLivePreview();} }
                    RowLayout {
                        DmeButton { objectName: "proceduralCorridor"; text: "Corridor"; checked: dialog.mapCtrl.generationPaintMode === 3; enabled: !!dialog.styleId; onClicked: dialog.armBrush(checked?0:3) }
                        DmeButton { objectName: "proceduralRoom"; text: "Room"; checked: dialog.mapCtrl.generationPaintMode === 4; enabled: !!dialog.styleId; onClicked: {if(!checked&&brushWidth.value<11)brushWidth.value=17;dialog.armBrush(checked?0:4);} }
                        DmeButton { text: "Prefab"; checked: dialog.mapCtrl.generationPaintMode === 5; enabled: dialog.brushPrefabs.length>0; onClicked: dialog.armBrush(checked?0:5) }
                        DmeButton { text: "Stop"; onClicked: dialog.mapCtrl.generationPaintMode=0 }
                    }
                    RowLayout {
                        Label { text: "Brush width" }
                        SpinBox { id: brushWidth; objectName: "proceduralWidth"; from: 3; to: 63; value: 7; editable: true; onValueModified: dialog.mapCtrl.generationBrushWidth=value }
                        Label { text: "Irregularity" }
                        SpinBox { id: roughness; from: 0; to: 100; value: 35; editable: true; onValueModified: dialog.mapCtrl.generationBrushRoughness=value }
                    }
                    CheckBox { id: livePreview; objectName: "proceduralLivePreview"; text: "Update preview while painting"; checked: true; onToggled: {if(checked)dialog.queueLivePreview();else{liveTimer.stop();dialog.previewDirty=false;}} }
                    CheckBox { id: connectRooms; text: "Connect new rooms to the pending area"; checked: true; onToggled: dialog.mapCtrl.generationConnectRooms=checked }
                    ComboBox { id: stampPrefabChoice; Layout.fillWidth: true; visible: dialog.brushPrefabs.length>0; model: dialog.brushPrefabs; textRole: "name" }
                    CheckBox { id: advanced; objectName: "proceduralAdvanced"; text: "Area editing and saved plans" }
                    RowLayout {
                        visible: advanced.checked
                        DmeButton { text: "Paint mask"; checked: dialog.mapCtrl.generationPaintMode === 1; onClicked: dialog.mapCtrl.generationPaintMode = checked ? 0 : 1 }
                        DmeButton { text: "Paint path"; checked: dialog.mapCtrl.generationPaintMode === 2; onClicked: dialog.mapCtrl.generationPaintMode = checked ? 0 : 2 }
                        DmeButton { text: "Stop"; onClicked: dialog.mapCtrl.generationPaintMode = 0 }
                    }
                    Label { text: dialog.mapCtrl.generationMask.length + " cells · " + dialog.mapCtrl.generationPaths.length + " path cells. Left drag adds; right drag erases."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    RowLayout {
                        visible: advanced.checked
                        DmeButton { text: "Mask from selection"; onClicked: dialog.mapCtrl.generationMaskFromSelection(false) }
                        DmeButton { text: "Extend with selection"; onClicked: dialog.mapCtrl.generationMaskFromSelection(true) }
                        DmeButton { text: "Clear"; onClicked: { dialog.mapCtrl.setGenerationMask([], []); dialog.pins = []; } }
                    }
                    RowLayout {
                        Label { text: "Seed" } SpinBox { id: seed; from: 1; to: 2147483646; value: 1; editable: true; Layout.preferredWidth: 175; onValueModified: {dialog.mapCtrl.generationBrushSeed=value;dialog.mapCtrl.invalidateGeneration();dialog.queueLivePreview();} }
                        Label { text: "Decor spacing" } SpinBox { id: spacing; from: 0; to: 16; value: 1; onValueModified: {dialog.mapCtrl.invalidateGeneration();dialog.queueLivePreview();} }
                    }
                    CheckBox { id: replace; text: "Replace contents inside the mask (default: preserve)"; onToggled: {dialog.mapCtrl.invalidateGeneration();dialog.queueLivePreview();} }
                    CheckBox { id: monsters; text: "Include learned monsters and spawns · NPCs stay manual"; onToggled: {dialog.mapCtrl.invalidateGeneration();dialog.queueLivePreview();} }
                    RowLayout {
                        DmeButton { text: "Preview"; enabled: !dialog.library.busy && !!dialog.styleId; onClicked: dialog.generate() }
                        DmeButton { text: "New variant"; enabled: !dialog.library.busy && !!dialog.styleId; onClicked: {seed.value = seed.value >= seed.to ? 1 : seed.value + 1; dialog.generate();} }
                        DmeButton { text: "Apply"; enabled: !dialog.library.busy && dialog.mapCtrl.generationPreviewActive; onClicked: dialog.applyPending() }
                        DmeButton { text: "Discard"; onClicked: dialog.discardPending() }
                    }
                    DmeButton { visible: advanced.checked; text: "Regenerate selected part of preview"; enabled: !dialog.library.busy && dialog.mapCtrl.generationPreviewActive && dialog.mapCtrl.selectionCount > 0; onClicked: {seed.value = seed.value >= seed.to ? 1 : seed.value + 1; dialog.generate(true);} }
                    RowLayout {
                        visible: advanced.checked
                        DmeButton { text: dialog.blueprintId ? "Update saved plan" : "Save plan"; enabled: !dialog.library.busy; onClicked: dialog.report(dialog.library.saveBlueprint(resourceName.text, dialog.blueprint(), dialog.blueprintId)) }
                        DmeButton { text: "Save as new"; enabled: !dialog.library.busy; onClicked: {dialog.blueprintId="";dialog.report(dialog.library.saveBlueprint(resourceName.text, dialog.blueprint()));} }
                    }
                    Repeater {
                        model: advanced.checked ? dialog.blueprints : []
                        delegate: RowLayout {
                            required property var modelData; Layout.fillWidth: true
                            DmeButton { text: modelData.name; Layout.fillWidth: true; onClicked: dialog.loadBlueprint(modelData) }
                            DmeButton { text: "Delete"; enabled: !dialog.library.busy; onClicked: dialog.report(dialog.library.remove(modelData.id)) }
                        }
                    }
                }
            }
            ColumnLayout {
                Label { text: "Select an entire building across its floors to save a prefab. Check which prefabs the style may use; exact matches in examples teach their frequency."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                DmeButton { text: "Save selection as prefab"; enabled: !dialog.library.busy && dialog.mapCtrl.selectionCount > 0; onClicked: dialog.report(dialog.mapCtrl.saveSelectionAsPrefab(resourceName.text, "Generation Library")) }
                ScrollView {
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                    Column {
                        width: parent.width
                        Repeater {
                            model: dialog.prefabModel
                            delegate: CheckBox {
                                required property var modelData
                                text: modelData.name + " · " + modelData.palette
                                checked: dialog.selectedPrefabs.indexOf(modelData.name) >= 0
                                onToggled: dialog.selectedPrefabs = dialog.toggle(dialog.selectedPrefabs, modelData.name, checked)
                            }
                        }
                    }
                }
                Label { text: "Pin a prefab at an exact position in the blueprint:" }
                ComboBox { id: pinPrefab; Layout.fillWidth: true; model: dialog.prefabModel; textRole: "name" }
                RowLayout {
                    SpinBox { id: pinX; from: 0; to: 65535; value: Math.max(0, dialog.mapCtrl.hoverX); editable: true; Layout.preferredWidth: 125 }
                    SpinBox { id: pinY; from: 0; to: 65535; value: Math.max(0, dialog.mapCtrl.hoverY); editable: true; Layout.preferredWidth: 125 }
                    SpinBox { id: pinZ; from: 0; to: 15; value: dialog.mapCtrl.floor; editable: true; Layout.preferredWidth: 85 }
                    DmeButton { text: "Pin"; enabled: pinPrefab.currentIndex >= 0; onClicked: { dialog.pins = dialog.pins.concat([{name:dialog.prefabModel[pinPrefab.currentIndex].name,x:pinX.value,y:pinY.value,z:pinZ.value}]); dialog.mapCtrl.invalidateGeneration(); } }
                }
                Repeater {
                    model: dialog.pins
                    delegate: RowLayout {
                        required property var modelData; required property int index
                        Label { text: modelData.name + " @ " + modelData.x + ", " + modelData.y + ", " + modelData.z; Layout.fillWidth: true }
                        DmeButton { text: "Unpin"; onClicked: { dialog.pins = dialog.pins.filter((p,i) => i !== index); dialog.mapCtrl.invalidateGeneration(); } }
                    }
                }
            }
            ColumnLayout {
                Label { text: "Existing Tile Map Generator recipes and AI blueprints retain their original format and engine."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                ListView {
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true; model: Backend.tileMapGenProfiles.profiles
                    delegate: Label { required property var modelData; width: ListView.view.width; text: "Legacy · " + modelData.name }
                }
                DmeButton { text: "Open legacy Tile Map Generator…"; onClicked: dialog.legacyRequested() }
            }
            ColumnLayout {
                Label {Layout.fillWidth: true;wrapMode: Text.WordWrap;text: "Zbuduj strukturę z całych prefabów. Przesunięcia liczymy od miejsca kliknięcia."}
                ComboBox {id: recipePrefab;Layout.fillWidth: true;model: dialog.prefabModel;textRole: "name"}
                RowLayout {
                    Label {text: "X"} SpinBox {id: partX;from: -1024;to: 1024;editable: true;Layout.preferredWidth: 90}
                    Label {text: "Y"} SpinBox {id: partY;from: -1024;to: 1024;editable: true;Layout.preferredWidth: 90}
                    Label {text: "Piętro"} SpinBox {id: partZ;from: -15;to: 15;editable: true;Layout.preferredWidth: 85}
                }
                ComboBox {id: repeatPart;Layout.fillWidth: true;model: ["Stała część", "Powtarzany odcinek mostu", "Końcowa część przesuwana z mostem"]}
                RowLayout {
                    Label {text: "Krok X"} SpinBox {id: stepX;from: -64;to: 64;value: 1;editable: true;Layout.preferredWidth: 85}
                    Label {text: "Krok Y"} SpinBox {id: stepY;from: -64;to: 64;editable: true;Layout.preferredWidth: 85}
                    DmeButton {objectName: "recipePartAdd";text: "Dodaj część";enabled: recipePrefab.currentIndex>=0;onClicked: {
                        dialog.recipeParts=dialog.recipeParts.concat([{prefab:dialog.prefabModel[recipePrefab.currentIndex].name,dx:partX.value,dy:partY.value,dz:partZ.value,repeat:repeatPart.currentIndex===1,afterSpan:repeatPart.currentIndex===2,stepX:stepX.value,stepY:stepY.value,stepZ:0}]);
                    }}
                }
                ScrollView {Layout.fillWidth: true;Layout.fillHeight: true;clip: true
                    Column {width: parent.width
                        Repeater {model: dialog.recipeParts
                            delegate: RowLayout {required property var modelData;required property int index;width: parent.width
                                Label {Layout.fillWidth: true;text: modelData.prefab+" · "+modelData.dx+", "+modelData.dy+", "+modelData.dz+(modelData.repeat?" · odcinek":modelData.afterSpan?" · koniec":"")}
                                DmeButton {text: "Usuń";onClicked: dialog.recipeParts=dialog.recipeParts.filter((p,i)=>i!==index)}
                            }
                        }
                    }
                }
                CheckBox {id: showConnections;text: "Sprawdzaj połączenia przejść"}
                GridLayout {visible: showConnections.checked;columns: 4
                    Label {text: "Wejście"}
                    SpinBox {id: portAX;from: -1024;to: 1024;editable: true;Layout.preferredWidth: 85}
                    SpinBox {id: portAY;from: -1024;to: 1024;editable: true;Layout.preferredWidth: 85}
                    SpinBox {id: portAZ;from: -15;to: 15;editable: true;Layout.preferredWidth: 85}
                    Label {text: "Wyjście"}
                    SpinBox {id: portBX;from: -1024;to: 1024;editable: true;Layout.preferredWidth: 85}
                    SpinBox {id: portBY;from: -1024;to: 1024;editable: true;Layout.preferredWidth: 85}
                    SpinBox {id: portBZ;from: -15;to: 15;editable: true;Layout.preferredWidth: 85}
                    DmeButton {Layout.columnSpan: 4;text: "Dodaj połączenie ("+dialog.recipeConnections.length+")";onClicked: dialog.recipeConnections=dialog.recipeConnections.concat([{from:{dx:portAX.value,dy:portAY.value,dz:portAZ.value},to:{dx:portBX.value,dy:portBY.value,dz:portBZ.value}}])}
                }
                DmeButton {objectName: "recipeSave";text: "Zapisz strukturę i dodaj do wzorca";enabled: !!dialog.styleId&&dialog.recipeParts.length>0&&!dialog.library.busy;onClicked: {
                    const saved=dialog.library.saveRecipe(resourceName.text,{parts:dialog.recipeParts,connections:dialog.recipeConnections});
                    if(!saved.success){dialog.message=saved.error;return;}
                    const attached=dialog.library.attachRecipe(dialog.styleId,saved.id,dialog.prefabModel);
                    if(attached.success){dialog.chooseStyle(dialog.styles.findIndex(s=>s.id===attached.id));dialog.recipeParts=[];dialog.recipeConnections=[];dialog.message="Struktura zapisana. Wybierz tryb Struktura w topbarze.";}else dialog.message=attached.error;
                }}
                RowLayout {
                    DmeButton {text: "Eksportuj bibliotekę…";onClicked: packageExport.open()}
                    DmeButton {text: "Importuj bibliotekę…";onClicked: packageImport.open()}
                }
            }
        }
        ProgressBar { Layout.fillWidth: true; visible: dialog.library.busy; value: dialog.library.progress / 100 }
        RowLayout {
            Label { Layout.fillWidth: true; text: dialog.library.busy ? dialog.library.status : dialog.message; wrapMode: Text.WordWrap; maximumLineCount: 5 }
            DmeButton { text: "Cancel"; visible: dialog.library.busy; onClicked: {dialog.waitingForPreview=false;dialog.library.cancel();} }
        }
    }
    signal legacyRequested()
    FileDialog {id: packageExport;title: "Eksport biblioteki proceduralnej";fileMode: FileDialog.SaveFile;nameFilters: ["Biblioteka DME (*.json)"];onAccepted: dialog.report(dialog.library.exportPackage(selectedFile.toString()))}
    FileDialog {id: packageImport;title: "Import biblioteki proceduralnej";nameFilters: ["Biblioteka DME (*.json)"];onAccepted: dialog.report(dialog.library.importPackage(selectedFile.toString()))}
    FileDialog {
        id: sourceFile; title: "Import a map example"; nameFilters: ["Tibia maps (*.otbm)"]
        onAccepted: {
            const materials = dialog.mapCtrl.generationContext();
            if (!materials.success) { dialog.message = materials.error; return; }
            if (!dialog.library.importExample(resourceName.text, selectedFile.toString(),
                {x:importX.value,y:importY.value,z:importZ.value,width:importW.value,height:importH.value,floors:importFloors.value}, materials)) dialog.message = dialog.library.status;
        }
    }
}
