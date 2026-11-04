import QtQuick
import QtTest
import Tibia 1.0
import "../../editor/qml/controllers" as Controllers

TestCase {
    id: testCase
    name: "LegacyMapLoad"
    property var calls: []
    property var chooseProfile: null

    QtObject { id: stub }
    QtObject {
        id: prefs
        property string clientPathsJson: "{}"
        property string mapProfilesJson: "{}"
    }
    QtObject {
        id: startup
        property bool loadingMap: false
        function chooseClientProfile(version, reason, callback) { testCase.chooseProfile = callback; }
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
        function prepareClientVersion(key, callback) {
            const success = ensureClientVersion(key);
            Qt.callLater(function() { callback(success, success ? "" : "Client data missing"); });
            return 1;
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
        chooseProfile = null;
        Backend.otbmReader.loaded = false;
        Backend.otbmReader.filePath = "";
        Backend.otbmReader.fileHeader = {success: true, otbmVersion: 0, clientVersion: 772};
        profiles.clientLoadSucceeds = true;
        profiles.clientPaths = {"760": "C:/client76", "1098": "C:/client"};
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

    function test_duplicateOpenStartsOneDecoder() {
        verify(documents.loadEverything("legacy.otbm", "760"));
        verify(documents.loadEverything("legacy.otbm", "760"));
        tryVerify(() => calls.length === 2);
        compare(calls, ["profile:760", "load:legacy.otbm"]);
    }

    function test_legacyLoadsMatchingDefinitionsBeforeDecoder() {
        verify(documents.loadEverything("legacy.otbm", "760"));
        tryVerify(() => calls.length === 2);
        compare(calls, ["profile:760", "load:legacy.otbm"]);
        compare(documents.activePreferredProfileKey, "760");
    }

    function test_missingDefinitionsWaitForProfileAndRetryBeforeDecode() {
        profiles.clientLoadSucceeds = false;
        verify(documents.loadEverything("legacy.otbm", "760"));
        tryVerify(() => chooseProfile !== null);
        compare(calls, ["profile:760"]);
        compare(documents.activeLoadPath, "");

        profiles.clientLoadSucceeds = true;
        chooseProfile("760");
        tryVerify(() => calls.length === 3);
        compare(calls, ["profile:760", "profile:760", "load:legacy.otbm"]);
    }

    function test_ambiguousMapWaitsForExplicitProfile() {
        Backend.otbmReader.fileHeader = {success: true, otbmVersion: 2, clientVersion: 1098};
        profiles.clientPaths = {"772": "C:/legacy", "1098": "C:/client", "Midhem": "C:/client"};
        profiles.customProfiles = [{name: "Midhem", base: 1098}];

        verify(documents.loadEverything("modern.otbm", "772"));
        verify(chooseProfile !== null);
        compare(calls.length, 0);
        chooseProfile("Midhem");
        tryVerify(() => calls.length === 2);
        compare(calls, ["profile:Midhem", "load:modern.otbm"]);
    }

    function test_selectedMidhemOverridesRememberedVanillaProfile() {
        Backend.otbmReader.fileHeader = {success: true, otbmVersion: 2, clientVersion: 1098};
        profiles.clientPaths = {"1098": "C:/client", "Midhem": "C:/client"};
        profiles.customProfiles = [{name: "Midhem", base: 1098}];
        profiles.mapProfiles = {"modern.otbm": "1098"};

        verify(documents.loadEverything("modern.otbm", "Midhem"));
        tryVerify(() => calls.length === 2);
        compare(calls, ["profile:Midhem", "load:modern.otbm"]);
    }

    function test_modernMapAlsoLoadsSubtypeDefinitionsBeforeDecode() {
        Backend.otbmReader.fileHeader = {success: true, otbmVersion: 2, clientVersion: 1098};
        verify(documents.loadEverything("modern.otbm", "1098"));
        tryVerify(() => calls.length === 2);
        compare(calls, ["profile:1098", "load:modern.otbm"]);
    }
}
