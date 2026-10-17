import QtQuick
import QtTest
import Tibia 1.0
import "../../editor/qml/controllers" as Controllers

TestCase {
    id: testCase
    name: "LegacyMapLoad"
    property var calls: []
    property int folderRequests: 0

    QtObject { id: stub }
    QtObject {
        id: prefs
        property string clientPathsJson: "{}"
    }
    QtObject {
        id: startup
        function openVersionFolderDialog() { testCase.folderRequests++; }
    }
    QtObject {
        id: map
        signal mapLoadFinished(bool success, string path, string error)
        signal atlasBuildFinished(bool success, string error)
        signal initialViewReady()
        function loadMap(path) {
            testCase.calls.push("load:" + path);
            return true;
        }
    }
    Controllers.ClientProfileController {
        id: profiles
        settings: prefs
        mapView: map
        property bool clientLoadSucceeds: true
        function ensureClientVersion(key) {
            testCase.calls.push("profile:" + key);
            return clientLoadSucceeds;
        }
    }
    Controllers.DocumentController {
        id: documents
        settings: prefs
        profiles: profiles
        palettes: stub
        mapView: map
        appWindow: stub
        startupWindow: startup
        versionFolderDialog: stub
        saveDialog: stub
        closeTabConfirm: stub
        appCloseConfirm: stub
    }

    function init() {
        calls = [];
        folderRequests = 0;
        Backend.otbmReader.loaded = false;
        Backend.otbmReader.filePath = "";
        Backend.otbmReader.fileHeader = {success: true, otbmVersion: 0, clientVersion: 772};
        profiles.clientLoadSucceeds = true;
        profiles.clientPaths = {};
        profiles.mapProfiles = {};
        documents.pendingMapPath = "";
        documents.pendingKey = "";
        documents.activeLoadPath = "";
        documents.started = false;
    }

    function cleanup() {
        Backend.otbmReader.loaded = true;
        Backend.otbmReader.fileHeader = {success: false};
    }

    function test_legacyLoadsMatchingDefinitionsBeforeDecoder() {
        verify(documents.loadEverything("legacy.otbm", "760"));
        compare(calls, ["profile:760", "load:legacy.otbm"]);
        compare(documents.activePreferredProfileKey, "760");
    }

    function test_missingDefinitionsWaitForFolderAndRetryBeforeDecode() {
        profiles.clientLoadSucceeds = false;
        verify(documents.loadEverything("legacy.otbm", "760"));
        compare(calls, ["profile:760"]);
        compare(folderRequests, 1);
        compare(documents.pendingKey, "760");
        compare(documents.activeLoadPath, "");

        profiles.clientLoadSucceeds = true;
        documents.onVersionFolderPicked("C:/client76");
        compare(profiles.clientPaths["760"], "C:/client76");
        compare(calls, ["profile:760", "profile:760", "load:legacy.otbm"]);
        compare(documents.pendingMapPath, "");
    }

    function test_modernMapAlsoLoadsSubtypeDefinitionsBeforeDecode() {
        Backend.otbmReader.fileHeader = {success: true, otbmVersion: 2, clientVersion: 1098};
        verify(documents.loadEverything("modern.otbm", "1098"));
        compare(calls, ["profile:1098", "load:modern.otbm"]);
    }
}
