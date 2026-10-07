import QtQuick
import QtTest
import "../../editor/qml" as Editor
import "../../editor/qml/controllers" as Controllers

TestCase {
    id: testCase
    name: "StartupClientVersion"
    property url storagePath

    Component {
        id: sessionComponent
        Item {
            id: session
            property alias settings: prefs
            property alias profiles: profiles
            property alias startup: startup
            property bool started: true
            property var recentMaps: []
            property string pendingKey: ""
            property string pendingMapPath: ""
            property alias clientPaths: profiles.clientPaths
            property alias loadedClientKey: profiles.loadedClientKey

            function allProfileKeys() { return profiles.allProfileKeys(); }
            function profileLabel(key) { return profiles.profileLabel(key); }
            function clientFiles(folder) { return {dat: "", spr: "", otb: ""}; }
            function isCustomKey(key) { return profiles.isCustomKey(key); }

            Controllers.AppSettings {
                id: prefs
                location: testCase.storagePath
            }
            Controllers.ClientProfileController {
                id: profiles
                settings: prefs
                mapView: session
            }
            Editor.StartupWindow {
                id: startup
                app: session
                settings: prefs
            }
        }
    }

    function init() {
        storagePath = Qt.resolvedUrl("../../build/tmp/startup-client-version-"
                                    + Date.now() + "-" + Math.random() + ".ini");
    }

    function newSession(loadProfiles) {
        ignoreWarning(/.*Cannot open: qrc:\/ui\/github\/app-icon\.png/);
        const session = sessionComponent.createObject(testCase);
        verify(session !== null);
        if (loadProfiles !== false)
            session.profiles.load();
        return session;
    }

    function closeSession(session) {
        session.destroy();
        wait(0); // Destroy Settings and flush pending writes before restarting.
    }

    function combo(session) {
        const control = findChild(session.startup, "startupClientVersion");
        verify(control !== null);
        return control;
    }

    function selectProfile(session, key) {
        const control = combo(session);
        control.handleActivation(session.allProfileKeys().indexOf(key));
        compare(control.selKey, key);
        compare(session.settings.selectedClientKey, key);
    }

    function test_standardVersionSurvivesRestart() {
        let session = newSession();
        compare(combo(session).selKey, "760");
        selectProfile(session, "1098");
        closeSession(session);

        session = newSession();
        compare(combo(session).selKey, "1098");
        selectProfile(session, "772");
        closeSession(session);

        session = newSession();
        compare(combo(session).selKey, "772");
        closeSession(session);
    }

    function test_customProfileRestoresAfterProfilesLoad() {
        let session = newSession();
        verify(session.profiles.addCustomProfile("Midhem", 1098));
        verify(session.profiles.addCustomProfile("Other", 1098));
        selectProfile(session, "Midhem");
        selectProfile(session, "Other");
        closeSession(session);

        session = newSession(false);
        // StartupWindow is constructed before AppController loads custom profiles.
        compare(session.settings.selectedClientKey, "Other");
        compare(combo(session).selKey, "760");
        session.profiles.load();
        compare(combo(session).selKey, "Other");
        compare(combo(session).currentText, "Other  (10.98)");

        // Restoring by key survives an earlier custom profile being removed.
        session.profiles.removeCustomProfile("Midhem");
        compare(combo(session).selKey, "Other");
        closeSession(session);

        session = newSession();
        compare(combo(session).selKey, "Other");
        closeSession(session);
    }

    function test_missingSavedProfileFallsBackToFirstVersion() {
        let session = newSession();
        session.settings.selectedClientKey = "Removed";
        closeSession(session);

        session = newSession();
        compare(combo(session).selKey, "760");
        selectProfile(session, "860");
        closeSession(session);

        session = newSession();
        compare(combo(session).selKey, "860");
        closeSession(session);
    }
}
