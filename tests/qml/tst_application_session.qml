import QtQuick
import QtTest
import "../../editor/qml/controllers" as Controllers
Item {
    id: scene
    property int requests: 0
    property var calls: []
    QtObject {
        id: prefs
        property string clientPathsJson: "{}"
        property string customProfilesJson: "[]"
        property string mapProfilesJson: "{}"
        property string recentMapsJson: "[]"
        property string clientFolder: ""
    }
    QtObject { id: welcome; property bool loadingMap: false; function openVersionFolderDialog() {} }
    QtObject {
        id: editorController
        property bool started: false
        property var recentMaps: []
        property string pendingKey: ""
        property string pendingMapPath: ""
        function loadEverything(path, key) { scene.calls.push(["open",path,key]); return true; }
        function createNewMap(key,w,h) { scene.calls.push(["new",key,w,h]); }
        function recoverPreviousSession() { scene.calls.push(["recover"]); }
    }
    QtObject { id: editorWindow; property var appController: editorController; property bool editorPresented: false }
    Component {
        id: sessionFactory
        Controllers.ApplicationSession {
            settings: prefs
            startupWindow: welcome
            onEditorNeeded: scene.requests++
        }
    }
    TestCase {
        name: "ApplicationSession"
        when: windowShown
        function init() { scene.requests=0; scene.calls=[]; welcome.loadingMap=false; }
        function test_startupHasNoViewportAndDefersRequests() {
            const session=createTemporaryObject(sessionFactory,scene);
            verify(session); compare(session.profiles.mapView,null);
            compare(session.editorWindow,null);
            verify(session.loadEverything("map.otbm","860"));
            compare(scene.requests,1); compare(scene.calls.length,0);
            verify(!session.loadEverything("other.otbm","860"));
            session.attachEditor(editorWindow);
            compare(scene.calls,[ ["open","map.otbm","860"] ]);
            compare(session.pendingOperation,null);
        }
        function test_newAndRecoveryAreDispatchedOnce() {
            const session=createTemporaryObject(sessionFactory,scene);
            verify(session.createNewMap("860",128,256));
            compare(scene.calls.length,0);
            session.attachEditor(editorWindow);
            compare(scene.calls,[ ["new","860",128,256] ]);
            session.recoverPreviousSession();
            compare(scene.calls.length,2);
            compare(scene.requests,1);
        }
        function test_failureKeepsWelcomeUsable() {
            const session=createTemporaryObject(sessionFactory,scene);
            verify(session.loadEverything("map.otbm","860"));
            session.failEditor("Failed");
            compare(session.pendingOperation,null); verify(!welcome.loadingMap);
            verify(session.createNewMap("860",128,128));
            compare(scene.requests,2);
        }
    }
}
