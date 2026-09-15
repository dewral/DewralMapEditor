import QtQuick
import QtQuick.Controls
import QtTest
import "../../editor/qml/dialogs" as Dialogs

Item {
    id: testRoot
    width: 1200; height: 850; visible: true
    Dialogs.AdvancedBrushEditor {
        id: editor
        mapCtrl: QtObject {
            function brushSelectionSnapshot(includeGround) {
                return {tiles:[{dx:0,dy:0,dz:0,items:[100,101]},{dx:1,dy:0,dz:1,items:[102]}]}
            }
        }
    }
    TestCase {
        name: "AdvancedBrushEditor"
        when: windowShown
        function test_editor() {
            editor.open()
            tryCompare(editor, "opened", true)
            compare(editor.kind, "walls")
            editor.addIds([100,101,100])
            compare(editor.pairs.length, 2)
            compare(editor.pairs[0][1], 100)
            editor.slotIndex = 6
            compare(editor.pairs.length, 0)
            editor.addIds([102])
            compare(editor.draft.items["0"].length, 2)
            compare(editor.draft.items["6"][0][0], 102)
            editor.reset("carpets")
            compare(editor.slotKeys.length, 13)
            editor.slotIndex = 12
            editor.addIds([200])
            compare(editor.draft.items.center[0][0], 200)
            editor.reset("doodads")
            editor.addIds([300])
            editor.addComposite(true)
            compare(editor.currentAlt.singles[0][0], 300)
            compare(editor.currentComposite.tiles.length, 2)
            compare(editor.currentComposite.tiles[1].dz, 1)
            editor.addComposite(false)
            compare(editor.currentAlt.composites.length, 2)
            editor.compositeIndex = 0
            wait(100)
            grabImage(testRoot).save("build/advanced-brush-editor-test.png")
            verify(editor.contentItem.width <= 1000, "Width: " + editor.contentItem.width)
            verify(editor.contentItem.height <= 720, "Height: " + editor.contentItem.height)
            editor.dirty = false
            editor.close()
        }
    }
}
