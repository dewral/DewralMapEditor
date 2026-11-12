import QtQuick
import QtTest
import Tibia 1.0
import "../../editor/qml/dialogs" as Dialogs

Item {
    id: root
    width: 800
    height: 800

    ListModel {
        id: catalog
        ListElement { serverId: 2148; clientId: 3031; itemName: "gold coin"; itemGroup: 0 }
        ListElement { serverId: 2152; clientId: 3035; itemName: "platinum coin"; itemGroup: 0 }
        ListElement { serverId: 9249; clientId: 8860; itemName: "backpack of holding"; itemGroup: 2 }
        function groupNameForServerId(id) { return id === 9249 ? "Container" : "Other"; }
    }

    QtObject {
        id: controller
        property var contents: ({})
        property var calls: []
        property bool allowAdd: true

        function contextContainerItems(path) { return contents[path.join("/")] || []; }
        function addContextContainerItem(path, serverId) {
            calls = calls.concat([{path: path.slice(), serverId: serverId}]);
            if (!allowAdd)
                return false;
            const rows = contextContainerItems(path);
            const row = catalog.get(serverId === 2148 ? 0 : serverId === 2152 ? 1 : 2);
            const copy = Object.assign({}, contents);
            copy[path.join("/")] = [{
                serverId: serverId, name: row.itemName, groupName: catalog.groupNameForServerId(serverId),
                childIndex: rows.length, childCount: 0, path: path.concat([rows.length]), spriteIds: []
            }].concat(rows);
            contents = copy;
            return true;
        }
    }

    Component {
        id: dialogComponent
        Dialogs.ContainerDialog { mapCtrl: controller }
    }

    TestCase {
        name: "ContainerDialog"
        when: windowShown
        property var dialog: null
        property var previousReader: null
        property string previousTheme: ""

        function child(name) { return findChild(dialog, name); }
        function picker() { return child("containerItemPicker"); }
        function init() {
            previousReader = Backend.otbReader;
            previousTheme = Backend.uiTheme.style;
            Backend.otbReader = catalog;
            controller.calls = [];
            controller.allowAdd = true;
            controller.contents = {"3": [{serverId: 9249, name: "backpack of holding",
                groupName: "Container", childIndex: 0, childCount: 0, path: [3, 0], spriteIds: []}]};
            dialog = dialogComponent.createObject(root);
            verify(dialog !== null);
        }
        function cleanup() {
            picker().close();
            dialog.close();
            dialog.destroy();
            dialog = null;
            wait(0);
            Backend.otbReader = previousReader;
            Backend.uiTheme.style = previousTheme;
        }
        function openContainer(theme) {
            Backend.uiTheme.style = theme || "fluent-dark";
            dialog.openContainer([3], "quest chest");
            tryCompare(dialog, "opened", true);
        }
        function openPicker() {
            mouseClick(child("containerAddItemButton"));
            tryCompare(picker(), "opened", true);
            tryCompare(child("itemPickerList"), "count", 3);
            tryVerify(() => child("itemPickerList").currentItem !== null);
            wait(0); // Allow the picker's deferred initial selection and focus to settle.
        }
        function search(text) {
            // Type through the focused native text editor, as a user would.
            keySequence("Ctrl+A");
            for (let i = 0; i < text.length; ++i)
                keyClick(text[i]);
            tryCompare(child("itemPickerSearchField"), "text", text);
        }

        function test_addToNestedContainer_data() {
            return [{tag: "fluent", theme: "fluent-dark"}, {tag: "tibia", theme: "tibia-ui"}];
        }
        function test_addToNestedContainer(data) {
            openContainer(data.theme);
            mouseClick(child("containerOpenButton"));
            compare(dialog.currentPath, [3, 0]);
            compare(dialog.items.length, 0);
            openPicker();
            compare(controller.calls.length, 0);
            search("gold coin");
            tryCompare(child("itemPickerList"), "count", 1);
            mouseClick(child("itemPickerSelectButton"));
            tryCompare(picker(), "opened", false);
            compare(controller.calls, [{path: [3, 0], serverId: 2148}]);
            compare(dialog.items.length, 1);
            compare(dialog.selectedItem.serverId, 2148);
            mouseClick(child("containerBackButton"));
            compare(dialog.currentPath, [3]);
            compare(dialog.items[0].serverId, 9249);
        }

        function test_searchByIdAndEnter() {
            openContainer();
            openPicker();
            search("2152");
            tryCompare(child("itemPickerList"), "count", 1);
            keyClick(Qt.Key_Return);
            tryCompare(picker(), "opened", false);
            compare(controller.calls, [{path: [3], serverId: 2152}]);
            compare(dialog.selectedItem.serverId, 2152);
        }

        function test_keyboardNavigation() {
            openContainer();
            openPicker();
            tryCompare(child("itemPickerList"), "currentIndex", 0);
            keyClick(Qt.Key_Down);
            compare(child("itemPickerList").currentIndex, 1);
            keyClick(Qt.Key_Up);
            compare(child("itemPickerList").currentIndex, 0);
            keyClick(Qt.Key_Return);
            compare(controller.calls, [{path: [3], serverId: 2148}]);
        }

        function test_initialItemSelection() {
            openContainer();
            picker().openFor(9249);
            tryCompare(picker(), "opened", true);
            tryVerify(() => child("itemPickerList").currentItem !== null
                           && child("itemPickerList").currentItem.serverIdValue === 9249);
            compare(controller.calls.length, 0);
            mouseClick(child("itemPickerSelectButton"));
            tryCompare(picker(), "opened", false);
            compare(controller.calls, [{path: [3], serverId: 9249}]);
        }

        function test_cancelAndReopen_data() {
            return [{tag: "button", escape: false}, {tag: "escape", escape: true}];
        }
        function test_cancelAndReopen(data) {
            openContainer();
            openPicker();
            search("no matching item");
            tryCompare(child("itemPickerList"), "count", 0);
            compare(child("itemPickerSelectButton").enabled, false);
            if (data.escape)
                keyClick(Qt.Key_Escape);
            else
                mouseClick(child("itemPickerCancelButton"));
            tryCompare(picker(), "opened", false);
            compare(dialog.opened, true);
            compare(controller.calls.length, 0);
            openPicker();
            compare(child("itemPickerSearchField").text, "");
            mouseDoubleClickSequence(child("itemPickerList").currentItem);
            tryCompare(picker(), "opened", false);
            compare(controller.calls.length, 1);
        }

        function test_failedAddShowsError() {
            openContainer();
            controller.allowAdd = false;
            openPicker();
            mouseClick(child("itemPickerSelectButton"));
            tryCompare(picker(), "opened", false);
            compare(dialog.items.length, 1);
            verify(child("containerAddError").text.length > 0);
            controller.allowAdd = true;
            openPicker();
            compare(child("containerAddError").text, "");
            mouseClick(child("itemPickerSelectButton"));
            tryCompare(dialog, "addError", "");
            compare(dialog.items.length, 2);
        }

        function test_pickerFitsAfterResize() {
            openContainer();
            openPicker();
            picker().height = 440;
            const button = child("itemPickerSelectButton");
            tryVerify(() => button.mapToItem(picker().contentItem, 0, button.height).y
                           <= picker().contentItem.height);
            mouseClick(button);
            tryCompare(picker(), "opened", false);
            compare(controller.calls.length, 1);
        }
    }
}
