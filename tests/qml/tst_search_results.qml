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
        function initTestCase() { scene.Window.window.width = 1000; scene.Window.window.height = 780; }
        function init() {
            controller.queryBusy = false; controller.searches = []; controller.destination = []; controller.cancellations = 0;
            panel.visible = true; panel.width = 340; Backend.uiTheme.style = "gray-dark";
            panel.openSearch("unique", false);
        }
        function cleanup() { panel.close(); controller.finish({cancelled: true}); }

        function test_navigationAndContents_data() {
            return ["classic", "windows-classic", "github", "gray-dark", "gray-modern", "fluent-dark", "tibia-ui"]
                .map(theme => ({tag: theme, theme: theme}));
        }
        function test_navigationAndContents(data) {
            Backend.uiTheme.style = data.theme;
            showResults();
            tryVerify(() => locate("searchResultRow_0") !== null);
            mouseClick(locate("searchResultContents_0"));
            compare(panel.rows.length, 4);
            tryVerify(() => locate("searchResultContents_0/0") !== null);
            mouseClick(locate("searchResultContents_0/0"));
            compare(panel.rows.length, 5);
            compare(panel.rows[2].item.serverId, 2148);
            compare(panel.rows[2].item.count, 72);
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
            compare(panel.rows.length, 2);
        }
        function test_filterKeyboardAndResize() {
            showResults();
            const field = locate("searchResultsFilter");
            field.text = "12698";
            compare(panel.rows.length, 1);
            compare(panel.rows[0].item.serverId, 12698);
            field.text = "5000"; compare(panel.rows.length, 2);
            field.text = "62378"; compare(panel.rows.length, 2);
            field.text = "backpack"; compare(panel.rows.length, 1);
            field.text = "missing"; compare(panel.rows.length, 0);
            verify(!locate("searchResultsGoTo").enabled);
            field.text = "";
            const list = locate("searchResultsList");
            list.forceActiveFocus(); keyClick(Qt.Key_Down); keyClick(Qt.Key_Right);
            compare(panel.rows.length, 4);
            keyClick(Qt.Key_Left); compare(panel.rows.length, 2);
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
