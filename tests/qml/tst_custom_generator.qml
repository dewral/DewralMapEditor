import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QtTest
import Tibia 1.0
import "../../editor/qml/dialogs" as Dialogs

Item {
    id:root
    width: 1200; height: 820
    property var previousBrushes
    property var previousSprites
    QtObject {
        id: sprites
        property bool loaded:true
        property int itemImagesRevision:1
        function itemImageSource(ids,width,height,layers) {
            return Qt.resolvedUrl("../../editor/qml/components/fonts/otclient-name.png");
        }
    }
    QtObject {
        id: brushes
        property int revision:1
        function groundBrushNames() { return ["Cave floor", "Stone floor", "Invalid floor", "Outer-only floor"]; }
        function wallBrushNames() { return ["Cave wall"]; }
        function slots(base) {
            const values=[[]];
            for (let i=1;i<=12;++i) values.push([{id:base+i,chance:100}]);
            values[1].push({id:base+13,chance:25});
            return values;
        }
        function groundBrushEdit(name) {
            if (name==="Cave floor") return {zorder:2,items:[{id:100,chance:100},{id:102,chance:40}],borders:[
                {align:"outer",to:"*",tiles:slots(200)}, {align:"inner",to:"Stone floor",tiles:slots(220)}]};
            if (name==="Stone floor") return {zorder:1,items:[{id:103,chance:100}],borders:[{align:"outer",to:"*",tiles:slots(300)}]};
            if (name==="Outer-only floor") return {zorder:1,items:[{id:103,chance:100}],borders:[
                {align:"outer",to:"",tiles:slots(300)},
                {align:"inner",to:"*",tiles:Array.from({length:13},()=>[])},
                {align:"inner",to:"",tiles:Array.from({length:13},()=>[])}]};
            return {items:[{id:100,chance:100},{id:104,chance:100}],borders:[]};
        }
        function advancedBrushEdit(kind,name) {
            return {items:{"0":[[400,100]],"3":[[403,100]],"6":[[406,80],[416,20]],"9":[[409,100]]}};
        }
    }
    QtObject {
        id: mapStub
        property int floor: 7
        property bool luaPreviewActive: false
        property int applied: 0
        function clearLuaPreview() { luaPreviewActive=false; }
        function setLuaPreview(patch,x,y,z) { luaPreviewActive=true; return {success:true}; }
        function applyLuaPreview() { applied++; luaPreviewActive=false; return {success:true}; }
        function centerOnTile(x,y) {}
        function luaGeneratorItemInfo(profile) {
            const result={};
            for (const role of Object.keys(profile.materials || {})) for (const entry of profile.materials[role])
                result[String(entry.id)]={valid:entry.id>=100 && entry.id<=416,ground:entry.id===100 || (entry.id>=102 && entry.id<=104),blocked:entry.id===104};
            return result;
        }
    }
    ListModel {
        id: assets
        property bool loaded:true
        property int itemCount:count
        ListElement { serverId:100; itemName:"Midhem floor"; itemWidth:1; itemHeight:1; layers:1 }
        ListElement { serverId:101; itemName:"Midhem rock"; itemWidth:1; itemHeight:1; layers:1 }
        function clientIdForServerId(id) { return id; }
        function rowForServerId(id) { return id>=100 && id<=416 ? id-100 : -1; }
        function detailsAt(row) { return {spriteIds:[row+1],itemWidth:1,itemHeight:1,layers:1}; }
    }
    Dialogs.CustomMapGeneratorDialog { id: dialog; mapCtrl:mapStub; clientKey:"midhem" }
    TestCase {
        name: "CustomGeneratorPanel"
        when: windowShown
        function initTestCase() {
            root.Window.window.width=1200; root.Window.window.height=820; Backend.otbReader=assets;
            root.previousBrushes=Backend.brushStore; Backend.brushStore=brushes;
            root.previousSprites=Backend.sprReader; Backend.sprReader=sprites;
        }
        function cleanupTestCase() { Backend.brushStore=root.previousBrushes; Backend.sprReader=root.previousSprites; }
        function init() {
            Backend.uiTheme.style="fluent-dark"; Backend.luaScripts.busy=false;
            mapStub.applied=0; dialog.awaitingResult=false;
            dialog.adopt(dialog.freshProfile()); dialog.open();
            tryCompare(dialog,"opened",true);
        }
        function cleanup() { dialog.close(); Backend.luaScripts.busy=false; }
        function addRequiredMaterials() {
            dialog.addItem("background",100); dialog.addItem("floor",100);
        }
        function test_materialsAndInvalidation() {
            dialog.addItem("background",100); dialog.addItem("floor",100);
            compare(dialog.profile.materials.floor.length,1);
            dialog.addItem("floor",101); verify(dialog.error);
            compare(dialog.profile.materials.floor.length,1);
            mapStub.luaPreviewActive=true;
            dialog.edit("seed",42); verify(!mapStub.luaPreviewActive);
            dialog.editMaterials("floor",[{id:100,weight:7}]);
            compare(dialog.profile.materials.floor[0].weight,7);
        }
        function test_generatePreviewApply() {
            verify(!findChild(dialog,"applyCustomMap").enabled);
            addRequiredMaterials();
            mouseClick(findChild(dialog,"generateCustomMap"));
            verify(Backend.luaScripts.busy); compare(Backend.luaScripts.lastMode,"profile");
            Backend.luaScripts.busy=false;
            Backend.luaScripts.finished({success:true,tileCount:10,itemCount:12});
            tryCompare(mapStub,"luaPreviewActive",true);
            mouseClick(findChild(dialog,"applyCustomMap"));
            compare(mapStub.applied,1); verify(!mapStub.luaPreviewActive);
        }
        function test_cancelAndClientChange() {
            addRequiredMaterials();
            mouseClick(findChild(dialog,"generateCustomMap"));
            dialog.clientKey="other";
            verify(!Backend.luaScripts.busy); verify(!mapStub.luaPreviewActive);
            Backend.luaScripts.finished({success:true,tileCount:10,itemCount:12});
            verify(!mapStub.luaPreviewActive); dialog.clientKey="midhem";
        }
        function test_dragMaterial() {
            wait(50);
            const cell=findChild(dialog.contentItem,"customGeneratorItem_100");
            const slot=findChild(dialog.contentItem,"materialSlot_background");
            verify(cell!==null); verify(slot!==null);
            const point=slot.mapToItem(cell,slot.width/2,slot.height/2);
            mousePress(cell,20,20);
            mouseMove(cell,40,30,20);
            mouseMove(cell,point.x,point.y,20);
            mouseRelease(cell,point.x,point.y);
            tryVerify(()=>!!dialog.profile.materials.background);
            compare(dialog.profile.materials.background[0].id,100);
        }
        function test_documentSwitchDiscardsResult() {
            addRequiredMaterials();
            mouseClick(findChild(dialog,"generateCustomMap"));
            Backend.otbmReaderChanged();
            verify(!Backend.luaScripts.busy);
            Backend.luaScripts.finished({success:true,tileCount:10,itemCount:12});
            verify(!mapStub.luaPreviewActive);
        }
        function test_loadingProfileRestoresEditedControls() {
            const width=findChild(dialog.contentItem,"customGenerator_width");
            width.value=80; width.valueModified(); compare(dialog.profile.width,80);
            const kind=findChild(dialog.contentItem,"customGeneratorKind");
            kind.currentIndex=1; kind.activated(1); compare(dialog.profile.kind,"cave");
            const name=findChild(dialog.contentItem,"customGeneratorName");
            name.text="Edited profile"; compare(dialog.profile.name,"Edited profile");
            dialog.adopt(dialog.freshProfile());
            tryCompare(name,"text","Midhem — jaskinia");
            tryCompare(width,"value",70); tryCompare(kind,"currentIndex",0);
        }
        function test_destinationControlsShareOneRow() {
            const x=findChild(dialog.contentItem,"customGenerator_x");
            const y=findChild(dialog.contentItem,"customGenerator_y");
            const z=findChild(dialog.contentItem,"customGenerator_z");
            compare(x.mapToItem(dialog.contentItem,0,0).y,y.mapToItem(dialog.contentItem,0,0).y);
            compare(y.mapToItem(dialog.contentItem,0,0).y,z.mapToItem(dialog.contentItem,0,0).y);
            verify(x.mapToItem(dialog.contentItem,0,0).x<y.mapToItem(dialog.contentItem,0,0).x);
            verify(y.mapToItem(dialog.contentItem,0,0).x<z.mapToItem(dialog.contentItem,0,0).x);
            verify(z.width<x.width); compare(x.to,65535); compare(y.to,65535); compare(z.to,15);
            x.value=65000; x.valueModified(); z.value=15; z.valueModified();
            compare(dialog.profile.x,65000); compare(dialog.profile.z,15);
        }
        function test_advancedSettingsEditRestoreAndReset() {
            wait(50);
            mouseClick(findChild(dialog.contentItem,"customGeneratorAdvancedButton"));
            const advanced=findChild(dialog,"customGeneratorAdvancedDialog");
            tryCompare(advanced,"opened",true);
            verify(advanced.y+advanced.height<=root.Window.window.height);
            const width=findChild(advanced,"customGenerator_roomWidth");
            const spacing=findChild(advanced,"customGenerator_roomSpacing");
            compare(width.value,7); compare(spacing.value,8);
            mapStub.luaPreviewActive=true;
            width.value=15; width.valueModified();
            compare(dialog.profile.roomWidth,15); verify(!mapStub.luaPreviewActive);
            spacing.value=20; spacing.valueModified();
            compare(dialog.profile.roomSpacing,20);
            findChild(advanced,"customGeneratorConnectionMode").handleActivation(1);
            compare(dialog.profile.connectionMode,"nearest");
            verify(!findChild(advanced,"customGeneratorCavePaths").enabled);
            verify(!findChild(advanced,"customGenerator_secondaryDensity").enabled);
            dialog.addItem("secondary",100);
            verify(findChild(advanced,"customGenerator_secondaryDensity").enabled);
            dialog.edit("kind","cave");
            verify(findChild(advanced,"customGeneratorCavePaths").enabled);
            findChild(advanced,"customGeneratorCavePaths").handleActivation(1);
            compare(dialog.profile.cavePaths,"direct");
            const profile=dialog.freshProfile(); delete profile.roomWidth; delete profile.roomSpacing;
            dialog.adopt(profile);
            compare(width.value,7); compare(spacing.value,8);
            width.value=11; width.valueModified();
            mouseClick(findChild(advanced,"resetCustomGeneratorAdvanced"));
            compare(width.value,7); compare(dialog.profile.roomWidth,7);
            compare(dialog.profile.width,70); compare(dialog.profile.connectionMode,"chain");
            Backend.luaScripts.busy=true; verify(!width.enabled);
            verify(!findChild(advanced,"resetCustomGeneratorAdvanced").enabled);
            Backend.luaScripts.busy=false;
            wait(50); grabImage(root).save("build/custom-generator-advanced-ui.png");
            dialog.close(); tryCompare(advanced,"opened",false);
        }
        function test_loadMultipleGroundBrushesAndEditTheirBorders() {
            dialog.activeRole="floor";
            findChild(dialog,"customGeneratorBrushPicker").handleActivation(1);
            compare(dialog.profile.materials.floor.length,2);
            compare(dialog.profile.materials.floor[1].weight,40);
            compare(dialog.profile.groundBorders.length,2);
            verify(dialog.profile.bordersEnabled);
            dialog.loadGroundBrush("floor","Stone floor");
            compare(dialog.profile.materials.floor.length,3);
            compare(dialog.profile.groundBorders.length,3);
            const category=findChild(dialog,"customGeneratorCategory");
            compare(category.model.length,7);
            category.currentIndex=4;
            compare(category.currentIndex,4);
            compare(dialog.shownSlots[2].role,"border3");
            grabImage(root).save("build/custom-generator-border-ui.png");
            tryVerify(()=>findChild(dialog.contentItem,"materialSlot_border3")!==null);
            compare(findChild(dialog.contentItem,"materialSlot_border3").entries[0].id,201);
            dialog.addItem("border3",101);
            compare(dialog.profile.groundBorders[0].materials.border3.length,3);
            compare(dialog.profile.groundBorders[2].materials.border3[0].id,301);
            verify(!dialog.profile.materials.border3);
            category.currentIndex=6;
            wait(0);
            compare(findChild(dialog.contentItem,"materialSlot_border3").entries[0].id,301);
            compare(dialog.profile.groundBorders[1].toGroundIds[0],103);
            dialog.adopt(dialog.freshProfile());
            compare(category.currentIndex,0);
        }
        function test_invalidGroundBrushIsAtomic() {
            dialog.addItem("floor",100);
            const before=JSON.stringify(dialog.profile);
            dialog.loadGroundBrush("floor","Invalid floor");
            verify(dialog.error); compare(JSON.stringify(dialog.profile),before);
        }
        function test_outerOnlyBrushKeepsItsTargetAndDoesNotCreateInnerBorders() {
            dialog.loadGroundBrush("floor","Outer-only floor");
            compare(dialog.profile.groundBorders.length,1);
            const set=dialog.profile.groundBorders[0];
            compare(set.align,"outer"); compare(set.to,"");
            compare(set.name,"Outer-only floor · outer → no neighboring ground");
            compare(set.materials.border3,[{id:301,weight:100},{id:313,weight:25}]);
            dialog.loadGroundBrush("floor","Outer-only floor");
            compare(dialog.profile.groundBorders.length,1);
            compare(dialog.profile.materials.floor.length,1);
        }
        function test_borderLabelsDistinguishTheTargetFromEmptyContents() {
            dialog.loadGroundBrush("background","Cave floor");
            compare(dialog.profile.groundBorders[0].name,"Cave floor · outer → any other ground");
            compare(dialog.profile.groundBorders[1].name,"Cave floor · inner → Stone floor");
            verify(dialog.profile.groundBorders[1].materials.border3.length>0);
            dialog.loadGroundBrush("floor","Outer-only floor");
            const category=findChild(dialog,"customGeneratorCategory");
            verify(category.model.indexOf("Borders · Outer-only floor · outer → no neighboring ground")>=0);
            category.currentIndex=6;
            verify(findChild(dialog,"customGeneratorBorderHint").text.indexOf("Border target: no neighboring ground")>=0);
        }
        function test_existingProfileGetsClearLabelsWithoutChangingItsRules() {
            const profile=dialog.freshProfile();
            profile.materials.floor=[{id:103,weight:100}];
            profile.groundBorders=[{name:"Stone floor · outer → empty",brush:"Stone floor",groundIds:[103],align:"outer",to:"",toGroundIds:[],zorder:1,materials:{border3:[{id:301,weight:100}]}},
                {name:"My custom rule",brush:"Stone floor",groundIds:[103],align:"inner",to:"*",toGroundIds:[],zorder:1,materials:{}}];
            const before=JSON.stringify(profile);
            dialog.adopt(profile);
            const category=findChild(dialog,"customGeneratorCategory");
            compare(category.model[4],"Borders · Stone floor · outer → no neighboring ground");
            compare(category.model[5],"Borders · My custom rule");
            compare(JSON.stringify(dialog.profile),before);
        }
        function test_wallBrushLoadsEveryWeightedVariant() {
            const picker=findChild(dialog,"customGeneratorBrushPicker");
            picker.currentIndex=2;
            findChild(dialog,"customGeneratorCategory").currentIndex=1;
            compare(picker.currentIndex,0);
            picker.handleActivation(1);
            verify(dialog.profile.wallsEnabled);
            compare(dialog.profile.materials.wall1,[{id:406,weight:80},{id:416,weight:20}]);
            compare(dialog.profile.materials.wall2[0].id,409);
            compare(dialog.profile.materials.wall3[0].id,400);
            compare(dialog.profile.materials.wall4[0].id,403);
        }
        function test_groundTargetFollowsTheActiveSlot() {
            const target=findChild(dialog,"customGeneratorGroundTarget");
            target.currentIndex=2; target.activated(2);
            compare(dialog.activeRole,"secondary");
            dialog.activeRole="floor";
            compare(target.currentIndex,1);
            dialog.loadGroundBrush(dialog.activeRole,"Cave floor");
            compare(dialog.profile.materials.floor.length,2);
            verify(!dialog.profile.materials.secondary);
        }
        function test_choosingGroundBrushAddsToEachRequiredSlot() {
            const picker=findChild(dialog,"customGeneratorBrushPicker");
            const target=findChild(dialog,"customGeneratorGroundTarget");
            compare(picker.currentIndex,0);
            verify(!dialog.profile.materials.background);
            mouseClick(picker,10,picker.height/2);
            tryCompare(picker,"open",true);
            const view=findChild(picker,"comboBoxTypeAhead").view;
            tryVerify(()=>view.itemAtIndex(1)!==null);
            mouseClick(view.itemAtIndex(1),20,10);
            tryCompare(picker,"open",false);
            compare(dialog.profile.materials.background.length,2);
            compare(dialog.slotSummary("background"),"Cave floor · 2 items");
            const slot=findChild(dialog.contentItem,"materialSlot_background");
            compare(slot.entries.length,2);
            const sprite=findChild(slot,"materialSprite_background_100");
            verify(sprite!==null);
            verify(String(sprite.source).length>0);
            tryCompare(sprite,"status",Image.Ready);
            target.handleActivation(1);
            compare(dialog.activeRole,"floor"); compare(picker.currentIndex,0);
            verify(!dialog.profile.materials.floor);
            picker.handleActivation(1);
            compare(dialog.profile.materials.floor.length,2);
            compare(dialog.profile.materials.background.length,2);
            picker.handleActivation(2);
            compare(dialog.profile.materials.floor.length,3);
            compare(dialog.slotSummary("floor"),"Cave floor, Stone floor · 3 items");
            picker.handleActivation(2);
            compare(dialog.profile.materials.floor.length,3);
            wait(50);
            grabImage(root).save("build/custom-generator-brush-feedback-ui.png");
            mouseClick(findChild(dialog,"generateCustomMap"));
            verify(Backend.luaScripts.busy);
        }
        function test_keyboardSelectionAddsOnlyTheFinalBrush() {
            const picker=findChild(dialog,"customGeneratorBrushPicker");
            mouseClick(picker,10,picker.height/2);
            tryCompare(picker,"open",true);
            tryCompare(findChild(picker,"comboBoxTypeAhead").view,"activeFocus",true);
            keyClick(Qt.Key_C);
            compare(picker.currentText,"Cave floor");
            verify(!dialog.profile.materials.background);
            keyClick(Qt.Key_S);
            compare(picker.currentText,"Stone floor");
            verify(!dialog.profile.materials.background);
            keyClick(Qt.Key_Return);
            tryCompare(picker,"open",false);
            compare(dialog.profile.materials.background,[{id:103,weight:100}]);
            compare(dialog.profile.groundBorders.length,1);
        }
        function test_missingMaterialsHighlightsTheEmptySlot() {
            const generate=findChild(dialog,"generateCustomMap");
            mouseClick(generate);
            verify(dialog.error); verify(!Backend.luaScripts.busy);
            verify(dialog.status.indexOf("Background and Rooms and corridors")>=0);
            findChild(dialog,"customGeneratorBrushPicker").handleActivation(1);
            findChild(dialog,"customGeneratorCategory").handleActivation(1);
            mouseClick(generate);
            verify(dialog.error); verify(!Backend.luaScripts.busy);
            verify(dialog.status.indexOf("Rooms and corridors")>=0);
            compare(dialog.activeRole,"floor");
            compare(findChild(dialog,"customGeneratorCategory").currentIndex,0);
            compare(findChild(dialog,"customGeneratorGroundTarget").currentIndex,1);
            findChild(dialog,"customGeneratorBrushPicker").handleActivation(1);
            mouseClick(generate);
            verify(!dialog.error); verify(Backend.luaScripts.busy);
        }
        function test_placeholderAndProfileLoadDoNotAddBrushes() {
            const picker=findChild(dialog,"customGeneratorBrushPicker");
            picker.handleActivation(0);
            verify(!dialog.profile.materials.background);
            picker.handleActivation(1);
            const profile=dialog.freshProfile();
            profile.materials={background:[{id:103,weight:9}],floor:[{id:100,weight:1}]};
            dialog.adopt(profile);
            compare(dialog.profile.materials.background,[{id:103,weight:9}]);
            compare(dialog.profile.materials.floor,[{id:100,weight:1}]);
            compare(picker.currentIndex,0);
        }
        function test_removingGroundPrunesItsBorderSets() {
            dialog.loadGroundBrush("floor","Cave floor");
            dialog.editMaterials("floor",[{id:102,weight:40}]);
            compare(dialog.profile.groundBorders[0].groundIds,[102]);
            dialog.editMaterials("floor",[]);
            compare(dialog.profile.groundBorders.length,0);
        }
        function test_profileTooltips() {
            const button=findChild(dialog,"exportGeneratorProfile");
            verify(button.ToolTip.text.indexOf("generator profile")>=0);
            verify(button.ToolTip.text.indexOf("OTBM")>=0);
            mouseMove(root,0,0);
            mouseMove(button,button.width/2,button.height/2);
            tryCompare(button.ToolTip,"visible",true,1500);
        }
        function test_layout() {
            dialog.addItem("background",100); dialog.addItem("floor",100);
            wait(100);
            const slot=findChild(dialog.contentItem,"materialSlot_floor");
            verify(slot.width>200); verify(slot.height>60);
            const panel=findChild(dialog,"customGeneratorItemsPanel");
            const grid=findChild(dialog,"customGeneratorItemsGrid");
            compare(grid.width,4*grid.cellWidth);
            compare(panel.width,grid.width+20);
            grabImage(root).save("build/custom-generator-ui.png");
        }
        function test_closeClearsPreview() {
            mapStub.luaPreviewActive=true; dialog.close();
            tryCompare(mapStub,"luaPreviewActive",false);
        }
    }
}
