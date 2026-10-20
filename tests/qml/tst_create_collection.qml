import QtQuick
import QtQuick.Controls
import QtTest
import Tibia 1.0
import "../../editor/qml/dialogs" as Dialogs

Item {
    id: testRoot
    width: 800
    height: 650
    QtObject {
        id: map
        property var selectedIds: [100, 200, 300]
        function selectionItemIds() { return selectedIds.slice(); }
    }
    QtObject {
        id: store
        property var saved: ({})
        property string errorString: ""
        property bool failSave: false
        function namesFor(category) { return Object.keys(saved); }
        function newTileset(category, name, ids) {
            if (failSave) { errorString = "Profile is read-only."; return false; }
            if (category !== "collection" || saved[name]) return false;
            const copy = Object.assign({}, saved);
            copy[name] = ids.slice();
            saved = copy;
            return true;
        }
    }
    Component {
        id: dialogComponent
        Dialogs.CreateCollectionDialog {
            mapCtrl: map
            tilesetStore: store
            itemSource: null
            spriteSource: null
        }
    }
    SignalSpy { id: created; signalName: "collectionCreated" }
    TestCase {
        name: "CreateCollection"
        when: windowShown
        function init() {
            map.selectedIds = [100, 200, 300];
            store.saved = ({});
            store.errorString = "";
            store.failSave = false;
            created.target = null;
            created.clear();
        }
        function cleanup() { Backend.uiTheme.style = "gray-dark"; }
        function openDialog() {
            const dialog = createTemporaryObject(dialogComponent, testRoot);
            verify(dialog);
            created.target = dialog;
            dialog.openForSelection();
            tryCompare(dialog, "opened", true);
            return dialog;
        }
        function test_previewAndSave_data() {
            return ["classic", "windows-classic", "github", "gray-dark", "gray-modern", "fluent-dark"]
                    .map(theme => ({tag: theme, theme}));
        }
        function test_previewAndSave(data) {
            Backend.uiTheme.style = data.theme;
            const dialog = openDialog();
            const grid = findChild(dialog, "collectionItems");
            compare(grid.count, 3);
            verify(grid.height > 100);
            verify(dialog.width <= testRoot.width);
            tryVerify(() => grid.itemAtIndex(1) !== null);
            waitForRendering(dialog.contentItem);
            grabImage(testRoot).save("build/create-collection-" + data.theme + ".png");
            const remove = findChild(grid.itemAtIndex(1), "removeCollectionItem_200");
            verify(remove);
            mouseClick(remove);
            compare(dialog.pendingServerIds, [100, 300]);
            compare(map.selectedIds, [100, 200, 300]);
            findChild(dialog, "collectionName").text = "  Cave decorations  ";
            mouseClick(findChild(dialog, "saveCollection"));
            compare(store.saved["Cave decorations"], [100, 300]);
            compare(created.count, 1);
            compare(created.signalArguments[0][0], "Cave decorations");
            compare(created.signalArguments[0][1], 100);
            tryCompare(dialog, "opened", false);
        }
        function test_cancelAndSnapshot() {
            const dialog = openDialog();
            map.selectedIds = [400];
            dialog.removeItem(100);
            compare(dialog.pendingServerIds, [200, 300]);
            mouseClick(findChild(dialog, "resetCollectionItems"));
            compare(dialog.pendingServerIds, [100, 200, 300]);
            mouseClick(findChild(dialog, "cancelCollection"));
            tryCompare(dialog, "opened", false);
            compare(Object.keys(store.saved).length, 0);
            compare(created.count, 0);
            dialog.openForSelection();
            compare(dialog.pendingServerIds, [400]);
            dialog.close();
        }
        function test_validationAndSaveFailure() {
            const dialog = openDialog();
            dialog.commit();
            verify(dialog.errorText.indexOf("name") >= 0);
            const name = findChild(dialog, "collectionName");
            name.text = "Existing";
            store.saved = ({Existing: [999]});
            dialog.commit();
            verify(dialog.errorText.indexOf("already exists") >= 0);
            compare(store.saved.Existing, [999]);
            name.text = "New";
            store.failSave = true;
            dialog.commit();
            compare(dialog.errorText, "Profile is read-only.");
            verify(dialog.opened);
            compare(created.count, 0);
            compare(dialog.pendingServerIds, [100, 200, 300]);
            dialog.removeItem(100);
            dialog.removeItem(200);
            dialog.removeItem(300);
            verify(!findChild(dialog, "saveCollection").enabled);
            dialog.commit();
            verify(dialog.errorText.indexOf("at least one") >= 0);
            dialog.close();
        }
        function test_emptySelection() {
            map.selectedIds = [];
            const dialog = openDialog();
            compare(findChild(dialog, "collectionItems").count, 0);
            verify(dialog.errorText.indexOf("no items") >= 0);
            verify(!findChild(dialog, "saveCollection").enabled);
            dialog.close();
        }
    }
}
