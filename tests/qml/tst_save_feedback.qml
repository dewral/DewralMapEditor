import QtQuick
import QtTest
import Tibia 1.0
import "../../editor/qml/controllers" as Controllers
TestCase {
    id: testCase
    name: "SaveFeedback"
    property int writes: 0
    property bool writeSuccess: true
    property var oldReader
    property var oldDocuments
    QtObject {
        id: reader
        property string filePath: "map.otbm"
        property bool dirty: true
        property string errorString: "Disk full"
        function applyClientVersions(v,major,minor) {}
        function saveFile(path) { writes++; if(writeSuccess) {filePath=path;dirty=false;} return writeSuccess; }
    }
    QtObject {
        id: secondReader
        property string filePath: "second.otbm"
        property bool dirty: true
        property string errorString: "Disk full"
        function applyClientVersions(v,major,minor) {}
        function saveFile(path) { writes++; if(writeSuccess) {filePath=path;dirty=false;} return writeSuccess; }
    }
    QtObject {
        id: documents
        readonly property QtObject current: currentIndex === 0 ? reader : secondReader
        property var tabs: [{dirty:true,title:"Map"}]
        property int currentIndex: 0
        signal autosaveFailed(string title,string error)
    }
    QtObject {
        id: window
        property int token: 0
        property int closed: 0
        property bool editorPresented: true
        property bool canPresent: true
        function requestPresentedFrame() { return canPresent ? ++token : 0; }
        function close() { closed++; }
    }
    QtObject {
        id: fakeProfiles
        property int loadedClientVersion: 860
        property string loadedClientKey: "860"
        property bool succeeds: true
        function prepareClientLoaded(r,key,callback) { Qt.callLater(()=>callback(succeeds,succeeds ? "" : "Client failed")); }
        function rememberMapProfile(path,key) {}
    }
    QtObject {
        id: stub
        signal mapLoadFinished(bool success,string path,string error)
        signal atlasBuildFinished(bool success,string error)
        signal initialViewReady()
    }
    QtObject { id: fakeSaveDialog; property int opened: 0; function open() {opened++;} }
    QtObject {
        id: fakeAppCloseConfirm
        property bool visible: false
        property string message: ""
        property int opened: 0
        function open() { visible=true; opened++; }
        function close() { visible=false; }
    }
    QtObject { id: fakeSettings; property string recentMapsJson: "[]" }
    Component {
        id: factory
        Controllers.DocumentController {
            settings: fakeSettings; profiles: fakeProfiles; palettes: stub; mapView: stub
            appWindow: window; startupWindow: stub; versionFolderDialog: stub
            saveDialog: fakeSaveDialog; closeTabConfirm: stub; appCloseConfirm: fakeAppCloseConfirm
        }
    }
    property var controller
    SignalSpy { id: quitSpy; target: testCase.controller; signalName: "quitRequested" }
    function init() {
        oldReader=Backend.otbmReader; oldDocuments=Backend.docMgr;
        documents.currentIndex=0; documents.tabs=[{dirty:true,title:"Map"}];
        Backend.otbmReader=Qt.binding(()=>documents.current); Backend.docMgr=documents;
        writes=0; writeSuccess=true; reader.filePath="map.otbm";reader.dirty=true;
        secondReader.filePath="second.otbm";secondReader.dirty=true;
        window.token=0; window.closed=0; window.editorPresented=true; window.canPresent=true;
        fakeProfiles.succeeds=true; fakeSaveDialog.opened=0;
        fakeAppCloseConfirm.visible=false; fakeAppCloseConfirm.opened=0;
        controller=factory.createObject(testCase);
        verify(controller!==null);
        quitSpy.clear();
    }
    function cleanup() {
        controller.destroy(); controller=null;
        Backend.otbmReader=oldReader; Backend.docMgr=oldDocuments;
    }
    function test_saveWaitsForPresentedFrameAndRejectsDuplicate() {
        verify(controller.saveMap());
        verify(controller.saving); compare(writes,0);
        verify(!controller.saveMap());
        tryVerify(()=>window.token>0);
        compare(writes,0);
        controller.afterPresentedFrame(window.token+1); compare(writes,0);
        controller.afterPresentedFrame(window.token);
        compare(writes,1); compare(controller.saving,false);
        controller.afterPresentedFrame(window.token); compare(writes,1);
        verify(controller.savedToast.indexOf("Saved:")===0);
    }
    function test_saveAsWaitsAndUpdatesOnlyAfterSuccess() {
        controller.handleSaveAsAccepted("copy.otbm");
        tryVerify(()=>window.token>0); compare(reader.filePath,"map.otbm");
        controller.afterPresentedFrame(window.token);
        compare(reader.filePath,"copy.otbm"); compare(controller.recentMaps[0],"copy.otbm");
    }
    function test_writeFailurePreservesDirtyAndReportsError() {
        writeSuccess=false; controller.saveMap();
        tryVerify(()=>window.token>0);controller.afterPresentedFrame(window.token);
        compare(reader.dirty,true);compare(controller.saving,false);compare(controller.savedToast,"Disk full");
    }
    function test_clientFailureNeverStartsWriter() {
        fakeProfiles.succeeds=false; controller.saveMap();
        tryCompare(controller,"saving",false);compare(writes,0);compare(window.token,0);
    }
    function test_saveAllWaitsAndDoesNotCloseOnFailure() {
        writeSuccess=false;controller.beginSaveAllAndClose();
        tryVerify(()=>window.token>0);compare(window.closed,0);compare(writes,0);
        controller.afterPresentedFrame(window.token);
        compare(writes,1);compare(window.closed,0);compare(controller.appCloseSaveQueue.length,0);
        compare(quitSpy.count,0);compare(controller.appCloseAllowed,false);
    }
    function test_saveAllClosesOnlyAfterConfirmedWrite() {
        controller.beginSaveAllAndClose();tryVerify(()=>window.token>0);
        compare(window.closed,0);controller.afterPresentedFrame(window.token);
        tryCompare(window,"closed",1);compare(writes,1);
        compare(quitSpy.count,1);compare(controller.appCloseAllowed,true);
    }
    function test_cleanCloseRequestsApplicationQuitOnce() {
        documents.tabs=[{dirty:false,title:"Map"}];
        controller.requestAppClose();
        compare(window.closed,1);compare(quitSpy.count,1);
        compare(fakeAppCloseConfirm.opened,0);
        controller.requestAppClose();controller.finishAppClose();
        compare(window.closed,1);compare(quitSpy.count,1);
    }
    function test_cancelCloseKeepsUnsavedDocumentsOpen() {
        controller.requestAppClose();
        verify(fakeAppCloseConfirm.visible);compare(fakeAppCloseConfirm.opened,1);
        controller.requestAppClose();compare(fakeAppCloseConfirm.opened,1);
        fakeAppCloseConfirm.close();
        compare(window.closed,0);compare(quitSpy.count,0);verify(reader.dirty);
        controller.requestAppClose();compare(fakeAppCloseConfirm.opened,2);
    }
    function test_discardClosesWithoutWritingAndRequestsQuit() {
        controller.requestAppClose();fakeAppCloseConfirm.close();
        controller.finishAppClose();
        compare(writes,0);compare(window.closed,1);compare(quitSpy.count,1);
    }
    function test_cancelSaveAsDuringCloseDoesNotQuit() {
        reader.filePath="";
        controller.beginSaveAllAndClose();
        verify(controller.appCloseSaveAsPending);compare(fakeSaveDialog.opened,1);
        controller.handleSaveAsRejected();
        compare(controller.appCloseSaveAsPending,false);
        compare(window.closed,0);compare(quitSpy.count,0);verify(reader.dirty);
    }
    function test_closeWhileSavingDoesNotQuit() {
        controller.saveMap();controller.requestAppClose();
        compare(window.closed,0);compare(quitSpy.count,0);
        compare(fakeAppCloseConfirm.opened,0);
        tryVerify(()=>window.token>0);controller.afterPresentedFrame(window.token);
        compare(writes,1);compare(quitSpy.count,0);
    }
    function test_clientFailureDuringCloseDoesNotQuit() {
        fakeProfiles.succeeds=false;controller.beginSaveAllAndClose();
        tryCompare(controller,"saving",false);
        compare(writes,0);compare(window.closed,0);compare(quitSpy.count,0);
        compare(controller.appCloseAllowed,false);verify(reader.dirty);
    }
    function test_laterDocumentSaveFailureDoesNotQuit() {
        documents.tabs=[{dirty:true,title:"First"},{dirty:true,title:"Second"}];
        controller.beginSaveAllAndClose();tryVerify(()=>window.token>0);
        const first=window.token;controller.afterPresentedFrame(first);
        tryVerify(()=>window.token>first);writeSuccess=false;
        controller.afterPresentedFrame(window.token);
        compare(writes,2);compare(window.closed,0);compare(quitSpy.count,0);
        compare(controller.appCloseAllowed,false);verify(secondReader.dirty);
    }
    function test_readerChangeBeforeFrameNeverWritesWrongMap() {
        controller.saveMap();tryVerify(()=>window.token>0);
        Backend.otbmReader=oldReader;
        controller.afterPresentedFrame(window.token);
        compare(writes,0);compare(controller.saving,false);
    }
    function test_hiddenWindowFailsInsteadOfWaitingForever() {
        window.canPresent=false;controller.saveMap();
        tryCompare(controller,"saving",false);compare(writes,0);
        verify(controller.savedToast.indexOf("Restore")===0);
    }
    function test_saveAllPresentsFeedbackForEachDocument() {
        documents.tabs=[{dirty:true,title:"First"},{dirty:true,title:"Second"}];
        controller.beginSaveAllAndClose();tryVerify(()=>window.token>0);
        const first=window.token;controller.afterPresentedFrame(first);
        tryVerify(()=>window.token>first);compare(writes,1);compare(window.closed,0);
        compare(controller.savingFileName,"second.otbm");
        controller.afterPresentedFrame(window.token);tryCompare(window,"closed",1);compare(writes,2);
        compare(quitSpy.count,1);
    }
    function test_minimizedBeforeFirstEditorFrameCancelsSafely() {
        window.editorPresented=false;controller.saveMap();tryVerify(()=>window.token>0);
        window.canPresent=false;controller.afterPresentedFrame(window.token);
        compare(writes,0);compare(controller.saving,false);
    }
    function test_firstEditorFrameDoesNotStartInvisibleSave() {
        window.editorPresented=false;controller.saveMap();tryVerify(()=>window.token>0);
        const first=window.token;controller.afterPresentedFrame(first);compare(writes,0);
        verify(window.token>first);window.editorPresented=true;controller.afterPresentedFrame(window.token);compare(writes,1);
    }
}
