import QtQuick
import QtQuick.Window
import QtTest
import Tibia 1.0
import "../../editor/qml/dialogs" as Dialogs

Item {
    id: scene
    width: 1100; height: 820

    QtObject {
        id: controller
        property bool queryBusy: false
        property int queryProgress: 0
        property var searches: []
        property var destination: []
        property int cancellations: 0
        signal mapAnalysisFinished(var report)
        signal itemLocationSearchFinished(var result)
        function startMapAnalysis() { queryBusy = true; return true; }
        function startItemLocationSearch(id, clientId) {
            if (queryBusy) return false;
            searches = searches.concat([{id: id, clientId: clientId}]);
            queryBusy = true;
            return true;
        }
        function centerOnPosition(x, y, z) { destination = [x, y, z]; }
        function cancelMapQuery() {
            ++cancellations;
            finish({cancelled: true});
        }
        function finish(result) {
            queryBusy = false;
            itemLocationSearchFinished(result);
        }
    }

    Dialogs.MapAnalyzerDialog { id: analyzer; mapCtrl: controller }

    TestCase {
        name: "MapAnalyzer"
        when: windowShown
        function locate(name) { return findChild(analyzer.contentItem, "mapAnalyzer" + name); }
        function row(x, z, containerPath) {
            return {serverId: 1000, clientId: 0, name: "Stone wall", x: x, y: 200,
                    z: z, count: 600, containerPath: containerPath || ""};
        }
        function search(id, clientId) {
            locate("IdType").currentIndex = clientId ? 1 : 0;
            locate("ItemId").text = String(id);
            mouseClick(locate("Search"));
            compare(controller.searches[controller.searches.length - 1], {id: id, clientId: clientId});
        }
        function initTestCase() {
            scene.Window.window.width = 1100;
            scene.Window.window.height = 820;
        }
        function init() {
            Backend.uiTheme.style = "gray-dark";
            controller.queryBusy = false;
            controller.searches = [];
            controller.destination = [];
            controller.cancellations = 0;
            analyzer.page = 1;
            locate("IdType").currentIndex = 0;
            locate("ItemId").text = "";
            analyzer.floatingPositionInitialized = false;
            analyzer.open();
            tryCompare(analyzer, "opened", true);
            controller.queryBusy = false;
            controller.mapAnalysisFinished({usage: [row(10, 7)], problems: [], summary: {}, floors: []});
            wait(20);
        }
        function cleanup() {
            analyzer.close();
            tryCompare(analyzer, "visible", false);
        }

        function test_navigationKeepsAnalyzerOpen_data() {
            return ["classic", "windows-classic", "github", "gray-dark", "gray-modern", "fluent-dark"]
                .map(theme => ({tag: theme, theme: theme}));
        }
        function test_navigationKeepsAnalyzerOpen(data) {
            Backend.uiTheme.style = data.theme;
            search(1000, false);
            controller.finish({results: [row(100, 7), row(101, 12, "Chest > Bag")], total: 2});
            tryVerify(() => locate("UsageRow_1") !== null);
            mouseDoubleClickSequence(locate("UsageRow_1"));
            compare(controller.destination, [101, 200, 12]);
            verify(analyzer.visible);
            compare(analyzer.locations.length, 2);
            mouseClick(locate("UsageRow_0"));
            mouseClick(locate("GoTo"));
            compare(controller.destination, [100, 200, 7]);
            verify(analyzer.visible);
            verify(!analyzer.modal);
        }

        function test_clientIdEnterRefreshAndClear() {
            locate("IdType").currentIndex = 1;
            locate("ItemId").text = "100";
            locate("ItemId").focusEditor();
            keyClick(Qt.Key_Return);
            compare(controller.searches, [{id: 100, clientId: true}]);
            controller.finish({results: [row(30, 8)], total: 1});
            mouseClick(locate("Refresh"));
            compare(controller.searches.length, 2);
            compare(controller.searches[1], {id: 100, clientId: true});
            controller.finish({results: [], total: 0});
            compare(locate("LocationSummary").text, "No matching items found.");
            verify(!locate("GoTo").enabled);
            mouseClick(locate("ClearSearch"));
            compare(locate("ItemId").text, "");
            verify(!analyzer.locationsSearched);
            compare(analyzer.displayedUsage.length, 1);
            verify(analyzer.visible);
        }

        function test_resultLimitAndScrolling() {
            search(1000, false);
            const rows = [];
            for (var i = 0; i < 500; ++i) rows.push(row(i, 7));
            controller.finish({results: rows, total: 600, truncated: true});
            verify(locate("LocationSummary").text.indexOf("Showing 500 of 600") === 0);
            const list = locate("UsageList");
            compare(list.count, 500);
            list.positionViewAtIndex(499, ListView.End);
            tryVerify(() => locate("UsageRow_499") !== null);
            mouseDoubleClickSequence(locate("UsageRow_499"));
            compare(controller.destination, [499, 200, 7]);
            verify(analyzer.visible);
        }

        function test_refreshOverviewAfterSearch() {
            search(1000, false);
            controller.finish({results: [row(30, 8)], total: 1});
            analyzer.page = 0;
            wait(20);
            mouseClick(locate("Refresh"));
            verify(analyzer.loading);
            verify(!analyzer.searchingLocations);
            verify(!analyzer.locationsSearched);
            compare(controller.searches.length, 1);
            controller.queryBusy = false;
            controller.mapAnalysisFinished({usage: [row(10, 7)]});
        }

        function test_invalidAndBusySearches() {
            for (const invalid of ["", "abc", "0", "65536", "-1", "1.5"]) {
                locate("ItemId").text = invalid;
                verify(!locate("Search").enabled);
                analyzer.runLocationSearch();
            }
            compare(controller.searches.length, 0);
            search(1000, false);
            verify(!locate("Search").enabled);
            verify(!locate("IdType").enabled);
            verify(!locate("ItemId").enabled);
            analyzer.runLocationSearch();
            compare(controller.searches.length, 1);
            mouseClick(locate("Refresh"));
            compare(controller.cancellations, 1);
            verify(!analyzer.loading);
            verify(!analyzer.locationsSearched);
            verify(locate("Search").enabled);
        }

        function test_changedIdClearsStaleResults() {
            search(1000, false);
            controller.finish({results: [row(40, 9)], total: 1});
            mouseClick(locate("UsageRow_0"));
            locate("ItemId").text = "1001";
            verify(!analyzer.locationsSearched);
            verify(!locate("GoTo").enabled);
            compare(analyzer.displayedUsage, analyzer.usage);
        }

        function test_layout() {
            for (const theme of ["classic", "gray-dark", "fluent-dark"]) {
                Backend.uiTheme.style = theme;
                const field = locate("ItemId"), button = locate("Search");
                verify(field.mapToItem(analyzer.contentItem, field.width, 0).x
                       <= button.mapToItem(analyzer.contentItem, 0, 0).x);
                const refresh = locate("Refresh");
                verify(refresh.mapToItem(analyzer.contentItem, refresh.width, refresh.height).y
                       <= analyzer.contentItem.height + 1);
            }
            search(1000, false);
            controller.finish({results: [row(762, 7), row(840, 8), row(950, 10, "Chest > Bag")], total: 3});
            wait(100);
            grabImage(scene.Window.window.contentItem).save("build/map-analyzer-search.png");
        }
    }
}
