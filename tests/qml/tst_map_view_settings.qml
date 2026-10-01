import QtQuick
import QtTest
import "../../editor/qml/controllers" as Controllers

TestCase {
    id: testCase
    name: "MapViewSettings"
    property url storagePath: Qt.resolvedUrl("../../build/tmp/map-view-settings-"
                                          + Date.now() + ".ini")

    Component {
        id: sessionComponent
        Item {
            property alias settings: prefs
            property alias mapView: view
            property alias preferenceNames: persistence.preferenceNames
            Controllers.AppSettings {
                id: prefs
                location: testCase.storagePath
            }
            QtObject {
                id: view
                signal viewFlagsChanged()
                signal torchChanged()
                signal selectionOptionsChanged()
                property bool showWallOutlines: true
                property bool showGrid: false
                property bool showPathing: false
                property bool showCreatures: true
                property bool showSpawns: true
                property bool showHouses: true
                property bool showZones: true
                property bool showZonesAlways: true
                property bool showAnimations: false
                property bool torchOn: false
                property int lightAmbient: 40
                property bool minimapOn: false
                property bool showShade: true
                property bool showLowerFloors: true
                property bool placeEffect: true
                property bool automagic: true
                property bool compensatedSelect: true
                property int selectionFloors: 0
                onShowWallOutlinesChanged: viewFlagsChanged()
                onShowGridChanged: viewFlagsChanged()
                onShowPathingChanged: viewFlagsChanged()
                onShowCreaturesChanged: viewFlagsChanged()
                onShowSpawnsChanged: viewFlagsChanged()
                onShowHousesChanged: viewFlagsChanged()
                onShowZonesChanged: viewFlagsChanged()
                onShowZonesAlwaysChanged: viewFlagsChanged()
                onTorchOnChanged: torchChanged()
                onLightAmbientChanged: torchChanged()
                onCompensatedSelectChanged: selectionOptionsChanged()
                onSelectionFloorsChanged: selectionOptionsChanged()
            }
            Controllers.MapViewSettings {
                id: persistence
                settings: prefs
                mapView: view
            }
        }
    }

    function newSession() {
        const session = sessionComponent.createObject(testCase);
        verify(session !== null);
        return session;
    }

    function closeSession(session) {
        session.destroy();
        wait(0); // Destroy Settings and flush its pending property writes.
    }

    function test_savedPreferencesSurviveNewSessions() {
        let session = newSession();
        const defaults = {};
        const changed = {};
        for (const name of session.preferenceNames) {
            defaults[name] = session.mapView[name];
            changed[name] = typeof defaults[name] === "boolean" ? !defaults[name]
                            : (name === "lightAmbient" ? 128 : 2);
        }
        compare(defaults.showWallOutlines, true);
        compare(defaults.showAnimations, false);
        compare(defaults.lightAmbient, 40);

        // Direct view changes exercise the path used by all UI entry points.
        for (const name of session.preferenceNames) {
            session.mapView[name] = changed[name];
            compare(session.settings[name], changed[name], name + " must save");
        }
        // Ensure already-persisted non-view preferences remain intact too.
        session.settings.showFps = false;
        session.settings.showClientBox = true;
        session.settings.paletteWidth = 321;
        closeSession(session);

        session = newSession();
        for (const name of session.preferenceNames) {
            compare(session.mapView[name], changed[name], name + " must restore");
            compare(session.settings[name], changed[name], name + " must remain saved");
        }
        compare(session.settings.showFps, false);
        compare(session.settings.showClientBox, true);
        compare(session.settings.paletteWidth, 321);

        // Saving the original defaults must work as well as saving nondefaults.
        for (const name of session.preferenceNames)
            session.mapView[name] = defaults[name];
        closeSession(session);
        session = newSession();
        for (const name of session.preferenceNames)
            compare(session.mapView[name], defaults[name], name + " must reset");
        closeSession(session);
    }
}
