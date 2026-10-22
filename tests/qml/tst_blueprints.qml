import QtQuick
import QtQuick.Controls
import QtTest
import Tibia 1.0
import "../../editor/qml/dialogs" as Dialogs
import "../../editor/qml/components" as Components

Item {
    id: testRoot
    width: 900
    height: 800
    QtObject {
        id: map
        property var calls: []
        property bool fail: false
        property bool emptySelection: false
        property bool blueprintActive: true
        property string blueprintPlacementMode: "merge"
        property int rotations: 0
        function blueprintSelectionSnapshot(mode) {
            calls = calls.concat([{action: "capture", mode: mode}]);
            if (emptySelection) return {success: false, error: "Select map tiles first."};
            return {success: true, payload: {mode: mode, width: 4, height: 3, floorCount: 2, source: {clientVersion: 860},
                tiles: [{dx: 0, dy: 0, dz: 0, items: [{id: 100, count: 7}]}]}, preview: ""};
        }
        function saveBlueprintSnapshot(name, category, payload) {
            calls = calls.concat([{action: "save", name: name, category: category, payload: payload}]);
            return fail ? {success: false, error: "Name already exists."} : {success: true};
        }
        function inspectBlueprintFile(url) {
            if (fail) return {success: false, error: "Item 999 is unavailable in this client."};
            const result = blueprintSelectionSnapshot("map");
            result.name = "Shared room"; result.category = "Imported";
            return result;
        }
        function blueprintDetails(name) {
            const result = blueprintSelectionSnapshot("map"); result.name = name; result.category = "Rooms";
            return result;
        }
        function rotateDoodadBrush() { rotations++; }
        function exportBlueprint(name, url) {
            calls = calls.concat([{action: "export", name: name, url: url}]);
            return fail ? {success: false, error: "Could not write file."} : {success: true};
        }
        function cancelBlueprint() { blueprintActive = false; }
    }
    QtObject {
        id: store
        property int revision: 1
        property var updated: ({})
        function prefabPaletteNames() { return ["Rooms", "Grounds"]; }
        function updateBlueprint(oldName, name, category) {
            updated = {oldName: oldName, name: name, category: category};
            return map.fail ? {success: false, error: "Could not write brushes.json."} : {success: true};
        }
        function removeBlueprint(name) {
            updated = {deleted: name};
            return map.fail ? {success: false, error: "Could not delete Blueprint."} : {success: true};
        }
    }
    QtObject {
        id: tilesets
        property int revision: 1
        function namesFor(kind) { return ["Structures", "Rooms"]; }
    }
    Component {
        id: dialogComponent
        Dialogs.BlueprintDialog { mapCtrl: map; brushStore: store; tilesetStore: tilesets }
    }
    Component {
        id: controlsComponent
        Components.BlueprintPlacementControls { mapCtrl: map }
    }
    SignalSpy { id: saved; signalName: "blueprintSaved" }
    TestCase {
        name: "Blueprints"
        when: windowShown
        function init() {
            map.calls = []; map.fail = false; map.emptySelection = false;
            map.blueprintActive = true; map.blueprintPlacementMode = "merge"; map.rotations = 0;
            store.updated = ({}); saved.target = null; saved.clear();
        }
        function cleanup() { Backend.uiTheme.style = "gray-dark"; }
        function createDialog() {
            const dialog = createTemporaryObject(dialogComponent, testRoot);
            verify(dialog); saved.target = dialog; return dialog;
        }
        function test_capture_data() {
            return ["classic", "windows-classic", "github-dark", "gray-dark", "gray-modern", "fluent-dark"]
                .map(theme => ({tag: theme, theme: theme}));
        }
        function test_capture(data) {
            Backend.uiTheme.style = data.theme;
            const dialog = createDialog(); dialog.openForSelection(); tryCompare(dialog, "opened", true);
            compare(dialog.payload.width, 4); compare(dialog.payload.floorCount, 2);
            verify(findChild(dialog, "blueprintDimensions").text.indexOf("2 floor") >= 0);
            verify(dialog.width <= testRoot.width && dialog.height <= testRoot.height);
            const mode = findChild(dialog, "blueprintCaptureMode"); mode.currentIndex = 1; mode.activated(1);
            compare(dialog.payload.mode, "ground");
            findChild(dialog, "blueprintName").text = "  Marsh patch  ";
            findChild(dialog, "blueprintNewCategory").text = "Swamps";
            waitForRendering(dialog.contentItem);
            grabImage(testRoot).save("build/blueprints-" + data.theme + ".png");
            mouseClick(findChild(dialog, "blueprintSave")); tryCompare(dialog, "opened", false);
            compare(saved.count, 1); compare(saved.signalArguments[0][0], "Swamps"); compare(saved.signalArguments[0][1], "Marsh patch");
            const call = map.calls[map.calls.length - 1];
            compare(call.action, "save"); compare(call.payload.mode, "ground"); compare(call.payload.floorCount, 2);
            compare(call.payload.tiles[0].items[0].count, 7);
        }
        function test_cancelAndEmptySelection() {
            const dialog = createDialog(); dialog.openForSelection(); tryCompare(dialog, "opened", true);
            mouseClick(findChild(dialog, "blueprintCancel")); tryCompare(dialog, "opened", false);
            compare(map.calls.filter(c => c.action === "save").length, 0);
            map.emptySelection = true; dialog.openForSelection(); tryCompare(dialog, "opened", true);
            verify(!findChild(dialog, "blueprintSave").enabled); verify(dialog.errorText.length > 0);
        }
        function test_validationAndSaveFailureKeepDraft() {
            const dialog = createDialog(); dialog.openForSelection(); tryCompare(dialog, "opened", true);
            mouseClick(findChild(dialog, "blueprintSave")); compare(dialog.errorText, "Enter a name and category.");
            findChild(dialog, "blueprintName").text = "Existing name"; map.fail = true;
            mouseClick(findChild(dialog, "blueprintSave")); verify(dialog.opened); compare(saved.count, 0);
            compare(findChild(dialog, "blueprintName").text, "Existing name"); compare(dialog.errorText, "Name already exists.");
            map.fail = false; findChild(dialog, "blueprintName").text = "New name";
            mouseClick(findChild(dialog, "blueprintSave")); tryCompare(dialog, "opened", false); compare(saved.count, 1);
        }
        function test_importPreviewAndFailure() {
            const dialog = createDialog(); map.fail = true; dialog.importFile("file:///bad.dmeblueprint");
            tryCompare(dialog, "opened", true); verify(!findChild(dialog, "blueprintSave").enabled);
            verify(dialog.errorText.indexOf("unavailable") >= 0);
            map.fail = false; dialog.importFile("file:///room.dmeblueprint");
            compare(findChild(dialog, "blueprintName").text, "Shared room");
            compare(dialog.payload.source.clientVersion, 860); compare(findChild(dialog, "blueprintNewCategory").text, "Imported");
            mouseClick(findChild(dialog, "blueprintSave")); tryCompare(dialog, "opened", false); compare(saved.count, 1);
        }
        function test_renameAndMove() {
            const dialog = createDialog(); dialog.openForBlueprint("Room"); tryCompare(dialog, "opened", true);
            verify(!findChild(dialog, "blueprintCaptureMode").visible);
            findChild(dialog, "blueprintName").text = "Hall";
            const picker = findChild(dialog, "blueprintCategory"); picker.currentIndex = dialog.categories.indexOf("Grounds");
            mouseClick(findChild(dialog, "blueprintSave")); tryCompare(dialog, "opened", false);
            compare(store.updated, {oldName: "Room", name: "Hall", category: "Grounds"}); compare(saved.count, 1);
            compare(map.calls.filter(c => c.action === "save").length, 0);
        }
        function test_placementControls_data() { return test_capture_data(); }
        function test_placementControls(data) {
            Backend.uiTheme.style = data.theme;
            const controls = createTemporaryObject(controlsComponent, testRoot); verify(controls); verify(controls.visible);
            const picker = findChild(controls, "blueprintPlacementMode"); picker.currentIndex = 1; picker.activated(1);
            compare(map.blueprintPlacementMode, "replace");
            map.blueprintPlacementMode = "merge"; compare(picker.currentIndex, 0);
            mouseClick(findChild(controls, "blueprintRotate")); compare(map.rotations, 1);
            mouseClick(findChild(controls, "blueprintStop")); compare(map.blueprintActive, false); verify(!controls.visible);
        }
        function test_exportAndDelete() {
            const dialog = createDialog(); dialog.openForBlueprint("Room"); tryCompare(dialog, "opened", true);
            map.fail = true; dialog.exportToFile("file:///room.dmeblueprint");
            compare(dialog.errorText, "Could not write file."); verify(dialog.opened);
            map.fail = false; dialog.exportToFile("file:///room.dmeblueprint");
            compare(dialog.errorText, ""); compare(map.calls[map.calls.length - 1].name, "Room");
            mouseClick(findChild(dialog, "blueprintDelete"));
            const confirm = findChild(dialog, "blueprintDeleteConfirm"); tryCompare(confirm, "opened", true);
            confirm.reject(); verify(dialog.opened); compare(store.updated, ({}));
            map.fail = true; dialog.deleteBlueprint();
            verify(dialog.opened); compare(dialog.errorText, "Could not delete Blueprint.");
            map.fail = false; dialog.deleteBlueprint(); tryCompare(dialog, "opened", false);
            compare(store.updated.deleted, "Room");
        }
    }
}
