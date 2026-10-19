import QtQuick
import QtTest
import "../../editor/qml/components" as Components
Item {
    id: scene
    width: 1920; height: 120
    QtObject {
        id: map
        property int brushServerId: 0
        property int selectionCount: 12345
        property int hoverX: 65535
        property int hoverY: 65535
        property int floor: 7
        property real tileSize: 14
        property string hoverText: "65535, 65535, 7  -  1 item(s): closed door"
        function zoomSteps(steps) { tileSize += steps; }
    }
    QtObject {
        id: timer
        property bool active: true
        property bool running: false
        property string taskName: "Blackridge Mine with a very long task name"
        property string taskElapsedText: "127:14:32"
        property string mapName: "Map"
        property var taskTimers: []
        property var history: []
        property bool checkpointEnabled: false
        property int breakReminderMinutes: 0
        property int idlePauseMinutes: 0
        property int operationCount: 0
        property int changedTileCount: 0
        property string todayText: "0:00:00"
        property string weekText: "0:00:00"
        property string totalText: "0:00:00"
        property string currentMapText: "0:00:00"
        function pauseSession() { running=false; }
        function resumeSession() { running=true; }
    }
    QtObject {
        id: activity
        property bool linked: false
        property bool busy: false
        property bool summaryLoading: false
        property int pendingCount: 0
        property string status: "Not connected"
        property string accountName: ""
        property string currentMapName: ""
        property string serverUrl: "http://localhost:3000"
        property string errorString: ""
        property string summaryError: ""
        property var summary: ({})
        signal changed()
    }
    Components.DmeStatusBar {
        id: bar
        width: scene.width; height: 44
        itemReader: QtObject {
            function clientIdForServerId(id) { return id + 100; }
            function nameForServerId(id) { return id === 10 ? "stone" : "door"; }
        }
        mapView: map; loaded: true; showFps: true; fps: 186
        timerComponent: Component { Components.WorkTimerStatus { statusBarMode: true; timerService: timer; activityService: activity } }
    }
    TestCase {
        name: "DmeStatusBar"
        when: windowShown
        function init() { map.brushServerId=0; bar.showWorkTimer=true; bar.showFps=true; scene.width=1920; map.floor=7; map.hoverX=65535; map.tileSize=14; }
        function test_responsiveGroupsNeverOverlap_data() {
            return [ {tag:"minimum",w:720}, {tag:"compact",w:900}, {tag:"boundary",w:1100}, {tag:"full",w:1280}, {tag:"laptop",w:1366}, {tag:"desktop",w:1920} ];
        }
        function test_responsiveGroupsNeverOverlap(data) {
            scene.width=data.w;
            wait(30);
            let right=0;
            for (const name of ["statusReady","statusSelection","statusPosition","statusItem","statusFloor","statusZoom","statusTimerSection","statusFps"]) {
                const item=findChild(bar,name);
                verify(item!==null);
                if (!item.visible) continue;
                const point=item.mapToItem(bar,0,0);
                verify(point.x >= right-.5,name+" overlaps previous section");
                verify(point.x+item.width <= bar.width+.5,name+" leaves the window");
                right=point.x+item.width;
            }
            const duration=findChild(bar,"workTimerDuration");
            const toggle=findChild(bar,"workTimerToggle");
            verify(duration.mapToItem(bar,duration.width,0).x <= toggle.mapToItem(bar,0,0).x,"Timer duration overlaps its button");
            verify(duration.implicitWidth <= duration.width,"Long duration clipped");
            const label=findChild(bar,"statusCoordinates");
            verify(label.implicitWidth <= findChild(bar,"statusPosition").width,"Coordinates do not fit");
            verify(findChild(bar,"statusZoomIn").width>=28);
        }
        function test_itemNameAndLiveFloor() {
            compare(bar.itemText,"closed door");
            const input=findChild(bar,"statusFloorControl");
            map.floor=3; compare(input.value,3);
            input.value=9; input.valueModified(); compare(map.floor,9);
            map.floor=12; compare(input.value,12);
        }
        function test_selectedItemThumbnailTracksBrush() {
            map.brushServerId=10;
            compare(bar.itemText,"stone"); compare(bar.itemClientId,110);
            const image=findChild(bar,"statusItemThumbnail");
            verify(image.visible); verify(image.source.toString().indexOf("/110/")>=0);
            map.brushServerId=20;
            compare(bar.itemText,"door"); verify(image.source.toString().indexOf("/120/")>=0);
            map.brushServerId=0;
            verify(!image.visible);
        }
        function test_zoomButtons() {
            const initial=map.tileSize;
            mouseClick(findChild(bar,"statusZoomIn")); compare(map.tileSize,initial+1);
            mouseClick(findChild(bar,"statusZoomOut")); compare(map.tileSize,initial);
        }
        function test_timerToggle() {
            timer.running=false;
            mouseClick(findChild(bar,"workTimerToggle")); compare(timer.running,true);
            mouseClick(findChild(bar,"workTimerToggle")); compare(timer.running,false);
        }
        function test_noCursorAndHiddenOptions() {
            map.hoverX=-1; compare(findChild(bar,"statusCoordinates").text,"X —  Y —  Z 7");
            bar.showWorkTimer=false; bar.showFps=false; wait(30);
            verify(!findChild(bar,"statusTimerSection").visible);
            verify(!findChild(bar,"statusFps").visible);
        }
    }
}
