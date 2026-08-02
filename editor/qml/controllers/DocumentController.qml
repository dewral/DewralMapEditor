import QtQuick
import Tibia 1.0

Item {
    id: controller
    visible: false
    width: 0
    height: 0

    required property var settings
    required property var profiles
    required property var palettes
    required property var mapView
    required property var appWindow
    required property var startupWindow
    required property var versionFolderDialog
    required property var saveDialog
    required property var closeTabConfirm
    required property var appCloseConfirm

    property bool started: false
    property var recentMaps: []
    property string pendingMapPath: ""
    property string pendingKey: ""
    property var pendingNewMap: null
    property string savedToast: ""
    property string activeLoadPath: ""
    property string activePreferredProfileKey: ""
    property bool waitingForAtlas: false
    property string atlasCompletionPath: ""
    property bool recoveringSession: false
    property var recoveryQueue: []
    property var activeRecovery: null

    property bool appCloseAllowed: false
    property var appCloseSaveQueue: []
    property bool appCloseSaveAsPending: false

    Connections {
        target: controller.mapView
        function onMapLoadFinished(success, path, error) {
            if (path !== controller.activeLoadPath)
                return;
            const preferred = controller.activePreferredProfileKey;
            controller.activeLoadPath = "";
            controller.activePreferredProfileKey = "";
            if (!success)
                return;
            controller.completeLoadedMap(path, preferred);
        }
        function onAtlasBuildFinished(success, error) {
            if (!controller.waitingForAtlas)
                return;
            controller.waitingForAtlas = false;
            var path = controller.atlasCompletionPath;
            controller.atlasCompletionPath = "";
            if (!success) {
                if (Backend.otbmReader.loading)
                    Backend.otbmReader.finishLoading(false);
                controller.showToast(error || "Could not build the sprite atlas");
                return;
            }
            controller.finalizeLoadedMap(path);
        }
    }

    function initialize() {
        loadRecent();
        profiles.load();
        palettes.load();
        Backend.docMgr.configureAutosave(settings.autosaveEnabled,
                                         settings.autosaveIntervalMinutes);
        if (Backend.docMgr.recoveryCount === 0
                && !settings.showStartup && recentMaps.length > 0)
            loadEverything(recentMaps[0]);
    }

    function recoverPreviousSession() {
        recoveryQueue = Backend.docMgr.recoveries.slice();
        if (recoveryQueue.length === 0)
            return;
        recoveringSession = true;
        recoverNextDocument();
    }

    function recoverNextDocument() {
        if (recoveryQueue.length === 0) {
            activeRecovery = null;
            recoveringSession = false;
            started = true;
            return;
        }
        var queue = recoveryQueue.slice();
        activeRecovery = queue.shift();
        recoveryQueue = queue;
        startupWindow.beginRecoveryLoad(activeRecovery.recoveryPath);
        if (!loadEverything(activeRecovery.recoveryPath,
                            activeRecovery.profileKey || "")) {
            showToast("Could not recover " + activeRecovery.title);
            activeRecovery = null;
            Qt.callLater(recoverNextDocument);
        }
    }

    function discardPreviousSession() {
        Backend.docMgr.discardRecoveries();
    }

    function loadRecent() {
        try {
            recentMaps = JSON.parse(settings.recentMapsJson) || [];
        } catch (e) {
            recentMaps = [];
        }
    }

    function addRecent(path) {
        var list = recentMaps.slice();
        var index = list.indexOf(path);
        if (index >= 0)
            list.splice(index, 1);
        list.unshift(path);
        if (list.length > 12)
            list = list.slice(0, 12);
        recentMaps = list;
        settings.recentMapsJson = JSON.stringify(list);
    }

    function showToast(message) {
        savedToast = message;
        toastTimer.restart();
    }

    function createNewMap(key, width, height) {
        if (!profiles.ensureClientVersion(key)) {
            pendingNewMap = {
                key: key,
                w: width,
                h: height
            };
            pendingKey = String(key);
            if (started)
                versionFolderDialog.open();
            else
                startupWindow.openVersionFolderDialog();
            return;
        }
        if (Backend.otbmReader.loaded || Backend.otbmReader.filePath !== "")
            Backend.docMgr.newDocument();
        Backend.otbmReader.newMap(width, height, profiles.profileVer(key), Backend.otbReader.majorVersion, Backend.otbReader.minorVersion);
        Backend.docMgr.setCurrentProfileKey(String(key));
        mapView.centerOnTile(Math.floor(width / 2), Math.floor(height / 2), 7);
        started = true;
    }

    function loadEverything(mapPath, preferredProfileKey) {
        if (mapPath === "")
            return false;

        var existing = Backend.docMgr.indexOfPath(mapPath);
        if (existing >= 0) {
            if (existing !== Backend.docMgr.currentIndex)
                Backend.docMgr.currentIndex = existing;
            if (Backend.docMgr.current.loaded)
                return completeLoadedMap(mapPath, preferredProfileKey);
        }

        if (Backend.otbmReader.loaded && Backend.otbmReader.filePath !== mapPath)
            Backend.docMgr.newDocument();
        activeLoadPath = mapPath;
        activePreferredProfileKey = preferredProfileKey === undefined
                                  || preferredProfileKey === null
                                  ? "" : String(preferredProfileKey);
        if (!mapView.loadMap(mapPath)) {
            activeLoadPath = "";
            activePreferredProfileKey = "";
            return false;
        }
        return true;
    }

    function completeLoadedMap(mapPath, preferredProfileKey) {
        if (!profiles.ensureClientLoaded(Backend.otbmReader, preferredProfileKey)) {
            if (Backend.otbmReader.loading)
                Backend.otbmReader.finishLoading(false);
            pendingMapPath = mapPath;
            var detectedVersion = Backend.otbmReader.suggestedClientVersion() > 0 ? Backend.otbmReader.suggestedClientVersion() : 772;
            var requestedKey = preferredProfileKey === undefined || preferredProfileKey === null ? "" : String(preferredProfileKey);
            pendingKey = requestedKey !== "" && profiles.profileVer(requestedKey) === detectedVersion ? requestedKey : profiles.resolveKeyForVersion(detectedVersion);
            if (started)
                versionFolderDialog.open();
            else
                startupWindow.openVersionFolderDialog();
            return false;
        }

        if (mapView.atlasBuilding) {
            waitingForAtlas = true;
            atlasCompletionPath = mapPath;
            return true;
        }
        return finalizeLoadedMap(mapPath);
    }

    function finalizeLoadedMap(mapPath) {
        if (activeRecovery && activeRecovery.recoveryPath === mapPath) {
            var recovery = activeRecovery;
            Backend.docMgr.adoptCurrentRecovery(recovery.id,
                                                recovery.originalPath || "");
            Backend.docMgr.setCurrentProfileKey(profiles.loadedClientKey);
            if (recovery.originalPath) {
                profiles.rememberMapProfile(recovery.originalPath,
                                            profiles.loadedClientKey);
                addRecent(recovery.originalPath);
            }
            if (Backend.otbmReader.loading)
                Backend.otbmReader.finishLoading(true);
            activeRecovery = null;
            Qt.callLater(recoverNextDocument);
            return true;
        }
        profiles.rememberMapProfile(mapPath, profiles.loadedClientKey);
        Backend.docMgr.setCurrentProfileKey(profiles.loadedClientKey);
        addRecent(mapPath);
        started = true;
        if (Backend.otbmReader.loading) {
            Backend.otbmReader.reportLoadingProgress(100, "Map ready");
            // Let the main window complete one scene-graph turn before the
            // startup loading window is removed.
            Qt.callLater(function () {
                if (Backend.otbmReader.loading)
                    Backend.otbmReader.finishLoading(true);
            });
        }
        return true;
    }

    function onVersionFolderPicked(folderUrl) {
        var folder = Backend.fileTools.toLocalFile(folderUrl);
        if (!folder)
            return;
        var pickedKey = pendingKey;
        profiles.setVersionFolder(pickedKey, folder);
        var mapPath = pendingMapPath;
        pendingMapPath = "";
        pendingKey = "";
        if (mapPath !== "") {
            if (Backend.otbmReader.loaded && Backend.otbmReader.filePath === mapPath)
                completeLoadedMap(mapPath, pickedKey);
            else
                loadEverything(mapPath, pickedKey);
        } else if (pendingNewMap) {
            var newMap = pendingNewMap;
            pendingNewMap = null;
            createNewMap(newMap.key, newMap.w, newMap.h);
        }
    }

    function saveMap() {
        if (Backend.otbmReader.filePath === "") {
            saveDialog.open();
            return;
        }
        Backend.otbmReader.applyClientVersions(profiles.loadedClientVersion, Backend.otbReader.majorVersion, Backend.otbReader.minorVersion);
        if (Backend.otbmReader.saveFile(Backend.otbmReader.filePath)) {
            profiles.rememberMapProfile(Backend.otbmReader.filePath, profiles.loadedClientKey);
            showToast("Saved: " + Backend.fileTools.fileName(Backend.otbmReader.filePath));
        }
    }

    function handleSaveAsAccepted(fileUrl) {
        var path = Backend.fileTools.toLocalFile(fileUrl);
        Backend.otbmReader.applyClientVersions(profiles.loadedClientVersion, Backend.otbReader.majorVersion, Backend.otbReader.minorVersion);
        if (Backend.otbmReader.saveFile(path)) {
            addRecent(path);
            profiles.rememberMapProfile(path, profiles.loadedClientKey);
            showToast("Saved: " + Backend.fileTools.fileName(path));
            if (appCloseSaveAsPending) {
                appCloseSaveAsPending = false;
                Qt.callLater(saveNextAndClose);
            }
        } else if (appCloseSaveAsPending) {
            abortSaveAllAndClose("Not all maps were saved");
        }
    }

    function handleSaveAsRejected() {
        if (appCloseSaveAsPending)
            abortSaveAllAndClose("");
    }

    function closeTab(index) {
        var tab = Backend.docMgr.tabs[index];
        if (tab && tab.dirty) {
            closeTabConfirm.tabIndex = index;
            closeTabConfirm.message = "Map \"" + tab.title + "\" has unsaved changes.\nClose without saving?";
            closeTabConfirm.open();
        } else {
            doCloseTab(index);
        }
    }

    function doCloseTab(index) {
        if (Backend.docMgr.closeDocument(index))
            started = false;
        else
            profiles.ensureClientLoaded(Backend.docMgr.current);
    }

    function dirtyTabIndices() {
        var result = [];
        var tabs = Backend.docMgr.tabs;
        for (var i = 0; i < tabs.length; ++i)
            if (tabs[i].dirty)
                result.push(i);
        return result;
    }

    function appCloseMessage(indices) {
        var tabs = Backend.docMgr.tabs;
        var names = [];
        var shown = Math.min(indices.length, 8);
        for (var i = 0; i < shown; ++i)
            names.push("- " + tabs[indices[i]].title);
        if (indices.length > shown)
            names.push("- ... and " + (indices.length - shown) + " more");
        return "Unsaved changes exist in " + indices.length + (indices.length === 1 ? " map:" : " maps:") + "\n\n" + names.join("\n") + "\n\nWhat would you like to do?";
    }

    function requestAppClose() {
        if (appCloseAllowed || appCloseConfirm.visible || appCloseSaveAsPending)
            return;
        var dirty = dirtyTabIndices();
        if (dirty.length === 0) {
            finishAppClose();
            return;
        }
        appCloseConfirm.message = appCloseMessage(dirty);
        appCloseConfirm.open();
    }

    function finishAppClose() {
        appCloseAllowed = true;
        appWindow.close();
    }

    function abortSaveAllAndClose(message) {
        appCloseSaveQueue = [];
        appCloseSaveAsPending = false;
        if (message && message.length > 0)
            showToast(message);
    }

    function beginSaveAllAndClose() {
        appCloseSaveQueue = dirtyTabIndices();
        saveNextAndClose();
    }

    function saveNextAndClose() {
        if (appCloseSaveQueue.length === 0) {
            finishAppClose();
            return;
        }

        var queue = appCloseSaveQueue.slice();
        var index = queue.shift();
        appCloseSaveQueue = queue;
        var tab = Backend.docMgr.tabs[index];
        if (!tab || !tab.dirty) {
            Qt.callLater(saveNextAndClose);
            return;
        }

        Backend.docMgr.currentIndex = index;
        var reader = Backend.docMgr.current;
        if (!reader || !profiles.ensureClientLoaded(reader)) {
            abortSaveAllAndClose("Not all maps were saved: client data is missing");
            return;
        }
        reader.applyClientVersions(profiles.loadedClientVersion, Backend.otbReader.majorVersion, Backend.otbReader.minorVersion);
        if (reader.filePath === "") {
            appCloseSaveAsPending = true;
            saveDialog.open();
            return;
        }
        if (!reader.saveFile(reader.filePath)) {
            abortSaveAllAndClose("Not all maps were saved");
            return;
        }
        profiles.rememberMapProfile(reader.filePath, profiles.loadedClientKey);
        Qt.callLater(saveNextAndClose);
    }

    Connections {
        target: Backend.docMgr
        function onCurrentChanged() {
            if (Backend.docMgr.current && Backend.docMgr.current.loaded)
                controller.profiles.ensureClientLoaded(Backend.docMgr.current);
        }
        function onAutosaveFailed(title, error) {
            controller.showToast("Autosave failed"
                                 + (title ? " for " + title : "")
                                 + ": " + error);
        }
    }

    Connections {
        target: Backend.tilesetStore
        function onErrorStringChanged() {
            if (Backend.tilesetStore.errorString.length > 0)
                controller.showToast(Backend.tilesetStore.errorString);
        }
    }

    Timer {
        id: toastTimer
        interval: 1800
        onTriggered: controller.savedToast = ""
    }
}
