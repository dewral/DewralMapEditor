import QtQuick
import QtQuick.Window
import QtTest
import Tibia 1.0
import "../../editor/qml/dialogs" as Dialogs

Item {
    id: scene
    width: 1100; height: 820
    QtObject {
        id: navigator
        property var selected: []
        function selectRaw(id) { selected = selected.concat([id]); }
    }
    QtObject {
        id: catalog
        property var calls: []
        property var response: ({results:[], total:0, truncated:false})
        function findItems(query) { calls = calls.concat([query]); return response; }
    }
    Dialogs.FindItemDialog {
        id: search
        paletteNavigator: navigator
        itemCatalog: catalog
    }
    TestCase {
        name: "FindItemDialog"
        when: windowShown
        readonly property var propertyKeys: ["unpassable","unmovable","blockmissiles","blockpathfinder","readable","writeable","pickupable","stackable","rotatable","hangable","hookeast","hooksouth","haselevation","ignorelook","floorchange"]
        readonly property var typeKeys: ["depot","mailbox","trashholder","container","door","magicfield","teleport","bed","key","podium"]
        function locate(name) { return findChild(search.contentItem,name); }
        function lastQuery() { return catalog.calls[catalog.calls.length-1]; }
        function clickMode(mode) { mouseClick(locate("findItemMode_"+mode)); compare(search.searchMode,mode); compare(lastQuery().mode,mode); }
        function fixture() {
            return {results:[
                {serverId:8779,clientId:8704,name:"closed door",group:"Door",spriteIds:[]},
                {serverId:8781,clientId:8706,name:"open door",group:"Door",spriteIds:[]}
            ],total:2,truncated:false};
        }
        function initTestCase() { scene.Window.window.width=1100; scene.Window.window.height=820; }
        function init() {
            scene.Window.window.width=1100; scene.Window.window.height=820;
            Backend.uiTheme.style="fluent-dark";
            navigator.selected=[];catalog.calls=[];catalog.response=fixture();
            search.floatingPositionInitialized=false;
            search.open();tryCompare(search,"visible",true);wait(30);
            // The test reuses the dialog; the application creates it on demand.
            for(const key of propertyKeys) locate("findItemProperty_"+key).checked=false;
            locate("findItemForceSelect").checked=false;
            locate("findItemTypesScroll").contentY=0;locate("findItemPropertiesScroll").contentY=0;
            search.runSearch();
        }
        function cleanup() { search.close();tryCompare(search,"visible",false); }
        function test_allOriginalOptionsAndModes() {
            for(const mode of ["server","client","name","type","properties"]) verify(locate("findItemMode_"+mode));
            for(const key of typeKeys) verify(locate("findItemType_"+key));
            for(const key of propertyKeys) verify(locate("findItemProperty_"+key));
            compare(lastQuery().serverId,100);compare(lastQuery().clientId,100);compare(lastQuery().type,"depot");
            for(const mode of ["server","client","name","type","properties"]) {
                clickMode(mode);
                compare(locate("findItemServerId").enabled,mode==="server");
                compare(locate("findItemForceSelect").enabled,mode==="server");
                compare(locate("findItemClientId").enabled,mode==="client");
                compare(locate("findItemName").enabled,mode==="name");
                compare(locate("findItemType_door").enabled,mode==="type");
                compare(locate("findItemProperty_unpassable").enabled,mode==="properties");
            }
        }
        function test_preservesQueryFieldsAndProperties() {
            clickMode("server");
            locate("findItemServerId").value=65000;
            mouseClick(locate("findItemForceSelect"));
            compare(lastQuery().serverId,65000);compare(lastQuery().force,true);
            clickMode("client");locate("findItemClientId").value=12345;
            tryVerify(()=>lastQuery().clientId===12345);
            clickMode("name");locate("findItemName").text="wooden door";
            tryVerify(()=>lastQuery().name==="wooden door");
            clickMode("type");mouseClick(locate("findItemType_door"));compare(lastQuery().type,"door");
            clickMode("properties");
            mouseClick(locate("findItemProperty_unpassable"));mouseClick(locate("findItemProperty_blockmissiles"));
            compare(lastQuery().properties.length,2);verify(lastQuery().properties.indexOf("unpassable")>=0);verify(lastQuery().properties.indexOf("blockmissiles")>=0);
            clickMode("server");compare(lastQuery().serverId,65000);compare(lastQuery().force,true);
            compare(locate("findItemName").text,"wooden door");
        }
        function test_typeImmediately_data() {
            return ["fluent-dark","gray-dark","gray-modern","github-dark","classic","windows-classic"].map(theme=>({tag:theme,theme:theme}));
        }
        function test_typeImmediately(data) {
            search.close();tryCompare(search,"visible",false);
            Backend.uiTheme.style=data.theme;
            search.searchMode="client";
            search.open();tryCompare(search,"visible",true);
            compare(search.searchMode,"name");verify(locate("findItemName").enabled);
            wait(30);
            for(const key of [Qt.Key_D,Qt.Key_O,Qt.Key_O,Qt.Key_R]) keyClick(key);
            tryCompare(locate("findItemName"),"text","door");
            tryVerify(()=>lastQuery().name==="door"&&lastQuery().mode==="name");
            search.close();tryCompare(search,"visible",false);search.open();tryCompare(search,"visible",true);wait(30);
            keyClick(Qt.Key_B);keyClick(Qt.Key_A);keyClick(Qt.Key_G);
            tryCompare(locate("findItemName"),"text","bag");
        }
        function test_nameTypingRemainsDebounced() {
            clickMode("name");const before=catalog.calls.length;
            locate("findItemName").text="do";locate("findItemName").text="door";
            compare(catalog.calls.length,before);
            tryVerify(()=>catalog.calls.length>before);compare(catalog.calls.length,before+1);compare(lastQuery().name,"door");
        }
        function test_acceptAndCancelKeepRawSelection() {
            wait(10);const row=locate("findItemResult_8781");verify(row);
            mouseClick(row);mouseClick(locate("findItemAccept"));
            compare(navigator.selected.length,1);compare(navigator.selected[0],8781);tryCompare(search,"visible",false);
            search.open();tryCompare(search,"visible",true);mouseClick(locate("findItemCancel"));
            compare(navigator.selected.length,1);tryCompare(search,"visible",false);
        }
        function test_doubleClickAndEmptyResults() {
            mouseDoubleClickSequence(locate("findItemResult_8779"));
            tryCompare(search,"visible",false);compare(navigator.selected[0],8779);
            search.open();tryCompare(search,"visible",true);catalog.response={results:[],total:0,truncated:false};search.runSearch();
            compare(search.results.length,0);compare(locate("findItemResults").currentIndex,-1);verify(!locate("findItemAccept").enabled);
            search.acceptSelection();compare(navigator.selected.length,1);verify(search.visible);
        }
        function test_layout_data() {
            return [
                {tag:"fluent",theme:"fluent-dark",width:1100,height:820},
                {tag:"gray",theme:"gray-dark",width:1100,height:820},
                {tag:"gray-modern",theme:"gray-modern",width:1000,height:750},
                {tag:"github",theme:"github-dark",width:1100,height:820},
                {tag:"classic",theme:"classic",width:1000,height:750},
                {tag:"windows-classic",theme:"windows-classic",width:1000,height:750},
                {tag:"compact",theme:"fluent-dark",width:800,height:600},
                {tag:"short",theme:"fluent-dark",width:900,height:480}
            ];
        }
        function test_layout(data) {
            Backend.uiTheme.style=data.theme;scene.Window.window.width=data.width;scene.Window.window.height=data.height;wait(70);
            verify(search.width<=data.width-20);verify(search.height<=data.height-20);
            const modes=locate("findItemModes"),fields=locate("findItemFields"),sections=locate("findItemSections");
            verify(modes.mapToItem(search.contentItem,0,modes.height).y<=fields.mapToItem(search.contentItem,0,0).y);
            verify(fields.mapToItem(search.contentItem,0,fields.height).y<=sections.mapToItem(search.contentItem,0,0).y);
            const panels=[locate("findItemTypesPanel"),locate("findItemPropertiesPanel"),locate("findItemResultsPanel")];
            for(let i=0;i<panels.length;i++) {
                verify(panels[i].width>0&&panels[i].height>0);
                const edge=panels[i].mapToItem(search.contentItem,panels[i].width,panels[i].height);
                verify(edge.x<=search.contentItem.width+1&&edge.y<=search.contentItem.height+1);
                if(i<panels.length-1) verify(edge.x<=panels[i+1].mapToItem(search.contentItem,0,0).x);
            }
            const accept=locate("findItemAccept");verify(accept.mapToItem(search.contentItem,accept.width,accept.height).y<=search.contentItem.height+1);
            const scroll=locate("findItemPropertiesScroll");verify(scroll.contentWidth<=scroll.width);
            if(data.tag==="short") {
                verify(scroll.contentHeight>scroll.height);
                clickMode("properties");scroll.contentY=scroll.contentHeight-scroll.height;wait(20);
                mouseClick(locate("findItemProperty_floorchange"));verify(lastQuery().properties.indexOf("floorchange")>=0);
                clickMode("type");const typesScroll=locate("findItemTypesScroll");typesScroll.contentY=typesScroll.contentHeight-typesScroll.height;wait(20);
                mouseClick(locate("findItemType_podium"));compare(lastQuery().type,"podium");
            }
            if(data.tag==="fluent") {
                clickMode("name");locate("findItemName").text="door";wait(220);
                grabImage(scene.Window.window.contentItem).save("build/find-item-layout/design.png");
            }
        }
    }
}
