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
        signal contentUpdated()
        function previewMonsterLabels(x, y, z, columns, rows) {
            ++queries
            return z === 7 ? [{name: "Rat", x: 101, y: 100, elevation: 4}] : []
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
        function test_healthBarPlacementAndScale() {
            camera.visualX = 100
            overlay.viewScale = 1
            const information = findChild(overlay, "monsterInformation")
            verify(information !== null)
            compare(information.x, 254.5)
            compare(information.y, 154)
            const fill = findChild(information, "monsterHealthFill")
            compare(fill.color, "#bf0a0a")
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
            map.contentUpdated()
            verify(map.queries > calls)
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
