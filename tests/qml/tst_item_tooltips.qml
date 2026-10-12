import QtQuick
import QtTest
import "../../editor/qml/components" as Components

Item {
    width: 640; height: 400
    QtObject {
        id: prefs
        property bool showClientBox: false
        property bool showTooltips: true
        property bool showWaypoints: false
        property bool showHouses: false
        property bool showLightSources: false
    }
    QtObject {
        id: map
        property int floor: 7
        property int tileSize: 32
        property real originX: 0
        property real originY: 0
        property string caption: "id: 5956"
        property bool extra: false
        signal contentUpdated()
        function renderOriginX() { return originX; }
        function renderOriginY() { return originY; }
        function mapOverlayData() {
            const rows=[{kind:"tooltip",x:10,y:10,text:caption},
                        {kind:"container",x:12,y:10,text:"id: 9713",itemCount:1,
                         items:[{spriteIds:[],count:2}]}];
            if(extra) rows.unshift({kind:"tooltip",x:1,y:10,text:"id: 100"});
            return JSON.parse(JSON.stringify(rows));
        }
    }
    Components.MapOverlay { id: overlay; anchors.fill: parent; mapCtrl: map; settings: prefs }
    TestCase {
        name: "ItemTooltips"
        when: windowShown
        function init() {
            prefs.showTooltips=true;map.originX=0;map.originY=0;
            map.extra=false;map.caption="id: 5956";overlay.refreshData(true);
        }
        function test_refresh_preserves_tooltips_and_contents() {
            const text=findChild(overlay,"itemTooltip"),container=findChild(overlay,"containerTooltip");
            verify(text!==null);verify(container!==null);
            for(let i=0;i<20;++i){map.originX=i/10;overlay.refreshData(true);}
            compare(findChild(overlay,"itemTooltip"),text);
            compare(findChild(overlay,"containerTooltip"),container);
            compare(container.modelData.items[0].count,2);
            map.extra=true;overlay.refreshData(true);
            compare(findChild(overlay,"containerTooltip"),container);
            map.caption="id: 5956\naid: 5000";overlay.refreshData(true);
            compare(text.modelData.text,map.caption);
        }
        function test_position_matches_pixel_camera_without_edge_flip() {
            const text=findChild(overlay,"itemTooltip"),container=findChild(overlay,"containerTooltip");
            const textX=text.x,containerY=container.y;
            map.originX=0.005;map.contentUpdated();compare(text.x,textX);
            map.originX=0.04;map.contentUpdated();compare(text.x,textX-1);
            map.originY=9;map.contentUpdated();
            compare(container.y,containerY-288);
            verify(container.y<0); // Remains above its tile instead of jumping below it.
        }
        function test_toggle_removes_and_restores_tooltips() {
            prefs.showTooltips=false;compare(findChild(overlay,"itemTooltip"),null);
            prefs.showTooltips=true;overlay.refreshData(true);
            verify(findChild(overlay,"itemTooltip")!==null);
        }
    }
}
