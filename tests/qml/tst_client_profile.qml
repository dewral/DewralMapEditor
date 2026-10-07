import QtQuick
import QtTest
import "../../editor/qml/controllers" as Controllers

TestCase {
    name: "ClientProfileSelection"

    QtObject {
        id: prefs
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
        // Isolate selection from loading copyrighted DAT/SPR/OTB assets.
        function ensureClientVersion(key) {
            requestedKey = key;
            return true;
        }
    }

    function init() {
        reader.version = 1098;
        profiles.clientPaths = {"1098": "C:/client", "Midhem": "C:/client"};
        profiles.customProfiles = [{name: "Midhem", base: 1098}];
        profiles.mapProfiles = {"C:/maps/template.otbm": "1098"};
        profiles.requestedKey = "";
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
}
