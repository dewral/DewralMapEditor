import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QtTest
import Tibia 1.0
import "../../editor/qml/dialogs" as Dialogs

Item {
    id:root
    width: 1200; height: 820
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
            return {"100":{valid:true,ground:true,blocked:false},"101":{valid:true,ground:false,blocked:false}};
        }
    }
    ListModel {
        id: assets
        property bool loaded:true
        property int itemCount:count
        ListElement { serverId:100; itemName:"Midhem floor"; itemWidth:1; itemHeight:1; layers:1 }
        ListElement { serverId:101; itemName:"Midhem rock"; itemWidth:1; itemHeight:1; layers:1 }
        function clientIdForServerId(id) { return id; }
        function rowForServerId(id) { return id===100 ? 0 : 1; }
        function detailsAt(row) { return get(row); }
    }
    Dialogs.CustomMapGeneratorDialog { id: dialog; mapCtrl:mapStub; clientKey:"midhem" }
    TestCase {
        name: "CustomGeneratorPanel"
        when: windowShown
        function initTestCase() { root.Window.window.width=1200; root.Window.window.height=820; Backend.otbReader=assets; }
        function init() {
            Backend.uiTheme.style="fluent-dark"; Backend.luaScripts.busy=false;
            mapStub.applied=0; dialog.awaitingResult=false;
            dialog.adopt(dialog.freshProfile()); dialog.open();
        }
        function cleanup() { dialog.close(); Backend.luaScripts.busy=false; }
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
            mouseClick(findChild(dialog,"generateCustomMap"));
            verify(Backend.luaScripts.busy); compare(Backend.luaScripts.lastMode,"profile");
            Backend.luaScripts.busy=false;
            Backend.luaScripts.finished({success:true,tileCount:10,itemCount:12});
            tryCompare(mapStub,"luaPreviewActive",true);
            mouseClick(findChild(dialog,"applyCustomMap"));
            compare(mapStub.applied,1); verify(!mapStub.luaPreviewActive);
        }
        function test_cancelAndClientChange() {
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
        function test_layout() {
            dialog.addItem("background",100); dialog.addItem("floor",100);
            wait(100);
            const slot=findChild(dialog.contentItem,"materialSlot_floor");
            verify(slot.width>200); verify(slot.height>60);
            grabImage(root).save("build/custom-generator-ui.png");
        }
        function test_closeClearsPreview() {
            mapStub.luaPreviewActive=true; dialog.close();
            tryCompare(mapStub,"luaPreviewActive",false);
        }
    }
}
