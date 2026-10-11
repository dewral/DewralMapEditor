import QtQuick
import QtQuick.Controls
import QtTest
import Tibia 1.0
import "../../editor/qml/dialogs" as Dialogs

Item {
    id: root
    width: 900; height: 750
    QtObject {
        id: mapStub
        property int floor: 7
        property int applied: 0
        function applyLuaPatch(patch,x,y,z) { ++applied; return {success:true,tileCount:1}; }
    }
    Dialogs.LuaGeneratorDialog { id: dialog; mapCtrl: mapStub }
    TestCase {
        name: "LuaGeneratorPanel"
        when: windowShown
        function init() { Backend.uiTheme.style="fluent-dark"; Backend.luaScripts.busy=false; dialog.pendingPatch=null; mapStub.applied=0; dialog.open(); }
        function cleanup() { Backend.luaScripts.busy=false; dialog.close(); }
        function test_generateThenApply() {
            const apply=findChild(dialog,"applyLua");
            verify(!apply.enabled);
            mouseClick(findChild(dialog,"generateLua"));
            compare(Backend.luaScripts.busy,true);
            compare(Backend.luaScripts.lastMode,"otmapgen");
            verify(!apply.enabled);
            Backend.luaScripts.busy=false;
            Backend.luaScripts.finished({success:true,tileCount:1,itemCount:2,elapsedMs:1,log:"Generated"});
            tryCompare(apply,"enabled",true);
            mouseClick(apply);
            compare(mapStub.applied,1);
            verify(!apply.enabled);
        }
        function test_cancelDiscardsPatch() {
            mouseClick(findChild(dialog,"generateLua"));
            mouseClick(findChild(dialog,"generateLua"));
            compare(Backend.luaScripts.busy,false);
            compare(dialog.pendingPatch,null);
            verify(dialog.statusError);
        }
    }
}
