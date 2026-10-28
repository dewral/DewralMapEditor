import QtQuick
import QtTest
import "../../editor/qml/components" as Components

Item {
    width: 600; height: 400
    QtObject {
        id: camera
        property real visualX: 100
        property real visualY: 100
        property int z: 7
    }
    QtObject {
        id: map
        property int queries: 0
        property QtObject otbm: QtObject { signal mapChanged() }
        property string creatureName: "Rat"
        property int creatureCount: 1
        signal contentUpdated()
        function previewMonsterLabels(x, y, z, columns, rows) {
            ++queries
            if (z !== 7) return [];
            const result=[];
            for(let i=0;i<creatureCount;++i)
                result.push({name: map.creatureName, x: 101+i%20, y: 100+Math.floor(i/20), elevation: 4});
            return result;
        }
    }
    Components.IngameMonsterOverlay {
        id: overlay
        width: 480; height: 352
        source: map; controller: camera
    }
    TestCase {
        name: "MonsterLabels"
        when: windowShown
        function test_map_camera_matches_renderer_pixel_rounding() {
            camera.visualX=100.25;camera.visualY=100;camera.z=7;
            overlay.viewScale=0.25;overlay.alignToMapPixels=true;
            const information=findChild(overlay,"monsterInformation");
            const firstX=information.x;
            camera.visualX=100.29;
            compare(information.x,firstX);
            camera.visualX=100.35;
            compare((information.x-firstX)*overlay.viewScale,-1);
            overlay.alignToMapPixels=false;overlay.viewScale=1;camera.visualX=100;
        }
        function test_healthBarPlacementAndScale() {
            camera.visualX = 100
            overlay.viewScale = 1
            const information = findChild(overlay, "monsterInformation")
            verify(information !== null)
            compare(information.x, 254)
            compare(information.y, 152)
            const fill = findChild(information, "monsterHealthFill")
            compare(fill.color, "#00c000")
            compare(fill.width, 25)
            compare(fill.height, 2)
            overlay.viewScale = 2
            compare(information.scale, 0.5)
            overlay.viewScale = 1
        }
        function test_visibleFloorAndSmoothMovement() {
            compare(overlay.creatures.length, 1)
            const calls = map.queries
            camera.visualX = 100.25
            compare(map.queries, calls) // Fractional walking does not rebuild the model.
            camera.z = 8
            compare(overlay.creatures.length, 0)
            camera.z = 7
            compare(overlay.creatures[0].name, "Rat")
            map.otbm.mapChanged()
            verify(map.queries > calls)
        }
        function test_panning_reuses_labels_and_skips_queries() {
            camera.visualX=100;camera.visualY=100;camera.z=7;
            overlay.refresh(true);
            const information=findChild(overlay,"monsterInformation");
            const calls=map.queries;
            for(let i=0;i<100;++i) {
                camera.visualX=100+i/100;
                map.contentUpdated();
            }
            compare(map.queries,calls);
            compare(findChild(overlay,"monsterInformation"),information);
            camera.visualX=120;
            verify(map.queries>calls);
            compare(findChild(overlay,"monsterInformation"),information);
            camera.visualX=100;
        }
        function test_edit_updates_existing_label() {
            overlay.refresh(true);
            const information=findChild(overlay,"monsterInformation");
            map.creatureName="Wolf";map.otbm.mapChanged();
            compare(overlay.creatures[0].name,"Wolf");
            compare(findChild(overlay,"monsterInformation"),information);
            compare(information.labelName,"Wolf");
            map.creatureName="Rat";map.otbm.mapChanged();
        }
        function test_dense_scene_does_not_rebuild_on_each_frame() {
            camera.visualX=100;camera.visualY=100;camera.z=7;
            map.creatureCount=500;map.otbm.mapChanged();
            compare(overlay.creatures.length,500);
            const calls=map.queries;
            const first=findChild(overlay,"monsterInformation");
            for(let i=0;i<200;++i) {
                camera.visualX=100+i/50;
                map.contentUpdated();
            }
            compare(map.queries,calls);
            compare(findChild(overlay,"monsterInformation"),first);
            map.creatureCount=1;map.otbm.mapChanged();camera.visualX=100;
        }
        function test_redBitmapPreservesFontMetrics() {
            const green = nameComponent.createObject(overlay, {text: "Rat"})
            const red = nameComponent.createObject(overlay, {text: "Rat", hostile: true})
            const blue = nameComponent.createObject(overlay, {text: "Rat", npc: true})
            compare(red.width, green.width)
            compare(red.height, green.height)
            verify(red.atlas.endsWith("otclient-name-red.png"))
            verify(blue.atlas.endsWith("otclient-name-blue.png"))
            compare(blue.width, green.width)
            green.destroy(); red.destroy(); blue.destroy()
        }
    }
    Component { id: nameComponent; Components.OtclientName {} }
}
