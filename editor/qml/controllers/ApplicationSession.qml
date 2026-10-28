import QtQuick
import Tibia 1.0

// Startup state has no dependency on a viewport or any editor dialog type.
Item {
    id: session
    width: 0; height: 0
    required property var settings
    property var startupWindow
    property var editorWindow: null
    readonly property var editorController: editorWindow ? editorWindow.appController : null
    readonly property bool started: editorController ? editorController.started : false
    readonly property bool editorPresented: editorWindow ? editorWindow.editorPresented : false
    readonly property alias profiles: catalog
    readonly property var clientPaths: catalog.clientPaths
    readonly property string loadedClientKey: catalog.loadedClientKey
    readonly property var recentMaps: editorController ? editorController.recentMaps : startupRecent
    readonly property string pendingKey: editorController ? editorController.pendingKey : folderProfileKey
    property var startupRecent: []
    property string folderProfileKey: ""
    property var pendingOperation: null
    property bool preparingEditor: false
    signal editorNeeded()
    signal operationFailed(string message)

    ClientProfileController { id: catalog; settings: session.settings }
    Component.onCompleted: {
        catalog.load();
        try { startupRecent = JSON.parse(settings.recentMapsJson) || []; }
        catch (error) { startupRecent = []; }
    }
    function allProfileKeys() { return catalog.allProfileKeys(); }
    function profileLabel(key) { return catalog.profileLabel(key); }
    function profileVer(key) { return catalog.profileVer(key); }
    function isCustomKey(key) { return catalog.isCustomKey(key); }
    function addCustomProfile(name, base) { return catalog.addCustomProfile(name, base); }
    function removeCustomProfile(key) { catalog.removeCustomProfile(key); }
    function clientFiles(folder) { return catalog.clientFiles(folder); }
    function request(operation) {
        if (pendingOperation) return false;
        pendingOperation = operation;
        if (!editorController && operation.kind !== "recover") {
            let key = operation.key;
            if (operation.kind === "open") {
                const header = Backend.otbmReader.inspectFileHeader(operation.path);
                if (header.success) key = catalog.keyForMap(header.clientVersion, operation.path, operation.key);
            }
            if (key && (catalog.clientPaths[key] || "") !== "") {
                operation.preparingClient = true;
                catalog.prepareClientVersion(key, function(success, error) {
                    if (session.pendingOperation !== operation) return;
                    operation.preparingClient = false;
                    session.dispatch();
                });
            }
        }
        Backend.workTimer.ensureLoaded();
        if (editorController) dispatch();
        else {
            preparingEditor = true;
            startupWindow.loadingMap = true;
            Backend.otbmReader.reportLoadingProgress(0, "Preparing editor...");
            editorNeeded();
        }
        return true;
    }
    function attachEditor(window) {
        editorWindow = window;
        preparingEditor = false;
        dispatch();
    }
    function failEditor(message) {
        pendingOperation = null; preparingEditor = false;
        startupWindow.loadingMap = false;
        operationFailed(message);
    }
    function dispatch() {
        if (!editorController || !pendingOperation || pendingOperation.preparingClient) return;
        const operation = pendingOperation;
        pendingOperation = null;
        if (operation.kind === "open") {
            if (!editorController.loadEverything(operation.path, operation.key)) {
                startupWindow.loadingMap = false;
                operationFailed("Could not open map.");
            }
        } else if (operation.kind === "new") {
            editorController.createNewMap(operation.key, operation.width, operation.height);
        } else if (operation.kind === "recover") {
            editorController.recoverPreviousSession();
        }
    }
    function loadEverything(path, key) { return request({kind:"open",path:path,key:key}); }
    function createNewMap(key, width, height) { return request({kind:"new",key:key,width:width,height:height}); }
    function recoverPreviousSession() { return request({kind:"recover"}); }
    function discardPreviousSession() { Backend.docMgr.discardRecoveries(); }
    function selectVersionFolder(key) {
        if (editorController) {
            editorController.pendingKey = key;
            editorController.pendingMapPath = "";
        } else folderProfileKey = key;
        startupWindow.openVersionFolderDialog();
    }
    function onVersionFolderPicked(url) {
        if (editorController) editorController.onVersionFolderPicked(url);
        else {
            const folder = Backend.fileTools.toLocalFile(url);
            if (folder) catalog.setVersionFolder(folderProfileKey, folder);
            folderProfileKey = "";
        }
    }
}
