import QtQuick
import QtQuick.Controls
import QtTest
import Tibia 1.0
import "../../editor/qml/dialogs" as Dialogs
import "../../editor/qml/components" as Components
Item {
    width: 1000; height: 900
    FontLoader {source: Qt.platform.os === "windows" ? "file:///C:/Windows/Fonts/segoeui.ttf" : ""}
    QtObject {
        id: map
        property int generationPaintMode: 0
        property bool generationInputBlocked: false
        property bool selectionMode: false
        signal generationBrushStopped()
        property int generationBrushWidth: 7
        property int generationBrushRoughness: 35
        property int generationBrushSeed: 1
        property bool generationConnectRooms: true
        property int applyCount: 0
        signal generationStrokeFinished()
        signal generationPrefabRequested(int x,int y,int z,bool erase)
        function applyGenerationPreview() {applyCount++;generationPreviewActive=false;invalidateGeneration();return {success:true};}
        property var generationMask: [{x:10,y:10,z:7}]
        property var generationPaths: []
        property bool generationPreviewActive: false
        property int selectionCount: 1
        property int generationRevision: 12
        property int floor: 7
        property int tileSize: 32
        signal contentUpdated()
        signal generationPreviewChanged()
        onGenerationPreviewActiveChanged: generationPreviewChanged()
        function renderOriginX() { return 0; }
        function renderOriginY() { return 0; }
        property int hoverX: 10
        property int hoverY: 10
        property int appliedPreviews: 0
        property int lastRevision: 0
        signal generationInvalidated()
        signal readersChanged()
        function forceActiveFocus() {}
        function generationCursorFootprint() {return generationPaintMode>=3&&generationPaintMode<=4?[{x:hoverX,y:hoverY,z:floor}]:[];}
        function generationPrefabs() { return [{name:"House",palette:"Buildings",tiles:[]}]; }
        function generationContext(refine) { return {success:true,revision:generationRevision,targetMask:generationMask}; }
        function setGenerationPreview(patch,revision) { lastRevision=revision;appliedPreviews++;generationPreviewActive=true;return {success:true}; }
        function clearGenerationPreview() { generationPreviewActive=false; }
        function invalidateGeneration() { generationRevision++;clearGenerationPreview();generationInvalidated(); }
        function setGenerationMask(mask,paths) { invalidateGeneration();generationMask=mask;generationPaths=paths; }
        function generationMaskFromSelection(extend) {}
        function centerOnTile(x,y,z) {}
        function generationExample() { return {success:true,tiles:[]}; }
        function saveSelectionAsPrefab(name,palette) { return {success:true}; }
    }
    Components.GenerationMaskOverlay { id: maskOverlay; anchors.fill: parent; mapView: map }
    Components.ProceduralBrushBar {id: mapBar;width: parent.width;session: dialog;mapView: map}
    Dialogs.GenerationLibraryDialog { id: dialog; mapCtrl: map; clientKey: "test-profile" }
    TestCase {
        name: "GenerationLibrary"; when: windowShown
        function init() {
            Backend.generationLibrary.entries=[{id:"style",kind:"style",name:"Forest"}];
            Backend.generationLibrary.result={};Backend.generationLibrary.busy=false;
            dialog.previewCells=[];dialog.diagnosticCells=[];dialog.frozenStyle=({});dialog.structureSpan=1;dialog.recipeParts=[];dialog.recipeConnections=[];dialog.mapSessionActive=false;dialog.quickLearning=false;dialog.commitAfterStroke=false;map.generationInputBlocked=false;map.generationPaintMode=0;map.applyCount=0;dialog.pins=[];dialog.previewDirty=false;map.generationMask=[{x:10,y:10,z:7}];map.generationPaths=[];map.generationRevision=12;map.appliedPreviews=0;map.generationPreviewActive=false;dialog.open();dialog.x=0;dialog.y=0;wait(30);
        }
        function cleanup() {if(dialog.mapSessionActive)dialog.endOnMap();dialog.close();}
        function test_on_map_stroke_commits_without_apply_button() {
            dialog.startOnMap();verify(dialog.mapSessionActive);verify(!dialog.visible);verify(mapBar.visible);
            compare(map.generationPaintMode,3);verify(!map.selectionMode);
            map.setGenerationMask([{x:20,y:20,z:7}],[]);map.generationStrokeFinished();
            verify(map.generationInputBlocked);tryCompare(Backend.generationLibrary,"busy",true,1000);
            verify(Backend.generationLibrary.lastBlueprint.brush.procedural);
            verify(!Backend.generationLibrary.lastBlueprint.brush.rawLayout);verify(!Backend.generationLibrary.lastBlueprint.brush.terrainPaint);
            Backend.generationLibrary.busy=false;
            Backend.generationLibrary.result={success:true,operation:"generate",patch:{success:true}};
            compare(map.applyCount,1);verify(!map.generationInputBlocked);compare(map.generationMask.length,0);compare(map.generationPaintMode,3);
        }
        function test_on_map_preview_commits_only_after_release() {
            dialog.startOnMap();map.setGenerationMask([{x:20,y:20,z:7}],[]);
            tryCompare(Backend.generationLibrary,"busy",true,1000);
            Backend.generationLibrary.busy=false;Backend.generationLibrary.result={success:true,operation:"generate",patch:{success:true}};
            compare(map.applyCount,0);verify(map.generationPreviewActive);
            map.generationStrokeFinished();compare(map.applyCount,1);verify(!map.generationInputBlocked);
        }
        function test_on_map_failure_unlocks_next_stroke() {
            dialog.startOnMap();map.setGenerationMask([{x:20,y:20,z:7}],[]);map.generationStrokeFinished();
            tryCompare(Backend.generationLibrary,"busy",true,1000);
            Backend.generationLibrary.busy=false;Backend.generationLibrary.result={success:false,operation:"generate",error:"Test conflict"};
            compare(map.applyCount,0);verify(!map.generationInputBlocked);compare(dialog.message,"Test conflict");
        }
        function test_on_map_source_uses_current_selection() {
            dialog.startOnMap();dialog.pickMapExample();verify(map.selectionMode);compare(map.generationPaintMode,0);
            dialog.learnMapSelection();verify(dialog.quickLearning);verify(Backend.generationLibrary.busy);
            compare(Backend.generationLibrary.learnedExamples[0],"captured-example");
            Backend.generationLibrary.entries=[{id:"new-style",kind:"style",name:"Nowy wzorzec",model:{prefabs:[]}}];
            Backend.generationLibrary.busy=false;Backend.generationLibrary.result={success:true,operation:"learn",id:"new-style"};
            compare(dialog.styleId,"new-style");compare(map.generationPaintMode,3);verify(!map.selectionMode);verify(!dialog.visible);
        }
        function test_icon_actions_keep_accessible_names_and_style_label() {
            dialog.startOnMap();
            for(const name of ["mapBrushToggle","mapBrushCorridor","mapBrushRoom","mapBrushPrefab","pickMapBrushSource","learnMapBrushSource","mapBrushOptions","mapBrushLibrary","closeMapBrush"]){
                const button=findChild(mapBar,name);verify(button);
                compare(button.display,AbstractButton.IconOnly);verify(button.icon.source.toString().endsWith(".svg"));
                verify(button.Accessible.name.length>0);verify(button.helpText.length>0);
            }
            const style=findChild(mapBar,"mapBrushStyle");compare(style.display,AbstractButton.TextBesideIcon);compare(style.text,"Forest");
        }
        function test_style_picker_arms_selected_style_and_closes() {
            Backend.generationLibrary.entries=[{id:"first",kind:"style",name:"Las"},{id:"second",kind:"style",name:"Śnieg"}];
            dialog.startOnMap();findChild(mapBar,"mapBrushStyle").clicked();
            const picker=findChild(mapBar,"mapBrushStylePicker");tryCompare(picker,"visible",true);
            findChild(picker.contentItem,"mapBrushStyleCard1").clicked();
            compare(dialog.styleId,"second");compare(map.generationPaintMode,3);verify(!picker.visible);
        }
        function test_escape_closes_picker_before_stopping_brush() {
            dialog.startOnMap();findChild(mapBar,"mapBrushStyle").clicked();
            const picker=findChild(mapBar,"mapBrushStylePicker");tryCompare(picker,"visible",true);
            keyClick(Qt.Key_Escape);tryCompare(picker,"visible",false);verify(dialog.mapSessionActive);
        }
        function test_live_preview_requested_during_continuous_stroke() {
            dialog.startOnMap();Backend.generationLibrary.lastBlueprint={};
            for(let i=0;i<6;++i){map.setGenerationMask([{x:20+i,y:20,z:7}],[]);wait(40);}
            verify(!!Backend.generationLibrary.lastBlueprint.mask,"Continuous movement postponed all preview requests until release");
            compare(map.applyCount,0);
        }
        function test_toolbar_wraps_on_narrow_window() {
            dialog.startOnMap();mapBar.width=460;wait(30);
            const source=findChild(mapBar,"learnMapBrushSource");
            verify(source.mapToItem(mapBar,source.width,0).x<=mapBar.width);
            verify(mapBar.height<240);mapBar.width=1000;
        }
        function test_stroke_outline_has_no_internal_tile_grid() {
            const edges=maskOverlay.outline([{x:1,y:1,z:7},{x:2,y:1,z:7}]);
            compare(edges.length,6);
            verify(!edges.some(e=>e.x===2&&e.y===1&&e.dx===0));
        }
        function test_toolbar_available_before_activation() {
            dialog.endOnMap();verify(mapBar.visible);verify(!mapBar.sessionActive);
            verify(findChild(mapBar,"mapBrushToggle").visible);
            verify(!findChild(mapBar,"mapBrushStyle").visible);
        }
        function test_render_compact_map_bar() {
            dialog.startOnMap();mapBar.x=0;mapBar.y=0;wait(30);
            verify(mapBar.height<150);compare(mapBar.width,1000);
            grabImage(mapBar).save(Qt.resolvedUrl("../../build/procedural-map-bar.png").toString().replace("file:///", ""));
        }
        function test_on_map_escape_ends_session() {
            dialog.startOnMap();map.generationBrushStopped();verify(!dialog.mapSessionActive);verify(!mapBar.sessionActive);compare(map.generationPaintMode,0);
        }
        function test_live_preview_keeps_brush_armed() {
            dialog.armBrush(3);
            map.setGenerationMask([{x:20,y:20,z:7},{x:21,y:20,z:7}],[]);
            map.generationStrokeFinished();
            tryCompare(Backend.generationLibrary,"busy",true,1000);
            compare(map.generationPaintMode,3);compare(Backend.generationLibrary.lastBlueprint.mask.length,2);
            Backend.generationLibrary.busy=false;
            Backend.generationLibrary.result={success:true,operation:"generate",patch:{success:true},diagnostics:[]};
            compare(map.generationPaintMode,3);compare(map.appliedPreviews,1);verify(map.generationPreviewActive);
        }
        function test_changed_stroke_supersedes_old_live_result() {
            dialog.armBrush(3);map.setGenerationMask([{x:20,y:20,z:7}],[]);
            tryCompare(Backend.generationLibrary,"busy",true,1000);
            map.setGenerationMask([{x:21,y:20,z:7}],[]);
            Backend.generationLibrary.result={success:true,operation:"generate",patch:{success:true}};
            compare(map.appliedPreviews,0);
            tryCompare(Backend.generationLibrary,"busy",true,1000);
            compare(Backend.generationLibrary.lastBlueprint.mask[0].x,21);
        }
        function test_apply_clears_pending_area_and_keeps_style_brush() {
            dialog.armBrush(4);map.generationPreviewActive=true;
            dialog.applyPending();compare(map.applyCount,1);compare(map.generationPaintMode,4);
            compare(map.generationMask.length,0);compare(dialog.pins.length,0);compare(dialog.styleId,"style");
        }
        function test_discard_removes_only_pending_plan() {
            dialog.armBrush(3);dialog.pins=[{name:"House",x:20,y:20,z:7}];
            dialog.discardPending();compare(map.generationMask.length,0);compare(dialog.pins.length,0);
            compare(map.generationPaintMode,3);compare(map.applyCount,0);
        }
        function test_prefab_stamp_extends_same_plan() {
            Backend.generationLibrary.entries=[{id:"style",kind:"style",name:"Forest",model:{prefabs:[{name:"House",tiles:[{dx:0,dy:0,dz:0,items:[100]},{dx:1,dy:0,dz:-1,items:[100]}]}]}}];
            dialog.armBrush(5);map.generationPrefabRequested(20,20,7,false);
            compare(dialog.pins.length,1);compare(dialog.pins[0].name,"House");
            compare(map.generationMask.length,3);compare(dialog.blueprint().brush.width,map.generationBrushWidth);
            map.generationPrefabRequested(20,20,7,true);compare(dialog.pins.length,0);
        }
        function test_mask_overlay_updates_spatial_index() {
            verify(!!maskOverlay.cellRows["7:10"][10]);
            map.generationMask=[{x:15,y:16,z:7}];map.generationPaths=[{x:15,y:16,z:7}];
            verify(!!maskOverlay.cellRows["7:16"][15]);verify(!!maskOverlay.pathRows["7:16"][15]);
            verify(!maskOverlay.cellRows["7:10"]);
        }
        function test_render_library() {
            const rendered=grabImage(dialog.contentItem);
            verify(rendered.width > 400);rendered.save(Qt.resolvedUrl("../../build/generation-library.png").toString().replace("file:///", ""));
        }
        function test_preview_uses_captured_revision() {
            dialog.generate();verify(Backend.generationLibrary.busy);
            compare(Backend.generationLibrary.lastBlueprint.mask.length,1);
            Backend.generationLibrary.busy=false;
            Backend.generationLibrary.result={success:true,operation:"generate",patch:{success:true},diagnostics:[]};
            compare(map.appliedPreviews,1);compare(map.lastRevision,12);verify(map.generationPreviewActive);
        }
        function test_map_change_cancels_pending_result() {
            dialog.generate();map.invalidateGeneration();verify(!Backend.generationLibrary.busy);
            Backend.generationLibrary.result={success:true,operation:"generate",patch:{success:true}};
            compare(map.appliedPreviews,0);
        }
        function test_shrinking_uses_procedural_close_area() {
            dialog.startOnMap();dialog.armBrush(6);compare(map.generationPaintMode,6);
            const plan=dialog.blueprint();verify(plan.brush.procedural);verify(plan.brush.closeArea);verify(!plan.brush.rawLayout);
        }
        function test_recipe_footprint_expands_whole_parts() {
            Backend.generationLibrary.entries=[{id:"style",kind:"style",name:"Bridge",model:{prefabs:[{name:"Segment",tiles:[{dx:0,dy:0,dz:0,items:[100]}]}],recipes:[{name:"Bridge recipe",parts:[{prefab:"Segment",repeat:true,stepX:1,dx:0,dy:0,dz:0}]}]}}];
            dialog.structureSpan=3;compare(dialog.brushPrefabs.length,2);compare(dialog.brushPrefabs[1].tiles.length,3);
            dialog.startOnMap();dialog.choosePrefab(1);dialog.stampPrefab(20,20,7,false);
            compare(dialog.pins[0].recipe,true);compare(dialog.pins[0].span,3);
            verify(map.generationMask.some(t=>t.x===22&&t.y===20));
        }
        function test_failure_marks_diagnostic_cells() {
            dialog.generate();Backend.generationLibrary.busy=false;
            Backend.generationLibrary.result={success:false,operation:"generate",error:"Missing corner",diagnostics:["10,12,7"]};
            compare(dialog.diagnosticCells.length,1);compare(dialog.diagnosticCells[0].x,10);compare(dialog.diagnosticCells[0].y,12);
        }
        function test_saved_plan_retains_frozen_style() {
            const snapshot={id:"style",profile:"test-profile",revision:"original",model:{algorithm:"procedural-v2"}};
            Backend.generationLibrary.entries=[{id:"style",kind:"style",name:"Forest"},{id:"plan",kind:"blueprint",name:"Plan",blueprint:{styleId:"style",styleSnapshot:snapshot,seed:7,mask:[{x:20,y:20,z:7}],paths:[],pins:[]}}];
            dialog.loadBlueprint({id:"plan",name:"Plan"});compare(dialog.blueprint().styleSnapshot.revision,"original");
        }
        function test_close_stops_mask_painting() {
            map.generationPaintMode=1;dialog.close();compare(map.generationPaintMode,0);
        }
        function test_blueprint_restores_editable_mask() {
            Backend.generationLibrary.entries=[{id:"style",kind:"style",name:"Forest"},
                {id:"plan",kind:"blueprint",name:"My forest",blueprint:{styleId:"style",seed:777,mask:[{x:20,y:20,z:7},{x:21,y:20,z:7}],paths:[{x:20,y:20,z:7}],pins:[]}}];
            dialog.loadBlueprint({id:"plan",name:"My forest"});compare(map.generationMask.length,2);compare(map.generationPaths.length,1);compare(dialog.blueprintId,"plan");
        }
    }
}
