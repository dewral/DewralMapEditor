import QtQuick
import QtQuick.Window
import QtTest
import Tibia 1.0
import "../../editor/qml/components"

Item {
    id: scene
    width: 1000; height: 780
    QtObject {
        id: controller
        property bool queryBusy: false
        property int queryProgress: 0
        property var searches: []
        property var destination: []
        property int cancellations: 0
        property QtObject otbm: QtObject { signal loadedChanged() }
        property QtObject otb: QtObject { signal modelReset() }
        property var dat: null
        signal itemSearchFinished(var response)
        signal readersChanged()
        function startItemSearch(type, selection) {
            if (queryBusy) return false;
            searches = searches.concat([{type: type, selection: selection}]);
            queryBusy = true;
            return true;
        }
        function cancelMapQuery() { ++cancellations; }
        function centerOnPosition(x, y, z) { destination = [x, y, z]; }
        function finish(response) { queryBusy = false; itemSearchFinished(response); }
    }
    SearchResultsPanel {
        id: panel
        anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
        width: 340; mapCtrl: controller
        onClosed: visible = false
    }
    Rectangle {
        id: mapSurface
        width: scene.width - panel.width; height: scene.height
        color: "#171717"
        MouseArea { anchors.fill: parent; onClicked: mapSurface.color = "#282828" }
    }
    Item {
        id: lazyDock
        width: 300; height: 600; visible: false
        function close() { visible = false; }
    }
    LazyToolHost {
        id: lazyTool
        toolSource: Qt.resolvedUrl("../../editor/qml/tools/SearchResultsPanelTool.qml")
        initialProperties: ({environment: {workspace: {mapView: controller}, searchResultsHost: lazyDock}})
    }
    TestCase {
        name: "SearchResultsSidebar"
        when: windowShown
        function locate(name) { return findChild(panel, name); }
        function item(id, name, contents) {
            const result = {serverId: id, clientId: 0, name: name, x: 867, y: 889, z: 6,
                actionId: 5000, uniqueId: 62378, count: 1, isContainer: contents !== undefined,
                childCount: contents ? contents.length : 0, containerPath: ""};
            if (contents) result.contents = contents;
            return result;
        }
        function showResults() {
            const coins = item(2148, "gold coin"); coins.count = 72; coins.stackable = true;
            const bag = item(1987, "bag", [coins]);
            const bookcase = item(12698, "bookcase", [bag, item(2376, "sword")]);
            controller.finish({results: [bookcase, item(1988, "backpack", [])], total: 2});
        }
        function showManyContainers() {
            const results = [];
            for (let i = 0; i < 250; ++i) {
                const coins = item(2148, "gold coin"); coins.count = 72; coins.stackable = true;
                const bag = item(1987, "bag", [coins]);
                bag.containerPath = "Container " + i;
                const root = item(1000 + i, "Container " + i, [bag, item(2376, "sword")]);
                if (i % 3 === 0) root.name += " with a longer name that wraps onto another line";
                results.push(root);
            }
            controller.finish({results: results, total: results.length});
        }
        function initTestCase() { scene.Window.window.width = 1000; scene.Window.window.height = 780; }
        function init() {
            controller.queryBusy = false; controller.searches = []; controller.destination = []; controller.cancellations = 0;
            scene.height = 780;
            panel.visible = true; panel.width = 340; Backend.uiTheme.style = "fluent-dark";
            panel.openSearch("unique", false);
        }
        function cleanup() { panel.close(); controller.finish({cancelled: true}); }

        function test_copyQuest_data() {
            return ["fluent-dark", "tibia-ui"]
                .map(theme => ({tag: theme, theme: theme}));
        }
        function test_copyQuest(data) {
            Backend.uiTheme.style = data.theme;
            const chest = item(9712, "quest chest", [item(6810, "plate armor")]);
            chest.x = 31391; chest.y = 31038; chest.z = 8; chest.uniqueId = 62474;
            controller.finish({results: [chest, item(1988, "backpack", [])], total: 2});
            tryVerify(() => locate("searchResultIds_0") !== null);
            mouseClick(locate("searchResultRow_1"), 10, 10);
            Backend.fileTools.setClipboard("unchanged");
            const menu = locate("searchResultsQuestMenu");
            mouseClick(locate("searchResultIds_0"), 10, 5, Qt.RightButton);
            tryCompare(menu, "visible", true);
            compare(panel.selectedRow.key, "0");
            compare(Backend.fileTools.clipboardText(), "unchanged");
            compare(controller.destination, []);
            mouseClick(locate("searchResultsCopyQuest"));
            tryCompare(menu, "visible", false);
            compare(Backend.fileTools.clipboardText(), "{\n"
                + "    position: {x: 31391, y: 31038, z: 8},\n"
                + "    name: \"\",\n"
                + "    reward: [{id: 6810, name: \"plate armor\"}]\n}");
            compare(panel.visibleRowCount, 2, "Copy must include collapsed contents without expanding");
            const copied = Backend.fileTools.clipboardText();
            mouseClick(locate("searchResultContents_0"));
            mouseClick(locate("searchResultIds_0"), 10, 5, Qt.RightButton);
            tryCompare(menu, "visible", true);
            mouseClick(locate("searchResultsCopyQuest"));
            compare(Backend.fileTools.clipboardText(), copied);
            mouseClick(locate("searchResultIds_0"), 10, 5, Qt.RightButton);
            tryCompare(menu, "visible", true);
            keyClick(Qt.Key_Escape);
            tryCompare(menu, "visible", false);
            verify(panel.visible, "Escape should dismiss the context menu before the panel");
        }
        function test_copyQuestRewardsAndEscaping() {
            const name = "hero's \"armor\" \\ reward\nnext line";
            const bag = item(1987, "bag", [item(2148, "gold coin")]);
            const chest = item(9712, "quest chest", [item(6810, name), bag, item(6810, "")]);
            controller.finish({results: [chest], total: 1});
            tryVerify(() => locate("searchResultIds_0") !== null);
            mouseClick(locate("searchResultIds_0"), 10, 5, Qt.RightButton);
            tryCompare(locate("searchResultsQuestMenu"), "visible", true);
            mouseClick(locate("searchResultsCopyQuest"));
            const copied = eval("(" + Backend.fileTools.clipboardText() + ")");
            compare(copied, {position: {x: 867, y: 889, z: 6}, name: "",
                reward: [{id: 6810, name: name}, {id: 1987, name: "bag"}, {id: 6810, name: ""}]});
        }
        function test_emptyQuestAndNoUniqueId() {
            const plain = item(2376, "sword"); plain.uniqueId = 0;
            controller.finish({results: [item(1988, "backpack", []), plain], total: 2});
            tryVerify(() => locate("searchResultIds_0") !== null);
            const menu = locate("searchResultsQuestMenu");
            mouseClick(locate("searchResultIds_0"), 10, 5, Qt.RightButton);
            tryCompare(menu, "visible", true);
            mouseClick(locate("searchResultsCopyQuest"));
            compare(eval("(" + Backend.fileTools.clipboardText() + ")").reward, []);
            Backend.fileTools.setClipboard("unchanged");
            mouseClick(locate("searchResultIds_1"), 10, 5, Qt.RightButton);
            verify(!menu.visible);
            compare(Backend.fileTools.clipboardText(), "unchanged");
            mouseClick(locate("searchResultIds_0"), 10, 5, Qt.RightButton);
            tryCompare(menu, "visible", true);
            controller.readersChanged();
            tryCompare(menu, "visible", false);
            verify(!locate("searchResultsCopyQuest").enabled);
        }
        function test_contentsGrouping_data() {
            return ["fluent-dark", "tibia-ui"]
                .map(theme => ({tag: theme, theme: theme}));
        }
        function test_contentsGrouping(data) {
            Backend.uiTheme.style = data.theme;
            showResults();
            mouseClick(locate("searchResultsExpandAll"));
            wait(50);
            const root = locate("searchResultCard_0"), child = locate("searchResultCard_0/0");
            const nested = locate("searchResultCard_0/0/0"), sibling = locate("searchResultCard_0/1");
            verify(child.x > root.x);
            verify(nested.x > child.x);
            compare(sibling.x, child.x);
            const rail = locate("searchResultGroupRail_0/0");
            verify(rail.visible);
            const row = locate("searchResultRow_0/0");
            verify(rail.y < 0, "The group rail must bridge the gap between related rows");
            compare(rail.y + rail.height, row.height);
            compare(locate("searchResultCard_1").x, root.x);
            verify(!locate("searchResultGroupRail_1").visible);
            panel.width = 220;
            wait(50);
            verify(nested.width > 0);
            verify(nested.mapToItem(panel, nested.width, 0).x <= panel.width);
        }
        function test_navigationAndContents_data() {
            return ["fluent-dark", "tibia-ui"]
                .map(theme => ({tag: theme, theme: theme}));
        }
        function test_navigationAndContents(data) {
            Backend.uiTheme.style = data.theme;
            showResults();
            tryVerify(() => locate("searchResultRow_0") !== null);
            mouseClick(locate("searchResultContents_0"));
            compare(panel.visibleRowCount, 4);
            tryVerify(() => locate("searchResultContents_0/0") !== null);
            mouseClick(locate("searchResultContents_0/0"));
            compare(panel.visibleRowCount, 5);
            compare(panel.rowAt(2).item.serverId, 2148);
            compare(panel.rowAt(2).item.count, 72);
            tryVerify(() => locate("searchResultRow_0/0/0") !== null);
            mouseDoubleClickSequence(locate("searchResultRow_0/0/0"), 10, 10);
            compare(controller.destination, [867, 889, 6]);
            verify(panel.visible);
            mouseClick(locate("searchResultsGoTo"));
            verify(panel.visible);
            mouseClick(mapSurface, 40, 40);
            compare(mapSurface.color, "#282828");
            verify(!locate("searchResultContents_1").enabled);
            mouseClick(locate("searchResultContents_0"));
            compare(panel.visibleRowCount, 2);
        }
        function test_filterKeyboardAndResize() {
            showResults();
            const field = locate("searchResultsFilter");
            field.text = "12698";
            compare(panel.visibleRowCount, 1);
            compare(panel.rowAt(0).item.serverId, 12698);
            field.text = "5000"; compare(panel.visibleRowCount, 2);
            field.text = "62378"; compare(panel.visibleRowCount, 2);
            field.text = "backpack"; compare(panel.visibleRowCount, 1);
            field.text = "missing"; compare(panel.visibleRowCount, 0);
            verify(!locate("searchResultsGoTo").enabled);
            field.text = "";
            const list = locate("searchResultsList");
            list.forceActiveFocus(); keyClick(Qt.Key_Down); keyClick(Qt.Key_Right);
            compare(panel.visibleRowCount, 4);
            keyClick(Qt.Key_Left); compare(panel.visibleRowCount, 2);
            keyClick(Qt.Key_Return); compare(controller.destination, [867, 889, 6]);
            panel.width = 220; wait(50);
            const go = locate("searchResultsGoTo"), refresh = locate("searchResultsRefresh");
            verify(go.mapToItem(panel, go.width, 0).x <= refresh.mapToItem(panel, 0, 0).x);
            verify(refresh.mapToItem(panel, refresh.width, refresh.height).y <= panel.height);
            keyClick(Qt.Key_Escape); verify(!panel.visible);
        }
        function test_repeatedSearchAndCancellation() {
            panel.openSearch("action", true);
            compare(controller.cancellations, 1);
            controller.finish({results: [item(100, "old")], total: 1});
            tryCompare(controller, "queryBusy", true);
            compare(controller.searches[1], {type: "action", selection: true});
            compare(panel.results.length, 0);
            showResults();
            mouseClick(locate("searchResultsRefresh")); verify(panel.loading);
            mouseClick(locate("searchResultsRefresh")); compare(controller.cancellations, 2);
            controller.finish({cancelled: true}); verify(!panel.loading);
            controller.readersChanged(); compare(panel.results.length, 0);
            verify(!locate("searchResultsGoTo").enabled);
        }
        function test_waitingAndClose() {
            panel.close(); verify(!panel.visible);
            panel.visible = true; panel.openSearch("container", false);
            verify(panel.pendingSearch);
            controller.finish({cancelled: true});
            tryCompare(controller, "queryBusy", true);
            showResults();
            mouseClick(locate("searchResultsClose")); verify(!panel.visible);
        }
        function test_reloadedMapAndClient() {
            showResults();
            controller.otbm.loadedChanged();
            compare(panel.results.length, 0);
            mouseClick(locate("searchResultsRefresh")); verify(panel.loading);
            controller.otb.modelReset();
            verify(!panel.loading);
            verify(!panel.pendingSearch);
            controller.finish({results: [item(100, "old")], total: 1});
            compare(panel.results.length, 0);
        }
        function test_lazyToolUsesSidebarHost() {
            verify(lazyTool.ensureLoaded());
            compare(lazyTool.item.parent, lazyDock);
            compare(lazyTool.item.width, lazyDock.width);
            compare(lazyTool.item.height, lazyDock.height);
            lazyDock.width = 260;
            compare(lazyTool.item.width, 260);
            lazyTool.active = false;
        }
        function test_expansionKeepsViewport_data() {
            return ["fluent-dark", "tibia-ui"]
                .map(theme => ({tag: theme, theme: theme}));
        }
        function test_expansionKeepsViewport(data) {
            Backend.uiTheme.style = data.theme;
            showManyContainers();
            const list = locate("searchResultsList");
            list.positionViewAtIndex(0, ListView.Beginning);
            tryVerify(() => locate("searchResultRow_0") !== null);
            mouseClick(locate("searchResultRow_0"), 10, 10);
            for (const key of ["150", "249"]) {
                list.positionViewAtIndex(Number(key), key === "150" ? ListView.Center : ListView.End);
                wait(80);
                const before = locate("searchResultRow_" + key).mapToItem(list, 0, 0).y;
                mouseClick(locate("searchResultContents_" + key));
                wait(100);
                const expanded = locate("searchResultRow_" + key);
                verify(expanded !== null, "Expanding scrolled the clicked container out of view");
                verify(Math.abs(expanded.mapToItem(list, 0, 0).y - before) < 1,
                       "Expanding moved the clicked container in the viewport");
                mouseClick(locate("searchResultContents_" + key));
                wait(100);
                const collapsed = locate("searchResultRow_" + key);
                verify(collapsed !== null);
                verify(Math.abs(collapsed.mapToItem(list, 0, 0).y - before) < 1,
                       "Collapsing moved the clicked container in the viewport");
                compare(panel.selectedRow.item.serverId, 1000 + Number(key));
            }
        }
        function test_expandAllNestedContentsAndCollapseAll() {
            verify(!locate("searchResultsExpandAll").enabled);
            showManyContainers();
            const list = locate("searchResultsList");
            list.positionViewAtIndex(150, ListView.Beginning);
            wait(80);
            const before = locate("searchResultRow_150").mapToItem(list, 0, 0).y;
            mouseClick(locate("searchResultsExpandAll"));
            compare(panel.visibleRowCount, 1000);
            verify(panel.expanded["0/0"] && panel.expanded["249/0"]);
            wait(100);
            verify(Math.abs(locate("searchResultRow_150").mapToItem(list, 0, 0).y - before) < 1);
            const coins = panel.rowAt(panel.rowIndex("150/0/0")).item;
            compare(coins.serverId, 2148); compare(coins.count, 72);
            mouseClick(locate("searchResultsCollapseAll"));
            compare(panel.visibleRowCount, 250);
            wait(100);
            verify(Math.abs(locate("searchResultRow_150").mapToItem(list, 0, 0).y - before) < 1);
            const field = locate("searchResultsFilter");
            field.text = "Container 150";
            compare(panel.visibleRowCount, 1);
            mouseClick(locate("searchResultsExpandAll"));
            compare(panel.visibleRowCount, 4);
            mouseClick(locate("searchResultsCollapseAll"));
            compare(panel.visibleRowCount, 1);
        }
        function test_nestedCollapsePreservesSelectionAndPosition() {
            showManyContainers();
            const list = locate("searchResultsList");
            list.positionViewAtIndex(150, ListView.Center);
            wait(50);
            mouseClick(locate("searchResultContents_150"));
            list.positionViewAtIndex(panel.rowIndex("150/0"), ListView.Center);
            wait(50);
            const before = locate("searchResultRow_150/0").mapToItem(list, 0, 0).y;
            mouseClick(locate("searchResultContents_150/0"));
            wait(100);
            verify(Math.abs(locate("searchResultRow_150/0").mapToItem(list, 0, 0).y - before) < 1);
            mouseClick(locate("searchResultRow_150/0/0"), 10, 10);
            compare(panel.selectedRow.key, "150/0/0");
            mouseClick(locate("searchResultContents_150/0"));
            compare(panel.selectedRow.key, "150/0");
            wait(100);
            verify(Math.abs(locate("searchResultRow_150/0").mapToItem(list, 0, 0).y - before) < 1);
            list.forceActiveFocus();
            keyClick(Qt.Key_End);
            compare(panel.selectedRow.key, "249");
            verify(locate("searchResultRow_249").mapToItem(list, 0, 0).y < list.height);
            keyClick(Qt.Key_Home);
            compare(panel.selectedRow.key, "0");
        }
        function test_compactPanelStillShowsResults() {
            showResults();
            panel.width = 220;
            scene.height = 185;
            wait(50);
            const list = locate("searchResultsList");
            verify(list.height > 0);
            const expand = locate("searchResultsExpandAll"), collapse = locate("searchResultsCollapseAll");
            verify(expand.mapToItem(panel, expand.width, 0).x <= collapse.mapToItem(panel, 0, 0).x);
            verify(collapse.mapToItem(panel, collapse.width, 0).x <= panel.width);
            mouseClick(expand); compare(panel.visibleRowCount, 5);
            mouseClick(collapse); compare(panel.visibleRowCount, 2);
        }
        function test_resultLimitScrollAndScreenshot() {
            const results = [];
            for (let i = 0; i < 500; ++i) results.push(item(1000 + i, "Item " + i));
            controller.finish({results: results, total: 11000, truncated: true});
            compare(locate("searchResultsSummary").text, "Showing 500 of 11000 matches");
            const list = locate("searchResultsList");
            list.positionViewAtIndex(499, ListView.End);
            tryVerify(() => locate("searchResultRow_499") !== null);
            mouseDoubleClickSequence(locate("searchResultRow_499"), 10, 10);
            compare(controller.destination, [867, 889, 6]);
            panel.openSearch("unique", false); showResults();
            mouseClick(locate("searchResultContents_0"));
            wait(80);
            grabImage(scene.Window.window.contentItem).save("build/search-results-sidebar.png");
        }
    }
}
