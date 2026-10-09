import QtQuick
import QtQuick.Controls
import QtTest
import Tibia 1.0
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
    QtObject {
        id: savingStore
        property var names: ["fiery wall", "other"]
        property var savedNames: ["other", "lava wall"]
        property var saveResult: ({success: true})
        property var savedArguments: null
        property var storedDraft: ({lookid: 11713, items: {"0": [[11713, 100]]}})
        function advancedBrushNames(kind) { return names }
        function advancedBrushEdit(kind, name) { return storedDraft }
        function saveAdvancedBrush(kind, name, originalName, draft) {
            savedArguments = {kind, name, originalName, draft: JSON.parse(JSON.stringify(draft))}
            if (saveResult.success) names = savedNames
            return saveResult
        }
    }
    TestCase {
        name: "AdvancedBrushEditor"
        when: windowShown
        property var previousBrushStore
        function init() {
            previousBrushStore = Backend.brushStore
            editor.dirty = false
            editor.reset("walls")
        }
        function cleanup() {
            editor.dirty = false
            editor.close()
            Backend.brushStore = previousBrushStore
        }
        function prepareRename() {
            savingStore.names = ["fiery wall", "other"]
            savingStore.savedNames = ["other", "lava wall"]
            savingStore.saveResult = {success: true}
            savingStore.savedArguments = null
            Backend.brushStore = savingStore
            editor.open()
            tryCompare(editor, "opened", true)
            editor.load("fiery wall")
            const combo = findChild(editor, "advancedBrushCombo")
            combo.currentIndex = 0
            const nameField = findChild(editor, "advancedBrushName")
            nameField.text = "  lava wall  "
            compare(editor.dirty, true)
            return nameField
        }
        function test_saveRenamesSelectedBrush() {
            const nameField = prepareRename()
            const draft = JSON.stringify(editor.draft)
            const saveButton = findChild(editor, "advancedBrushSave")
            mouseClick(saveButton, saveButton.width / 2, saveButton.height / 2)
            compare(savingStore.savedArguments.kind, "walls")
            compare(savingStore.savedArguments.originalName, "fiery wall")
            compare(savingStore.savedArguments.name, "  lava wall  ")
            compare(JSON.stringify(savingStore.savedArguments.draft), draft)
            compare(editor.originalName, "lava wall")
            compare(nameField.text, "lava wall")
            compare(editor.names, ["other", "lava wall"])
            compare(findChild(editor, "advancedBrushCombo").currentIndex, 1)
            compare(findChild(editor, "advancedBrushStatus").text, "Saved.")
            compare(editor.dirty, false)
            compare(editor.opened, true)
            // Subsequent saves must edit the renamed brush.
            mouseClick(saveButton, saveButton.width / 2, saveButton.height / 2)
            compare(savingStore.savedArguments.originalName, "lava wall")
        }
        function test_failedRenameKeepsDraft() {
            const nameField = prepareRename()
            const draft = JSON.stringify(editor.draft)
            savingStore.saveResult = {success: false, error: "That name already exists."}
            const saveButton = findChild(editor, "advancedBrushSave")
            mouseClick(saveButton, saveButton.width / 2, saveButton.height / 2)
            compare(editor.originalName, "fiery wall")
            compare(nameField.text, "  lava wall  ")
            compare(JSON.stringify(editor.draft), draft)
            compare(editor.names, ["fiery wall", "other"])
            compare(editor.dirty, true)
            compare(findChild(editor, "advancedBrushStatus").text, savingStore.saveResult.error)
        }
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
