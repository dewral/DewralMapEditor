import QtQuick
import QtTest
import "../../editor/qml/controllers" as Controllers

TestCase {
    name: "ClientProfileSelection"

    QtObject {
        id: prefs
        property string selectedClientKey: ""
        property string clientPathsJson: "{}"
        property string customProfilesJson: "[]"
        property string mapProfilesJson: "{}"
    }
    QtObject { id: mapViewStub }
    QtObject {
        id: reader
        property string filePath: "C:/maps/template.otbm"
        property int version: 1098
        function suggestedClientVersion() { return version; }
    }
    Controllers.ClientProfileController {
        id: profiles
        settings: prefs
        mapView: mapViewStub
        property string requestedKey: ""
        property bool clientLoadSucceeds: true
        // Isolate selection from loading copyrighted DAT/SPR/OTB assets.
        function ensureClientVersion(key) {
            requestedKey = key;
            return clientLoadSucceeds;
        }
        function prepareClientVersion(key, callback) {
            const success = ensureClientVersion(key);
            Qt.callLater(function() { callback(success, success ? "" : "Client data missing"); });
            return 1;
        }
    }

    function init() {
        prefs.selectedClientKey = "";
        reader.version = 1098;
        profiles.clientPaths = {"1098": "C:/client", "Midhem": "C:/client"};
        profiles.customProfiles = [{name: "Midhem", base: 1098}];
        profiles.mapProfiles = {"C:/maps/template.otbm": "1098"};
        profiles.requestedKey = "";
        profiles.clientLoadSucceeds = true;
    }

    function test_selectedCustomProfileOverridesRememberedBaseVersion() {
        verify(profiles.ensureClientLoaded(reader, "Midhem"));
        compare(profiles.requestedKey, "Midhem");
    }

    function test_rememberedCustomProfileLoadsWithoutExplicitSelection() {
        profiles.mapProfiles = {"C:/maps/template.otbm": "Midhem"};
        verify(profiles.ensureClientLoaded(reader));
        compare(profiles.requestedKey, "Midhem");
    }

    function test_incompatibleSelectionUsesDetectedVersion() {
        profiles.customProfiles = [{name: "Midhem", base: 760}];
        verify(profiles.ensureClientLoaded(reader, "Midhem"));
        compare(profiles.requestedKey, "1098");
    }

    function test_profileKeyIsRememberedForNextOpen() {
        profiles.rememberMapProfile(reader.filePath, "Midhem");
        compare(JSON.parse(prefs.mapProfilesJson)[reader.filePath], "Midhem");
        verify(profiles.ensureClientLoaded(reader));
        compare(profiles.requestedKey, "Midhem");
    }

    function test_legacyHeaderRespects760Selection() {
        profiles.clientPaths = {"760": "C:/client76", "772": "C:/client772"};
        verify(profiles.ensureClientForMap(772, reader.filePath, "760"));
        compare(profiles.requestedKey, "760");
    }

    function test_legacyHeaderRespectsCustom760Selection() {
        profiles.customProfiles = [{name: "Custom76", base: 760}];
        verify(profiles.ensureClientForMap(772, reader.filePath, "Custom76"));
        compare(profiles.requestedKey, "Custom76");
        verify(!profiles.profileMatchesVersion("Custom76", 1098));
    }

    function test_preflightUsesRememberedProfileBeforeDocumentLoads() {
        profiles.mapProfiles = {"C:/maps/legacy.otbm": "Midhem"};
        verify(profiles.ensureClientForMap(1098, "C:/maps/legacy.otbm"));
        compare(profiles.requestedKey, "Midhem");
    }

    function test_switchingClientProfileSavesSelection() {
        verify(profiles.switchMapProfile("Midhem"));
        tryCompare(prefs, "selectedClientKey", "Midhem");
        verify(profiles.switchMapProfile("1098"));
        tryCompare(prefs, "selectedClientKey", "1098");
    }

    function test_failedProfileSwitchKeepsSavedSelection() {
        prefs.selectedClientKey = "Midhem";
        profiles.clientLoadSucceeds = false;
        verify(profiles.switchMapProfile("1098"));
        wait(10);
        tryCompare(prefs, "selectedClientKey", "Midhem");
    }
    function test_redux_profiles_and_version_labels() {
        verify(profiles.knownVersions.includes(740));
        verify(profiles.knownVersions.includes(10100));
        verify(profiles.knownVersions.includes(1330));
        verify(!profiles.knownVersions.includes(1525));
        compare(profiles.versionLabel(10100),"10.100");
        compare(profiles.versionLabel(1330),"13.30");
    }
}
