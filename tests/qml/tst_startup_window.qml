import QtQuick
import QtTest
import Tibia 1.0
import "../../editor/qml" as Editor
Item {
    QtObject { id: prefs; property string selectedClientKey: "860" }
    QtObject {
        id: session
        property bool started: false
        property string loadedClientKey: "860"
        property string pendingKey: "860"
        property var clientPaths: ({})
        property var recentMaps: []
        function allProfileKeys() { return ["740", "772", "860"]; }
        function profileLabel(key) { return String(key); }
        function profileVer(key) { return Number(key); }
        function isCustomKey(key) { return false; }
        function clientFiles(folder) { return {dat:"", spr:"", otb:""}; }
        function createNewMap(key,w,h) {}
    }
    Component { id: factory; Editor.StartupWindow { app: session; settings: prefs } }
    TestCase {
        name: "StartupLazySections"
        when: windowShown
        property var welcome
        function init() {
            Backend.docMgr.recoveryCount = 0; Backend.docMgr.recoveries = [];
            prefs.selectedClientKey = "860"; session.pendingKey = "860";
            welcome = factory.createObject(null);
            verify(welcome !== null);
        }
        function cleanup() { welcome.destroy(); welcome = null; Backend.docMgr.recoveryCount = 0; }
        function test_initiallyNoHiddenDialogsOrCards() {
            for (const name of ["startupFolderHost", "startupOpenMapHost", "startupNewMapHost",
                                "startupRecoveryHost", "startupLoadingHost"])
                compare(findChild(welcome, name).item, null, name);
        }
        function test_newMapIsCreatedOnceAndKeepsProfileBinding() {
            welcome.openNewMapDialog();
            const host = findChild(welcome,"startupNewMapHost");
            verify(host.item !== null);
            compare(host.item.preferredProfileKey,"860");
            const first = host.item;
            host.item.close();
            prefs.selectedClientKey = "740";
            welcome.openNewMapDialog();
            compare(host.item,first);
            compare(host.item.preferredProfileKey,"740");
            host.item.close();
        }
        function test_fileDialogsReceiveContextAndKeepLiveTitle() {
            const folder = findChild(welcome,"startupFolderHost");
            verify(folder.ensureLoaded());
            compare(folder.item.startupWindow,welcome);
            verify(folder.item.title.indexOf("860") >= 0);
            session.pendingKey = "740";
            verify(folder.item.title.indexOf("740") >= 0);
            const open = findChild(welcome,"startupOpenMapHost");
            verify(open.ensureLoaded());
            compare(open.item.startupWindow,welcome);
            compare(open.item.nameFilters[0],"OTBM maps (*.otbm)");
            welcome.loadingMap = true;
            folder.item.rejected();
            compare(welcome.loadingMap,false);
        }
        function test_cardsKeepTheirOriginalSize_data() {
            return ["classic","windows-classic","github-dark","gray-dark","gray-modern","fluent-dark","tibia-ui"]
                .map(style => ({tag:style,style:style}));
        }
        function test_cardsKeepTheirOriginalSize(data) {
            Backend.uiTheme.style=data.style;
            welcome.loadingMap=true;
            const loading=findChild(welcome,"startupLoadingHost");
            tryVerify(()=>loading.item!==null);
            compare(loading.item.width,420);compare(loading.item.height,142);
            compare(loading.width,420);compare(loading.height,142);
            welcome.loadingMap=false;Backend.docMgr.recoveryCount=1;
            const recovery=findChild(welcome,"startupRecoveryHost");
            tryVerify(()=>recovery.item!==null);
            compare(recovery.item.width,Math.min(welcome.width-40,620));
            compare(recovery.item.height,Math.min(welcome.height-80,330));
        }
        function test_recoveryAndLoadingOnlyWhenNeeded() {
            const recovery = findChild(welcome,"startupRecoveryHost");
            Backend.docMgr.recoveryCount = 1;
            tryVerify(() => recovery.item !== null);
            const loading = findChild(welcome,"startupLoadingHost");
            welcome.loadingMap = true;
            tryVerify(() => loading.item !== null);
            compare(recovery.item,null);
            welcome.loadingMap = false;
            tryVerify(() => recovery.item !== null);
            compare(loading.item,null);
            Backend.docMgr.recoveryCount = 0;
            compare(recovery.item,null);
        }
    }
}
